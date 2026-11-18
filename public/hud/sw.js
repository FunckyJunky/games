/* Glasses HUD panel (error.blue/hud) as an installed web app: the page, font and icons are kept
   so the app opens even with a bad connection. Always tries the network first, so updates show
   straight away. Only /hud/ files; the live data (/api/) is never cached. */
const CACHE = "hud-v1";
const SHELL = ["/hud/", "/hud/vga.woff2", "/hud/icon-192.png", "/hud/apple-touch-icon.png", "/hud/manifest.webmanifest"];
self.addEventListener("install", (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(SHELL)).then(() => self.skipWaiting()));
});
self.addEventListener("activate", (e) => {
  e.waitUntil(caches.keys().then((ks) => Promise.all(ks.filter((k) => k !== CACHE).map((k) => caches.delete(k)))).then(() => self.clients.claim()));
});
self.addEventListener("fetch", (e) => {
  const u = new URL(e.request.url);
  if (e.request.method !== "GET" || u.origin !== location.origin || !u.pathname.startsWith("/hud/")) return;
  const key = e.request.mode === "navigate" ? "/hud/" : e.request;
  e.respondWith(
    fetch(e.request).then((r) => {
      if (r.ok && r.type === "basic") { const c = r.clone(); caches.open(CACHE).then((x) => x.put(key, c)); }
      return r;
    }).catch(() => caches.match(key, { ignoreSearch: true }).then((r) => r || Response.error()))
  );
});
