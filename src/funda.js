// FUNDA WATCH: error.blue/other/funda keeps an eye on a house for sale on funda.nl.
//
//   GET    /api/funda/<id>            the listing + every reading so far (price, views, saves, status)
//   POST   /api/funda/<id>/check      read Funda right now (at most once every 5 minutes)
//   PUT    /api/funda/<id>/settings   {every}: how often the watcher reads Funda, in minutes (30 ... 1440)
//   GET    /api/funda/<id>/notify?endpoint=...   this phone's notification choices
//   POST   /api/funda/<id>/notify     {sub, prefs}: turn notifications on / change them for one phone
//   DELETE /api/funda/<id>/notify     {endpoint}: turn them off
//   POST   /api/funda/<id>/notify/test {endpoint}: send that phone a test notification
//   GET    /api/funda/<id>/buren?n=20 the neighbours within 150 m with their WOZ values (src/buren.js)
//
// The cron in wrangler.jsonc runs every 30 minutes and calls fundaCron(), which reads a
// listing when it is due (its "every" setting, once a day if not set) and saves one row in
// funda_snap. Then it tells every phone that asked: price or status changed, new saves,
// every 50/100/... views. Once a day it also asks Walter Living for the price history.
//
// Funda is read in two ways, the first that works wins:
//   1. "app":  the JSON API the Funda phone app uses (listing-detail-page.funda.io)
//   2. "web":  the normal www.funda.nl page, read as text

// The houses to watch: Funda's number (the last part of the address) -> its Funda page.
import { handleBuren, handleForSale } from "./buren.js";

export const FUNDA_WATCH = {
  "44666454": "https://www.funda.nl/detail/koop/lelystad/huis-rozengaard-11-4/44666454/",
};

// What we saw on Funda when the watcher was made (24 Nov 2026), so the page has a start.
const SEED = {
  "44666454": {
    info: {
      address: "Rozengaard 11 4", postcode: "8212 DA", city: "Lelystad", neighbourhood: "Archipel, Beukenhof, Rozengaard",
      living: 122, plot: 146, rooms: 5, bedrooms: 4, energy: "B", built: "1978", onFunda: "30-10-2026",
    },
    snap: { ts: Date.UTC(2026, 10, 24, 11, 42), price: 425000, orig: 450000, views: 1409, saves: 50, status: "Beschikbaar" },
  },
};

const CHECK_GAP = 5 * 60 * 1000; // "check now" at most this often
const EVERY_CHOICES = [30, 60, 180, 360, 720, 1440]; // minutes; the cron runs every 30
const EVERY_DEFAULT = 1440; // once a day
const VIEW_STEPS = [0, 50, 100, 250, 500];
const MAX_SUBS = 50;
const HISTORY_GAP = 24 * 3600 * 1000; // Walter Living price history once a day

function json(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "cache-control": "no-store" },
  });
}

let ready = null;
function schema(env) {
  if (!ready) {
    const stmts = [
      env.DB.prepare(`CREATE TABLE IF NOT EXISTS funda_listing (
        id TEXT PRIMARY KEY, info TEXT NOT NULL DEFAULT '{}', history TEXT,
        last_try INTEGER, last_ok INTEGER, last_error TEXT, source TEXT, history_try INTEGER)`),
      env.DB.prepare(`CREATE TABLE IF NOT EXISTS funda_snap (
        id TEXT NOT NULL, ts INTEGER NOT NULL, price INTEGER, orig INTEGER, views INTEGER, saves INTEGER,
        status TEXT, source TEXT, PRIMARY KEY (id, ts))`),
    ];
    for (const id in SEED) {
      const s = SEED[id];
      stmts.push(env.DB.prepare(`INSERT OR IGNORE INTO funda_listing (id, info) VALUES (?, ?)`).bind(id, JSON.stringify(s.info)));
      stmts.push(env.DB.prepare(`INSERT OR IGNORE INTO funda_snap (id, ts, price, orig, views, saves, status, source) VALUES (?, ?, ?, ?, ?, ?, ?, 'seed')`)
        .bind(id, s.snap.ts, s.snap.price, s.snap.orig, s.snap.views, s.snap.saves, s.snap.status));
    }
    stmts.push(env.DB.prepare(`CREATE TABLE IF NOT EXISTS funda_subs (
      endpoint TEXT PRIMARY KEY, id TEXT NOT NULL, p256dh TEXT NOT NULL, auth TEXT NOT NULL,
      prefs TEXT NOT NULL, vmark INTEGER, created INTEGER NOT NULL)`));
    ready = env.DB.batch(stmts)
      .then(() => env.DB.prepare(`ALTER TABLE funda_listing ADD COLUMN every INTEGER`).run().catch(() => {})) // already there
      .catch((e) => { ready = null; throw e; });
  }
  return ready;
}

