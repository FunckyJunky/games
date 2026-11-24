/* Funda EYE (error.blue/other/funda, formerly FUNDA WATCH) as an installed web app.
   - opens without a connection: the page, icons and the last readings are kept
   - always tries the network first, so new readings and page updates show straight away
   - shows the notifications the server sends (price, status, saves, every N views) */
const CACHE = "funda-v9";
const SHELL = ["/other/funda/", "/other/funda/buren.js", "/other/funda/manifest.webmanifest", "/other/funda/icon-192.png", "/other/funda/apple-touch-icon.png", "/other/funda/badge-96.png"];
self.addEventListener("install", (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(SHELL)).then(() => self.skipWaiting()));
});
self.addEventListener("activate", (e) => {
  e.waitUntil(caches.keys().then((ks) => Promise.all(ks.filter((k) => k.startsWith("funda-") && k !== CACHE).map((k) => caches.delete(k)))).then(() => self.clients.claim()));
});
self.addEventListener("fetch", (e) => {
  const u = new URL(e.request.url);
  if (e.request.method !== "GET" || u.origin !== location.origin) return;
  const page = u.pathname.startsWith("/other/funda/");
  const data = /^\/api\/funda\/\d+(\/buren)?$/.test(u.pathname); // readings + neighbours: kept for offline, never preferred
  if (!page && !data) return;
  const key = e.request.mode === "navigate" ? "/other/funda/" : data ? u.pathname : e.request;
  e.respondWith(
    fetch(e.request).then((r) => {
      if (r.ok && r.type === "basic") { const c = r.clone(); caches.open(CACHE).then((x) => x.put(key, c)); }
      return r;
    }).catch(() => caches.match(key, { ignoreSearch: !data }).then((r) => r || Response.error()))
  );
});
self.addEventListener("push", (e) => {
  let d = {};
  try { d = e.data ? e.data.json() : {}; } catch (err) { d = { body: e.data ? e.data.text() : "" }; }
  e.waitUntil(self.registration.showNotification(d.title || "Funda EYE", {
    body: d.body || "",
    tag: d.tag || "funda",
    renotify: true,
    icon: d.icon || "/other/funda/icon-192.png",
    badge: "/other/funda/badge-96.png",
    data: { url: d.url || "/other/funda/" },
  }));
});
self.addEventListener("notificationclick", (e) => {
  e.notification.close();
  const url = (e.notification.data && e.notification.data.url) || "/other/funda/";
  e.waitUntil(self.clients.matchAll({ type: "window", includeUncontrolled: true }).then((list) => {
    for (const c of list) if (new URL(c.url).pathname.startsWith("/other/funda/") && "focus" in c) return c.navigate ? c.navigate(url).then((w) => (w || c).focus()) : c.focus();
    return self.clients.openWindow(url);
  }));
});
