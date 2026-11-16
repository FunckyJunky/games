/*
  ERROR-DOS on-screen keyboard (the same keyboard as ERROR-CHAT).

    const kb = ErrorKbd.mount(containerElement, {
      onKey: key => {},            // a character, 'DEL', 'SEND', or a function key name
      fnKeys: ['EXIT', 'LINK'],    // optional top row
      sendLabel: 'ENTER',
      sound: true,
      ua: true, lang: 'ua',        // optional: a Ukrainian (ЙЦУКЕН) layout and a УКР/EN key
    });
    kb.setShift(true); kb.setSendLabel('GO'); kb.flash('a'); kb.setLang('abc');
*/
(function () {
  const CSS = `
  .ekb { display: flex; flex-direction: column; gap: clamp(4px, .9vh, 7px); touch-action: none; user-select: none; -webkit-user-select: none; }
  .ekb .krow { display: flex; gap: clamp(3px, 1vw, 7px); }
  .ekb .k {
    flex: 1 1 0; min-width: 0; position: relative; height: clamp(34px, 6.1dvh, 52px);
    display: grid; place-items: center; font-family: 'VT323', ui-monospace, monospace;
    font-size: clamp(19px, 5.2vw, 28px); line-height: 1;
    color: var(--fg, #39ff7a); border: 2px solid var(--dim, #1b7a3c); border-bottom-width: 4px; border-radius: 4px;
    background: linear-gradient(to bottom, rgba(255,255,255,.04), rgba(0,0,0,.25));
    box-shadow: inset 0 0 8px rgba(0,0,0,.6); cursor: pointer; text-shadow: 0 0 5px var(--glow, rgba(57,255,122,.55));
  }
  .ekb .k.sm { font-size: clamp(14px, 3.6vw, 20px); letter-spacing: .04em; }
  .ekb .k.fn { height: clamp(26px, 4.4dvh, 38px); border-bottom-width: 2px; border-style: dashed; font-size: clamp(14px, 3.6vw, 20px); }
  .ekb .k.lang { font-size: clamp(13px, 3.4vw, 18px); }
  .ekb .k.w15 { flex-grow: 1.5; } .ekb .k.w2 { flex-grow: 2.2; } .ekb .k.w5 { flex-grow: 5.5; }
  .ekb .k.send { background: var(--fg, #39ff7a); color: var(--bg, #031208); text-shadow: none; border-color: var(--fg, #39ff7a); box-shadow: 0 0 10px var(--glow, rgba(57,255,122,.55)); }
  .ekb .k.down { background: var(--fg, #39ff7a); color: var(--bg, #031208); text-shadow: none; border-color: var(--fg, #39ff7a); transform: translateY(2px); border-bottom-width: 2px; }
  .ekb .k .led { position: absolute; top: 4px; right: 5px; width: 6px; height: 6px; border-radius: 50%; border: 1px solid var(--dim, #1b7a3c); }
  .ekb .k .led.on { background: var(--fg, #39ff7a); box-shadow: 0 0 6px var(--fg, #39ff7a); border-color: var(--fg, #39ff7a); }
  .ekb .k .led.lock { box-shadow: 0 0 6px var(--fg, #39ff7a), 0 0 0 2px var(--bg, #031208), 0 0 0 3px var(--fg, #39ff7a); }
  `;
  let styled = false, ac = null;
  function click() {
    try {
      if (!ac) ac = new (window.AudioContext || window.webkitAudioContext)();
      if (ac.state === 'suspended') ac.resume();
      const t = ac.currentTime, n = ac.createBufferSource(), b = ac.createBuffer(1, 800, ac.sampleRate), d = b.getChannelData(0);
      for (let i = 0; i < d.length; i++) d[i] = (Math.random() * 2 - 1) * (1 - i / d.length);
      n.buffer = b;
      const f = ac.createBiquadFilter(); f.type = 'bandpass'; f.frequency.value = 3600; f.Q.value = 0.8;
      const g = ac.createGain(); g.gain.value = 0.2;
      n.connect(f).connect(g).connect(ac.destination); n.start(t);
    } catch (e) {}
  }

  function mount(el, opts) {
    opts = opts || {};
    if (!styled) { const s = document.createElement('style'); s.textContent = CSS; document.head.appendChild(s); styled = true; }
    el.classList.add('ekb');
    const fn = (opts.fnKeys || []).map(k => [k, 'fn']);
    const LAYOUT = {
      abc: [fn, '1234567890'.split(''), 'qwertyuiop'.split(''), 'asdfghjkl?'.split(''),
        [['SHIFT', 'w2 sm shift'], 'z', 'x', 'c', 'v', 'b', 'n', 'm', ['DEL', 'w2 sm del']],
        [['SYM', 'w15 sm sym'], ',', ['SPACE', 'w5 sm space'], '.', ['SEND', 'w2 sm send']]],
      ua: [fn, '1234567890'.split(''), 'йцукенгшщзхї'.split(''), "фівапролджє'".split(''),
        [['SHIFT', 'w15 sm shift'], 'я', 'ч', 'с', 'м', 'и', 'т', 'ь', 'б', 'ю', 'ґ', ['DEL', 'w15 sm del']],
        [['SYM', 'w15 sm sym'], ['EN', 'w15 sm lang'], ',', ['SPACE', 'w5 sm space'], '.', '?', ['SEND', 'w2 sm send']]],
      sym: [fn, '!@#$%&*()='.split(''), '-_+/\\:;"\'~'.split(''), '<>[]{}^|`?'.split(''),
        [['ABC', 'w15 sm abc'], [':)', 'sm'], [':(', 'sm'], [';)', 'sm'], [':D', 'sm'], ['<3', 'sm'], [':P', 'sm'], ['DEL', 'w2 sm del']],
        [['ABC', 'w15 sm abc'], ',', ['SPACE', 'w5 sm space'], '.', ['SEND', 'w2 sm send']]],
    };
    const isLetter = k => k.length === 1 && k.toLowerCase() !== k.toUpperCase();
    // with the Ukrainian layout, the letter row of the Latin layout gets a УКР key
    if (opts.ua) LAYOUT.abc[5] = [['SYM', 'w15 sm sym'], ['УКР', 'w15 sm lang'], ',', ['SPACE', 'w5 sm space'], '.', ['SEND', 'w2 sm send']];
    let letters = opts.ua && opts.lang !== 'abc' ? 'ua' : 'abc';
    let layer = letters, shift = false, caps = false, lastShift = 0, sendLabel = opts.sendLabel || 'ENTER', repeatT = null;
    function build() {
      el.innerHTML = '';
      for (const row of LAYOUT[layer]) {
        if (!row.length) continue;
        const r = document.createElement('div'); r.className = 'krow';
        for (const k of row) {
          const [label, cl] = Array.isArray(k) ? k : [k, ''];
          const b = document.createElement('div');
          b.className = 'k ' + cl; b.dataset.k = label;
          if (cl.includes('shift')) b.innerHTML = 'SHIFT<span class="led"></span>';
          else if (cl.includes('send')) b.textContent = sendLabel;
          else b.textContent = label;
          r.appendChild(b);
        }
        el.appendChild(r);
      }
      refresh();
    }
    function refresh() {
      const up = shift || caps;
      el.querySelectorAll('.k').forEach(b => {
        const k = b.dataset.k;
        if (isLetter(k)) b.textContent = up ? k.toUpperCase() : k.toLowerCase();
      });
      const led = el.querySelector('.shift .led');
      if (led) { led.classList.toggle('on', up); led.classList.toggle('lock', caps); }
    }
    function press(k) {
      if (opts.sound !== false) click();
      if (k === 'SHIFT') {
        const now = performance.now();
        if (caps) { caps = false; shift = false; } else if (shift && now - lastShift < 400) caps = true; else shift = !shift;
        lastShift = now; refresh(); return;
      }
      if (k === 'SYM') { layer = 'sym'; build(); return; }
      if (k === 'ABC') { layer = letters; build(); return; }
      if (k === 'EN' || k === 'УКР') { letters = layer = k === 'EN' ? 'abc' : 'ua'; build(); opts.onLang && opts.onLang(letters); return; }
      if (k === 'SPACE') { opts.onKey && opts.onKey(' '); return; }
      const letter = isLetter(k);
      if (letter) {
        opts.onKey && opts.onKey((shift || caps) ? k.toUpperCase() : k.toLowerCase());
        if (shift && !caps) { shift = false; refresh(); }
        return;
      }
      opts.onKey && opts.onKey(k);
    }
    el.addEventListener('pointerdown', e => {
      const b = e.target.closest('.k'); if (!b) return;
      e.preventDefault();
      b.classList.add('down');
      press(b.dataset.k);
      if (b.dataset.k === 'DEL') {
        clearTimeout(repeatT);
        const rep = () => { opts.onKey && opts.onKey('DEL'); repeatT = setTimeout(rep, 60); };
        repeatT = setTimeout(rep, 420);
      }
    });
    const up = () => { el.querySelectorAll('.k.down').forEach(x => x.classList.remove('down')); clearTimeout(repeatT); };
    el.addEventListener('pointerup', up); el.addEventListener('pointercancel', up); el.addEventListener('pointerleave', up);
    build();
    return {
      el,
      setShift(on) { if (!caps) shift = !!on; refresh(); },
      setSendLabel(s) { sendLabel = s; const b = el.querySelector('.send'); if (b) b.textContent = s; },
      setLang(l) { if (!opts.ua) return; letters = l === 'abc' ? 'abc' : 'ua'; if (layer !== 'sym') layer = letters; build(); },
      get lang() { return letters; },
      flash(k) {
        const b = Array.from(el.querySelectorAll('.k')).find(x => x.dataset.k.toLowerCase() === String(k).toLowerCase());
        if (b) { b.classList.add('down'); setTimeout(() => b.classList.remove('down'), 90); }
      },
    };
  }
  window.ErrorKbd = { mount };
})();
