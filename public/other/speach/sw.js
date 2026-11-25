/* Speech trainer (error.blue/other/speach) as an installed web app.
   - opens without a connection: the page, icons and fonts are kept
   - the page always comes from the network first, so updates show straight away */
const CACHE = "speach-v1";
const SHELL = ["/other/speach/", "/other/speach/manifest.webmanifest", "/other/speach/icon-192.png", "/other/speach/icon-512.png", "/other/speach/icon-maskable-512.png", "/other/speach/apple-touch-icon.png"];
self.addEventListener("install", (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(SHELL)).then(() => self.skipWaiting()));
});
self.addEventListener("activate", (e) => {
  e.waitUntil(caches.keys().then((ks) => Promise.all(ks.filter((k) => k.startsWith("speach-") && k !== CACHE).map((k) => caches.delete(k)))).then(() => self.clients.claim()));
});
self.addEventListener("fetch", (e) => {
  const req = e.request, u = new URL(req.url);
  if (req.method !== "GET") return;
  if (u.origin === location.origin) {
    if (!u.pathname.startsWith("/other/speach/")) return;
    const key = req.mode === "navigate" ? "/other/speach/" : req;
    e.respondWith(
      fetch(req).then((r) => {
        if (r.ok && r.type === "basic") { const c = r.clone(); caches.open(CACHE).then((x) => x.put(key, c)); }
        return r;
      }).catch(() => caches.match(key, { ignoreSearch: true }).then((r) => r || Response.error()))
    );
    return;
  }
  // Google Fonts: kept once loaded, refreshed in the background
  if (u.hostname === "fonts.googleapis.com" || u.hostname === "fonts.gstatic.com") {
    e.respondWith(caches.match(req).then((hit) => {
      const net = fetch(req).then((r) => {
        if (r.ok || r.type === "opaque") { const c = r.clone(); caches.open(CACHE).then((x) => x.put(req, c)); }
        return r;
      }).catch(() => hit || Response.error());
      return hit || net;
    }));
  }
});