// ---------- reading Funda ----------

// the first number in a value: 1409, "1.409x" -> 1409, "122 m²" -> 122, "5 kamers (4 slaapkamers)" -> 5
const int = (v) => {
  if (v == null || v === "") return null;
  if (typeof v === "number") return Number.isFinite(v) ? Math.round(v) : null;
  const m = String(v).match(/\d{1,3}(?:\.\d{3})+(?!\d)|\d+/);
  return m ? parseInt(m[0].replace(/\./g, ""), 10) : null;
};
// an amount in euros: "€ 425.000 kosten koper" -> 425000; null when there is no € in it
const euro = (v) => {
  const m = String(v ?? "").match(/€\s*(\d{1,3}(?:\.\d{3})+|\d+)/);
  return m ? parseInt(m[1].replace(/\./g, ""), 10) : null;
};
const clean = (v) => (v == null ? null : String(v).replace(/\s+/g, " ").trim() || null);

const hex = (n) => Array.from(crypto.getRandomValues(new Uint8Array(n)), (b) => b.toString(16).padStart(2, "0")).join("");
const digits = (n) => Array.from(crypto.getRandomValues(new Uint8Array(n)), (b) => String(b % 10)).join("").replace(/^0/, "1");

// 1. The phone app's API (same headers as the Android app).
async function fromApp(id) {
  const trace = digits(19), parent = hex(7);
  const res = await fetch(`https://listing-detail-page.funda.io/api/v4/listing/object/nl/tinyId/${id}`, {
    headers: {
      "user-agent": "Dart/3.11 (dart:io)",
      "x-funda-app-platform": "android",
      "x-funda-app-version": "7.14.11",
      "content-type": "application/json",
      "accept-encoding": "gzip",
      "x-datadog-sampling-priority": "0",
      "x-datadog-origin": "rum",
      "x-datadog-parent-id": trace,
      "tracestate": `dd=s:0;o:rum;p:${parent}`,
      "traceparent": `00-${Math.floor(Date.now() / 1000).toString(16)}00000000${trace.slice(0, 16)}-${parent}-00`,
    },
  });
  if (!res.ok) throw new Error(`app API answered ${res.status}`);
  const d = await res.json();

  // the "Kenmerken" list: label -> value (e.g. "Status" -> "Beschikbaar")
  const kv = {};
  const walk = (list) => { for (const it of Array.isArray(list) ? list : []) { if (it && it.Label && it.Value != null) kv[it.Label] = it.Value; if (it) walk(it.KenmerkenList); } };
  for (const sec of Array.isArray(d.KenmerkSections) ? d.KenmerkSections : []) walk(sec && sec.KenmerkenList);

  const price = d.Price || {};
  const fast = d.FastView || {};
  const addr = d.AddressDetails || {};
  const photos = (d.Media && d.Media.Photos) || {};
  const first = (photos.Items || [])[0];
  const ins = d.ObjectInsights || {};

  const snap = {
    price: int(price.NumericSellingPrice) ?? euro(price.SellingPrice),
    orig: euro(kv["Oorspronkelijke vraagprijs"]),
    views: int(ins.Views),
    saves: int(ins.Saves),
    status: clean(kv["Status"]),
  };
  if (snap.price == null && snap.views == null) throw new Error("app API: no price or views in the answer");
  const info = {
    address: clean(addr.Title), postcode: clean(addr.PostCode), city: clean(addr.City), neighbourhood: clean(addr.NeighborhoodName),
    living: int(fast.LivingArea), plot: int(fast.PlotArea), bedrooms: int(fast.NumberOfBedrooms), rooms: int(kv["Aantal kamers"]),
    energy: clean(fast.EnergyLabel), built: clean(kv["Bouwjaar"]), published: clean(d.PublicationDate),
    since: clean(kv["Aangeboden sinds"]), priceText: clean(price.SellingPrice),
    photo: first && photos.MediaBaseUrl ? String(photos.MediaBaseUrl).replace("{id}", first.Id) : null,
    url: clean(d.Urls && d.Urls.FriendlyUrl && d.Urls.FriendlyUrl.FullUrl),
  };
  return { snap, info, source: "app" };
}

