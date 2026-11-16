// FULL SCREEN, the switch in ERROR-DOS setup, on every page of the site.
// The setting is "full" in localStorage "errordos-settings", shared by the
// menu and the programs it starts.
// - Browsers only allow full screen right after a tap or a key press, and they
//   leave it when a new page opens. So with FULL SCREEN on, each page goes full
//   screen at its first tap or key (not Esc).
// - Leaving full screen yourself (Esc, the back gesture, a page's own button)
//   turns the setting off, so it doesn't come back at the next tap.
// - Where a page can't go full screen (iPhone), nothing happens.
// window.ErrorFS: supported(), active(), enter(), exit(), get(), set(on);
// a page may set ErrorFS.hold = () => true to pause the first-tap entry, and
// ErrorFS.onchange to hear about changes.
(function () {
  if (window.ErrorFS) return;
  var KEY = 'errordos-settings', d = document, el = d.documentElement;
  var pending = false, done = false, gen = 0;
  function fsEl() { return d.fullscreenElement || d.webkitFullscreenElement || null; }
  function supported() {
    return !!(d.fullscreenEnabled || d.webkitFullscreenEnabled) && !!(el.requestFullscreen || el.webkitRequestFullscreen);
  }
  function read() { try { return JSON.parse(localStorage.getItem(KEY) || '{}') || {}; } catch (e) { return {}; } }
  function get() { return !!read().full; }
  function set(on) { try { var s = read(); s.full = !!on; localStorage.setItem(KEY, JSON.stringify(s)); } catch (e) {} }
  function enter() {
    if (!supported() || fsEl() || pending) return;
    var r = el.requestFullscreen || el.webkitRequestFullscreen;
    pending = true;
    try {
      var p = r.call(el, { navigationUI: 'hide' });
      if (p && p.then) p.then(function () { pending = false; done = true; }, function () { pending = false; });
      else { pending = false; done = true; }
    } catch (e) { pending = false; }
  }
  function exit() {
    var x = d.exitFullscreen || d.webkitExitFullscreen;
    if (!fsEl() || !x) return;
    try { var p = x.call(d); if (p && p.catch) p.catch(function () {}); } catch (e) {}
  }
  function gesture(e) {
    if (done || !get() || (FS.hold && FS.hold())) return;
    if (e && e.type === 'keydown' && (e.key === 'Escape' || e.key === 'Esc')) return;
    enter();
  }
  function changed() {
    if (FS.onchange) try { FS.onchange(); } catch (e) {}
    if (fsEl()) return;
    // out of full screen: if the page is still here a moment later, it was you
    var g = gen;
    setTimeout(function () {
      if (g !== gen || fsEl() || d.visibilityState !== 'visible' || !get()) return;
      set(false);
      if (FS.onchange) try { FS.onchange(); } catch (e) {}
    }, 400);
  }
  addEventListener('pointerdown', gesture, true);
  addEventListener('keydown', gesture, true);
  d.addEventListener('fullscreenchange', changed);
  d.addEventListener('webkitfullscreenchange', changed);
  addEventListener('pagehide', function () { gen++; });
  // back on this page from the browser's history: the next tap may go full screen again
  addEventListener('pageshow', function (e) { if (e.persisted) { gen++; done = !!fsEl(); } });
  var FS = window.ErrorFS = {
    supported: supported, active: function () { return !!fsEl(); },
    enter: enter, exit: exit, get: get, set: set, hold: null, onchange: null,
  };
})();
