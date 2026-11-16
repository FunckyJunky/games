// ERROR-DOS service worker: lets error.blue work as an installed app
// and open the menu (and games you've played) without a connection.
const CACHE = 'errordos-v4';
const SHELL = ['/', '/games.json', '/lib/gamelab.js', '/lib/appback.js', '/404.html', '/manifest.webmanifest', '/icons/icon-192.png'];

self.addEventListener('install', (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(SHELL)).then(() => self.skipWaiting()));
});

self.addEventListener('activate', (e) => {
  e.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))))
      .then(() => self.clients.claim())
  );
});

function keep(req, res) {
  if (res && (res.ok || res.type === 'opaque')) {
    const copy = res.clone();
    caches.open(CACHE).then((c) => c.put(req, copy));
  }
  return res;
}

self.addEventListener('fetch', (e) => {
  const req = e.request;
  if (req.method !== 'GET' || req.headers.has('range')) return;          // music streams go straight through
  const url = new URL(req.url);
  const same = url.origin === self.location.origin;
  if (same && url.pathname.startsWith('/api/')) return;                   // scores, chat, rooms: always live

  // pages, game code and lists: always fresh from the network (so an update shows
  // at once), the cached copy only when offline
  if (req.mode === 'navigate' || (same && /\.(json|js|css|html|webmanifest)$/.test(url.pathname))) {
    e.respondWith(
      fetch(req).then((res) => keep(req, res)).catch(() =>
        caches.match(req).then((hit) => hit || (req.mode === 'navigate' ? caches.match('/') : Response.error()))
      )
    );
    return;
  }

  // big files (songs, the commentary) are not kept twice
  if (same && /\.(mp3|m4a)$/.test(url.pathname)) return;
  // pictures and fonts: cached, refreshed in the background
  if (same || url.hostname.endsWith('gstatic.com') || url.hostname.endsWith('googleapis.com')) {
    e.respondWith(
      caches.match(req).then((hit) => {
        const net = fetch(req).then((res) => keep(req, res)).catch(() => hit);
        return hit || net;
      })
    );
  }
});

// a notification from error.blue (for now: "someone is in TUSA")
self.addEventListener('push', (e) => {
  let d = {};
  try { d = e.data ? e.data.json() : {}; } catch (err) { d = { body: e.data ? e.data.text() : '' }; }
  e.waitUntil(self.registration.showNotification(d.title || 'error.blue', {
    body: d.body || '',
    tag: d.tag || 'errorblue',
    renotify: true,
    icon: '/tusa/icons/tusa-192.png',
    badge: '/tusa/icons/tusa-192.png',
    data: { url: d.url || '/' },
  }));
});
self.addEventListener('notificationclick', (e) => {
  e.notification.close();
  const url = (e.notification.data && e.notification.data.url) || '/';
  e.waitUntil(self.clients.matchAll({ type: 'window', includeUncontrolled: true }).then((list) => {
    for (const c of list) if (new URL(c.url).pathname.startsWith(url) && 'focus' in c) return c.focus();
    return self.clients.openWindow(url);
  }));
});