// HTML -> lines of visible text.
function textLines(html) {
  return html
    .replace(/<(script|style|noscript|svg|template)\b[\s\S]*?<\/\1>/gi, " ")
    .replace(/<br\s*\/?>/gi, "\n")
    .replace(/<[^>]+>/g, "\n")
    .replace(/&nbsp;|&#160;/g, " ").replace(/&euro;|&#8364;/g, "€").replace(/&sup2;|&#178;/g, "²")
    .replace(/&#(\d+);/g, (_, n) => String.fromCharCode(+n)).replace(/&amp;/g, "&").replace(/&quot;/g, '"').replace(/&#39;|&apos;/g, "'").replace(/&lt;/g, "<").replace(/&gt;/g, ">")
    .split("\n").map((s) => s.replace(/\s+/g, " ").trim()).filter(Boolean);
}

// 2. The website, read as text: "Vraagprijs" / "€ 425.000 kosten koper", "1.409x" / "Bekeken", ...
async function fromWeb(id, url) {
  const res = await fetch(url, {
    headers: {
      "user-agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36",
      "accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
      "accept-language": "nl-NL,nl;q=0.9,en;q=0.8",
    },
    redirect: "follow",
  });
  if (!res.ok) throw new Error(`website answered ${res.status}`);
  const html = await res.text();
  return parseWeb(html, id);
}

export function parseWeb(html, id) {
  const L = textLines(html);
  const all = L.join(" | ");
  const is = (i, label) => L[i] && L[i].toLowerCase() === label.toLowerCase();
  // the line after / before a label; with ok(), the first such line that passes the test
  const after = (label, ok = () => true) => { for (let i = 0; i < L.length - 1; i++) if (is(i, label) && ok(L[i + 1])) return L[i + 1]; return null; };
  const before = (label, ok = () => true) => { for (let i = 1; i < L.length; i++) if (is(i, label) && ok(L[i - 1])) return L[i - 1]; return null; };
  const pick = (re) => { const m = all.match(re); return m ? m[1] : null; };
  const hasEuro = (s) => /€\s*\d/.test(s);
  const times = (s) => /^[\d.]+\s*x$/i.test(s);

  const meta = (prop) => {
    const m = html.match(new RegExp(`<meta[^>]+(?:property|name)=["']${prop}["'][^>]*content=["']([^"']+)["']`, "i"))
      || html.match(new RegExp(`<meta[^>]+content=["']([^"']+)["'][^>]*(?:property|name)=["']${prop}["']`, "i"));
    return m ? m[1].replace(/&amp;/g, "&") : null;
  };

  const snap = {
    price: euro(after("Vraagprijs", hasEuro)) ?? euro(pick(/(€\s*[\d.]{5,})\s*(?:kosten koper|k\.k\.|vrij op naam|v\.o\.n\.)/i)),
    orig: euro(after("Oorspronkelijke vraagprijs", hasEuro)),
    views: int(before("Bekeken", times) ?? pick(/([\d.]+)\s*x\s*\|?\s*Bekeken/i) ?? pick(/Bekeken\s*\|\s*([\d.]+)\s*x/i)),
    saves: int(before("Bewaard", times) ?? pick(/([\d.]+)\s*x\s*\|?\s*Bewaard/i) ?? pick(/Bewaard\s*\|\s*([\d.]+)\s*x/i)),
    status: clean(after("Status", (s) => s.length < 40 && !/:/.test(s))),
  };
  if (snap.price == null && snap.views == null) {
    const why = /captcha|access denied|are you a robot|ben je een robot|cf-chl|datadome/i.test(html) ? "website showed a robot check" : "website: no price or views found";
    throw new Error(why);
  }
  const onFunda = before("Op Funda", (s) => /\d{1,2}-\d{1,2}-\d{4}/.test(s));
  const info = {
    photo: meta("og:image"),
    onFunda: onFunda && /\d{1,2}-\d{1,2}-\d{4}/.test(onFunda) ? onFunda : null,
    since: clean(after("Aangeboden sinds")),
    energy: clean(pick(/Energielabel\s*\|\s*([A-G]\+*)\b/)),
    built: clean(pick(/Bouwjaar\s*\|\s*(\d{4})/)),
    priceText: clean(after("Vraagprijs", hasEuro)),
  };
  if (info.since && /log in/i.test(info.since)) info.since = null;
  return { snap, info, source: "web" };
}

// 3. Walter Living: earlier asking prices / sales of the same address.
async function walterHistory(url, info) {
  if (!info.address || !info.postcode) throw new Error("no address yet");
  const res = await fetch("https://api.walterliving.com/hunter/lookup", {
    method: "POST",
    headers: { accept: "application/json", "content-type": "application/json" },
    body: JSON.stringify({ url, address: info.address, zipcode: info.postcode }),
  });
  if (!res.ok) throw new Error(`Walter answered ${res.status}`);
  const d = await res.json();
  if (d.status !== "ok") throw new Error("Walter: no history");
  const changes = (Array.isArray(d.changes) ? d.changes : []).map((c) => ({
    price: int(c.price), text: clean(c.human_price), status: clean(c.status), date: clean(c.date), ts: int(c.timestamp), source: clean(c.source), badge: clean(c.badge_text),
  }));
  return { changes, fetched: Date.now(), report: d.report && d.report.url ? String(d.report.url) : null };
}

// Read one listing and save what we saw.
async function check(env, id, push) {
  await schema(env);
  const url = FUNDA_WATCH[id];
  const now = Date.now();
  const row = await env.DB.prepare(`SELECT info, history, history_try FROM funda_listing WHERE id = ?`).bind(id).first();
  const prev = await env.DB.prepare(`SELECT price, orig, views, saves, status FROM funda_snap WHERE id = ? ORDER BY ts DESC LIMIT 1`).bind(id).first();
  let info = {};
  try { info = JSON.parse((row && row.info) || "{}"); } catch {}

  const errors = [];
  const msg = (e) => String((e && e.message) || e);
  let got = null;
  try { got = await fromApp(id); } catch (e) { errors.push(msg(e)); }
  // no answer, or an answer without views/saves: read the website too and fill the gaps
  if (!got || got.snap.views == null || got.snap.saves == null) {
    try {
      const web = await fromWeb(id, url);
      if (!got) got = web;
      else {
        for (const k in web.snap) if (got.snap[k] == null && web.snap[k] != null) got.snap[k] = web.snap[k];
        for (const k in web.info) if (got.info[k] == null && web.info[k] != null) got.info[k] = web.info[k];
        got.source = "app+web";
      }
    } catch (e) { errors.push(msg(e)); }
  }

  if (!got) {
    await env.DB.prepare(`INSERT INTO funda_listing (id, last_try, last_error) VALUES (?, ?, ?)
      ON CONFLICT(id) DO UPDATE SET last_try = excluded.last_try, last_error = excluded.last_error`).bind(id, now, errors.join(" · ")).run();
    return { ok: false, error: errors.join(" · ") };
  }

  // keep earlier info when this reading did not have it
  for (const k in got.info) if (got.info[k] != null) info[k] = got.info[k];
  const s = got.snap;
  const stmts = [
    env.DB.prepare(`INSERT OR REPLACE INTO funda_snap (id, ts, price, orig, views, saves, status, source) VALUES (?, ?, ?, ?, ?, ?, ?, ?)`)
      .bind(id, now, s.price, s.orig, s.views, s.saves, s.status, got.source),
    env.DB.prepare(`INSERT INTO funda_listing (id, info, last_try, last_ok, last_error, source) VALUES (?, ?, ?, ?, ?, ?)
      ON CONFLICT(id) DO UPDATE SET info = excluded.info, last_try = excluded.last_try, last_ok = excluded.last_ok, last_error = excluded.last_error, source = excluded.source`)
      .bind(id, JSON.stringify(info), now, now, errors.length ? "(used " + got.source + ") " + errors.join(" · ") : null, got.source),
  ];
  await env.DB.batch(stmts);

  // price history from Walter Living, once a day (a failure here never stops the watcher)
  if (!row || !row.history_try || now - row.history_try > HISTORY_GAP) {
    await env.DB.prepare(`UPDATE funda_listing SET history_try = ? WHERE id = ?`).bind(now, id).run();
    try {
      const h = await walterHistory(info.url || url, info);
      await env.DB.prepare(`UPDATE funda_listing SET history = ? WHERE id = ?`).bind(JSON.stringify(h), id).run();
    } catch (e) {
      console.log("funda: walter", id, String((e && e.message) || e));
    }
  }
  let sent = 0;
  if (push) {
    try { sent = await notify(env, id, push, prev, s, info); } catch (e) { console.error("funda: notify", id, e); }
  }
  return { ok: true, source: got.source, snap: s, sent };
}

// ---------- notifications ----------

const nl = (v) => (v == null ? "?" : Math.round(v).toLocaleString("nl-NL"));
const money = (v) => "€ " + nl(v);
const signed = (v, f = nl) => (v > 0 ? "+" : v < 0 ? "−" : "±") + f(Math.abs(v)).replace("€ ", "€");

// What changed since the last reading, for one phone's choices.
export function changesFor(prefs, prev, s, vmark) {
  const lines = [];
  let title = null, mark = vmark;
  if (prev) {
    if (prefs.price && prev.price != null && s.price != null && prev.price !== s.price) {
      const d = s.price - prev.price;
      title = d < 0 ? "Price lowered" : "Price raised";
      lines.push(`${money(prev.price)} → ${money(s.price)} (${signed(d, money)}, ${signed(Math.round(d / prev.price * 1000) / 10, (x) => x.toLocaleString("nl-NL"))}%)`);
    }
    if (prefs.status && prev.status && s.status && prev.status !== s.status) {
      title = title || "Status: " + s.status;
      lines.push(`Status ${prev.status} → ${s.status}`);
    }
    if (prefs.saves && prev.saves != null && s.saves != null && s.saves !== prev.saves) {
      title = title || (s.saves > prev.saves ? "Saved again" : "Saves changed");
      lines.push(`${nl(s.saves)} saves (${signed(s.saves - prev.saves)})`);
    }
  }
  const step = prefs.views | 0;
  if (step > 0 && s.views != null) {
    const reached = Math.floor(s.views / step) * step;
    if (mark == null) mark = reached; // first reading after turning it on: start counting from here
    else if (reached > mark) {
      title = title || `${nl(reached)} views`;
      lines.push(`Passed ${nl(reached)} views (now ${nl(s.views)}${s.saves != null ? ", " + nl(s.saves) + " saves" : ""})`);
      mark = reached;
    }
  }
  return { title, lines, mark };
}

async function notify(env, id, push, prev, s, info) {
  const { results } = await env.DB.prepare(`SELECT endpoint, p256dh, auth, prefs, vmark FROM funda_subs WHERE id = ?`).bind(id).all();
  let sent = 0;
  const where = info.address || "Funda " + id;
  for (const sub of results || []) {
    let prefs = {};
    try { prefs = JSON.parse(sub.prefs); } catch {}
    const c = changesFor(prefs, prev, s, sub.vmark);
    if (c.mark !== sub.vmark) await env.DB.prepare(`UPDATE funda_subs SET vmark = ? WHERE endpoint = ?`).bind(c.mark, sub.endpoint).run();
    if (!c.lines.length || !push.ok(sub.endpoint)) continue;
    const msg = { title: `🏠 ${c.title} · ${where}`, body: c.lines.join("\n"), tag: "funda-" + id, url: "/other/funda/" + (id === "44666454" ? "" : "?id=" + id), icon: "/other/funda/icon-192.png" };
    try { await push.send(env, sub, msg); sent++; } catch (e) { console.error("funda: push", e); }
  }
  return sent;
}

async function handleNotify(request, env, id, push, test) {
  await schema(env);
  if (!push) return json({ error: "push is not set up" }, 500);
  if (request.method === "GET") {
    const ep = new URL(request.url).searchParams.get("endpoint") || "";
    const row = await env.DB.prepare(`SELECT prefs FROM funda_subs WHERE endpoint = ? AND id = ?`).bind(ep, id).first();
    return json({ on: !!row, prefs: row ? JSON.parse(row.prefs) : null });
  }
  let body;
  try { body = await request.json(); } catch { return json({ error: "bad request" }, 400); }
  const sub = body.sub || {};
  const endpoint = String(sub.endpoint || body.endpoint || "");
  if (!push.ok(endpoint)) return json({ error: "not a push address" }, 400);

  if (test) {
    if (request.method !== "POST") return json({ error: "use POST" }, 405);
    const row = await env.DB.prepare(`SELECT endpoint, p256dh, auth FROM funda_subs WHERE endpoint = ? AND id = ?`).bind(endpoint, id).first();
    if (!row) return json({ error: "notifications are not on for this phone" }, 404);
    const last = await env.DB.prepare(`SELECT price, views, saves FROM funda_snap WHERE id = ? ORDER BY ts DESC LIMIT 1`).bind(id).first();
    const status = await push.send(env, row, {
      title: "🏠 FUNDA WATCH works",
      body: last ? `${money(last.price)} · ${nl(last.views)} views · ${nl(last.saves)} saves` : "You'll get a message when something changes.",
      tag: "funda-test", url: "/other/funda/", icon: "/other/funda/icon-192.png",
    });
    return json({ ok: status >= 200 && status < 300, status });
  }
  if (request.method === "DELETE") {
    await env.DB.prepare(`DELETE FROM funda_subs WHERE endpoint = ?`).bind(endpoint).run();
    return json({ ok: true, on: false });
  }
  if (request.method !== "POST") return json({ error: "use GET, POST or DELETE" }, 405);
  const keys = sub.keys || {};
  if (!/^[A-Za-z0-9_-]{80,100}$/.test(keys.p256dh || "") || !/^[A-Za-z0-9_-]{16,32}$/.test(keys.auth || "")) return json({ error: "bad keys" }, 400);
  const p = body.prefs || {};
  const prefs = { price: !!p.price, status: !!p.status, saves: !!p.saves, views: VIEW_STEPS.includes(p.views | 0) ? p.views | 0 : 0 };
  const old = await env.DB.prepare(`SELECT prefs, vmark FROM funda_subs WHERE endpoint = ?`).bind(endpoint).first();
  if (!old) {
    const n = await env.DB.prepare(`SELECT COUNT(*) AS n FROM funda_subs`).first();
    if (n && n.n >= MAX_SUBS) return json({ error: "full" }, 429);
  }
  // count views from now on (and again whenever the step changes)
  let vmark = old ? old.vmark : null;
  let oldStep = 0;
  try { oldStep = old ? JSON.parse(old.prefs).views | 0 : 0; } catch {}
  if (!old || oldStep !== prefs.views) {
    const last = await env.DB.prepare(`SELECT views FROM funda_snap WHERE id = ? ORDER BY ts DESC LIMIT 1`).bind(id).first();
    vmark = prefs.views && last && last.views != null ? Math.floor(last.views / prefs.views) * prefs.views : null;
  }
  await env.DB.prepare(`INSERT INTO funda_subs (endpoint, id, p256dh, auth, prefs, vmark, created) VALUES (?, ?, ?, ?, ?, ?, ?)
    ON CONFLICT(endpoint) DO UPDATE SET id = excluded.id, p256dh = excluded.p256dh, auth = excluded.auth, prefs = excluded.prefs, vmark = excluded.vmark`)
    .bind(endpoint, id, keys.p256dh, keys.auth, JSON.stringify(prefs), vmark, Date.now()).run();
  return json({ ok: true, on: true, prefs });
}

// ---------- when to read Funda ----------

// the cron fires at :07 and :37 (UTC); a reading is due a few minutes early so it never slips a run
function due(row, now) {
  const every = (row && row.every) || EVERY_DEFAULT;
  const sinceOk = now - ((row && row.last_ok) || 0), sinceTry = now - ((row && row.last_try) || 0);
  // after a failure, try again within the hour even on a slow schedule
  return sinceOk >= every * 60000 - 8 * 60000 && sinceTry >= Math.min(every, 60) * 60000 - 8 * 60000;
}
function nextRun(row, now) {
  const every = (row && row.every) || EVERY_DEFAULT;
  const earliest = Math.max(((row && row.last_ok) || 0) + every * 60000, ((row && row.last_try) || 0) + Math.min(every, 60) * 60000) - 8 * 60000;
  let t = Math.max(earliest, now);
  const d = new Date(t), m = d.getUTCMinutes();
  d.setUTCSeconds(0, 0);
  if (m <= 7) d.setUTCMinutes(7); else if (m <= 37) d.setUTCMinutes(37); else { d.setUTCHours(d.getUTCHours() + 1); d.setUTCMinutes(7); }
  if (d.getTime() < t) d.setUTCMinutes(d.getUTCMinutes() + 30);
  return d.getTime();
}

export async function fundaCron(env, push) {
  await schema(env);
  const now = Date.now();
  for (const id in FUNDA_WATCH) {
    try {
      const row = await env.DB.prepare(`SELECT last_ok, last_try, every FROM funda_listing WHERE id = ?`).bind(id).first();
      if (!due(row, now)) continue;
      const r = await check(env, id, push);
      console.log("funda", id, JSON.stringify(r));
    } catch (e) {
      console.error("funda", id, e);
    }
  }
}

// Old readings are thinned for the page: everything from the last 7 days, before that
// one reading per 3 hours, plus every reading where the price, status or source changed.
function thin(rows, now) {
  const out = [];
  let prev = null, bucket = null;
  for (const r of rows) {
    const b = Math.floor(r.ts / (3 * 3600 * 1000));
    const changed = !prev || prev.price !== r.price || prev.status !== r.status || prev.orig !== r.orig;
    if (changed || r.ts > now - 7 * 86400 * 1000 || b !== bucket) out.push(r);
    bucket = b;
    prev = r;
  }
  if (rows.length && out[out.length - 1] !== rows[rows.length - 1]) out.push(rows[rows.length - 1]);
  return out;
}

async function view(env, id, extra = {}) {
  await schema(env);
  const [row, snaps] = await Promise.all([
    env.DB.prepare(`SELECT * FROM funda_listing WHERE id = ?`).bind(id).first(),
    env.DB.prepare(`SELECT ts, price, orig, views, saves, status, source FROM funda_snap WHERE id = ? ORDER BY ts`).bind(id).all(),
  ]);
  const now = Date.now();
  let info = {}, history = null;
  try { info = JSON.parse((row && row.info) || "{}"); } catch {}
  try { history = row && row.history ? JSON.parse(row.history) : null; } catch {}
  const rows = snaps.results || [];
  return json({
    id, url: FUNDA_WATCH[id], info, history,
    lastTry: row ? row.last_try : null, lastOk: row ? row.last_ok : null, lastError: row ? row.last_error : null, source: row ? row.source : null,
    count: rows.length, every: (row && row.every) || EVERY_DEFAULT, choices: EVERY_CHOICES, next: nextRun(row, now), now,
    snaps: thin(rows, now).map((r) => [r.ts, r.price, r.orig, r.views, r.saves, r.status, r.source]),
    ...extra,
  });
}

export async function handleFunda(request, env, url, push) {
  const m = url.pathname.match(/^\/api\/funda\/(\d{5,10})(?:\/(check|settings|notify|buren|forsale)(\/test)?)?$/);
  if (!m) return json({ watching: Object.keys(FUNDA_WATCH) }, url.pathname === "/api/funda" ? 200 : 404);
  const id = m[1], what = m[2];
  if (!FUNDA_WATCH[id]) return json({ error: "this listing is not on the watch list" }, 404);
  if (m[3] && what !== "notify") return json({ error: "not found" }, 404);

  if (what === "check") {
    if (request.method !== "POST") return json({ error: "use POST" }, 405);
    await schema(env);
    const row = await env.DB.prepare(`SELECT last_try FROM funda_listing WHERE id = ?`).bind(id).first();
    if (row && row.last_try && Date.now() - row.last_try < CHECK_GAP) {
      return view(env, id, { checked: false, wait: Math.ceil((CHECK_GAP - (Date.now() - row.last_try)) / 1000) });
    }
    const r = await check(env, id, push);
    return view(env, id, { checked: true, result: r });
  }
  if (what === "settings") {
    if (request.method !== "PUT") return json({ error: "use PUT" }, 405);
    let body;
    try { body = await request.json(); } catch { return json({ error: "bad request" }, 400); }
    const every = body.every | 0;
    if (!EVERY_CHOICES.includes(every)) return json({ error: "every must be one of " + EVERY_CHOICES.join(", ") }, 400);
    await schema(env);
    await env.DB.prepare(`INSERT INTO funda_listing (id, every) VALUES (?, ?) ON CONFLICT(id) DO UPDATE SET every = excluded.every`).bind(id, every).run();
    return view(env, id, { saved: true });
  }
  if (what === "notify") return handleNotify(request, env, id, push, !!m[3]);
  if (what === "buren" || what === "forsale") {
    if (request.method !== "GET") return json({ error: "use GET" }, 405);
    await schema(env);
    const row = await env.DB.prepare(`SELECT info FROM funda_listing WHERE id = ?`).bind(id).first();
    let info = {};
    try { info = JSON.parse((row && row.info) || "{}"); } catch {}
    return what === "buren" ? handleBuren(request, env, url, id, info) : handleForSale(request, env, url, id, info);
  }
  return view(env, id);
}
