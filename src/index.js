// Game Lab server: everything under /api/* runs here.
// All other URLs are served straight from the public/ folder.
//
//   GET  /api/scores?game=pixel-hop&limit=10   top scores for a game
//   POST /api/scores  {game, name, score}      save a score
//   GET  /api/save/<game>/<playerId>           load a player's saved progress
//   PUT  /api/save/<game>/<playerId>           save a player's progress (JSON body)
//   GET  /api/room/<game>/<CODE>               multiplayer room (WebSocket)
//   GET  /api/wall                             the shared Pixel Wall (WebSocket)
//   GET  /api/chat/<CODE>                      ERROR-CHAT channel (WebSocket)
//   GET  /api/tusa?char=VITO                   Туса У Берліні: the shared Hoongy (WebSocket)
//   GET  /api/vgadai/<SSEE>?name=X&mode=create Вгадай що?!: one episode of the quiz show (WebSocket)
//   GET  /api/vgadai/live                      Вгадай що?!: episodes on air
//   GET  /api/vgadai/<SSEE>/info               Вгадай що?!: one episode's leaderboard (for the lobby)
//   POST /api/vgadai/look {name, new}          Вгадай що?!: a player's random look
//   GET  /api/health                           quick check that the server runs

import { DurableObject } from "cloudflare:workers";

const GAME_RE = /^[a-z0-9-]{1,40}$/;
const PLAYER_RE = /^[A-Za-z0-9_-]{8,64}$/;
const ROOM_RE = /^[A-Za-z0-9]{3,8}$/;
const MAX_SAVE_BYTES = 64 * 1024;
const MAX_PLAYERS_PER_ROOM = 16;

function json(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "cache-control": "no-store" },
  });
}

function cleanName(value) {
  return String(value ?? "")
    .replace(/[\u0000-\u001f\u007f<>]/g, "")
    .trim()
    .slice(0, 16);
}

// Create the tables the first time the database is used.
let schemaReady = null;
function ensureSchema(env) {
  if (!schemaReady) {
    schemaReady = env.DB.batch([
      env.DB.prepare(
        `CREATE TABLE IF NOT EXISTS scores (
           id INTEGER PRIMARY KEY AUTOINCREMENT,
           game TEXT NOT NULL,
           name TEXT NOT NULL,
           score INTEGER NOT NULL,
           created_at TEXT NOT NULL DEFAULT (datetime('now'))
         )`
      ),
      env.DB.prepare(`CREATE INDEX IF NOT EXISTS scores_by_game ON scores (game, score DESC)`),
      env.DB.prepare(
        `CREATE TABLE IF NOT EXISTS saves (
           game TEXT NOT NULL,
           player TEXT NOT NULL,
           data TEXT NOT NULL,
           updated_at TEXT NOT NULL DEFAULT (datetime('now')),
           PRIMARY KEY (game, player)
         )`
      ),
    ]).catch((err) => {
      schemaReady = null;
      throw err;
    });
  }
  return schemaReady;
}

async function handleScores(request, env, url) {
  await ensureSchema(env);

  if (request.method === "GET") {
    const game = url.searchParams.get("game") || "";
    if (!GAME_RE.test(game)) return json({ error: "unknown game" }, 400);
    const limit = Math.min(50, Math.max(1, parseInt(url.searchParams.get("limit") || "10", 10) || 10));
    const { results } = await env.DB.prepare(
      `SELECT name, score, created_at FROM scores WHERE game = ? ORDER BY score DESC, id ASC LIMIT ?`
    )
      .bind(game, limit)
      .all();
    return json({ game, scores: results });
  }

  if (request.method === "POST") {
    let body;
    try {
      body = await request.json();
    } catch {
      return json({ error: "send JSON" }, 400);
    }
    const game = String(body?.game || "");
    const name = cleanName(body?.name) || "Player";
    const score = Number(body?.score);
    if (!GAME_RE.test(game)) return json({ error: "unknown game" }, 400);
    if (!Number.isInteger(score) || score < 0 || score > 1_000_000_000) return json({ error: "bad score" }, 400);

    await env.DB.prepare(`INSERT INTO scores (game, name, score) VALUES (?, ?, ?)`).bind(game, name, score).run();
    const higher = await env.DB.prepare(`SELECT COUNT(*) AS n FROM scores WHERE game = ? AND score > ?`)
      .bind(game, score)
      .first("n");
    return json({ ok: true, rank: (higher ?? 0) + 1 }, 201);
  }

  return json({ error: "method not allowed" }, 405);
}

async function handleSave(request, env, game, player) {
  await ensureSchema(env);

  if (request.method === "GET") {
    const row = await env.DB.prepare(`SELECT data, updated_at FROM saves WHERE game = ? AND player = ?`)
      .bind(game, player)
      .first();
    if (!row) return json({ found: false, data: null });
    return json({ found: true, data: JSON.parse(row.data), updated_at: row.updated_at });
  }

  if (request.method === "PUT" || request.method === "POST") {
    const text = await request.text();
    if (text.length > MAX_SAVE_BYTES) return json({ error: "save is too big (max 64 KB)" }, 413);
    try {
      JSON.parse(text);
    } catch {
      return json({ error: "send JSON" }, 400);
    }
    await env.DB.prepare(
      `INSERT INTO saves (game, player, data, updated_at) VALUES (?, ?, ?, datetime('now'))
       ON CONFLICT (game, player) DO UPDATE SET data = excluded.data, updated_at = excluded.updated_at`
    )
      .bind(game, player, text)
      .run();
    return json({ ok: true });
  }

  if (request.method === "DELETE") {
    await env.DB.prepare(`DELETE FROM saves WHERE game = ? AND player = ?`).bind(game, player).run();
    return json({ ok: true });
  }

  return json({ error: "method not allowed" }, 405);
}


// ---------------------------------------------------------------------------
// Programs with an access code (Leviathan). The 5-digit code is chosen the
// first time someone opens the game; only a salted hash is kept, in D1 (not in
// this public repository). A Worker secret LEVIATHAN_CODE, if set, wins.
// A right code sets a cookie for 30 days; the game's files need that cookie.
// ---------------------------------------------------------------------------
const LOCKED = { "leviathan-voyage": "lv_key" };
// games that live at their own short address instead of /games/<id>/
const SHORT_PATHS = { "leviathan-voyage": "/voyage" };
// added to every game page: app mode, no zoom on double-tap or pinch
const NO_ZOOM_HEAD = '<meta name="mobile-web-app-capable" content="yes"><meta name="apple-mobile-web-app-capable" content="yes">' +
  '<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">' +
  '<style>html,body{touch-action:manipulation;overscroll-behavior:none;-webkit-text-size-adjust:100%}*{-webkit-tap-highlight-color:transparent}</style>' +
  '<script src="/lib/nozoom.js"></script>';
