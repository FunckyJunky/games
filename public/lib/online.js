/* ERROR-ONLINE: tells error.blue which game this browser is in, so the who's-online list
   (GET /api/online, shown on the ERROR-OLED box) knows who plays what.

   Add to a page:   <script src="/lib/online.js" defer></script>
   (game pages under /games/, /voyage and /music3d get it from the server by themselves)

   - Every 25 s (and right away when the page opens or comes back) it POSTs
     { sid, path, name, detail } to /api/online. A closed or hidden tab says "bye".
   - The name is window.errorOnline.name if the game set it, else the name typed in
     ERROR-CHAT / GameLab ("gamelab-player-name"), else the TUSA / Вгадай що name, else "guest".
   - A game can say more:   window.errorOnline = { name: 'VIKA', detail: 'S01E04' };
                            window.ErrorOnline && ErrorOnline.update();   // send it now
   - data-detail="..." / data-name="..." on the script tag: a fixed detail / a name to use when there's none
     (the glasses send their app's name, and "glasses").
   Inside an iframe it stays silent. */
(() => {
  'use strict';
  if (window.top !== window || window.ErrorOnline) return;
  const me = document.currentScript;
  const fixedDetail = (me && me.dataset.detail) || '';
  const get = (k) => { try { return localStorage.getItem(k) || ''; } catch (e) { return ''; } };
  let sid = '';
  try { sid = sessionStorage.getItem('error-online-sid') || ''; } catch (e) {}
  if (!/^[A-Za-z0-9]{12}$/.test(sid)) {
    const a = 'abcdefghijkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789', b = new Uint8Array(12);
    crypto.getRandomValues(b);
    sid = Array.from(b, (x) => a[x % a.length]).join('');
    try { sessionStorage.setItem('error-online-sid', sid); } catch (e) {}
  }
  function body(bye) {
    const o = window.errorOnline || {};
    const name = String(o.name || get('gamelab-player-name') || get('tusa-name') || (me && me.dataset.name) || 'guest').trim().slice(0, 16);
    const detail = String(o.detail || fixedDetail || '').slice(0, 24);
    return JSON.stringify(bye ? { sid, bye: 1 } : { sid, path: location.pathname, name, detail });
  }
  let timer = 0, last = '';
  function send(bye) {
    const b = body(bye);
    if (bye) {
      try { if (navigator.sendBeacon && navigator.sendBeacon('/api/online', new Blob([b], { type: 'application/json' }))) return; } catch (e) {}
    }
    last = b;
    try { fetch('/api/online', { method: 'POST', headers: { 'content-type': 'application/json' }, body: b, keepalive: true }).catch(() => {}); } catch (e) {}
  }
  function start() { clearInterval(timer); send(false); timer = setInterval(() => send(false), 25000); }
  function stop() { clearInterval(timer); timer = 0; send(true); }
  document.addEventListener('visibilitychange', () => (document.hidden ? stop() : start()));
  addEventListener('pagehide', stop);
  addEventListener('pageshow', (e) => { if (e.persisted && !document.hidden) start(); });
  window.ErrorOnline = {
    update() { if (!document.hidden && body(false) !== last) start(); },
    sid: () => sid,
  };
  if (!document.hidden) start();
})();
