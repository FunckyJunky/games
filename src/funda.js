// FUNDA WATCH: error.blue/other/funda keeps an eye on a house for sale on funda.nl.
//
//   GET  /api/funda/<id>         the listing + every reading so far (price, views, saves, status)
//   POST /api/funda/<id>/check   read Funda right now (at most once every 5 minutes)
//
// Every 30 minutes the cron in wrangler.jsonc calls fundaCron(), which reads each
// watched listing and saves one row in funda_snap. Once a day it also asks
// Walter Living for the price history of the address.
//
// Funda is read in two ways, the first that works wins:
//   1. "app":  the JSON API the Funda phone app uses (listing-detail-page.funda.io)
//   2. "web":  the normal www.funda.nl page, read as text

// The houses to watch: Funda's number (the last part of the address) -> its Funda page.
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
    ready = env.DB.batch(stmts).catch((e) => { ready = null; throw e; });
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
async function check(env, id) {
  await schema(env);
  const url = FUNDA_WATCH[id];
  const now = Date.now();
  const row = await env.DB.prepare(`SELECT info, history, history_try FROM funda_listing WHERE id = ?`).bind(id).first();
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
  return { ok: true, source: got.source, snap: s };
}

export async function fundaCron(env) {
  for (const id in FUNDA_WATCH) {
    try {
      const r = await check(env, id);
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
    count: rows.length, every: 30, now,
    snaps: thin(rows, now).map((r) => [r.ts, r.price, r.orig, r.views, r.saves, r.status, r.source]),
    ...extra,
  });
}

export async function handleFunda(request, env, url) {
  const m = url.pathname.match(/^\/api\/funda\/(\d{5,10})(\/check)?$/);
  if (!m) return json({ watching: Object.keys(FUNDA_WATCH) }, url.pathname === "/api/funda" ? 200 : 404);
  const id = m[1];
  if (!FUNDA_WATCH[id]) return json({ error: "this listing is not on the watch list" }, 404);

  if (m[2]) {
    if (request.method !== "POST") return json({ error: "use POST" }, 405);
    await schema(env);
    const row = await env.DB.prepare(`SELECT last_try FROM funda_listing WHERE id = ?`).bind(id).first();
    if (row && row.last_try && Date.now() - row.last_try < CHECK_GAP) {
      return view(env, id, { checked: false, wait: Math.ceil((CHECK_GAP - (Date.now() - row.last_try)) / 1000) });
    }
    const r = await check(env, id);
    return view(env, id, { checked: true, result: r });
  }
  return view(env, id);
}
