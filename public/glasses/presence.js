/* Glasses HUD link: keeps the glasses and the control panel (error.blue/hud) in step, live.
   Every glasses app loads it:   <script src="/glasses/presence.js" data-app="flight"></script>

   - Live channel: a WebSocket room /api/room/hud/<CODE> (the glasses join as "glasses", the
     panel as "panel"). Settings changed on the panel arrive here within a moment, are stored
     in localStorage "glasses.settings" and handed to the open app (GlassesPresence.onSettings).
     The app that's open and the card you look at go the other way, straight away.
   - Backup: POST /api/hud/<CODE>/ping every 15 s (and "hidden" when the display goes off), so
     the panel also knows when the glasses were last on.
   CODE is the 6-letter pairing code that SETUP.EXE shows (localStorage "glasses.code").
   Inside an iframe (the panel's live preview) it stays silent. */
(() => {
  "use strict";
  const listeners = [], handlers = {};
  if (window.top !== window) {
    window.GlassesPresence = { code: () => null, setApp() {}, focus() {}, onSettings(fn) { listeners.push(fn); }, on() {}, send() { return false; }, sendSettings() {}, live: () => false };
    return;
  }
  const ALPHA = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  function code() {
    let c = null;
    try { c = JSON.parse(localStorage.getItem("glasses.code") || "null"); } catch {}
    if (typeof c !== "string" || !/^[A-HJ-NP-Z2-9]{6}$/.test(c)) {
      const b = new Uint8Array(6);
      crypto.getRandomValues(b);
      c = Array.from(b, (x) => ALPHA[x % ALPHA.length]).join("");
      try { localStorage.setItem("glasses.code", JSON.stringify(c)); } catch {}
    }
    return c;
  }
  const readSettings = () => { try { return JSON.parse(localStorage.getItem("glasses.settings") || "{}") || {}; } catch { return {}; } };
  const me = document.currentScript;
  let app = (me && me.dataset.app) || "unknown", focus = null, timer = 0, lastSent = 0, later = 0;
  // who's online on error.blue (ERROR-OLED): "glasses, in FLIGHT.EXE"
  try {
    const on = document.createElement("script");
    on.src = "/lib/online.js"; on.defer = true; on.dataset.detail = app.toUpperCase().slice(0, 8) + ".EXE"; on.dataset.name = "glasses";
    (document.head || document.documentElement).appendChild(on);
  } catch {}
  const state = () => (document.hidden ? "hidden" : "active");

  /* ---------- backup heartbeat (also what the panel shows after the fact) ---------- */
  const url = () => "/api/hud/" + code() + "/ping";
  function ping(st) {
    const body = JSON.stringify({ app, state: st, focus });
    lastSent = Date.now();
    if (st === "hidden" && navigator.sendBeacon) { try { if (navigator.sendBeacon(url(), body)) return; } catch {} }
    try { fetch(url(), { method: "POST", body, keepalive: true }).catch(() => {}); } catch {}
  }
  function start() { clearInterval(timer); ping("active"); timer = setInterval(() => ping("active"), 15000); }

  /* ---------- live channel ---------- */
  let ws = null, tries = 0, retryT = 0;
  function say(d) { if (ws && ws.readyState === 1) { try { ws.send(JSON.stringify(d)); return true; } catch {} } return false; }
  const hello = (t = "hello") => ({ t, app, focus, state: state(), updated: readSettings().updatedAt || 0 });
  function connect() {
    if (ws && ws.readyState <= 1) return;
    clearTimeout(retryT);
    try {
      ws = new WebSocket((location.protocol === "https:" ? "wss:" : "ws:") + "//" + location.host + "/api/room/hud/" + code() + "?name=glasses");
    } catch { ws = null; retry(); return; }
    ws.onopen = () => { tries = 0; say(hello()); };
    ws.onmessage = (e) => {
      let m; try { m = JSON.parse(e.data); } catch { return; }
      if (m.type === "join" && m.player && m.player.name === "panel") say(hello());
      if (m.type !== "msg" || !m.data) return;
      const d = m.data;
      if (d.t === "who") say(hello());
      else if (d.t === "settings" && d.settings) apply(d.settings, Number(d.updated) || 0);
      else if (typeof d.t === "string" && handlers[d.t]) {          // app messages, e.g. "geo" for GEOLOCK.EXE
        for (const fn of handlers[d.t]) { try { fn(d); } catch (err) { console.error(err); } }
        say({ t: "ack", kind: d.t, updated: d.updated || 0, app });
      }
    };
    ws.onclose = () => { ws = null; retry(); };
    ws.onerror = () => {};
  }
  function retry() { clearTimeout(retryT); if (document.hidden) return; retryT = setTimeout(connect, Math.min(15000, 800 * 2 ** tries++)); }
  setInterval(() => { if (ws && ws.readyState === 1) ws.send('{"type":"ping"}'); }, 25000);

  // settings from the panel: keep them, hand them to the open app
  function apply(s, upd) {
    const cur = readSettings();
    if ((cur.updatedAt || 0) >= upd) { say({ t: "ack", updated: upd, app, same: true }); return; }
    const next = Object.assign({}, cur, s, { updatedAt: upd });
    try { localStorage.setItem("glasses.settings", JSON.stringify(next)); } catch {}
    say({ t: "ack", updated: upd, app });
    for (const fn of listeners) { try { fn(next, cur); } catch (err) { console.error(err); } }
  }

  document.addEventListener("visibilitychange", () => {
    if (document.hidden) { clearInterval(timer); ping("hidden"); say({ t: "state", app, focus, state: "hidden" }); }
    else { start(); if (!ws) connect(); else say(hello("state")); }
  });
  window.addEventListener("pageshow", (e) => { if (e.persisted) { start(); connect(); say(hello("state")); } });
  window.addEventListener("pagehide", () => { try { if (ws) ws.close(1000, "bye"); } catch {} });

  window.GlassesPresence = {
    code,
    live: () => !!(ws && ws.readyState === 1),
    setApp(a) { if (a && a !== app) { app = a; ping("active"); say({ t: "state", app, focus, state: state() }); } },
    focus(id) {                                   // the home screen: which card you're looking at
      if (id === focus) return;
      focus = id;
      say({ t: "state", app, focus, state: state() });
      clearTimeout(later);
      const wait = 1200 - (Date.now() - lastSent);
      if (wait <= 0) ping("active"); else later = setTimeout(() => ping("active"), wait);
    },
    onSettings(fn) { listeners.push(fn); },        // fn(newSettings, oldSettings) when the panel changes something
    on(type, fn) { (handlers[type] = handlers[type] || []).push(fn); },   // other live messages from the panel
    sendSettings(settings, updated) { say({ t: "settings", settings, updated }); },   // changed on the glasses (SETUP.EXE)
    send(type, data) { return say(Object.assign({}, data, { t: type, app })); },      // app news for the panel, e.g. a DRUNK.EXE result
  };
  start();
  connect();
})();
