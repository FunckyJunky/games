/* Glasses HUD presence: tells the control panel at error.blue/hud that the glasses are on,
   and which app is open. Every glasses app includes it:
     <script src="/glasses/presence.js" data-app="flight"></script>
   It pings POST /api/hud/<CODE>/ping every 15 s while the page is visible, and once more
   ("hidden") when the display goes off. CODE is the 6-letter pairing code that SETUP.EXE
   shows; it lives in localStorage "glasses.code" (created here on first use).
   Inside an iframe (the panel's live preview) it stays silent. */
(() => {
  "use strict";
  if (window.top !== window) { window.GlassesPresence = { code: () => null, setApp() {}, focus() {} }; return; }
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
  const me = document.currentScript;
  let app = (me && me.dataset.app) || "unknown", focus = null, timer = 0, lastSent = 0, later = 0;
  const url = () => "/api/hud/" + code() + "/ping";
  function send(state) {
    const body = JSON.stringify({ app, state, focus });
    lastSent = Date.now();
    if (state === "hidden" && navigator.sendBeacon) { try { if (navigator.sendBeacon(url(), body)) return; } catch {} }
    try { fetch(url(), { method: "POST", body, keepalive: true }).catch(() => {}); } catch {}
  }
  function start() { clearInterval(timer); send("active"); timer = setInterval(() => send("active"), 15000); }
  document.addEventListener("visibilitychange", () => {
    if (document.hidden) { clearInterval(timer); send("hidden"); } else start();
  });
  window.addEventListener("pageshow", (e) => { if (e.persisted) start(); });
  window.GlassesPresence = {
    code,
    setApp(a) { if (a && a !== app) { app = a; send("active"); } },
    focus(id) {                                   // the home screen: which card you're looking at
      if (id === focus) return;
      focus = id;
      clearTimeout(later);
      const wait = 1200 - (Date.now() - lastSent);
      if (wait <= 0) send("active"); else later = setTimeout(() => send("active"), wait);
    },
  };
  start();
})();
