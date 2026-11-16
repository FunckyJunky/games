// error.blue pages behave like an installed app: no zooming in by double-tapping
// or pinching. (Taps and fast tapping in games keep working.)
(() => {
  // a page without a viewport tag gets one
  if (!document.querySelector('meta[name="viewport"]')) {
    const m = document.createElement('meta'); m.name = 'viewport';
    m.content = 'width=device-width, initial-scale=1, maximum-scale=1, minimum-scale=1, viewport-fit=cover, user-scalable=no';
    document.head.appendChild(m);
  }
  for (const ev of ['gesturestart', 'gesturechange', 'gestureend']) document.addEventListener(ev, e => e.preventDefault(), { passive: false });
  document.addEventListener('touchmove', e => { if (e.touches.length > 1 && e.cancelable) e.preventDefault(); }, { passive: false });
  document.addEventListener('dblclick', e => e.preventDefault(), { passive: false });
})();