const LOCK_TRIES = 8, LOCK_WINDOW_MS = 10 * 60 * 1000;
async function sha256hex(text) {
  const buf = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(text));
  return [...new Uint8Array(buf)].map((b) => b.toString(16).padStart(2, "0")).join("");
}
function lockedGame(path) {
  for (const id in SHORT_PATHS) if (path === SHORT_PATHS[id] || path.startsWith(SHORT_PATHS[id] + "/")) return LOCKED[id] ? id : null;
  const m = path.match(/^\/games\/([a-z0-9-]+)\//);
  return m && LOCKED[m[1]] ? m[1] : null;
}
const isGamePath = (path) => path.startsWith("/games/") || Object.values(SHORT_PATHS).some((p) => path === p || path.startsWith(p + "/"));
let lockTables = false;
async function lockDb(env) {
  if (!lockTables) {
    await env.DB.batch([
      env.DB.prepare("CREATE TABLE IF NOT EXISTS settings (k TEXT PRIMARY KEY, v TEXT NOT NULL)"),
      env.DB.prepare("CREATE TABLE IF NOT EXISTS unlock_tries (ip TEXT NOT NULL, ts INTEGER NOT NULL)"),
    ]);
    lockTables = true;
  }
  return env.DB;
}
// programs with a fixed code: only a salted hash of it is written here (it's a
// fun lock, not strong security)
const LOCK_FIXED = {
  "leviathan-voyage": { salt: "a3e2a88b9906e46afc3adb34115d9eff", hash: "e6674910dc64ccfc2a5c0a589ce404c5d4ea77d1153a6d98095ab40109cf1334" },
};
// what the code must hash to: { salt, hash } or null if no code was chosen yet
async function lockRecord(env, game) {
  if (env.LEVIATHAN_CODE && game === "leviathan-voyage") return { salt: "secret", hash: await sha256hex("secret:" + env.LEVIATHAN_CODE), fixed: true };
  if (LOCK_FIXED[game]) return { ...LOCK_FIXED[game], fixed: true };
  const db = await lockDb(env);
  const row = await db.prepare("SELECT v FROM settings WHERE k = ?").bind("lock:" + game).first();
  return row ? JSON.parse(row.v) : null;
}
async function lockToken(rec, game) { return sha256hex("cookie:" + game + ":" + rec.hash); }
function readCookie(request, name) {
  const m = (request.headers.get("Cookie") || "").match(new RegExp("(?:^|;\\s*)" + name + "=([a-f0-9]{64})"));
  return m ? m[1] : null;
}
async function isUnlocked(request, env, game) {
  const rec = await lockRecord(env, game);
  if (!rec) return false;
  const c = readCookie(request, LOCKED[game]);
  return !!c && c === (await lockToken(rec, game));
}
async function triesLeft(env, ip) {
  const db = await lockDb(env), since = Date.now() - LOCK_WINDOW_MS;
  const row = await db.prepare("SELECT COUNT(*) AS n, MIN(ts) AS first FROM unlock_tries WHERE ip = ? AND ts > ?").bind(ip, since).first();
  return { left: LOCK_TRIES - (row ? row.n : 0), wait: row && row.first ? Math.ceil((row.first + LOCK_WINDOW_MS - Date.now()) / 1000) : 0 };
}
async function handleUnlock(request, env, url) {
  const ip = request.headers.get("CF-Connecting-IP") || "local";
  if (request.method === "GET") {
    const game = url.searchParams.get("game") || "";
    if (!LOCKED[game]) return json({ unlocked: true });
    const rec = await lockRecord(env, game);
    return json({ unlocked: rec ? await isUnlocked(request, env, game) : false, setup: !rec, fixed: !!(rec && rec.fixed) });
  }
  if (request.method !== "POST") return json({ error: "use GET or POST" }, 405);
  let body;
  try { body = await request.json(); } catch { return json({ error: "bad request" }, 400); }
  const game = String(body.game || ""), code = String(body.code || "");
  if (!LOCKED[game]) return json({ error: "unknown program" }, 400);
  if (!/^\d{5}$/.test(code)) return json({ error: "the code is 5 digits" }, 400);
  const t = await triesLeft(env, ip);
  if (t.left <= 0) return json({ ok: false, wait: t.wait }, 429);
  let rec = await lockRecord(env, game);
  // changing the code: the old code, then the new one (old cookies stop working)
  if (body.newCode != null) {
    const neu = String(body.newCode);
    if (!/^\d{5}$/.test(neu)) return json({ error: "the code is 5 digits" }, 400);
    if (!rec) return json({ ok: false, setup: true }, 409);
    if (rec.fixed) return json({ ok: false, fixed: true }, 409);
    const db = await lockDb(env);
    if ((await sha256hex(rec.salt + ":" + code)) !== rec.hash) {
      await db.prepare("INSERT INTO unlock_tries (ip, ts) VALUES (?, ?)").bind(ip, Date.now()).run();
      return json({ ok: false, left: t.left - 1 }, 403);
    }
    const salt = [...crypto.getRandomValues(new Uint8Array(16))].map((b) => b.toString(16).padStart(2, "0")).join("");
    rec = { salt, hash: await sha256hex(salt + ":" + neu) };
    await db.prepare("UPDATE settings SET v = ? WHERE k = ?").bind(JSON.stringify(rec), "lock:" + game).run();
  } else if (!rec) {
    // the very first time: this code becomes the code
    if (!body.setup) return json({ ok: false, setup: true }, 409);
    const salt = [...crypto.getRandomValues(new Uint8Array(16))].map((b) => b.toString(16).padStart(2, "0")).join("");
    rec = { salt, hash: await sha256hex(salt + ":" + code) };
    const db = await lockDb(env);
    const res = await db.prepare("INSERT OR IGNORE INTO settings (k, v) VALUES (?, ?)").bind("lock:" + game, JSON.stringify(rec)).run();
    if (!res.meta || !res.meta.changes) return json({ ok: false, setup: false }, 409);   // someone was quicker
  } else if ((await sha256hex(rec.salt + ":" + code)) !== rec.hash) {
    const db = await lockDb(env);
    await db.prepare("INSERT INTO unlock_tries (ip, ts) VALUES (?, ?)").bind(ip, Date.now()).run();
    await db.prepare("DELETE FROM unlock_tries WHERE ts < ?").bind(Date.now() - LOCK_WINDOW_MS).run();
    return json({ ok: false, left: t.left - 1 }, 403);
  }
  const token = await lockToken(rec, game);
  return new Response(JSON.stringify({ ok: true }), {
    headers: {
      "Content-Type": "application/json; charset=utf-8",
      "Cache-Control": "no-store",
      "Set-Cookie": LOCKED[game] + "=" + token + "; Path=/; Max-Age=2592000; HttpOnly; Secure; SameSite=Lax",
    },
  });
}

// =====================================================================
// push notifications ("someone is in TUSA!"): standard Web Push, sent
// straight from here. The VAPID key pair is made on first use and kept in
// D1, so nothing secret lives in this (public) repository.
// =====================================================================
const b64u = (buf) => btoa(String.fromCharCode(...new Uint8Array(buf))).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
function unb64u(s) {
  s = String(s).replace(/-/g, "+").replace(/_/g, "/");
  while (s.length % 4) s += "=";
  const b = atob(s), u = new Uint8Array(b.length);
  for (let i = 0; i < b.length; i++) u[i] = b.charCodeAt(i);
  return u;
}
const utf8 = (s) => new TextEncoder().encode(s);
function concat(...parts) {
  const out = new Uint8Array(parts.reduce((n, p) => n + p.length, 0));
  let at = 0;
  for (const p of parts) { out.set(p, at); at += p.length; }
  return out;
}
async function hmac(key, data) {
  const k = await crypto.subtle.importKey("raw", key, { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  return new Uint8Array(await crypto.subtle.sign("HMAC", k, data));
}
let pushTables = false;
async function pushDb(env) {
  const db = await lockDb(env);
  if (!pushTables) {
    await db.prepare("CREATE TABLE IF NOT EXISTS push_subs (endpoint TEXT PRIMARY KEY, p256dh TEXT NOT NULL, auth TEXT NOT NULL, created INTEGER NOT NULL)").run();
    pushTables = true;
  }
  return db;
}
async function vapidKeys(env) {
  const db = await pushDb(env);
  let row = await db.prepare("SELECT v FROM settings WHERE k = 'vapid'").first();
  if (!row) {
    const kp = await crypto.subtle.generateKey({ name: "ECDSA", namedCurve: "P-256" }, true, ["sign", "verify"]);
    const rec = { pub: b64u(await crypto.subtle.exportKey("raw", kp.publicKey)), jwk: await crypto.subtle.exportKey("jwk", kp.privateKey) };
    await db.prepare("INSERT OR IGNORE INTO settings (k, v) VALUES ('vapid', ?)").bind(JSON.stringify(rec)).run();
    row = await db.prepare("SELECT v FROM settings WHERE k = 'vapid'").first();
  }
  return JSON.parse(row.v);
}
// only real push services (Apple, Google, Mozilla, Microsoft) are ever called
const PUSH_HOSTS = /(^|\.)(push\.apple\.com|fcm\.googleapis\.com|android\.googleapis\.com|push\.services\.mozilla\.com|notify\.windows\.com)$/;
function pushEndpointOk(endpoint, local) {
  try {
    const u = new URL(endpoint);
    if (local && u.protocol === "http:" && u.hostname === "127.0.0.1") return true;   // local tests only
    return u.protocol === "https:" && PUSH_HOSTS.test(u.hostname);
  } catch { return false; }
}
// the message, encrypted for this one phone (RFC 8291, aes128gcm)
async function pushBody(sub, text) {
  const uaPub = unb64u(sub.p256dh), auth = unb64u(sub.auth);
  const mine = await crypto.subtle.generateKey({ name: "ECDH", namedCurve: "P-256" }, true, ["deriveBits"]);
  const asPub = new Uint8Array(await crypto.subtle.exportKey("raw", mine.publicKey));
  const theirs = await crypto.subtle.importKey("raw", uaPub, { name: "ECDH", namedCurve: "P-256" }, false, []);
  const shared = new Uint8Array(await crypto.subtle.deriveBits({ name: "ECDH", public: theirs }, mine.privateKey, 256));
  const ikm = await hmac(await hmac(auth, shared), concat(utf8("WebPush: info"), new Uint8Array([0]), uaPub, asPub, new Uint8Array([1])));
  const salt = crypto.getRandomValues(new Uint8Array(16));
  const prk = await hmac(salt, ikm);
  const cek = (await hmac(prk, concat(utf8("Content-Encoding: aes128gcm"), new Uint8Array([0, 1])))).slice(0, 16);
  const nonce = (await hmac(prk, concat(utf8("Content-Encoding: nonce"), new Uint8Array([0, 1])))).slice(0, 12);
  const key = await crypto.subtle.importKey("raw", cek, "AES-GCM", false, ["encrypt"]);
  const sealed = new Uint8Array(await crypto.subtle.encrypt({ name: "AES-GCM", iv: nonce }, key, concat(utf8(text), new Uint8Array([2]))));
  return concat(salt, new Uint8Array([0, 0, 16, 0]), new Uint8Array([65]), asPub, sealed);
}
async function sendPush(env, sub, msg) {
  const v = await vapidKeys(env);
  const head = b64u(utf8(JSON.stringify({ typ: "JWT", alg: "ES256" })));
  const claims = b64u(utf8(JSON.stringify({ aud: new URL(sub.endpoint).origin, exp: Math.floor(Date.now() / 1000) + 12 * 3600, sub: "https://error.blue" })));
  const signKey = await crypto.subtle.importKey("jwk", v.jwk, { name: "ECDSA", namedCurve: "P-256" }, false, ["sign"]);
  const sig = await crypto.subtle.sign({ name: "ECDSA", hash: "SHA-256" }, signKey, utf8(head + "." + claims));
  const res = await fetch(sub.endpoint, {
    method: "POST",
    headers: {
      Authorization: "vapid t=" + head + "." + claims + "." + b64u(sig) + ", k=" + v.pub,
      "Content-Encoding": "aes128gcm",
      "Content-Type": "application/octet-stream",
      TTL: "3600",
      Urgency: "high",
    },
    body: await pushBody(sub, JSON.stringify(msg)),
  });
  // the phone turned notifications off or the app was removed: forget it
  if (res.status === 404 || res.status === 410) await (await pushDb(env)).prepare("DELETE FROM push_subs WHERE endpoint = ?").bind(sub.endpoint).run();
  return res.status;
}
async function handlePush(request, env, url) {
  const local = env.PUSH_LOCAL_TEST === "1";   // only set in .dev.vars on a developer's computer
  if (request.method === "GET") return json({ key: (await vapidKeys(env)).pub });
  let body;
  try { body = await request.json(); } catch { return json({ error: "bad request" }, 400); }
  const db = await pushDb(env);
  const endpoint = String(body.endpoint || "");
  if (!pushEndpointOk(endpoint, local)) return json({ error: "not a push address" }, 400);
  if (request.method === "DELETE") {
    await db.prepare("DELETE FROM push_subs WHERE endpoint = ?").bind(endpoint).run();
    return json({ ok: true });
  }
  if (request.method !== "POST") return json({ error: "use GET, POST or DELETE" }, 405);
  const keys = body.keys || {};
  if (!/^[A-Za-z0-9_-]{80,100}$/.test(keys.p256dh || "") || !/^[A-Za-z0-9_-]{16,32}$/.test(keys.auth || "")) return json({ error: "bad keys" }, 400);
  const n = await db.prepare("SELECT COUNT(*) AS n FROM push_subs").first();
  if (n && n.n >= 60) return json({ error: "full" }, 429);
  await db.prepare("INSERT OR REPLACE INTO push_subs (endpoint, p256dh, auth, created) VALUES (?, ?, ?, ?)").bind(endpoint, keys.p256dh, keys.auth, Date.now()).run();
  return json({ ok: true });
}
// someone typed their name in TUSA: tell every other phone that asked for it
// (at most once every 10 minutes per friend, so reconnecting doesn't spam)
const TUSA_HELLO = {
  VIKA: "Vika is net in de game gestapt",
  VITO: "Vito is net in de game gestapt",
  OLEKSIY: "Oleksiy is net in de game gestapt",
};
async function handleHello(request, env, ctx, url) {
  if (request.method !== "POST") return json({ error: "use POST" }, 405);
  let body;
  try { body = await request.json(); } catch { return json({ error: "bad request" }, 400); }
  const who = String(body.char || "");
  if (!TUSA_HELLO[who]) return json({ error: "who?" }, 400);
  const db = await pushDb(env), now = Date.now();
  const last = await db.prepare("SELECT v FROM settings WHERE k = ?").bind("hello:" + who).first();
  if (last && now - Number(last.v) < 10 * 60 * 1000) return json({ ok: true, sent: 0, quiet: true });
  await db.prepare("INSERT OR REPLACE INTO settings (k, v) VALUES (?, ?)").bind("hello:" + who, String(now)).run();
  const me = String(body.me || "");
  const { results } = await db.prepare("SELECT endpoint, p256dh, auth FROM push_subs").all();
  const subs = (results || []).filter((r) => r.endpoint !== me);
  const msg = { title: "ТУСА У БЕРЛІНІ 🍜", body: TUSA_HELLO[who] + " · Kom je ook naar Hoongy?", tag: "tusa-" + who, url: "/tusa/" };
  const local = env.PUSH_LOCAL_TEST === "1";
  const work = Promise.allSettled(subs.filter((s) => pushEndpointOk(s.endpoint, local)).map((s) => sendPush(env, s, msg)));
  if (ctx && ctx.waitUntil) ctx.waitUntil(work); else await work;
  return json({ ok: true, sent: subs.length });
}

export default {
  async fetch(request, env, ctx) {
    const url = new URL(request.url);
    const path = url.pathname;

    try {
      if (path === "/api/health") return json({ ok: true });

      if (path === "/api/scores") return await handleScores(request, env, url);

      let m = path.match(/^\/api\/save\/([^/]+)\/([^/]+)$/);
      if (m) {
        const [, game, player] = m;
        if (!GAME_RE.test(game) || !PLAYER_RE.test(player)) return json({ error: "bad game or player id" }, 400);
        return await handleSave(request, env, game, player);
      }

      m = path.match(/^\/api\/room\/([^/]+)\/([^/]+)$/);
      if (m) {
        const [, game, code] = m;
        if (!GAME_RE.test(game) || !ROOM_RE.test(code)) return json({ error: "bad game or room code" }, 400);
        if (request.headers.get("Upgrade") !== "websocket") return json({ error: "connect with a WebSocket" }, 426);
        const room = env.ROOMS.get(env.ROOMS.idFromName(`${game}:${code.toUpperCase()}`));
        return room.fetch(request);
      }

      m = path.match(/^\/api\/chat\/([^/]+)$/);
      if (m) {
        const code = m[1].toUpperCase();
        if (!ROOM_RE.test(code)) return json({ error: "bad channel code" }, 400);
        if (request.headers.get("Upgrade") !== "websocket") return json({ error: "connect with a WebSocket" }, 426);
        return env.CHAT.get(env.CHAT.idFromName(code)).fetch(request);
      }

      if (path === "/api/tusa") {
        if (request.headers.get("Upgrade") !== "websocket") return json({ error: "connect with a WebSocket" }, 426);
        return env.TUSA.get(env.TUSA.idFromName("hoongy")).fetch(request);
      }

      m = path.match(/^\/api\/vgadai\/(\d{4})\/info$/);
      if (m) {
        if (!VG_CODE.test(m[1])) return json({ error: "season and episode are 01-99" }, 400);
        return env.VGADAI.get(env.VGADAI.idFromName("ep:" + m[1])).fetch(request);
      }

      m = path.match(/^\/api\/vgadai\/(\d{4})$/);
      if (m) {
        if (request.headers.get("Upgrade") !== "websocket") return json({ error: "connect with a WebSocket" }, 426);
        if (!VG_CODE.test(m[1])) return json({ error: "season and episode are 01-99" }, 400);
        const u = new URL(request.url); u.searchParams.set("code", m[1]);
        return env.VGADAI.get(env.VGADAI.idFromName("ep:" + m[1])).fetch(new Request(u, request));
      }

      if (path.startsWith("/api/vgadai/")) return await handleVgadai(request, env, url);

      if (path === "/api/wall") {
        if (request.headers.get("Upgrade") !== "websocket") return json({ error: "connect with a WebSocket" }, 426);
        return env.WALL.get(env.WALL.idFromName("main")).fetch(request);
      }

      if (path === "/api/unlock") return handleUnlock(request, env, url);

      if (path === "/api/push") return await handlePush(request, env, url);

      if (path === "/api/tusa/hello") return await handleHello(request, env, ctx, url);

      if (path.startsWith("/api/")) return json({ error: "not found" }, 404);

      // old addresses of games that moved to a short one (error.blue/voyage)
      m = path.match(/^\/games\/([a-z0-9-]+)(\/.*)?$/);
      if (m && SHORT_PATHS[m[1]]) return new Response(null, { status: 301, headers: { Location: SHORT_PATHS[m[1]] + (m[2] || "/") + url.search } });

      // a program with an access code: no code, no files
      const lockedId = lockedGame(path);
      if (lockedId && !(await isUnlocked(request, env, lockedId))) {
        const page = request.mode === "navigate" || (request.headers.get("Accept") || "").includes("text/html") || path.endsWith("/") || path.endsWith(".html");
        return page
          ? new Response(null, { status: 302, headers: { Location: "/?unlock=" + lockedId, "Cache-Control": "no-store" } })
          : new Response("locked", { status: 403, headers: { "Cache-Control": "no-store" } });
      }

      const res = await env.ASSETS.fetch(request);
      // game pages get a small back button when the site runs as an installed app
      if (isGamePath(path) && (res.headers.get("content-type") || "").includes("text/html")) {
        // ...and they behave like an app: no zooming in by double-tapping or pinching
        let viewport = false;
        return new HTMLRewriter()
          .on('meta[name="viewport"]', {
            element(el) {
              viewport = true;
              el.setAttribute("content", "width=device-width, initial-scale=1, maximum-scale=1, minimum-scale=1, viewport-fit=cover, user-scalable=no");
            },
          })
          .on("head", {
            element(el) {
              el.append(NO_ZOOM_HEAD, { html: true });
            },
          })
          .on("body", {
            element(el) {
              el.append('<script src="/lib/appback.js" defer></script>', { html: true });
            },
          })
          .transform(res);
      }
      return res;
    } catch (err) {
      console.error(err);
      return json({ error: "server error" }, 500);
    }
  },
};

// One GameRoom exists per game + room code. It relays messages between
// everyone in the room. Players are told who joins and leaves.
export class GameRoom extends DurableObject {
  constructor(ctx, env) {
    super(ctx, env);
    // Answer keep-alive pings without waking the room up.
    this.ctx.setWebSocketAutoResponse(new WebSocketRequestResponsePair('{"type":"ping"}', '{"type":"pong"}'));
  }

  async fetch(request) {
    const url = new URL(request.url);
    const name = cleanName(url.searchParams.get("name")) || "Player";

    const pair = new WebSocketPair();
    const [client, server] = Object.values(pair);
    this.ctx.acceptWebSocket(server);

    if (this.openSockets().length > MAX_PLAYERS_PER_ROOM) {
      server.send(JSON.stringify({ type: "error", message: "room is full" }));
      server.close(4000, "room is full");
      return new Response(null, { status: 101, webSocket: client });
    }

    const me = { id: crypto.randomUUID().slice(0, 8), name, joinedAt: Date.now() };
    server.serializeAttachment(me);

    server.send(JSON.stringify({ type: "welcome", id: me.id, players: this.players() }));
    this.broadcast({ type: "join", player: me }, server);

    return new Response(null, { status: 101, webSocket: client });
  }

  openSockets() {
    return this.ctx.getWebSockets().filter((ws) => ws.readyState === WebSocket.OPEN);
  }

  players() {
    return this.openSockets()
      .map((ws) => ws.deserializeAttachment())
      .filter(Boolean)
      .sort((a, b) => a.joinedAt - b.joinedAt);
  }

  broadcast(message, except) {
    const text = JSON.stringify(message);
    for (const ws of this.openSockets()) {
      if (ws === except) continue;
      try {
        ws.send(text);
      } catch {
        // that player is gone; their close event will clean up
      }
    }
  }

  async webSocketMessage(ws, message) {
    if (typeof message !== "string" || message.length > 16 * 1024) return;
    const me = ws.deserializeAttachment();
    if (!me) return;
    let data;
    try {
      data = JSON.parse(message);
    } catch {
      return;
    }
    // "to" sends a message to one player only; otherwise everyone else gets it.
    if (data && typeof data === "object" && typeof data.to === "string") {
      const target = this.openSockets().find((s) => s.deserializeAttachment()?.id === data.to);
      if (target) target.send(JSON.stringify({ type: "msg", from: me.id, data: data.data }));
      return;
    }
    this.broadcast({ type: "msg", from: me.id, data }, ws);
  }

  async webSocketClose(ws, code, reason) {
    this.leave(ws);
    try {
      ws.close(code, reason);
    } catch {
      // already closed
    }
  }

  async webSocketError(ws) {
    this.leave(ws);
  }

  leave(ws) {
    const me = ws.deserializeAttachment();
    if (!me || me.left) return;
    ws.serializeAttachment({ ...me, left: true });
    this.broadcast({ type: "leave", id: me.id }, ws);
  }
}

// ---------------------------------------------------------------------------
// Pixel Wall: one shared board that everyone can paint on.
// The board is saved, so every visitor sees what others painted.
const WALL_W = 48;
const WALL_H = 48;
const WALL_COLORS = 16;
const PAINT_COOLDOWN_MS = 1000;

export class PixelWall extends DurableObject {
  constructor(ctx, env) {
    super(ctx, env);
    this.ctx.setWebSocketAutoResponse(new WebSocketRequestResponsePair('{"type":"ping"}', '{"type":"pong"}'));
    this.board = new Uint8Array(WALL_W * WALL_H);
    this.lastBy = null;
    this.paints = 0;
    ctx.blockConcurrencyWhile(async () => {
      const saved = await ctx.storage.get(["board", "lastBy", "paints"]);
      const board = saved.get("board");
      if (board instanceof Uint8Array && board.length === WALL_W * WALL_H) this.board = board;
      this.lastBy = saved.get("lastBy") || null;
      this.paints = saved.get("paints") || 0;
    });
  }

  async fetch(request) {
    const pair = new WebSocketPair();
    const [client, server] = Object.values(pair);
    this.ctx.acceptWebSocket(server);
    server.serializeAttachment({ last: 0 });

    server.send(
      JSON.stringify({
        type: "board",
        w: WALL_W,
        h: WALL_H,
        cooldown: PAINT_COOLDOWN_MS,
        data: Array.from(this.board, (v) => v.toString(16)).join(""),
        lastBy: this.lastBy,
        paints: this.paints,
      })
    );
    this.broadcastOnline();
    return new Response(null, { status: 101, webSocket: client });
  }

  openSockets(except) {
    return this.ctx.getWebSockets().filter((ws) => ws !== except && ws.readyState === WebSocket.OPEN);
  }

  broadcast(message, except) {
    const text = JSON.stringify(message);
    for (const ws of this.openSockets(except)) {
      try {
        ws.send(text);
      } catch {
        // gone
      }
    }
  }

  broadcastOnline(except) {
    this.broadcast({ type: "online", n: this.openSockets(except).length }, except);
  }

  async webSocketMessage(ws, message) {
    if (typeof message !== "string" || message.length > 512) return;
    let d;
    try {
      d = JSON.parse(message);
    } catch {
      return;
    }
    if (!d || d.t !== "paint") return;

    const x = Number(d.x), y = Number(d.y), c = Number(d.c);
    if (![x, y, c].every(Number.isInteger)) return;
    if (x < 0 || y < 0 || x >= WALL_W || y >= WALL_H || c < 0 || c >= WALL_COLORS) return;

    const me = ws.deserializeAttachment() || { last: 0 };
    const now = Date.now();
    const wait = PAINT_COOLDOWN_MS - (now - me.last);
    if (wait > 50) {
      ws.send(JSON.stringify({ type: "cooldown", wait }));
      return;
    }
    me.last = now;
    ws.serializeAttachment(me);

    const name = cleanName(d.name) || "anon";
    const i = y * WALL_W + x;
    this.board[i] = c;
    this.paints += 1;
    this.lastBy = { name, x, y };
    await this.ctx.storage.put({ board: this.board, lastBy: this.lastBy, paints: this.paints });
    this.broadcast({ type: "px", x, y, c, by: name, paints: this.paints });
  }

  async webSocketClose(ws, code, reason) {
    this.broadcastOnline(ws);
    try {
      ws.close(code, reason);
    } catch {
      // already closed
    }
  }

  async webSocketError(ws) {
    this.broadcastOnline(ws);
  }
}

// ---------------------------------------------------------------------------
// ERROR-CHAT: one ChatRoom per channel code. Everyone with the same code
// talks together. The last messages are kept so newcomers can read back;
// a channel is wiped after 7 days without messages.
const CHAT_KEEP = 300;
const CHAT_HISTORY = 80;
const CHAT_MAX_LEN = 300;
const CHAT_COOLDOWN_MS = 350;
const CHAT_EXPIRE_MS = 7 * 24 * 60 * 60 * 1000;

export class ChatRoom extends DurableObject {
  constructor(ctx, env) {
    super(ctx, env);
    this.ctx.setWebSocketAutoResponse(new WebSocketRequestResponsePair('{"type":"ping"}', '{"type":"pong"}'));
    this.sql = ctx.storage.sql;
    this.sql.exec(
      `CREATE TABLE IF NOT EXISTS msgs (
         id INTEGER PRIMARY KEY AUTOINCREMENT,
         at INTEGER NOT NULL,
         kind TEXT NOT NULL,
         name TEXT NOT NULL,
         text TEXT NOT NULL
       )`
    );
  }

  openSockets(except) {
    return this.ctx.getWebSockets().filter((ws) => ws !== except && ws.readyState === WebSocket.OPEN);
  }

  who(except) {
    return this.openSockets(except)
      .map((ws) => ws.deserializeAttachment()?.name)
      .filter(Boolean);
  }

  broadcast(message, except) {
    const text = JSON.stringify(message);
    for (const ws of this.openSockets(except)) {
      try {
        ws.send(text);
      } catch {
        // gone
      }
    }
  }

  save(kind, name, text) {
    const at = Date.now();
    const row = this.sql
      .exec(`INSERT INTO msgs (at, kind, name, text) VALUES (?, ?, ?, ?) RETURNING id`, at, kind, name, text)
      .one();
    this.sql.exec(`DELETE FROM msgs WHERE id <= ?`, row.id - CHAT_KEEP);
    return { id: row.id, at, kind, name, text };
  }

  async fetch(request) {
    const url = new URL(request.url);
    const code = (url.pathname.split("/").pop() || "").toUpperCase();
    const name = cleanName(url.searchParams.get("name")) || "guest";

    const pair = new WebSocketPair();
    const [client, server] = Object.values(pair);
    this.ctx.acceptWebSocket(server);

    if (this.openSockets().length > 64) {
      server.send(JSON.stringify({ type: "error", message: "channel is full" }));
      server.close(4000, "channel is full");
      return new Response(null, { status: 101, webSocket: client });
    }

    server.serializeAttachment({ name, last: 0 });
    const history = this.sql
      .exec(`SELECT id, at, kind, name, text FROM msgs ORDER BY id DESC LIMIT ?`, CHAT_HISTORY)
      .toArray()
      .reverse();
    server.send(JSON.stringify({ type: "hello", code, name, history, who: this.who() }));
    this.broadcast({ type: "join", name, who: this.who() }, server);
    return new Response(null, { status: 101, webSocket: client });
  }

  async webSocketMessage(ws, message) {
    if (typeof message !== "string" || message.length > 2048) return;
    let d;
    try {
      d = JSON.parse(message);
    } catch {
      return;
    }
    const me = ws.deserializeAttachment();
    if (!me || !d) return;

    if (d.t === "say") {
      const text = String(d.text ?? "")
        .replace(/[\u0000-\u0008\u000b-\u001f\u007f]/g, "")
        .trim()
        .slice(0, CHAT_MAX_LEN);
      if (!text) return;
      const now = Date.now();
      if (now - me.last < CHAT_COOLDOWN_MS) {
        ws.send(JSON.stringify({ type: "error", message: "slow down" }));
        return;
      }
      me.last = now;
      ws.serializeAttachment(me);
      const msg = this.save("msg", me.name, text);
      this.broadcast({ type: "msg", ...msg });
      await this.ctx.storage.setAlarm(now + CHAT_EXPIRE_MS);
    } else if (d.t === "nick") {
      const to = cleanName(d.name);
      if (!to || to === me.name) return;
      const from = me.name;
      me.name = to;
      ws.serializeAttachment(me);
      this.broadcast({ type: "nick", from, to, who: this.who() });
    }
  }

  async webSocketClose(ws, code, reason) {
    this.leave(ws);
    try {
      ws.close(code, reason);
    } catch {
      // already closed
    }
  }

  async webSocketError(ws) {
    this.leave(ws);
  }

  leave(ws) {
    const me = ws.deserializeAttachment();
    if (!me || me.left) return;
    ws.serializeAttachment({ ...me, left: true });
    this.broadcast({ type: "leave", name: me.name, who: this.who(ws) }, ws);
  }

  // 7 days without a message: forget the channel
  async alarm() {
    this.sql.exec(`DELETE FROM msgs`);
  }
}


// ---------------------------------------------------------------------------
// Туса У Берліні: one shared Hoongy restaurant for Vika, Vito and Oleksiy.
// Each player's state lives on their own WebSocket (it survives hibernation);
// the clock and the radio are saved. The clock starts at 18:00 when a second
// friend arrives and runs 4x faster than real time, so 22:00 comes after one
// real hour. When everyone has gone, the clock and radio reset.
const TUSA_CHARS = ["VIKA", "VITO", "OLEKSIY"];
const TUSA_SEATS = 14;
const TUSA_ITEMS = ["lassi", "coffee", "shrimp", "fish"];
const TUSA_EMOTES = ["hug", "heart", "joke", "nose"];
// the nose kiss is only for Vito and Oleksiy
const TUSA_NOSE = { VITO: "OLEKSIY", OLEKSIY: "VITO" };
const TUSA_SERVE_MS = 10500;

export class TusaRoom extends DurableObject {
  constructor(ctx, env) {
    super(ctx, env);
    this.ctx.setWebSocketAutoResponse(new WebSocketRequestResponsePair('{"t":"ping"}', '{"type":"pong"}'));
    this.g = { clockStart: 0, radio: { ch: 0, since: 0, epoch: 0 } };
    ctx.blockConcurrencyWhile(async () => {
      const g = await ctx.storage.get("g");
      if (g) this.g = g;
    });
  }

  live(except) {
    return this.ctx.getWebSockets().filter((ws) => {
      if (ws === except || ws.readyState !== WebSocket.OPEN) return false;
      const a = ws.deserializeAttachment();
      return a && a.char && !a.gone;
    });
  }

  players(except) {
    return this.live(except).map((ws) => ws.deserializeAttachment());
  }

  send(ws, msg) {
    try {
      ws.send(JSON.stringify(msg));
    } catch {
      // gone
    }
  }

  broadcast(msg, except) {
    const text = JSON.stringify(msg);
    for (const ws of this.live(except)) {
      try {
        ws.send(text);
      } catch {
        // gone
      }
    }
  }

  async saveG() {
    await this.ctx.storage.put("g", this.g);
    this.broadcast({ type: "g", g: this.g, now: Date.now() });
  }

  async updateClock() {
    const n = this.players().length;
    if (n === 0) {
      if (this.g.clockStart || this.g.radio.ch || this.g.radio.epoch) {
        this.g = { clockStart: 0, radio: { ch: 0, since: 0, epoch: 0 } };
        await this.ctx.storage.put("g", this.g);
      }
    } else if (n >= 2 && !this.g.clockStart) {
      const now = Date.now();
      this.g.clockStart = now;
      await this.saveG();
      // the kitchen opens: whoever is already sitting gets the waiter now
      for (const ws of this.live()) {
        const a = ws.deserializeAttachment();
        if (a.seat >= 0 && !a.order) {
          a.order = { ph: "wait", t: now };
          ws.serializeAttachment(a);
          this.broadcast({ type: "p", p: a, now });
        }
      }
    }
  }

  async fetch(request) {
    const url = new URL(request.url);
    const char = String(url.searchParams.get("char") || "").toUpperCase();
    if (!TUSA_CHARS.includes(char)) return new Response("unknown character", { status: 400 });

    // the same friend logging in again replaces the old connection
    for (const old of this.live()) {
      const a = old.deserializeAttachment();
      if (a.char === char) {
        old.serializeAttachment({ ...a, gone: true });
        this.send(old, { type: "kicked" });
        try {
          old.close(4001, "logged in elsewhere");
        } catch {
          // already closed
        }
        this.broadcast({ type: "gone", char }, old);
      }
    }

    const pair = new WebSocketPair();
    const [client, server] = Object.values(pair);
    this.ctx.acceptWebSocket(server);
    const now = Date.now();
    const arrive = ["taxi", "amarok", "scooter"].includes(url.searchParams.get("arrive")) ? url.searchParams.get("arrive") : "";
    const me = {
      char,
      x: Math.max(0, Math.min(800, Number(url.searchParams.get("x")) || 200)),
      y: 150,
      dir: 1,
      scene: "street",
      anim: "",
      ride: arrive === "scooter",
      scoot: -1,
      sd: 1,
      seat: -1,
      order: null,
      arrive,
      arriveAt: now,
    };
    server.serializeAttachment(me);
    // a new evening: every radio station starts at 20% of its song and keeps going from here
    if (!this.g.radio.epoch || this.players(server).length === 0) {
      if (this.players(server).length === 0) this.g = { clockStart: 0, radio: { ch: 0, since: 0, epoch: now } };
      else this.g.radio.epoch = now;
      await this.ctx.storage.put("g", this.g);
    }
    this.send(server, { type: "welcome", now, you: me, players: this.players(server), g: this.g });
    this.broadcast({ type: "p", p: me, now }, server);
    await this.updateClock();
    return new Response(null, { status: 101, webSocket: client });
  }

  async webSocketMessage(ws, message) {
    // voice call set-up messages (SDP) are a few KB; everything else is small
    if (typeof message !== "string" || message.length > 24000) return;
    let d;
    try {
      d = JSON.parse(message);
    } catch {
      return;
    }
    const me = ws.deserializeAttachment();
    if (!me || me.gone || !d) return;
    const now = Date.now();

    switch (d.t) {
      case "st": {
        const x = Number(d.x), y = Number(d.y);
        if (Number.isFinite(x)) me.x = Math.max(-50, Math.min(900, x));
        if (Number.isFinite(y)) me.y = Math.max(0, Math.min(400, y));
        me.dir = d.dir === -1 ? -1 : 1;
        if (d.scene === "street" || d.scene === "inside") me.scene = d.scene;
        // nobody goes into Hoongy alone: it opens once a friend has shown up (the evening runs)
        if (me.scene === "inside" && !this.g.clockStart && this.players(ws).length === 0) me.scene = "street";
        me.anim = typeof d.anim === "string" ? d.anim.slice(0, 12) : "";
        // Oleksiy's e-scooter: riding it, or where it is parked (-1 = not parked)
        me.ride = d.ride === 1 && me.char === "OLEKSIY";
        const sc = Number(d.scoot);
        me.scoot = Number.isFinite(sc) && sc >= 0 ? Math.min(900, Math.round(sc)) : -1;
        me.sd = d.sd === -1 ? -1 : 1;
        // how hard this friend is laughing (0-4), rolling on the floor, facing angle
        me.lg = Math.max(0, Math.min(4, Number(d.lg) | 0));
        me.rf = d.rf === 1;
        const a = Number(d.a);
        me.a = Number.isFinite(a) ? Math.round(Math.max(-7, Math.min(7, a)) * 100) / 100 : 0;
        if (me.scene === "street") { me.seat = -1; me.order = null; }
        ws.serializeAttachment(me);
        this.broadcast({ type: "p", p: me, now }, ws);
        return;
      }
      case "sit": {
        const seat = Number(d.seat);
        if (!Number.isInteger(seat) || seat < 0 || seat >= TUSA_SEATS) return;
        if (this.players(ws).some((p) => p.seat === seat)) {
          this.send(ws, { type: "taken", seat });
          return;
        }
        me.seat = seat;
        // the kitchen only opens once a friend has arrived (the evening clock runs)
        me.order = this.g.clockStart ? { ph: "wait", t: now } : null;
        break;
      }
      case "stand":
        me.seat = -1;
        me.order = null;
        break;
      case "order":
        if (me.seat < 0 || !this.g.clockStart || !TUSA_ITEMS.includes(d.item)) return;
        me.order = { ph: "cook", item: d.item, t: now, bites: 0 };
        break;
      case "eat":
        if (!me.order || me.order.ph !== "cook" || now < me.order.t + TUSA_SERVE_MS) return;
        me.order.bites = Math.min(5, (me.order.bites || 0) + 1);
        break;
      case "more":
        if (me.seat < 0 || !this.g.clockStart) return;
        me.order = { ph: "wait", t: now };
        break;
      case "emote": {
        if (!TUSA_EMOTES.includes(d.kind)) return;
        if (now - (me.lastEmote || 0) < 350) return;
        me.lastEmote = now;
        ws.serializeAttachment(me);
        const to = TUSA_CHARS.includes(d.to) ? d.to : "";
        if (d.kind === "nose" && TUSA_NOSE[me.char] !== to) return;
        const j = Number.isInteger(d.j) ? Math.max(0, Math.min(999, d.j)) : 0;
        this.broadcast({ type: "emote", from: me.char, kind: d.kind, to, j, t: now });
        return;
      }
      case "voice":
        // talking with the microphone on (a three-way call between the friends)
        me.voice = !!d.on;
        break;
      case "rtc": {
        // pass call set-up messages to one friend
        if (!TUSA_CHARS.includes(d.to) || !d.data || message.length > 24000) return;
        for (const s of this.live(ws)) {
          const a = s.deserializeAttachment();
          if (a.char === d.to) this.send(s, { type: "rtc", from: me.char, data: d.data });
        }
        return;
      }
      case "radio": {
        const ch = Number(d.ch);
        if (!Number.isInteger(ch) || ch < 0 || ch > 30) return;
        // like a real radio: on/off and the channel; the songs keep their own time
        this.g.radio = { ch, since: now, last: ch || this.g.radio.last || 1, epoch: this.g.radio.epoch || now };
        await this.saveG();
        return;
      }
      default:
        return;
    }
    ws.serializeAttachment(me);
    this.broadcast({ type: "p", p: me, now });
  }

  async webSocketClose(ws, code, reason) {
    await this.leave(ws);
    try {
      ws.close(code, reason);
    } catch {
      // already closed
    }
  }

  async webSocketError(ws) {
    await this.leave(ws);
  }

  async leave(ws) {
    const me = ws.deserializeAttachment();
    if (!me || me.gone) return;
    ws.serializeAttachment({ ...me, gone: true });
    this.broadcast({ type: "gone", char: me.char }, ws);
    await this.updateClock();
  }
}

// ---------------------------------------------------------------------------
// Вгадай що?! (error.blue/vgadai): a TV quiz show. Every game is an episode:
// a room with a 4-digit code made of the season and the episode (S03 E12 = 0312).
// One player types a question; the others type one word or one number and race
// to press the green button; the one who asked says yes or no. A right answer
// wins the round (+1), then the next player asks. Each episode keeps its own
// leaderboard. Vika, Vito and Oleksiy look like in TUSA; everybody else gets a
// random look (kept in D1, so it is the same in every episode).
// ---------------------------------------------------------------------------
const VG_HOONGY = { VIKA: 1, VITO: 1, OLEKSIY: 1 };
const VG_SEATS = 6;            // players in one episode
const VG_LOCK_MS = 3000;       // after a wrong answer that player waits a moment
const VG_REVEAL_MS = 7000;     // the winner is shown, then the next player asks
const VG_HIST = 80;
const VG_NAME = /^[A-Z0-9]{2,10}$/;
const VG_CODE = /^(0[1-9]|[1-9]\d)(0[1-9]|[1-9]\d)$/;   // season 01-99 + episode 01-99
const vgClean = (v, n) => String(v ?? "").replace(/[\u0000-\u001f\u007f]/g, " ").replace(/\s+/g, " ").trim().slice(0, n);
// one word or one number: no spaces, only letters, digits and - ' . ,
function vgWord(v) {
  const w = String(v ?? "").trim().split(/\s+/)[0] || "";
  return w.replace(/[^\p{L}\p{M}\p{N}'’\-.,]/gu, "").replace(/^[.,]+|[.,]+$/g, "").slice(0, 24);
}
const vgSeed = () => crypto.getRandomValues(new Uint32Array(1))[0] >>> 1;
function vgFresh() {
  return {
    code: "", created: 0, seats: [], looks: {}, scores: {}, asked: {},
    phase: "ask", host: "", last: "", round: 0,
    q: "", hint: "", buzzer: "", lock: {}, answer: "",
    tries: [], winner: "", reveal: "", nextAt: 0, hist: [],
  };
}
let vgTables = false;
async function vgDb(env) {
  if (!vgTables) {
    await env.DB.batch([
      env.DB.prepare(`CREATE TABLE IF NOT EXISTS vgadai_players (
        name TEXT PRIMARY KEY, seed INTEGER NOT NULL, updated INTEGER NOT NULL DEFAULT 0)`),
      env.DB.prepare(`CREATE TABLE IF NOT EXISTS vgadai_rooms (
        code TEXT PRIMARY KEY, players TEXT NOT NULL, n INTEGER NOT NULL, updated INTEGER NOT NULL)`),
    ]);
    vgTables = true;
  }
  return env.DB;
}
// the episodes that are on air right now (for the lobby)
async function handleVgadai(request, env, url) {
  const db = await vgDb(env);
  if (url.pathname === "/api/vgadai/live") {
    const live = await db.prepare(`SELECT code, players, n FROM vgadai_rooms WHERE n > 0 AND updated > ?
      ORDER BY updated DESC LIMIT 12`).bind(Date.now() - 6 * 3600 * 1000).all();
    return json({ live: live.results.map((r) => ({ code: r.code, n: r.n, players: JSON.parse(r.players) })) });
  }
  if (url.pathname === "/api/vgadai/look") {
    // your look: { name } → { seed }; with new: true a fresh random look
    if (request.method !== "POST") return json({ error: "use POST" }, 405);
    let body;
    try { body = await request.json(); } catch { return json({ error: "send JSON" }, 400); }
    const name = String(body.name || "").toUpperCase();
    if (!VG_NAME.test(name)) return json({ error: "bad name" }, 400);
    if (VG_HOONGY[name]) return json({ seed: 0, hoongy: true });
    let row = await db.prepare(`SELECT seed FROM vgadai_players WHERE name = ?`).bind(name).first();
    if (!row || body.new) {
      const seed = vgSeed();
      await db.prepare(`INSERT INTO vgadai_players (name, seed, updated) VALUES (?, ?, ?)
        ON CONFLICT (name) DO UPDATE SET seed = excluded.seed, updated = excluded.updated`).bind(name, seed, Date.now()).run();
      row = { seed };
    }
    return json({ seed: row.seed });
  }
  return json({ error: "not found" }, 404);
}

export class VgadaiRoom extends DurableObject {
  constructor(ctx, env) {
    super(ctx, env);
    this.env = env;
    this.ctx.setWebSocketAutoResponse(new WebSocketRequestResponsePair('{"t":"ping"}', '{"type":"pong"}'));
    this.s = vgFresh();
    this.subs = {};   // phones that want a notification: endpoint -> { name, endpoint, p256dh, auth }
    ctx.blockConcurrencyWhile(async () => {
      const s = await ctx.storage.get("s");
      if (s) this.s = { ...vgFresh(), ...s };
      this.subs = (await ctx.storage.get("subs")) || {};
    });
  }

  // a notification to everyone in this episode who turned them on (not to the one who did it)
  notify(except, body, hostBody) {
    const code = this.s.code, local = this.env.PUSH_LOCAL_TEST === "1";
    const title = "Вгадай що?! · S" + code.slice(0, 2) + " E" + code.slice(2);
    const list = Object.values(this.subs).filter((x) => x.name !== except && pushEndpointOk(x.endpoint, local));
    if (!list.length) return;
    const work = Promise.allSettled(list.map(async (x) => {
      const msg = { title, body: x.name === this.s.host && hostBody ? hostBody : body, tag: "vgadai-" + code,
        url: "/vgadai/?ep=" + code, icon: "/vgadai/icons/vgadai-192.png", quiet: "/vgadai/" };
      const st = await sendPush(this.env, x, msg);
      if (st === 404 || st === 410) { delete this.subs[x.endpoint]; await this.ctx.storage.put("subs", this.subs); }
    }));
    if (this.ctx.waitUntil) this.ctx.waitUntil(work);
  }

  // the leaderboard lives in D1; a hiccup there never stops the show
  async db(sql, ...args) {
    try {
      const db = await vgDb(this.env);
      return await db.prepare(sql).bind(...args).run();
    } catch (e) {
      console.error("vgadai db", e);
      return null;
    }
  }

  live(except) {
    return this.ctx.getWebSockets().filter((ws) => {
      if (ws === except || ws.readyState !== WebSocket.OPEN) return false;
      const a = ws.deserializeAttachment();
      return a && a.name && !a.gone;
    });
  }

  online() {
    const on = new Set(this.live().map((ws) => ws.deserializeAttachment().name));
    return this.s.seats.filter((c) => on.has(c));
  }

  send(ws, msg) {
    try { ws.send(JSON.stringify(msg)); } catch { /* gone */ }
  }

  broadcast(msg) {
    const text = JSON.stringify(msg);
    for (const ws of this.live()) {
      try { ws.send(text); } catch { /* gone */ }
    }
  }

  view() {
    const on = this.online();
    return { ...this.s, online: on, paused: on.length < 2 };
  }

  // save, and tell everyone; ev is something to animate (a buzz, a no, a yes...)
  async push(ev) {
    await this.ctx.storage.put("s", this.s);
    const due = this.s.phase === "reveal" ? this.s.nextAt : 0;
    if (due) await this.ctx.storage.setAlarm(due);
    else await this.ctx.storage.deleteAlarm();
    this.broadcast({ type: "s", s: this.view(), ev: ev || null, now: Date.now() });
  }

  // which episodes are on air (for the lobby)
  async onAir() {
    if (!this.s.code) return;
    const on = this.online();
    await this.db(`INSERT INTO vgadai_rooms (code, players, n, updated) VALUES (?, ?, ?, ?)
      ON CONFLICT (code) DO UPDATE SET players = excluded.players, n = excluded.n, updated = excluded.updated`,
      this.s.code, JSON.stringify(on), on.length, Date.now());
  }

  nextHost(after) {
    const on = this.online(), seats = this.s.seats;
    if (!on.length) return "";
    let i = seats.indexOf(after);
    if (i < 0) i = -1;
    for (let k = 1; k <= seats.length; k++) {
      const c = seats[(i + k) % seats.length];
      if (on.includes(c)) return c;
    }
    return on[0];
  }

  newRound(host) {
    const s = this.s;
    if (s.host) s.last = s.host;
    s.host = host || "";
    s.phase = "ask";
    s.q = ""; s.hint = ""; s.buzzer = ""; s.lock = {}; s.answer = "";
    s.tries = []; s.winner = ""; s.reveal = ""; s.nextAt = 0;
  }

  // after someone comes or goes: there is always someone asking, and nobody
  // holds the button who isn't here
  tidy() {
    const s = this.s, on = this.online();
    if (!on.length) return;
    if (!s.host || !on.includes(s.host)) {
      // the one asking left: this round is off, the next player asks
      this.newRound(this.nextHost(s.host || s.last));
      return;
    }

  }

  async fetch(request) {
    const url = new URL(request.url);
    if (url.pathname.endsWith("/info")) {
      const s = this.s;
      return json({ exists: !!s.created, created: s.created, online: this.online(), scores: s.scores, asked: s.asked, looks: s.looks, rounds: s.round || 0 });
    }
    const name = String(url.searchParams.get("name") || "").toUpperCase();
    const code = String(url.searchParams.get("code") || "");
    const mode = url.searchParams.get("mode") === "create" ? "create" : "join";
    if (!VG_NAME.test(name) || !VG_CODE.test(code)) return new Response("bad name or code", { status: 400 });

    const pair = new WebSocketPair();
    const [client, server] = Object.values(pair);
    this.ctx.acceptWebSocket(server);
    const nope = (why, extra) => {
      server.serializeAttachment({ gone: true });
      this.send(server, { type: "nope", why, ...(extra || {}) });
      try { server.close(4002, why); } catch { /* closed */ }
      return new Response(null, { status: 101, webSocket: client });
    };

    // the same player logging in again replaces the old connection
    const others = this.live(server).filter((ws) => ws.deserializeAttachment().name !== name);
    if (mode === "create") {
      // a new episode: only a season + episode that doesn't exist yet
      if (others.length) return nope("live", { players: this.online().filter((n) => n !== name) });
      if (this.s.created) return nope("exists");
      this.s = vgFresh();
      this.s.code = code;
      this.s.created = Date.now();
      this.subs = {};
      await this.ctx.storage.put("subs", this.subs);
    } else if (!this.s.created) {
      return nope("none");
    }
    const s = this.s;
    if (!s.seats.includes(name)) {
      if (s.seats.length >= VG_SEATS) {
        // make room: someone who isn't here, the one with the fewest points first
        const on = new Set(others.map((ws) => ws.deserializeAttachment().name));
        const away = s.seats.filter((n) => !on.has(n) && n !== s.host).sort((a, b) => (s.scores[a] || 0) - (s.scores[b] || 0));
        if (!away.length) return nope("full");
        s.seats = s.seats.filter((n) => n !== away[0]);
      }
      s.seats.push(name);
      if (s.scores[name] == null) s.scores[name] = 0;
      // a look for everyone who isn't Vika, Vito or Oleksiy (the same on every phone)
      if (!VG_HOONGY[name]) {
        let seed = 0;
        try {
          const db = await vgDb(this.env);
          const row = await db.prepare(`SELECT seed FROM vgadai_players WHERE name = ?`).bind(name).first();
          seed = row ? row.seed : 0;
        } catch { /* no db: a look from the name */ }
        if (!seed) {
          seed = vgSeed();
          await this.db(`INSERT OR IGNORE INTO vgadai_players (name, seed, updated) VALUES (?, ?, ?)`, name, seed, Date.now());
        }
        s.looks[name] = seed;
      }
    }
    for (const old of this.live(server)) {
      const a = old.deserializeAttachment();
      if (a.name === name) {
        old.serializeAttachment({ ...a, gone: true });
        this.send(old, { type: "kicked" });
        try { old.close(4001, "logged in elsewhere"); } catch { /* closed */ }
      }
    }
    server.serializeAttachment({ name, at: Date.now() });
    this.tidy();
    this.send(server, { type: "hello", you: name, code });
    await this.push({ k: mode === "create" ? "created" : "join", by: name });
    await this.onAir();
    return new Response(null, { status: 101, webSocket: client });
  }

  async webSocketMessage(ws, message) {
    if (typeof message !== "string" || message.length > 2000) return;
    let d;
    try { d = JSON.parse(message); } catch { return; }
    const me = ws.deserializeAttachment();
    if (!me || me.gone || !me.name || !d) return;
    const who = me.name, s = this.s, now = Date.now();
    const on = this.online(), paused = on.length < 2;
    const host = who === s.host;

    switch (d.t) {
      case "ask": {
        // the question (it can be written while waiting for the others)
        const q = vgClean(d.q, 160);
        if (!host || s.phase !== "ask" || q.length < 2) return;
        s.q = q; s.phase = "guess"; s.round = (s.round || 0) + 1;
        s.asked[who] = (s.asked[who] || 0) + 1;
        this.notify(who, "❓ " + who + " питає: " + q);
        return this.push({ k: "ask", by: who });
      }
      case "buzz": {
        // the answer is typed first; the green button sends it. The first one to press is heard.
        if (host || paused || s.phase !== "guess") {
          if (s.phase === "judge" && s.buzzer !== who) this.send(ws, { type: "late", by: s.buzzer });
          return;
        }
        if ((s.lock[who] || 0) > now) return;
        const w = vgWord(d.w);
        if (!w) return;
        s.phase = "judge"; s.buzzer = who; s.answer = w;
        this.notify(who, "💬 " + who + " каже: «" + w + "»", "💬 " + who + " каже: «" + w + "» — правильно? Тисни ТАК або НІ!");
        return this.push({ k: "buzz", by: who, w });
      }
      case "judge": {
        if (!host || s.phase !== "judge") return;
        const by = s.buzzer, w = s.answer;
        if (d.ok === true) {
          s.tries.push({ by, w, ok: true });
          s.scores[by] = (s.scores[by] || 0) + 1;
          s.phase = "reveal"; s.winner = by; s.reveal = w; s.nextAt = now + VG_REVEAL_MS;
          this.keep();
          return this.push({ k: "yes", by, w });
        }
        s.tries.push({ by, w, ok: false });
        s.phase = "guess"; s.buzzer = ""; s.answer = ""; s.lock[by] = now + VG_LOCK_MS;
        return this.push({ k: "no", by, w });
      }
      case "hint": {
        const h = vgClean(d.h, 80);
        if (!host || !["guess", "judge"].includes(s.phase) || !h) return;
        s.hint = h;
        return this.push({ k: "hint", by: who });
      }
      case "giveup": {
        // nobody gets it: the one who asked tells the answer, no points
        if (!host || s.phase !== "guess") return;
        s.phase = "reveal"; s.winner = ""; s.reveal = vgClean(d.a, 40); s.buzzer = ""; s.nextAt = now + VG_REVEAL_MS;
        this.keep();
        return this.push({ k: "giveup", by: who });
      }
      case "next": {
        if (s.phase !== "reveal") return;
        this.newRound(this.nextHost(s.host));
        return this.push({ k: "next", by: who });
      }
      case "react": {
        const e = Number(d.e);
        if (!Number.isInteger(e) || e < 0 || e > 5 || now - (me.lastReact || 0) < 300) return;
        me.lastReact = now; ws.serializeAttachment(me);
        this.broadcast({ type: "fx", k: "react", by: who, e });
        return;
      }
      case "push": {
        // notifications for this episode on this phone: on (with the subscription) or off
        const sub = d.sub || {}, keys = sub.keys || {}, endpoint = String(sub.endpoint || "");
        if (!pushEndpointOk(endpoint, this.env.PUSH_LOCAL_TEST === "1")) return;
        if (!d.on) delete this.subs[endpoint];
        else {
          if (!/^[A-Za-z0-9_-]{80,100}$/.test(keys.p256dh || "") || !/^[A-Za-z0-9_-]{16,32}$/.test(keys.auth || "")) return;
          if (Object.keys(this.subs).length >= 16 && !this.subs[endpoint]) return;
          this.subs[endpoint] = { name: who, endpoint, p256dh: keys.p256dh, auth: keys.auth };
        }
        await this.ctx.storage.put("subs", this.subs);
        this.send(ws, { type: "pushok", on: !!d.on });
        return;
      }
      case "reset": {
        // this episode's leaderboard back to 0, and an empty history
        s.scores = {}; s.asked = {};
        for (const n of s.seats) s.scores[n] = 0;
        s.hist = []; s.round = 0;
        this.newRound(s.host);
        this.tidy();
        return this.push({ k: "reset", by: who });
      }
      default:
        return;
    }
  }

  // a finished round goes into the history
  keep() {
    const s = this.s;
    s.hist.push({ n: s.round, host: s.host, q: s.q, hint: s.hint, tries: s.tries.slice(), winner: s.winner, a: s.reveal, t: Date.now() });
    if (s.hist.length > VG_HIST) s.hist = s.hist.slice(-VG_HIST);
  }

  async alarm() {
    const s = this.s, now = Date.now();
    if (s.phase === "reveal" && s.nextAt && now >= s.nextAt - 50) {
      this.newRound(this.nextHost(s.host));
      return this.push({ k: "next" });
    }
    if (s.phase === "reveal" && s.nextAt) await this.ctx.storage.setAlarm(Math.max(s.nextAt, now + 100));
  }

  async webSocketClose(ws, code, reason) {
    await this.leave(ws);
    try { ws.close(code, reason); } catch { /* closed */ }
  }

  async webSocketError(ws) {
    await this.leave(ws);
  }

  async leave(ws) {
    const me = ws.deserializeAttachment();
    if (!me || me.gone || !me.name) return;
    ws.serializeAttachment({ ...me, gone: true });
    this.tidy();
    await this.push({ k: "gone", by: me.name });
    await this.onAir();
  }
}
