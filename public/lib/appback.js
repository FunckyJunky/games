// When error.blue runs as an installed app there is no browser back button.
// This adds a small "< ERROR-DOS" button to game pages that have no way back.
(function () {
  var standalone = (window.matchMedia && (matchMedia('(display-mode: standalone)').matches || matchMedia('(display-mode: fullscreen)').matches)) || navigator.standalone;
  if (!standalone) return;
  function add() {
    if (document.querySelector('.back, [data-errordos-back]')) return;
    var b = document.createElement('a');
    b.href = '/';
    b.setAttribute('data-errordos-back', '');
    b.textContent = '< ERROR-DOS';
    b.style.cssText = 'position:fixed;left:max(8px,env(safe-area-inset-left));bottom:max(8px,env(safe-area-inset-bottom));z-index:2147483647;' +
      'font:16px VT323,ui-monospace,monospace;color:#39ff7a;background:rgba(3,18,8,.75);border:1px solid #1b7a3c;' +
      'padding:4px 8px;text-decoration:none;opacity:.85;transition:opacity .6s;text-shadow:0 0 5px rgba(57,255,122,.6)';
    b.addEventListener('click', function () { try { sessionStorage.setItem('errordos-ran', document.title || 'GAME'); } catch (e) {} });
    document.body.appendChild(b);
    setTimeout(function () { b.style.opacity = '.3'; }, 4000);
    b.addEventListener('pointerenter', function () { b.style.opacity = '.9'; });
  }
  if (document.body) add(); else document.addEventListener('DOMContentLoaded', add);
})();
