// HOME FRONT: screens at home (an ESP32-S3 board, or any browser at error.blue/thuisfront/dev) that show
// one web app, controlled from a phone at error.blue/thuisfront.
//
//   GET  /thuisfront/dev/ws?id=HF-XXXXXX&...   a device (WebSocket). Also at /api/hf/dev/ws.
//   GET  /api/hf/ws?id=HF-XXXXXX&home=CODE     a controller for one device (WebSocket)
//   GET  /api/hf/list?home=CODE                the devices with that home code ("" = no code)
//   POST /api/hf/forget {id, home, key}        take an offline device off the list
//
// A device can have a password. Then a controller gets nothing (no state, no photo) and can change nothing
// until it sends {t:"auth", key} with a key it got before, or {t:"auth", pw} with the password.
//
// Everything lives in one Durable Object ("main"). Protocol (all numbers little-endian):
//   device -> server   {t:"hi", id, name, kind, ver, wifi, ip, rssi, home, tb, rev}   once, after connecting
//                      {t:"ping", rssi, fps, heap}                                  every 15 s
//                      {t:"got", s}                                                 a video frame is on screen
//   server -> device   {t:"state", state}                                           whenever it changes
//                      {t:"ota", ver, url, size, md5}   new firmware: get it over WiFi (boards that said ota:1)
//                      binary "HFT1..."  the text picture (sent before the state that names it)
//                      binary "HFP1..."  the photo (a JPEG, sent before the state that names it)
//                      binary "HFV1..." / "HFV2..."  one video frame (only after the last one was "got")
//   controller -> server  {t:"auth", key} or {t:"auth", pw}  first, always (ignored when there is no password)
//                         {t:"setpw", pw, old}  set / change / remove ("") the password    {t:"nopw"}  "no password, thanks"
//                         {t:"set", p:{...}}  change the state     {t:"name", name}  rename the device
//                         binary "HFT1..."    new text picture     binary "HFP1..."  new photo
//                         binary "HFV1..." / "HFV2..."  video frame
//   server -> controller  {t:"state", state, dev}   {t:"dev", dev}   {t:"got", s}   {t:"err", msg}
//                         {t:"authok", key}   {t:"locked", dev}   {t:"authfail", wait}   {t:"pwset", key}   {t:"pwfail", msg, wait}
//
// HFT1 (text): "HFT1" u16 w, u16 h, i16 x, i16 y, u16 n, u16 flags, n x (u16 x,y,w,h) letter boxes,
//   then the w*h picture (levels 0-15) run-length coded: byte b, level b&15, count (b>>4)+1,
//   or when b>>4 == 15: count 16 + next byte.
// HFV1 (video, grey for the DOS filters): "HFV1" u16 w, u16 h, u32 seq, then w*h 4-bit levels, two per byte,
//   high nibble first.  HFV2 (video in colour): "HFV2" u16 w, u16 h, u32 seq, then a JPEG.
// HFP1 (photo): "HFP1" then a JPEG (320x172, colour; the screen applies the filter itself).

import { DurableObject } from "cloudflare:workers";
import { FIRMWARE } from "./hf_firmware.js";

// "1.10" > "1.9": compare version numbers part by part
function newer(a, b) {
  const x = String(a || "").split(".").map(Number), y = String(b || "").split(".").map(Number);
  for (let i = 0; i < Math.max(x.length, y.length); i++) { const d = (x[i] || 0) - (y[i] || 0); if (d) return d > 0; }
  return false;
}

const ID_RE = /^(HF|WEB)-[A-F0-9]{4,12}$/;
const HOME_RE = /^[A-Z0-9]{0,12}$/;
const TEXT_MAX = 1000000;          // bytes of one HFT1
const FRAME_MAX = 120000;          // bytes of one video frame (HFV1 320x172 is 27532, HFV2 is a JPEG)
const PHOTO_MAX = 500000;          // bytes of one HFP1
const CHUNK = 100000;              // HFT1 is stored in pieces this big
const ONLINE_MS = 45000;           // no ping for this long = offline
const FLOW_MS = 1500;              // a frame the device never "got" is given up after this
const PW_ITER = 10000;             // PBKDF2 rounds for device passwords
const KEYS_MAX = 30;               // browsers that know the password, per device

