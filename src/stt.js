// stt.js: speech to text for EAR-CUBE (esp32/EarCube), Whisper large v3 turbo on Cloudflare Workers AI.
//   POST /api/stt?task=transcribe|translate&lang=nl&id=CUBE-XXXX   body: a WAV (the cube sends 16 kHz mono 16 bit)
//   -> {ok, text, lang, secs}   or {error} with 400 (not a WAV), 413 (too long), 429 (today's limit), 502 (Whisper)
// The cube cuts speech at its pauses, so a piece is 0.5 .. 30 s. Workers AI's free allowance is 10,000 neurons a
// day, about 214 minutes of audio with this model; STT_DAY_MINUTES (default 200) keeps the site under it.
// Usage per day is counted in D1 (stt_use): all together, and per address (hashed). The audio isn't kept.

const MODEL = "@cf/openai/whisper-large-v3-turbo";
const MAX_BYTES = 3 * 1024 * 1024;
const MAX_SECS = 60;
const DAY_MINUTES = 200;
const LANG_RE = /^[a-z]{2}$/;
const ID_RE = /^[A-Za-z0-9-]{1,24}$/;
// Whisper's well-known inventions on quiet or noisy audio (subtitle credits it learnt from films): dropped
const INVENTED = [/amara\.org/i, /ondertitel(d|ing) door/i, /untertitel (im auftrag|der|von)/i, /subtitles by/i,
  /sous-titr(es|age) (par|réalisés)/i, /transcribed by/i, /субтитр(ы|и) (сделал|створ|подготов)/i, /редактор субтитров/i];

function json(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "cache-control": "no-store" },
  });
}

let tableReady = null;
function usageTable(env) {
  if (!tableReady) {
    tableReady = env.DB.prepare(
      `CREATE TABLE IF NOT EXISTS stt_use (day TEXT NOT NULL, k TEXT NOT NULL, secs REAL NOT NULL DEFAULT 0,
         n INTEGER NOT NULL DEFAULT 0, PRIMARY KEY (day, k))`
    ).run().catch((e) => { tableReady = null; throw e; });
  }
  return tableReady;
}

// a WAV's length in seconds (null = not a PCM WAV)
function wavSeconds(b) {
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  const tag = (o) => String.fromCharCode(b[o], b[o + 1], b[o + 2], b[o + 3]);
  if (b.length < 44 || tag(0) !== "RIFF" || tag(8) !== "WAVE") return null;
  let o = 12, rate = 0, ch = 0, bits = 0;
  while (o + 8 <= b.length) {
    const id = tag(o), size = dv.getUint32(o + 4, true);
    if (id === "fmt " && size >= 16) { ch = dv.getUint16(o + 10, true); rate = dv.getUint32(o + 12, true); bits = dv.getUint16(o + 22, true); }
    if (id === "data") {
      if (!rate || !ch || !bits) return null;
      const bytes = Math.min(size, b.length - o - 8);
      return bytes / (rate * ch * (bits / 8));
    }
    o += 8 + size + (size & 1);
  }
  return null;
}

async function sha256hex(text) {
  const buf = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(text));
  return [...new Uint8Array(buf)].map((x) => x.toString(16).padStart(2, "0")).join("");
}

function base64(b) {
  let s = "";
  for (let i = 0; i < b.length; i += 0x8000) s += String.fromCharCode.apply(null, b.subarray(i, i + 0x8000));
  return btoa(s);
}

export async function handleStt(request, env, ctx) {
  if (request.method !== "POST") return json({ error: "send a WAV with POST" }, 405);
  if (!env.AI) return json({ error: "speech to text is not set up on this site" }, 503);
  const url = new URL(request.url);
  const task = url.searchParams.get("task") === "translate" ? "translate" : "transcribe";
  const lang = (url.searchParams.get("lang") || "").toLowerCase();
  const id = url.searchParams.get("id") || "";
  if (lang && !LANG_RE.test(lang)) return json({ error: "bad language" }, 400);
  if (id && !ID_RE.test(id)) return json({ error: "bad id" }, 400);
  const len = Number(request.headers.get("content-length") || 0);
  if (len > MAX_BYTES) return json({ error: "too long" }, 413);
  const audio = new Uint8Array(await request.arrayBuffer());
  if (audio.length > MAX_BYTES) return json({ error: "too long" }, 413);
  const secs = wavSeconds(audio);
  if (secs == null) return json({ error: "not a wav" }, 400);
  if (secs < 0.2) return json({ ok: true, text: "", secs });
  if (secs > MAX_SECS) return json({ error: "too long" }, 413);

  // today's minutes, all together and from this address
  const day = new Date().toISOString().slice(0, 10);
  const ip = request.headers.get("CF-Connecting-IP") || "local";
  const ipKey = "ip:" + (await sha256hex("stt:" + ip)).slice(0, 20);   // not the address itself
  const cap = Math.max(1, Number(env.STT_DAY_MINUTES) || DAY_MINUTES) * 60;
  let used = 0, mine = 0;
  if (env.DB) {
    try {
      await usageTable(env);
      const rows = await env.DB.prepare(`SELECT k, secs FROM stt_use WHERE day = ? AND k IN ('all', ?)`).bind(day, ipKey).all();
      for (const r of rows.results || []) { if (r.k === "all") used = r.secs; else mine = r.secs; }
    } catch (e) { /* no counting today: still answer */ }
  }
  if (used + secs > cap || mine + secs > cap * 0.75) return json({ error: "daily limit reached", used: Math.round(used / 60) }, 429);

  const input = { audio: base64(audio), task, vad_filter: true };
  if (lang) input.language = lang;
  let r;
  try {
    r = await env.AI.run(MODEL, input);
  } catch (e) {
    const msg = String((e && e.message) || e);
    // Workers AI says so when the account's daily allowance is used up
    if (/neuron|limit|quota|429/i.test(msg)) return json({ error: "daily limit reached" }, 429);
    return json({ error: "whisper: " + msg.slice(0, 120) }, 502);
  }
  let text = String((r && r.text) || "").replace(/\s+/g, " ").trim();
  if (INVENTED.some((re) => re.test(text))) text = "";

  if (env.DB) {
    const add = env.DB.prepare(
      `INSERT INTO stt_use (day, k, secs, n) VALUES (?, ?, ?, 1)
       ON CONFLICT(day, k) DO UPDATE SET secs = secs + excluded.secs, n = n + 1`
    );
    const p = env.DB.batch([add.bind(day, "all", secs), add.bind(day, ipKey, secs)]).catch(() => {});
    if (ctx && ctx.waitUntil) ctx.waitUntil(p); else await p;
  }
  const info = (r && r.transcription_info) || {};
  return json({ ok: true, text, lang: info.language || lang || "", secs: Math.round(secs * 10) / 10 });
}