const APPS = ["", "comm", "clock"];
const TZ_RE = /^[A-Za-z0-9_+\-\/]{1,40}$/;                     // an IANA time zone, for browsers
const PTZ_RE = /^[A-Z]{3}[-+]?\d{1,2}(:\d\d)?([A-Z]{3}([-+]?\d{1,2}(:\d\d)?)?(,M\d{1,2}\.\d\.\d(\/\d{1,2}(:\d\d)?)?){2})?$/;   // the same as a POSIX TZ, for the board
const MODES = ["text", "video", "both", "photo"];
const ANIMS = ["static", "scroll", "rise", "type", "blink", "wave", "glitch", "pulse"];
const STYLES = ["plain", "shadow", "outline", "bold"];

function json(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "Content-Type": "application/json; charset=utf-8", "Cache-Control": "no-store", "Access-Control-Allow-Origin": "*" },
  });
}
const int = (v, lo, hi, d) => { const n = Math.round(Number(v)); return Number.isFinite(n) ? Math.max(lo, Math.min(hi, n)) : d; };
const pick = (v, list, d) => (list.includes(v) ? v : d);
const str = (v, max) => String(v == null ? "" : v).slice(0, max);
function cleanName(v) {
  return String(v || "").replace(/[\u0000-\u001f<>"\\]/g, "").trim().slice(0, 20).toUpperCase();
}
const b64 = (u8) => { let s = ""; for (const c of u8) s += String.fromCharCode(c); return btoa(s); };
const unb64 = (s) => Uint8Array.from(atob(s), (c) => c.charCodeAt(0));
const enc = (s) => new TextEncoder().encode(s);
async function pwHash(pw, salt) {
  const k = await crypto.subtle.importKey("raw", enc(pw), "PBKDF2", false, ["deriveBits"]);
  return b64(new Uint8Array(await crypto.subtle.deriveBits({ name: "PBKDF2", hash: "SHA-256", salt, iterations: PW_ITER }, k, 256)));
}
const sha = async (s) => b64(new Uint8Array(await crypto.subtle.digest("SHA-256", enc(s))));
const cleanPw = (v) => (typeof v === "string" ? v.slice(0, 64) : "");
const cleanHome = (v) => { const h = String(v || "").toUpperCase().replace(/[^A-Z0-9]/g, "").slice(0, 12); return HOME_RE.test(h) ? h : ""; };

export function defaultState() {
  return {
    rev: 1, app: "", screen: 1, bright: 80, scan: 0, fx: 1,      // fx 0 = all effects off (no animation, scanlines, glitches)
    comm: {
      mode: "text", tb: "", pb: "", anim: "static", speed: 5, tpal: 0, vpal: 0, bg: 0,
      ppal: 4, pfit: "fill", pbri: 0, pcon: 0,
      text: "", font: "vt323", size: 0, align: "c", valign: "m", style: "plain", caps: 0,
      vq: "hi", look: "smooth", vbri: 0, vcon: 0, mirror: 1, fps: 12,
    },
    // CLOCK: the time in big dot-matrix digits. sec 0 = no seconds, 1 = a row of 60 dots, 2 = HH:MM:SS
    clock: { pal: 0, bg: 0, h24: 1, sec: 1, date: 1, tz: "", ptz: "" },
  };
}

// only known settings, with sane values
function mergeState(s, p) {
  if (!p || typeof p !== "object") return false;
  let changed = false;
  const set = (o, k, v) => { if (o[k] !== v) { o[k] = v; changed = true; } };
  if ("app" in p) set(s, "app", pick(p.app, APPS, s.app));
  if ("screen" in p) set(s, "screen", p.screen ? 1 : 0);
  if ("bright" in p) set(s, "bright", int(p.bright, 5, 100, s.bright));
  if ("scan" in p) set(s, "scan", p.scan ? 1 : 0);
  if ("fx" in p) set(s, "fx", p.fx ? 1 : 0);
  const k = p.clock, q = s.clock;
  if (k && typeof k === "object") {
    if ("pal" in k) set(q, "pal", int(k.pal, 0, 3, q.pal));
    if ("bg" in k) set(q, "bg", int(k.bg, 0, 1, q.bg));
    if ("h24" in k) set(q, "h24", k.h24 ? 1 : 0);
    if ("sec" in k) set(q, "sec", int(k.sec, 0, 2, q.sec));
    if ("date" in k) set(q, "date", k.date ? 1 : 0);
    if ("tz" in k && (k.tz === "" || TZ_RE.test(String(k.tz)))) set(q, "tz", String(k.tz));
    if ("ptz" in k && (k.ptz === "" || (String(k.ptz).length <= 60 && PTZ_RE.test(String(k.ptz))))) set(q, "ptz", String(k.ptz));
  }
  const c = p.comm, o = s.comm;
  if (c && typeof c === "object") {
    if ("mode" in c) set(o, "mode", pick(c.mode, MODES, o.mode));
    if ("anim" in c) set(o, "anim", pick(c.anim, ANIMS, o.anim));
    if ("speed" in c) set(o, "speed", int(c.speed, 1, 10, o.speed));
    if ("tpal" in c) set(o, "tpal", int(c.tpal, 0, 3, o.tpal));
    if ("vpal" in c) set(o, "vpal", int(c.vpal, 0, 4, o.vpal));          // 4 = no filter, real colours
    if ("ppal" in c) set(o, "ppal", int(c.ppal, 0, 4, o.ppal));
    if ("pfit" in c) set(o, "pfit", pick(c.pfit, ["fill", "fit"], o.pfit));
    if ("pbri" in c) set(o, "pbri", int(c.pbri, -50, 50, o.pbri));
    if ("pcon" in c) set(o, "pcon", int(c.pcon, -50, 50, o.pcon));
    if ("bg" in c) set(o, "bg", int(c.bg, 0, 2, o.bg));
    if ("text" in c) set(o, "text", str(c.text, 2000));
    if ("font" in c) set(o, "font", str(c.font, 24).replace(/[^a-z0-9-]/g, ""));
    if ("size" in c) set(o, "size", int(c.size, 0, 172, o.size));
    if ("align" in c) set(o, "align", pick(c.align, ["l", "c", "r"], o.align));
    if ("valign" in c) set(o, "valign", pick(c.valign, ["t", "m", "b"], o.valign));
    if ("style" in c) set(o, "style", pick(c.style, STYLES, o.style));
    if ("caps" in c) set(o, "caps", c.caps ? 1 : 0);
    if ("vq" in c) set(o, "vq", pick(c.vq, ["lo", "hi"], o.vq));
    if ("look" in c) set(o, "look", pick(c.look, ["smooth", "dither"], o.look));
    if ("vbri" in c) set(o, "vbri", int(c.vbri, -50, 50, o.vbri));
    if ("vcon" in c) set(o, "vcon", int(c.vcon, -50, 50, o.vcon));
    if ("mirror" in c) set(o, "mirror", c.mirror ? 1 : 0);
    if ("fps" in c) set(o, "fps", int(c.fps, 1, 30, o.fps));
  }
  return changed;
}

const magic = (u8) => (u8.length >= 4 ? String.fromCharCode(u8[0], u8[1], u8[2], u8[3]) : "");

export class HomeFront extends DurableObject {
  constructor(ctx, env) {
    super(ctx, env);
    this.devs = new Map();     // id -> device record (cache of storage "dev:<id>")
    this.states = new Map();   // id -> state (cache of storage "st:<id>")
    this.flow = new Map();     // id -> { busy, at, pending }   video frames on their way
    this.tries = new Map();    // id -> { n, until }   wrong passwords (in memory)
    this.otaAt = new Map();    // id -> when it was last told to update (in memory)
  }

  // ---------- storage ----------
  async dev(id) {
    if (!this.devs.has(id)) this.devs.set(id, (await this.ctx.storage.get("dev:" + id)) || null);
    return this.devs.get(id);
  }
  async saveDev(d) { this.devs.set(d.id, d); await this.ctx.storage.put("dev:" + d.id, d); }
  async state(id) {
    if (!this.states.has(id)) {
      const s = (await this.ctx.storage.get("st:" + id)) || defaultState();
      s.comm = { ...defaultState().comm, ...(s.comm || {}) };
      s.clock = { ...defaultState().clock, ...(s.clock || {}) };
      this.states.set(id, s);
    }
    return this.states.get(id);
  }
  async saveState(id, s) { this.states.set(id, s); await this.ctx.storage.put("st:" + id, s); }
  async loadText(id) { return this.loadBlob("tb", id); }
  async saveText(id, tb, data) { return this.saveBlob("tb", id, tb, data); }
  async loadPhoto(id) { return this.loadBlob("pb", id); }
  async savePhoto(id, pb, data) { return this.saveBlob("pb", id, pb, data); }
  // kind "tb" = text picture, "pb" = photo; stored as <kind>m:<id> (meta) + <kind>c:<id>:<n> (pieces)
  async loadBlob(kind, id) {
    const meta = await this.ctx.storage.get(kind + "m:" + id);
    if (!meta) return null;
    const keys = []; for (let i = 0; i < meta.n; i++) keys.push(kind + "c:" + id + ":" + i);
    const got = await this.ctx.storage.get(keys);
    const out = new Uint8Array(meta.size);
    let o = 0;
    for (const k of keys) { const part = got.get(k); if (!part) return null; out.set(new Uint8Array(part), o); o += part.byteLength; }
    return { tb: meta.tb, data: out };
  }
  async saveBlob(kind, id, tag, data) {
    const old = await this.ctx.storage.get(kind + "m:" + id);
    const put = {};
    let n = 0;
    for (let o = 0; o < data.length; o += CHUNK, n++) put[kind + "c:" + id + ":" + n] = data.slice(o, o + CHUNK).buffer;
    put[kind + "m:" + id] = { tb: tag, n, size: data.length };
    await this.ctx.storage.put(put);
    if (old && old.n > n) { const del = []; for (let i = n; i < old.n; i++) del.push(kind + "c:" + id + ":" + i); await this.ctx.storage.delete(del); }
  }

  // ---------- sockets ----------
  socks(role, id) { return this.ctx.getWebSockets(role + ":" + id); }
  devSock(id) {
    const list = this.socks("dev", id);
    return list.length ? list[list.length - 1] : null;      // the newest connection wins
  }
  online(id) {
    const now = Date.now();
    return this.socks("dev", id).some((ws) => { const a = ws.deserializeAttachment() || {}; return now - (a.seen || 0) < ONLINE_MS; });
  }
  send(ws, msg) { try { ws.send(typeof msg === "string" || msg instanceof ArrayBuffer || ArrayBuffer.isView(msg) ? msg : JSON.stringify(msg)); } catch { /* gone */ } }
  // controllers that may see this device (no password, or they gave it)
  ctlSocks(id) { return this.socks("ctl", id).filter((ws) => (ws.deserializeAttachment() || {}).ok); }
  toCtl(id, msg) { const s = JSON.stringify(msg); for (const ws of this.ctlSocks(id)) this.send(ws, s); }
  // passwords: the device record has pw {salt, hash} and keys (SHA-256 of the keys handed out to browsers)
  async keyOk(d, key) { return !d.pw || (typeof key === "string" && key.length > 10 && key.length < 80 && (d.keys || []).includes(await sha(key))); }
  async newKey(d) {
    const k = b64(crypto.getRandomValues(new Uint8Array(24))).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
    d.keys = [...(d.keys || []), await sha(k)].slice(-KEYS_MAX);
    return k;
  }
  // a wrong password counts; 5 wrong ones = wait 30 s, then twice as long each time (max 15 min)
  // (tk = the counter: the device id for typing the password to get in, id + "#set" for changing it)
  waitFor(tk) { const t = this.tries.get(tk); return t && t.until > Date.now() ? Math.ceil((t.until - Date.now()) / 1000) : 0; }
  async pwOk(tk, d, pw) {
    if (await pwHash(pw, unb64(d.pw.salt)) === d.pw.hash) { this.tries.delete(tk); return true; }
    const t = this.tries.get(tk) || { n: 0, until: 0 };
    t.n++; if (t.n >= 5) t.until = Date.now() + Math.min(15 * 60000, 30000 * 2 ** (t.n - 5));
    this.tries.set(tk, t);
    return false;
  }
  async sendAll(ws, id) {                       // a controller that may see it: the photo (for its preview), then the state
    const s = await this.state(id), d = await this.dev(id);
    if (s.comm.pb) { const p = await this.loadPhoto(id); if (p && p.tb === s.comm.pb) this.send(ws, p.data); }
    this.send(ws, { t: "state", state: { ...s, name: d ? d.name : "" }, dev: await this.devInfo(id) });
  }
  async devInfo(id) {
    const d = await this.dev(id);
    if (!d) return null;
    const ws = this.devSock(id), a = ws ? ws.deserializeAttachment() || {} : {};
    const s = await this.state(id);
    return {
      id: d.id, name: d.name, kind: d.kind, ver: d.ver, wifi: d.wifi, ip: d.ip, rssi: a.rssi ?? d.rssi,
      fps: a.fps || 0, seen: Math.max(d.seen || 0, a.seen || 0), online: this.online(id), app: s.app, screen: s.screen, fx: s.fx === 0 ? 0 : 1,
      mode: s.comm.mode, homeSet: !!d.home, locked: !!d.pw, pwAsked: !!(d.pwAsked || d.pw),
      ota: !!d.ota, fwNew: d.kind === "esp" && newer(FIRMWARE.ver, d.ver) ? FIRMWARE.ver : "",
    };
  }
  async pushState(id, alsoText, alsoPhoto) {
    const s = await this.state(id), d = await this.dev(id);
    const out = { ...s, name: d ? d.name : "" };
    const t = alsoText ? await this.loadText(id) : null, p = alsoPhoto ? await this.loadPhoto(id) : null;
    for (const ws of this.socks("dev", id)) {
      if (t && t.tb === s.comm.tb) this.send(ws, t.data);
      if (p && p.tb === s.comm.pb) this.send(ws, p.data);
      this.send(ws, { t: "state", state: out });
    }
    if (p && p.tb === s.comm.pb) for (const ws of this.ctlSocks(id)) this.send(ws, p.data);   // other phones see the new photo too
    this.toCtl(id, { t: "state", state: out, dev: await this.devInfo(id) });
  }

  async fetch(request) {
    const url = new URL(request.url);
    const path = url.pathname;

    if (path === "/api/hf/list") {
      const home = cleanHome(url.searchParams.get("home"));
      const all = await this.ctx.storage.list({ prefix: "dev:" });
      const out = [];
      for (const d of all.values()) {
        this.devs.set(d.id, d);
        if ((d.home || "") !== home) continue;
        out.push(await this.devInfo(d.id));
      }
      out.sort((a, b) => (b.online - a.online) || (b.seen - a.seen));
      return json({ devices: out, now: Date.now() });
    }

    if (path === "/api/hf/forget" && request.method === "POST") {
      let b = {};
      try { b = await request.json(); } catch { /* empty */ }
      const id = String(b.id || "");
      const d = ID_RE.test(id) ? await this.dev(id) : null;
      if (!d) return json({ error: "no such device" }, 404);
      if ((d.home || "") !== cleanHome(b.home)) return json({ error: "wrong home code" }, 403);
      if (!(await this.keyOk(d, b.key))) return json({ error: "it has a password: open it with the password first" }, 403);
      if (this.online(id)) return json({ error: "it is online" }, 409);
      const del = ["dev:" + id, "st:" + id];
      for (const kind of ["tb", "pb"]) {
        const meta = await this.ctx.storage.get(kind + "m:" + id);
        del.push(kind + "m:" + id);
        if (meta) for (let i = 0; i < meta.n; i++) del.push(kind + "c:" + id + ":" + i);
      }
      await this.ctx.storage.delete(del);
      this.devs.delete(id); this.states.delete(id); this.tries.delete(id); this.tries.delete(id + "#set");
      return json({ ok: true });
    }

    const isDev = path === "/thuisfront/dev/ws" || path === "/api/hf/dev/ws";
    if (isDev || path === "/api/hf/ws") {
      if (request.headers.get("Upgrade") !== "websocket") return json({ error: "connect with a WebSocket" }, 426);
      const id = String(url.searchParams.get("id") || "").toUpperCase();
      if (!ID_RE.test(id)) return json({ error: "bad device id" }, 400);
      let d = null;
      if (!isDev) {
        d = await this.dev(id);
        if (!d) return json({ error: "no such device" }, 404);
        if ((d.home || "") !== cleanHome(url.searchParams.get("home"))) return json({ error: "wrong home code" }, 403);
      }
      const pair = new WebSocketPair();
      const role = isDev ? "dev" : "ctl";
      const ok = isDev || !d.pw;                // a phone for a device with a password: first {t:"auth"}
      this.ctx.acceptWebSocket(pair[1], [role + ":" + id]);
      pair[1].serializeAttachment({ role, id, seen: Date.now(), ok });
      if (!isDev && ok) await this.sendAll(pair[1], id);
      return new Response(null, { status: 101, webSocket: pair[0] });
    }
    return json({ error: "not found" }, 404);
  }

  async webSocketMessage(ws, msg) {
    const a = ws.deserializeAttachment() || {};
    const id = a.id;
    if (!id) return;
    if (typeof msg !== "string") return a.role === "ctl" && a.ok ? this.ctlBinary(id, new Uint8Array(msg)) : undefined;
    let m;
    try { m = JSON.parse(msg); } catch { return; }
    if (!m || typeof m !== "object") return;
    if (a.role === "dev") return this.devMessage(ws, a, id, m);
    if (m.t === "auth") return this.ctlAuth(ws, a, id, m);
    if (!a.ok) return;                          // a password is needed first
    return this.ctlMessage(ws, id, m);
  }

  // ---------- a controller and the password ----------
  async ctlAuth(ws, a, id, m) {
    if (a.ok) return;
    const d = await this.dev(id);
    if (!d) return;
    const letIn = async (key) => { a.ok = true; ws.serializeAttachment(a); this.send(ws, { t: "authok", key: key || "" }); await this.sendAll(ws, id); };
    if (await this.keyOk(d, m.key)) return letIn("");
    const pw = cleanPw(m.pw);
    if (!pw) return this.send(ws, { t: "locked", dev: await this.devInfo(id) });
    const wait = this.waitFor(id);
    if (wait) return this.send(ws, { t: "authfail", wait });
    if (await this.pwOk(id, d, pw)) { const key = await this.newKey(d); await this.saveDev(d); return letIn(key); }
    this.send(ws, { t: "authfail", wait: this.waitFor(id) });
  }

  // ---------- from a device ----------
  async devMessage(ws, a, id, m) {
    const now = Date.now();
    if (m.t === "hi") {
      const old = await this.dev(id);
      const d = old || { id, created: now };
      // the device's own name counts the first time, or when it was renamed on the device itself (rename: 1);
      // otherwise the name set from a phone (stored here) wins
      if (!d.name || m.rename) d.name = cleanName(m.name) || id;
      d.kind = m.kind === "web" ? "web" : "esp";
      d.ver = str(m.ver, 16); d.wifi = str(m.wifi, 32); d.ip = str(m.ip, 40); d.rssi = int(m.rssi, -120, 0, 0);
      d.home = cleanHome(m.home); d.seen = now;
      d.ota = m.ota ? 1 : 0;
      await this.saveDev(d);
      ws.serializeAttachment({ ...a, seen: now, rssi: d.rssi, fps: 0 });
      this.flow.delete(id);
      // only one device connection per id: close older ones
      for (const o of this.socks("dev", id)) if (o !== ws) { try { o.close(4000, "replaced"); } catch { /* closed */ } }
      const s = await this.state(id);
      if (s.comm.tb && m.tb !== s.comm.tb) { const t = await this.loadText(id); if (t && t.tb === s.comm.tb) this.send(ws, t.data); }
      if (s.comm.pb && m.pb !== s.comm.pb) { const p = await this.loadPhoto(id); if (p && p.tb === s.comm.pb) this.send(ws, p.data); }
      this.send(ws, { t: "state", state: { ...s, name: d.name } });
      if (m.ota) this.offerFirmware(id, ws, m.otafail === FIRMWARE.ver ? 60 * 60000 : 10 * 60000);
      this.toCtl(id, { t: "dev", dev: await this.devInfo(id) });
      return;
    }
    if (m.t === "ping") {
      const wasOn = this.online(id);
      ws.serializeAttachment({ ...a, seen: now, rssi: int(m.rssi, -120, 0, a.rssi || 0), fps: int(m.fps, 0, 99, 0) });
      const d = await this.dev(id);
      if (d && now - (d.seen || 0) > 5 * 60 * 1000) { d.seen = now; await this.saveDev(d); }   // "last seen", now and then
      if (!wasOn || this.socks("ctl", id).length) this.toCtl(id, { t: "dev", dev: await this.devInfo(id) });
      this.offerFirmware(id, ws, 10 * 60000);   // new firmware also reaches boards that stay connected (a deploy keeps them on)
      return;
    }
    if (m.t === "got") {
      const f = this.flow.get(id);
      if (f) {
        f.busy = false;
        if (f.pending) { const p = f.pending; f.pending = null; this.sendFrame(id, p); }
      }
      this.toCtl(id, { t: "got", s: int(m.s, 0, 4294967295, 0) });
    }
  }

  // a board with older firmware that can update over WiFi: tell it where the new one is (not more often than "every" ms)
  offerFirmware(id, ws, every) {
    const d = this.devs.get(id);
    if (!ws || !d || d.kind !== "esp" || !d.ota || !newer(FIRMWARE.ver, d.ver)) return false;
    const now = Date.now();
    if (now - (this.otaAt.get(id) || 0) < every) return false;
    this.otaAt.set(id, now);
    this.send(ws, { t: "ota", ver: FIRMWARE.ver, url: FIRMWARE.path, size: FIRMWARE.size, md5: FIRMWARE.md5 });
    return true;
  }

  sendFrame(id, data) {
    const ws = this.devSock(id);
    if (!ws) return false;
    const f = this.flow.get(id) || { busy: false, at: 0, pending: null };
    this.flow.set(id, f);
    const now = Date.now();
    if (f.busy && now - f.at < FLOW_MS) { f.pending = data; return true; }   // only the newest one waits
    f.busy = true; f.at = now; f.pending = null;
    this.send(ws, data);
    return true;
  }

  // ---------- from a controller ----------
  async ctlBinary(id, u8) {
    const kind = magic(u8);
    if (kind === "HFV1" || kind === "HFV2") {
      if (u8.length < 12 || u8.length > FRAME_MAX) return;
      if (!this.sendFrame(id, u8)) this.toCtl(id, { t: "err", msg: "device offline" });
      return;
    }
    if (kind === "HFT1") {
      if (u8.length < 16 || u8.length > TEXT_MAX) return this.toCtl(id, { t: "err", msg: "text too big" });
      const tb = Date.now().toString(36);
      await this.saveText(id, tb, u8);
      const s = await this.state(id);
      s.comm.tb = tb; s.rev++;
      await this.saveState(id, s);
      await this.pushState(id, true, false);
    }
    if (kind === "HFP1") {
      if (u8.length < 100 || u8.length > PHOTO_MAX) return this.toCtl(id, { t: "err", msg: "photo too big" });
      const pb = "p" + Date.now().toString(36);
      await this.savePhoto(id, pb, u8);
      const s = await this.state(id);
      s.comm.pb = pb; s.rev++;
      await this.saveState(id, s);
      await this.pushState(id, false, true);
    }
  }

  async ctlMessage(ws, id, m) {
    if (m.t === "setpw") {                      // set, change or remove ("") the password; changing needs the old one
      const d = await this.dev(id);
      if (!d) return;
      const pw = cleanPw(m.pw);
      if (pw && pw.length < 4) return this.send(ws, { t: "pwfail", msg: "at least 4 letters" });
      if (d.pw) {
        const wait = this.waitFor(id + "#set");
        if (wait) return this.send(ws, { t: "pwfail", msg: "too many tries", wait });
        if (!(await this.pwOk(id + "#set", d, cleanPw(m.old)))) return this.send(ws, { t: "pwfail", msg: "wrong password", wait: this.waitFor(id + "#set") });
      }
      d.pwAsked = true;
      let key = "";
      if (pw) {
        const salt = crypto.getRandomValues(new Uint8Array(16));
        d.pw = { salt: b64(salt), hash: await pwHash(pw, salt) };
        d.keys = [];
        key = await this.newKey(d);             // this browser stays in; every other one has to type the new password
      } else { d.pw = null; d.keys = []; }
      await this.saveDev(d);
      this.send(ws, { t: "pwset", key });
      const info = await this.devInfo(id);
      if (pw) {
        for (const o of this.socks("ctl", id)) {
          if (o === ws) continue;
          const oa = o.deserializeAttachment() || {};
          if (oa.ok) { oa.ok = false; o.serializeAttachment(oa); this.send(o, { t: "locked", dev: info }); }
        }
      }
      return this.toCtl(id, { t: "dev", dev: info });
    }
    if (m.t === "ota") {                        // UPDATE FIRMWARE from a phone
      const ok = this.offerFirmware(id, this.devSock(id), 0);
      return this.send(ws, ok ? { t: "err", msg: "updating: about a minute" } : { t: "err", msg: "no update for this screen" });
    }
    if (m.t === "nopw") {
      const d = await this.dev(id);
      if (d && !d.pwAsked) { d.pwAsked = true; await this.saveDev(d); this.toCtl(id, { t: "dev", dev: await this.devInfo(id) }); }
      return;
    }
    if (m.t === "set") {
      const s = await this.state(id);
      if (mergeState(s, m.p)) { s.rev++; await this.saveState(id, s); await this.pushState(id, false, false); }
      return;
    }
    if (m.t === "name") {
      const d = await this.dev(id), name = cleanName(m.name);
      if (!d || !name) return;
      d.name = name; await this.saveDev(d);
      return this.pushState(id, false, false);
    }
    if (m.t === "hello") this.send(ws, { t: "state", state: { ...(await this.state(id)), name: (await this.dev(id)).name }, dev: await this.devInfo(id) });
  }

  async webSocketClose(ws, code, reason) {
    await this.gone(ws);
    try { ws.close(code, reason); } catch { /* closed */ }
  }
  async webSocketError(ws) { await this.gone(ws); }
  async gone(ws) {
    const a = ws.deserializeAttachment() || {};
    if (a.role !== "dev" || !a.id) return;
    ws.serializeAttachment({ ...a, seen: 0 });
    this.flow.delete(a.id);
    const d = await this.dev(a.id);
    if (d) { d.seen = Date.now(); await this.saveDev(d); }
    this.toCtl(a.id, { t: "dev", dev: await this.devInfo(a.id) });
  }
}
