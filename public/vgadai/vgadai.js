// Вгадай що?! — a TV quiz show on error.blue.
// Every game is an episode: season + episode make the 4-digit room code
// (S03 E12 = 0312). One player asks a question; the others type one word or
// one number and race to press the green button; the one who asked says
// yes or no. Vika, Vito and Oleksiy are the friends from TUSA; anyone else gets
// a random look. The studio is drawn in 3D (floor, podiums, lights, curtains)
// with the characters standing in it, filmed by a slightly shaky handheld
// camera. The show host reads questions in Cyrillic out loud in Ukrainian.
'use strict';
(() => {
const A = ART;
const $ = id => document.getElementById(id);
const esc = s => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const lerp = (a, b, k) => a + (b - a) * k;
const ease = k => k < 0 ? 0 : k > 1 ? 1 : 1 - Math.pow(1 - k, 3);

// =====================================================================
// who is who: Vika, Vito and Oleksiy look like in TUSA; everybody else gets
// a random look (its seed is kept on the server, so it's the same everywhere)
// =====================================================================
const NAMES = {
  VITO: ['VITO'],
  VIKA: ['VIKA', 'VICA', 'VICTORIA', 'VIKTORIA', 'VIKTORIIA', 'VOCTORIA', 'VIKTORIJA', 'VIKUSYA'],
  OLEKSIY: ['OLEKSIY', 'OLEKSII', 'OLEKSIJ', 'OLEKSI', 'ALEKSEY', 'OLEXIY'],
};
const HOONGY = { VIKA: 1, VITO: 1, OLEKSIY: 1 };
const TAG = { VITO: '#7fd3ff', VIKA: '#ff9ec0', OLEKSIY: '#b8e07a' };
function canon(typed) {
  const n = String(typed).toUpperCase().replace(/[^A-Z0-9]/g, '');
  for (const k in NAMES) if (NAMES[k].includes(n)) return k;
  return n;
}
const SKINS = [['#fbdcc3', '#eebc9a'], ['#f3c9a3', '#d9a983'], ['#efc29e', '#d9a179'], ['#e0ac84', '#c48a62'], ['#c68a5e', '#a86e46'], ['#a06a44', '#845232'], ['#7a4a2c', '#603820']];
const HAIRS = [['#1b1b1f', '#33333a'], ['#3a2a1e', '#5a4232'], ['#6a4428', '#4a2e18'], ['#e8c886', '#b8915a'], ['#c8582a', '#a03e1a'], ['#e8e4dc', '#b8b4ac'], ['#ff7ab8', '#d04a8a'], ['#5a8aff', '#3a5ad0'], ['#8a5ac8', '#6a3aa0'], ['#3ab87a', '#2a8a5a']];
const TOPS = [['#e8443a', '#b8302a'], ['#3a7ad8', '#2a5aa8'], ['#f2b42a', '#c88a1a'], ['#3aa86a', '#2a7a4a'], ['#8a4ac8', '#6a32a0'], ['#ff7aa8', '#d0507e'], ['#2ab8c8', '#1a8a98'], ['#f4f2ea', '#d4d0c4'], ['#33363c', '#22242a'], ['#ff9a4a', '#d0702a']];
const LEGS = ['#2b3247', '#3a3f5a', '#2f2f33', '#4a3a2a', '#2a4a6a', '#5a5a62'];
const SHOES = ['#1d1d22', '#4a3020', '#f4f2ea', '#c83a2a'];
const EYES = ['#6a4a35', '#3a6a9a', '#4a7a3a', '#2b1d17', '#8a6a2a'];
const HATS = ['#e8443a', '#3a7ad8', '#f2b42a', '#2a2a30', '#3aa86a', '#ff7aa8', '#8a4ac8'];
const FELT = [['#56462e', '#3d3121'], ['#2a2a30', '#18181c'], ['#8a3a2a', '#6a2a1a'], ['#c8b08a', '#a08a68']];
const BEARDS = [['#8a5a32', '#6a4426'], ['#2a2220', '#1a1412'], ['#d0a060', '#a07840'], ['#b8b4ac', '#8a8680'], ['#c8582a', '#a03e1a']];
const JACKETS = [['#ddd6c8', '#bdb6a8'], ['#c8a06a', '#a07e4e'], ['#2a2a30', '#18181c'], ['#6a8aa8', '#4a6a88']];
const BRIGHT = ['#ffd34a', '#c39bff', '#ff9a6a', '#6ae0d0', '#ff7aa8', '#9ad0ff', '#e0e070', '#ffb0e0'];
function rng(seed) {
  let a = (seed >>> 0) || 1;
  return () => { a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; };
}
function makeLook(seed) {
  const r = rng(seed), pick = a => a[r() * a.length | 0];
  const [skin, skin2] = pick(SKINS), [hair, hair2] = pick(HAIRS), [top, top2] = pick(TOPS);
  const style = pick(['short', 'short', 'long', 'long', 'bun', 'cap', 'beanie', 'beret', 'olek']);
  const L = { skin, skin2, hair, hair2, top, top2, legs: pick(LEGS), shoe: pick(SHOES), style, eye: pick(EYES), tag: pick(BRIGHT) };
  if (r() < 0.25) L.plump = true;
  const v = r();
  if (!L.plump && v < 0.3) { L.hood = true; L.tee = pick(['#f4f2ea', '#ffe9a0', '#cfe8ff']); }
  else if (!L.plump && v < 0.5) { L.coat = true; L.shirt = '#f4f2ea'; }
  else if (L.plump && v < 0.6) { const [j, j2] = pick(JACKETS); L.jacket = j; L.jacket2 = j2; L.sleeve = j; L.sleeve2 = j2; }
  if (style === 'cap' || style === 'beanie' || style === 'beret') L.hatc = pick(HATS);
  if (style === 'olek') { const [h, h2] = pick(FELT), [b, b2] = pick(BEARDS); Object.assign(L, { hat: h, hat2: h2, beard: b, beard2: b2, must: b2, shirt: '#f4f2ea' }); }
  return L;
}
function nameSeed(n) { let h = 2166136261; for (const ch of n) h = Math.imul(h ^ ch.charCodeAt(0), 16777619); return h >>> 1; }
const SEEDS = {};   // name -> look seed
function keyOf(name, seed) {
  if (HOONGY[name]) return name;
  const sd = seed || (S.s && S.s.looks && S.s.looks[name]) || SEEDS[name] || nameSeed(name);
  const k = 'P:' + name + ':' + sd;
  if (!A.LOOK[k]) A.LOOK[k] = makeLook(sd);
  return k;
}
const tagOf = (name, seed) => TAG[name] || A.LOOK[keyOf(name, seed)].tag;
// the show host: a gold jacket, a bow tie, a microphone
A.LOOK.__HOST = { skin: '#f3c9a3', skin2: '#d9a983', hair: '#2a1a12', hair2: '#5a3a22', top: '#e0ac2a', top2: '#b07e12', legs: '#1f1f24', shoe: '#101014', style: 'short', coat: true, shirt: '#f4f2ea', eye: '#3a6a9a' };

// =====================================================================
// settings
// =====================================================================
let SET = { sound: true };
try { Object.assign(SET, JSON.parse(localStorage.getItem('errordos-settings') || '{}')); } catch (e) {}
const PREF = { voice: true, push: false, lang: 'ua' };
try { Object.assign(PREF, JSON.parse(localStorage.getItem('vgadai-prefs') || '{}')); } catch (e) {}
const savePref = () => { try { localStorage.setItem('vgadai-prefs', JSON.stringify(PREF)); } catch (e) {} };
function setSound(on) {
  SET.sound = on;
  try { const o = JSON.parse(localStorage.getItem('errordos-settings') || '{}'); o.sound = on; localStorage.setItem('errordos-settings', JSON.stringify(o)); } catch (e) {}
}

// =====================================================================
// sound: a little TV studio, all synthesized
// =====================================================================
let ac = null, master = null;
function audio() {
  if (!SET.sound) return null;
  if (!ac) {
    try {
      ac = new (window.AudioContext || window.webkitAudioContext)();
      master = ac.createGain(); master.gain.value = 0.8;
      const lp = ac.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.value = 7000;
      master.connect(lp).connect(ac.destination);
    } catch (e) { return null; }
  }
  if (ac.state === 'suspended') ac.resume();
  return ac;
}
function tone(f, d, v, type, when, f2) {
  const a = audio(); if (!a) return;
  const t = a.currentTime + (when || 0), o = a.createOscillator(), g = a.createGain();
  o.type = type || 'square'; o.frequency.setValueAtTime(f, t); if (f2) o.frequency.exponentialRampToValueAtTime(f2, t + d);
  g.gain.setValueAtTime(0.0001, t); g.gain.exponentialRampToValueAtTime(v, t + 0.008); g.gain.exponentialRampToValueAtTime(0.0001, t + d);
  o.connect(g).connect(master); o.start(t); o.stop(t + d + 0.03);
}
let nbuf = null;
function noise(d, v, f, q, when, f2) {
  const a = audio(); if (!a) return;
  if (!nbuf) { nbuf = a.createBuffer(1, a.sampleRate, a.sampleRate); const x = nbuf.getChannelData(0); for (let i = 0; i < x.length; i++) x[i] = Math.random() * 2 - 1; }
  const t = a.currentTime + (when || 0), s = a.createBufferSource(), bf = a.createBiquadFilter(), g = a.createGain();
  s.buffer = nbuf; s.loop = true; bf.type = 'bandpass'; bf.frequency.setValueAtTime(f, t); if (f2) bf.frequency.exponentialRampToValueAtTime(f2, t + d); bf.Q.value = q;
  g.gain.setValueAtTime(v, t); g.gain.exponentialRampToValueAtTime(0.0001, t + d);
  s.connect(bf).connect(g).connect(master); s.start(t, Math.random() * 0.5); s.stop(t + d + 0.03);
}
const SFX = {
  click() { noise(0.02, 0.18, 3600, 0.8); },
  key() { tone(1300 + Math.random() * 400, 0.025, 0.012, 'square'); },
  nope() { tone(180, 0.08, 0.04, 'square'); },
  buzz() { tone(98, 0.55, 0.12, 'sawtooth'); tone(104, 0.55, 0.09, 'square'); tone(196, 0.55, 0.04, 'sawtooth'); },
  ding() { [1046.5, 1318.5, 1568, 2093].forEach((f, i) => tone(f, 0.4, 0.06, 'triangle', i * 0.1)); },
  fanfare(w) {
    w = w || 0;
    [[523, 0], [659, 0.12], [784, 0.24], [1046, 0.36], [784, 0.6], [1046, 0.72]].forEach(([f, t]) => { tone(f, 0.22, 0.045, 'sawtooth', w + t); tone(f * 1.006, 0.22, 0.03, 'square', w + t); });
    tone(1046, 1, 0.05, 'sawtooth', w + 0.84); tone(1318, 1, 0.035, 'sawtooth', w + 0.84); tone(1568, 1, 0.035, 'triangle', w + 0.84);
  },
  wrong() { tone(233, 0.3, 0.09, 'sawtooth', 0, 220); tone(175, 0.75, 0.11, 'sawtooth', 0.32, 120); },
  drum(d) { const n = Math.round((d || 1.4) / 0.045); for (let i = 0; i < n; i++) noise(0.04, 0.03 + i / n * 0.07, 650 + (i % 2) * 220, 0.8, i * 0.045); },
  cymbal(w) { noise(1.3, 0.11, 8000, 0.4, w || 0); },
  applause(d) { d = d || 2.4; for (let i = 0; i < 100; i++) noise(0.02 + Math.random() * 0.04, 0.025 + Math.random() * 0.05, 1800 + Math.random() * 3500, 1.5, Math.random() * d); },
  whoosh() { noise(0.45, 0.14, 300, 1.2, 0, 4000); },
  pop() { tone(660, 0.07, 0.05, 'square', 0, 990); },
  boing() { tone(220, 0.35, 0.06, 'sine', 0, 660); },
  tick() { tone(2200, 0.03, 0.02, 'square'); },
  sad() { [[392, 0], [370, 0.35], [349, 0.7]].forEach(([f, w]) => tone(f, 0.32, 0.06, 'sawtooth', w, f * 0.97)); tone(330, 1.1, 0.07, 'sawtooth', 1.05, 290); },
  join() { tone(660, 0.12, 0.05, 'triangle'); tone(990, 0.2, 0.05, 'triangle', 0.1); },
  sting() { tone(392, 0.12, 0.05, 'square'); tone(523, 0.12, 0.05, 'square', 0.1); tone(784, 0.35, 0.05, 'square', 0.2); noise(0.3, 0.06, 6000, 0.6, 0.2); },
};

// =====================================================================
// the show host's voice: a question in Cyrillic is read out loud in Ukrainian
// =====================================================================
const VOICE = { talking: false, warned: false, unlocked: false };
function ukVoice() {
  try { return speechSynthesis.getVoices().find(v => /^uk([-_]|$)/i.test(v.lang)) || null; } catch (e) { return null; }
}
try { if ('speechSynthesis' in window) speechSynthesis.onvoiceschanged = () => {}; } catch (e) {}
function speakQ(text) {
  if (!PREF.voice || !('speechSynthesis' in window) || !/[\u0400-\u04FF]/.test(text)) return;
  const voices = speechSynthesis.getVoices(), v = ukVoice();
  if (voices.length && !v) {
    if (!VOICE.warned) { VOICE.warned = true; toast('🗣 На цьому пристрої немає українського голосу'); }
    return;
  }
  try {
    speechSynthesis.cancel();
    const u = new SpeechSynthesisUtterance(text);
    u.lang = 'uk-UA'; if (v) u.voice = v; u.rate = 1; u.pitch = 1.05;
    u.onstart = () => { VOICE.talking = true; };
    u.onend = u.onerror = () => { VOICE.talking = false; };
    VOICE.talking = true;
    setTimeout(() => speechSynthesis.speak(u), 650);
    setTimeout(() => { if (!speechSynthesis.speaking) VOICE.talking = false; }, 4000);
  } catch (e) { VOICE.talking = false; }
}
// a phone only speaks after a tap: say nothing once, on the first tap
function unlockVoice() {
  if (VOICE.unlocked || !('speechSynthesis' in window)) return;
  VOICE.unlocked = true;
  try { const u = new SpeechSynthesisUtterance(' '); u.volume = 0; speechSynthesis.speak(u); } catch (e) {}
}
addEventListener('pointerdown', () => { unlockVoice(); audio(); }, { capture: true });

// =====================================================================
// screens, toasts, dialogs
// =====================================================================
function show(id) {
  for (const s of ['login', 'lobby', 'app']) $(s).classList.toggle('show', s === id);
  SCREEN = id; document.body.dataset.screen = id;
}
let SCREEN = 'login';
function toast(msg) {
  const d = document.createElement('div'); d.textContent = msg; $('toast').appendChild(d);
  setTimeout(() => d.remove(), 2900);
}
function dialog(html, acts) {
  const d = $('dlg'); d.innerHTML = html; d.classList.add('show');
  MENU = false;
  dlgSel(0);
  d.onclick = e => {
    const b = e.target.closest('[data-a]'); if (!b) return;
    SFX.click();
    const f = acts && acts[b.dataset.a];
    d.classList.remove('show'); MENU = false;
    if (f && f() === 'keep') d.classList.add('show');
  };
  // press and hold (1.6 s) to confirm something that can't be undone
  d.querySelectorAll('[data-hold]').forEach(b => {
    let t0 = 0, raf = 0;
    const stop = () => { t0 = 0; cancelAnimationFrame(raf); b.style.setProperty('--p', '0%'); };
    const step = () => {
      if (!t0) return;
      const k = Math.min(1, (performance.now() - t0) / 1600);
      b.style.setProperty('--p', (k * 100).toFixed(1) + '%');
      if (k < 1) { if ((k * 20 | 0) !== ((k - 0.02) * 20 | 0)) SFX.tick(); raf = requestAnimationFrame(step); return; }
      stop(); SFX.wrong(); d.classList.remove('show'); MENU = false;
      const f = acts && acts[b.dataset.hold]; if (f) f();
    };
    b.addEventListener('pointerdown', e => { e.preventDefault(); t0 = performance.now(); raf = requestAnimationFrame(step); });
    for (const ev of ['pointerup', 'pointerleave', 'pointercancel']) b.addEventListener(ev, stop);
  });
}
const dlgOpen = () => $('dlg').classList.contains('show');
const closeDialog = () => { $('dlg').classList.remove('show'); MENU = false; };
// on a computer: arrow keys choose in a dialog, Enter presses
function dlgSel(i) {
  const list = [...$('dlg').querySelectorAll('[data-a]')];
  if (!list.length) return;
  i = (i + list.length) % list.length;
  list.forEach((b, k) => b.classList.toggle('sel', k === i));
}
function dlgKey(e) {
  const list = [...$('dlg').querySelectorAll('[data-a]')], at = list.findIndex(b => b.classList.contains('sel'));
  if (e.key === 'ArrowDown' || e.key === 'ArrowRight' || e.key === 'Tab') { e.preventDefault(); dlgSel(at + 1); SFX.tick(); }
  else if (e.key === 'ArrowUp' || e.key === 'ArrowLeft') { e.preventDefault(); dlgSel(at - 1); SFX.tick(); }
  else if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); if (list[at]) list[at].click(); }
}

// =====================================================================
// full screen on a computer: F switches it (when you aren't typing), and so
// does the small ⛶ F button. Esc opens the menu.
// =====================================================================
const DESK = matchMedia('(pointer: fine)').matches && !/iPad|iPhone|iPod|Android/i.test(navigator.userAgent)
  && !(navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1) && !matchMedia('(display-mode: fullscreen)').matches;
const fsEl = () => document.fullscreenElement || document.webkitFullscreenElement;
const fsOk = () => DESK && !!(document.fullscreenEnabled || document.webkitFullscreenEnabled);
let MENU = false;
function goFull() {
  if (fsEl() || !fsOk()) return;
  const el = document.documentElement, r = el.requestFullscreen || el.webkitRequestFullscreen;
  try { const p = r.call(el, { navigationUI: 'hide' }); if (p && p.then) p.then(lockEsc, () => {}); else lockEsc(); } catch (e) {}
}
// in full screen, Esc comes to the game (Chrome, Edge): it opens the menu; holding Esc still leaves
function lockEsc() { try { if (navigator.keyboard && navigator.keyboard.lock) navigator.keyboard.lock(['Escape']).catch(() => {}); } catch (e) {} }
function leaveFull() {
  try { if (navigator.keyboard && navigator.keyboard.unlock) navigator.keyboard.unlock(); } catch (e) {}
  if (fsEl()) try { (document.exitFullscreen || document.webkitExitFullscreen).call(document); } catch (e) {}
}
function toggleFull() { SFX.click(); if (fsEl()) leaveFull(); else goFull(); }
// typing a name, a question or an answer: then F is just the letter F
const typingNow = () => SCREEN === 'login' || (SCREEN === 'app' && !!S.field);
function fsShow() {
  const on = !!fsEl();
  for (const id of ['fsBtn', 'fsFix']) {
    const b = $(id); if (!b) continue;
    b.style.display = fsOk() ? '' : 'none';
    b.textContent = (on ? '🗗' : '⛶') + ' F';
    b.title = on ? 'Вийти з повного екрана (F)' : 'Повний екран (F)';
  }
}
for (const id of ['fsBtn', 'fsFix']) $(id).addEventListener('click', e => { e.stopPropagation(); toggleFull(); });
document.addEventListener('fullscreenchange', fsShow);
document.addEventListener('webkitfullscreenchange', fsShow);
fsShow();
function openMenu() {
  const inStudio = SCREEN === 'app', full = !!fsEl();
  const opt = (a, ico, label, sub) => '<button class="opt" data-a="' + a + '"><span class="ico">' + ico + '</span><span>' + label + (sub ? '<small>' + sub + '</small>' : '') + '</span></button>';
  dialog('<h3>⏸ Меню</h3>'
    + opt('go', '▶', 'Продовжити', 'Esc')
    + (fsOk() ? (full ? opt('window', '🗗', 'Вийти з повного екрана', 'клавіша F') : opt('full', '⛶', 'Повний екран', 'клавіша F')) : '')
    + opt('set', '⚙', 'Налаштування', 'сповіщення, голос ведучого, звук')
    + (inStudio ? opt('leave', '🚪', 'Вийти з епізоду', esc(epName(S.code)) + ' · бали залишаться') : '')
    + opt('dos', '◄', 'Terug naar ERROR-DOS', ''), {
    go: () => {},
    window: leaveFull,
    full: goFull,
    set: () => { setTimeout(() => openSettings(inStudio), 30); },
    leave: () => leaveEpisode(),
    dos: () => { try { sessionStorage.setItem('errordos-ran', 'VGADAI'); } catch (e) {} location.href = '/'; },
  });
  MENU = true;
  SFX.whoosh();
}
const epName = code => 'S' + code.slice(0, 2) + ' · E' + code.slice(2);
const epLong = code => 'СЕЗОН ' + (+code.slice(0, 2)) + ' · ЕПІЗОД ' + (+code.slice(2));

// =====================================================================
// state
// =====================================================================
const S = {
  me: '', seed: 0,
  code: '', s: null, off: 0,                  // the episode, its state, the server clock offset
  text: { q: '', a: '', h: '', g: '' }, field: '',
  sub: '', pend: 0,
  lastQ: '', lastTries: -1, lastScores: {}, histKey: '', leadKey: '',
  introT: -99, tab: 'lead',
  season: 1, ep: 1,
};
const now = () => Date.now() + S.off;

// =====================================================================
// login: the DOS screen with the ERROR keyboard (like TUSA)
// =====================================================================
const term = $('term');
let typed = '', loginLines = [], loginBusy = false;
function termRender() {
  term.innerHTML = loginLines.join('\n') + (loginBusy ? '' : '\nLOGIN: ' + esc(typed) + '<span class="cur"> </span>');
}
function loginKey(k) {
  if (SCREEN !== 'login' || loginBusy) return;
  audio();
  if (k === 'DEL') typed = typed.slice(0, -1);
  else if (k === 'SEND') return tryLogin();
  else if (k.length === 1 && /[a-z0-9]/i.test(k) && typed.length < 12) typed += k.toUpperCase();
  else return;
  SFX.click(); termRender();
}
const loginKb = ErrorKbd.mount($('loginKbd'), { onKey: loginKey, sendLabel: 'ENTER', sound: false });
function tryLogin() {
  const who = canon(typed);
  loginLines.push('LOGIN: ' + esc(typed));
  if (!/^[A-Z0-9]{2,10}$/.test(who)) {
    loginLines.push('<span class="err">Bad command or file name.</span>');
    loginLines.push('<span class="dim">Naam: 2 tot 10 letters of cijfers.</span>');
    typed = ''; SFX.nope(); termRender(); return;
  }
  try { localStorage.setItem('tusa-name', who); } catch (e) {}
  loginBusy = true;
  loginLines.push('');
  loginLines.push(HOONGY[who] ? 'Ласкаво просимо, ' + who + '! <span class="dim">(з Hoongy)</span>' : 'Нове обличчя в студії: ' + who + '!');
  loginLines.push('');
  termRender();
  let i = 0;
  const bar = () => {
    i++;
    loginLines[loginLines.length - 1] = 'LOADING VGADAI.EXE [' + '#'.repeat(i) + '.'.repeat(Math.max(0, 16 - i)) + ']';
    termRender();
    if (i < 16) setTimeout(bar, 45); else setTimeout(() => { S.me = who; SFX.sting(); enterLobby(); }, 250);
  };
  setTimeout(bar, 250);
}
function showLogin() {
  show('login');
  try { typed = localStorage.getItem('tusa-name') || ''; } catch (e) { typed = ''; }
  loginLines = [
    '<span class="dim">ERROR-DOS 1.0 · C:\\ONLINE\\VGADAI.EXE</span>',
    '',
    '<span class="big">Вгадай що?!</span>',
    '<span class="dim">(c) 2026 error.blue · телешоу · de quizshow</span>',
    '',
    'Один питає. Інші пишуть відповідь',
    'і тиснуть зелену кнопку.',
    'Wie ben jij? Typ je naam en druk op ENTER.',
    '',
  ];
  if (fsOk()) loginLines.splice(loginLines.length - 1, 0, '<span class="dim">⛶ F: volledig scherm · ESC: menu</span>');
  loginBusy = false;
  termRender();
}

// =====================================================================
// lobby: pick a season and an episode (= the room code)
// =====================================================================
const logoTxt = $('logoTxt');
logoTxt.innerHTML = [...'Вгадай що?!'].map((ch, i) => ch === ' ' ? ' ' : '<span style="color:' + ['#ffe23a', '#ff9ec0', '#7fd3ff', '#b8e07a', '#ffb35a'][i % 5] + ';animation-delay:' + (i * 0.09) + 's">' + esc(ch) + '</span>').join('');
try { const e = JSON.parse(localStorage.getItem('vgadai-ep') || 'null'); if (e) { S.season = e[0]; S.ep = e[1]; } else S.ep = 1 + (Math.random() * 99 | 0); } catch (e) {}
const codeOf = () => String(S.season).padStart(2, '0') + String(S.ep).padStart(2, '0');
function dialShow(flip) {
  const set = (id, v) => {
    const f = $(id).querySelectorAll('.flap'), t = String(v).padStart(2, '0');
    f.forEach((el, i) => {
      if (el.textContent !== t[i]) { el.textContent = t[i]; if (flip) { el.classList.remove('flip'); void el.offsetWidth; el.classList.add('flip'); } }
    });
  };
  set('dialS', S.season); set('dialE', S.ep);
  $('codeTxt').textContent = codeOf();
  try { localStorage.setItem('vgadai-ep', JSON.stringify([S.season, S.ep])); } catch (e) {}
  peekSoon();
}
function dialStep(which, d) {
  if (which === 'S') S.season = ((S.season - 1 + d + 99) % 99) + 1; else S.ep = ((S.ep - 1 + d + 99) % 99) + 1;
  SFX.tick(); dialShow(true); $('lobMsg').textContent = '';
}
for (const [id, w] of [['dialS', 'S'], ['dialE', 'E']]) {
  for (const [cls, d] of [['up', 1], ['dn', -1]]) {
    const b = $(id).querySelector('.' + cls);
    let rep = null;
    const stop = () => { clearTimeout(rep); rep = null; };
    b.addEventListener('pointerdown', e => {
      e.preventDefault(); dialStep(w, d);
      const again = (ms) => { rep = setTimeout(() => { dialStep(w, d); again(70); }, ms); };
      again(420);
    });
    b.addEventListener('pointerup', stop); b.addEventListener('pointerleave', stop); b.addEventListener('pointercancel', stop);
  }
}
$('rndBtn').onclick = () => { S.season = 1 + (Math.random() * 9 | 0); S.ep = 1 + (Math.random() * 99 | 0); SFX.boing(); dialShow(true); };
$('createBtn').onclick = () => { SFX.click(); goEpisode('create'); };
$('joinBtn').onclick = () => { SFX.click(); goEpisode('join'); };
$('lobDos').onclick = () => { try { sessionStorage.setItem('errordos-ran', 'VGADAI'); } catch (e) {} location.href = '/'; };
$('lobSet').onclick = () => openSettings(false);
$('reroll').onclick = async () => {
  SFX.boing();
  try {
    const r = await (await fetch('/api/vgadai/look', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ name: S.me, new: true }) })).json();
    if (r.seed) { S.seed = SEEDS[S.me] = r.seed; drawMe(); }
  } catch (e) { toast('Geen verbinding'); }
};
function lobMsg(t, err) { const m = $('lobMsg'); m.innerHTML = t; m.classList.toggle('err', !!err); }

let peekT = 0, peekFor = '';
function peekSoon() { clearTimeout(peekT); peekT = setTimeout(peek, 300); }
async function peek() {
  const code = codeOf(); peekFor = code;
  let r;
  try { r = await (await fetch('/api/vgadai/' + code + '/info')).json(); } catch (e) { return; }
  if (peekFor !== code) return;
  const el = $('peek');
  if (!r.exists) {
    el.innerHTML = '<div class="empty">✨ ' + esc(epLong(code)) + ' — ще не знято. Натисни 🎬 НОВИЙ ЕПІЗОД!</div>';
    return;
  }
  Object.assign(SEEDS, r.looks || {});
  const names = Object.keys(r.scores || {}).sort((a, b) => (r.scores[b] - r.scores[a]) || ((r.asked[b] || 0) - (r.asked[a] || 0)));
  let h = '<h4>🏆 ЛІДЕРИ ' + esc(epName(code)) + '</h4>';
  if (r.online && r.online.length) h += '<div class="empty" style="padding:2px 0 8px">🔴 Зараз в ефірі: ' + r.online.map(esc).join(', ') + '</div>';
  h += leadRows(names.slice(0, 5), r.scores, r.asked || {}, r.online || [], r.looks || {});
  h += '<div class="empty" style="padding:4px 0 0">' + (r.rounds || 0) + ' раундів · натисни 📺 ПРИЄДНАТИСЯ</div>';
  el.innerHTML = h; paintAvatars(el);
}
let liveT = 0;
async function loadLive() {
  clearTimeout(liveT);
  if (SCREEN !== 'lobby') return;
  try {
    const r = await (await fetch('/api/vgadai/live')).json();
    const el = $('liveList');
    el.innerHTML = r.live && r.live.length ? r.live.map(x => '<button class="liveRow" data-code="' + esc(x.code) + '"><span class="dot"></span><span class="ep">' + esc(epName(x.code)) + '</span><span class="ps">' + x.players.map(esc).join(', ') + '</span>▶</button>').join('')
      : '<div class="empty">Зараз нічого не йде. Зніми свій епізод! 🎬</div>';
  } catch (e) {}
  liveT = setTimeout(loadLive, 15000);
}
$('liveList').addEventListener('click', e => {
  const b = e.target.closest('.liveRow'); if (!b) return;
  const c = b.dataset.code; S.season = +c.slice(0, 2); S.ep = +c.slice(2); dialShow(true); SFX.click(); goEpisode('join');
});
async function enterLobby(msg) {
  show('lobby');
  $('meName').textContent = S.me;
  $('meWho').textContent = HOONGY[S.me] ? 'з Hoongy (TUSA) 🍜' : 'твій випадковий вигляд';
  $('reroll').style.display = HOONGY[S.me] ? 'none' : '';
  dialShow(false);
  lobMsg(msg || '', !!msg);
  loadLive();
  if (!HOONGY[S.me]) {
    try {
      const r = await (await fetch('/api/vgadai/look', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ name: S.me }) })).json();
      if (r.seed) S.seed = SEEDS[S.me] = r.seed;
    } catch (e) {}
  }
  drawMe();
  // opened from a notification: straight into that episode
  const ep = new URLSearchParams(location.search).get('ep');
  if (ep && /^\d{4}$/.test(ep) && !enterLobby.done) {
    enterLobby.done = true;
    S.season = +ep.slice(0, 2) || 1; S.ep = +ep.slice(2) || 1; dialShow(false); goEpisode('join');
  }
}
// your character in the lobby, waving
const meCv = $('meCv');
function drawMe(t) {
  const d = Math.min(2, devicePixelRatio || 1), W = 92, H = 110;
  if (meCv.width !== W * d) { meCv.width = W * d; meCv.height = H * d; }
  const x = meCv.getContext('2d'), k = d * 2.6;
  x.setTransform(1, 0, 0, 1, 0, 0); x.clearRect(0, 0, meCv.width, meCv.height);
  x.fillStyle = 'rgba(255,226,58,.18)'; x.beginPath(); x.ellipse(W * d / 2, H * d - 10 * d, 34 * d, 8 * d, 0, 0, Math.PI * 2); x.fill();
  x.setTransform(k, 0, 0, k, W * d / 2, H * d - 8 * d);
  A.use(x);
  const tt = t || 0;
  A.friend(keyOf(S.me, S.seed), 0, 0, { t: tt, dir: 1, anim: (tt % 4) < 1.4 ? 'wave' : '', laugh: (tt % 4) < 1.4 ? 1 : 0 });
  A.use(X);
}

// =====================================================================
// the connection to one episode
// =====================================================================
const NET = { ws: null, code: '', mode: 'join', retry: 0, want: false };
function goEpisode(mode) {
  const code = codeOf();
  lobMsg('📡 ' + (mode === 'create' ? 'Знімаємо ' : 'Підключаємось до ') + esc(epName(code)) + '<span class="dots"></span>');
  NET.code = code; NET.mode = mode; NET.want = true; NET.retry = 0;
  openWs();
}
function openWs() {
  if (NET.ws) { try { NET.ws.onclose = null; NET.ws.close(); } catch (e) {} }
  window.errorOnline = { name: S.me, detail: 'S' + NET.code.slice(0, 2) + 'E' + NET.code.slice(2) }; window.ErrorOnline && ErrorOnline.update();
  const url = (location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/api/vgadai/' + NET.code + '?name=' + encodeURIComponent(S.me) + '&mode=' + NET.mode;
  let ws;
  try { ws = new WebSocket(url); } catch (e) { return lobMsg('Geen verbinding.', true); }
  NET.ws = ws;
  ws.onopen = () => { NET.retry = 0; };
  ws.onmessage = e => { let m; try { m = JSON.parse(e.data); } catch (x) { return; } onMsg(m); };
  ws.onclose = e => {
    if (NET.ws !== ws) return;
    NET.ws = null;
    if (!NET.want || e.code === 4001 || e.code === 4002) return;
    NET.mode = 'join';
    if (SCREEN === 'app') renderCtrl(true);
    setTimeout(() => { if (NET.want && !NET.ws) openWs(); }, Math.min(8000, 500 * Math.pow(2, NET.retry++)));
  };
}
function send(o) { if (NET.ws && NET.ws.readyState === 1) NET.ws.send(JSON.stringify(o)); }
setInterval(() => send({ t: 'ping' }), 25000);
function leaveEpisode() {
  NET.want = false;
  if (NET.ws) { try { NET.ws.close(1000); } catch (e) {} }
  NET.ws = null; S.s = null; S.code = '';
  try { speechSynthesis.cancel(); } catch (e) {}
  enterLobby();
}
function onMsg(m) {
  if (m.type === 'hello') {
    S.code = m.code;
    if (SCREEN !== 'app') enterStudio();
    if (PREF.push && PUSH.sub) send({ t: 'push', on: true, sub: PUSH.sub.toJSON() });
    return;
  }
  if (m.type === 'nope') {
    NET.want = false;
    const ep = esc(epLong(NET.code));
    const msg = {
      live: ep + ' вже в ефірі! Тисни 📺 ПРИЄДНАТИСЯ.',
      exists: ep + ' вже існує. Тисни 📺 ПРИЄДНАТИСЯ, або вибери інший епізод для нового.',
      none: ep + ' ще не існує. Тисни 🎬 НОВИЙ ЕПІЗОД, щоб його зняти.',
      full: 'Студія повна: 6 гравців. Вибери інший епізод.',
    }[m.why] || 'Не вийшло. Probeer het nog eens.';
    if (SCREEN === 'app') enterLobby(msg); else lobMsg(msg, true);
    SFX.nope();
    return;
  }
  if (m.type === 'erased') {
    NET.want = false;
    enterLobby(m.what === 'players' ? '🧹 Гравців стерто. Зайди в епізод знову.' : '🧹 Цей епізод стерто.');
    return;
  }
  if (m.type === 'kicked') {
    NET.want = false;
    dialog('<h3>👋</h3><p>Je bent op een ander apparaat in deze aflevering gestapt.</p><button class="btn go" data-a="again">OPNIEUW HIER</button> <button class="btn" data-a="lobby">LOBBY</button>',
      { again: () => { NET.want = true; NET.mode = 'join'; openWs(); }, lobby: () => leaveEpisode() });
    return;
  }
  if (m.type === 's') {
    const prev = S.s;
    S.s = m.s; S.off = (m.now || Date.now()) - Date.now();
    Object.assign(SEEDS, m.s.looks || {});
    onState(prev, m.ev);
    return;
  }
  if (m.type === 'fx' && m.k === 'react') { reactFx(m.by, m.e); return; }
  if (m.type === 'late') { S.pend = 0; toast('⚡ Перший — ' + m.by + '!'); renderCtrl(true); return; }
  if (m.type === 'pushok') { toast(m.on ? '🔔 Сповіщення увімкнено для ' + epName(S.code) : '🔕 Сповіщення вимкнено'); return; }
}

// =====================================================================
// the studio: everything around the 3D picture
// =====================================================================
function enterStudio() {
  show('app');
  S.introT = T; S.lastQ = ''; S.lastTries = -1; S.histKey = ''; S.leadKey = ''; S.lastScores = {};
  S.text = { q: '', a: '', h: '', g: '' }; S.sub = ''; S.field = '';
  chipCache = {}; $('scores').innerHTML = '';
  FX.confetti.length = 0;
  SFX.fanfare(0.2); SFX.applause(2.2);
  setTimeout(() => banner(epLong(S.code)), 400);
  resize();
}
$('exitBtn').onclick = () => {
  SFX.click();
  dialog('<h3>Вийти?</h3><p>Uit ' + esc(epName(S.code)) + ' stappen? Je punten blijven bewaard.</p><button class="btn go" data-a="yes">ТАК, ВИЙТИ</button> <button class="btn" data-a="no">НІ</button>', { yes: leaveEpisode });
};
$('setBtn').onclick = () => openSettings(true);
$('histBtn').onclick = () => openSheet();
$('sheetClose').onclick = closeSheet; $('veil').onclick = closeSheet;
function openSheet() { SFX.whoosh(); $('sheet').classList.add('show'); $('veil').classList.add('show'); S.histKey = S.leadKey = ''; renderPanels(); }
function closeSheet() { $('sheet').classList.remove('show'); $('veil').classList.remove('show'); }
document.querySelectorAll('.tabs').forEach(tb => tb.addEventListener('click', e => {
  const b = e.target.closest('.t'); if (!b) return;
  S.tab = b.dataset.tab; SFX.click();
  document.querySelectorAll('.tabs .t').forEach(x => x.classList.toggle('on', x.dataset.tab === S.tab));
  S.histKey = S.leadKey = ''; renderPanels();
}));

// reactions
const REACT = ['😂', '😮', '🤔', '👏', '❤️', '🔥'];
$('reacts').innerHTML = REACT.map((e, i) => '<button data-e="' + i + '" aria-label="reactie">' + e + '</button>').join('');
$('reacts').addEventListener('click', e => { const b = e.target.closest('[data-e]'); if (!b) return; send({ t: 'react', e: +b.dataset.e }); SFX.pop(); });

function banner(text) {
  const b = $('banner'); b.querySelector('span').textContent = text;
  b.classList.remove('go'); void b.offsetWidth; b.classList.add('go');
}
function stamp(yes) {
  const s = $('stamp');
  s.className = ''; s.querySelector('.s').textContent = yes ? 'ТАК!' : 'НІ!';
  s.querySelector('.sub').textContent = yes ? 'Goed zo!' : 'Helaas!';
  void s.offsetWidth; s.className = (yes ? 'yes' : 'no') + ' go';
}

// ---------- reacting to what happens ----------
function onState(prev, ev) {
  const s = S.s;
  // the field I type in follows the phase
  if (ev && ev.k === 'buzz' && ev.by === S.me) { S.text.a = ''; S.pend = 0; }
  if (ev && ev.k === 'ask' && s.host === S.me) S.text.q = '';
  if (s.phase !== 'guess') S.sub = '';
  render();
  if (!ev || !prev) return;
  const me = S.me, by = ev.by;
  switch (ev.k) {
    case 'join': if (by !== me) { toast('👋 ' + by + ' в студії!'); SFX.join(); fxOf(by).wave = T + 1.6; } break;
    case 'gone': if (by !== me) { toast('🚪 ' + by + ' іде зі студії'); } break;
    case 'ask':
      SFX.sting(); SFX.whoosh();
      banner('РАУНД ' + s.round);
      FX.qmarks = T + 3;
      speakQ(s.q);
      if (by !== me) vibrate(60);
      break;
    case 'buzz':
      SFX.buzz(); vibrate(by === me ? 120 : 40);
      fxOf(by).bzz = T + 1.3; fxOf(by).jump = T; FX.shake = T + 0.35;
      setTimeout(() => { if (S.s && S.s.phase === 'judge') SFX.drum(1.3); }, 500);
      if (s.host === me) toast('💬 ' + by + ': «' + ev.w + '» — правильно?');
      break;
    case 'yes':
      stamp(true); SFX.ding(); SFX.fanfare(0.35); SFX.applause(3); SFX.cymbal(0.3); vibrate([60, 40, 120]);
      confetti(160); FX.rainbow = T + 6; FX.applause = T + 4;
      fxOf(by).win = T + 4; fxOf(by).plus = T;
      for (const n of s.seats) if (n !== by) fxOf(n).clap = T + 2.2;
      break;
    case 'no':
      stamp(false); SFX.wrong(); vibrate(200);
      fxOf(by).rain = T + 2.6; FX.redFlash = T + 0.7;
      for (const n of s.seats) if (n !== by && n !== s.host) fxOf(n).giggle = T + 1.6;
      break;
    case 'hint': banner('💡 ПІДКАЗКА!'); SFX.boing(); break;
    case 'giveup': SFX.sad(); banner('🏳 НІХТО?!'); FX.sadT = T + 3; break;
    case 'next': if (s.host) { banner('🎤 ПИТАЄ ' + s.host); SFX.whoosh(); } break;
    case 'reset': toast('🔄 Нова гра! Усі бали на 0'); SFX.boing(); break;
  }
}
function vibrate(p) { try { if (navigator.vibrate) navigator.vibrate(p); } catch (e) {} }

function render() {
  if (SCREEN !== 'app') return;
  renderChips(); renderQ(); renderTries(); renderCtrl(); renderPanels();
}

// ---------- the score along the top ----------
let chipCache = {};
function renderChips() {
  const s = S.s, el = $('scores'); if (!s) return;
  const seats = s.seats, max = Math.max(0, ...seats.map(n => s.scores[n] || 0));
  el.classList.toggle('compact', seats.length > 3);
  if (Object.keys(chipCache).join() !== seats.join()) {
    el.innerHTML = ''; chipCache = {};
    for (const n of seats) {
      const c = document.createElement('div'); c.className = 'chip';
      c.innerHTML = '<span class="badge"></span><canvas></canvas><span class="nm"></span><span class="sc">0</span>';
      c.querySelector('.nm').textContent = n;
      avatar(c.querySelector('canvas'), n, 28);
      el.appendChild(c); chipCache[n] = c;
    }
  }
  for (const n of seats) {
    const c = chipCache[n], sc = s.scores[n] || 0;
    c.classList.toggle('off', !s.online.includes(n));
    c.classList.toggle('host', s.host === n);
    c.querySelector('.badge').textContent = s.host === n ? '🎤' : (max > 0 && sc === max ? '👑' : '');
    const scEl = c.querySelector('.sc');
    if (scEl.textContent !== String(sc)) {
      scEl.textContent = sc;
      if (S.lastScores[n] != null && sc > S.lastScores[n]) { c.classList.remove('pop'); void c.offsetWidth; c.classList.add('pop'); }
    }
    S.lastScores[n] = sc;
  }
}
// a round picture of someone's head
function avatar(cv, name, size, seed) {
  const d = Math.min(2, devicePixelRatio || 1), W = size * d;
  cv.width = cv.height = W;
  const x = cv.getContext('2d');
  x.fillStyle = tagOf(name, seed); x.fillRect(0, 0, W, W);
  const k = W / 25;
  x.setTransform(k, 0, 0, k, W / 2 - 0.7 * k, W / 2 + 26.5 * k);
  A.use(x); A.friend(keyOf(name, seed), 0, 0, { t: 0.4, dir: 1 }); A.use(X);
}
function paintAvatars(root) {
  root.querySelectorAll('canvas[data-av]').forEach(cv => avatar(cv, cv.dataset.av, 36, +cv.dataset.seed || 0));
}

// ---------- the question card ----------
function renderQ() {
  const s = S.s, tab = $('qcard').querySelector('.tab'), qt = $('qtext'), qh = $('qhint');
  if (!s) return;
  const r = s.phase === 'ask' ? (s.round || 0) + 1 : s.round;
  tab.innerHTML = esc(epName(S.code)) + ' · РАУНД ' + r + (s.host ? ' · <b>' + esc(s.host) + '</b> питає' : '');
  let key, html, wait = true;
  if (s.phase === 'ask' || !s.q) {
    if (s.host === S.me) html = '🎤 Твоє питання з\'явиться тут…';
    else if (s.paused) html = '⏳ Чекаємо на гравців<span class="dots"></span>';
    else html = '<b style="color:#33241b">' + esc(s.host) + '</b> вигадує питання<span class="dots"></span>';
    key = 'w:' + html;
  } else { key = 'q:' + s.q; wait = false; }
  if (key !== S.lastQ) {
    const isNew = !wait && S.lastQ !== '' ;
    S.lastQ = key;
    qt.classList.toggle('wait', wait);
    if (wait) qt.innerHTML = html;
    else {
      // the question pops in letter by letter (words stay together)
      const n = s.q.length, step = Math.min(30, 1600 / Math.max(1, n));
      let i = 0;
      qt.innerHTML = s.q.split(' ').map(w => '<span class="w">' + [...w].map(ch => '<span class="ch" style="animation-delay:' + (isNew ? (i++ * step) : 0).toFixed(0) + 'ms">' + esc(ch) + '</span>').join('') + '</span>').join(' ');
      if (isNew) for (let k = 0; k < Math.min(n, 40); k += 2) setTimeout(SFX.key, k * step);
    }
  }
  const hint = s.phase !== 'ask' && s.hint ? '💡 ' + s.hint : '';
  if (qh.textContent !== hint) { qh.textContent = hint; qh.classList.toggle('show', !!hint); }
}
function renderTries() {
  const s = S.s; if (!s) return;
  const el = $('tries'), list = s.phase === 'ask' ? [] : s.tries;
  if (list.length === S.lastTries && el.childElementCount === list.length) return;
  const old = Math.max(0, S.lastTries);
  S.lastTries = list.length;
  el.innerHTML = list.map((t, i) => tryChip(t, i >= old && old >= 0)).join('');
}
function tryChip(t, fresh) {
  return '<span class="try ' + (t.ok ? 'ok' : 'no') + (fresh ? ' new' : '') + '"><i style="color:' + deep(tagOf(t.by)) + '">' + esc(t.by) + '</i>' + (t.ok ? esc(t.w) + ' ✓' : '<s>' + esc(t.w) + '</s> ✗') + '</span>';
}
function deep(hex) { const [r, g, b] = rgbOf(hex); return 'rgb(' + (r * 0.55 | 0) + ',' + (g * 0.55 | 0) + ',' + (b * 0.55 | 0) + ')'; }
function rgbOf(hex) { const n = parseInt(hex.slice(1), 16); return [n >> 16 & 255, n >> 8 & 255, n & 255]; }

// ---------- the controls under the question ----------
let ctrlKey = '';
const kbd = ErrorKbd.mount($('kbd'), { onKey: typeKey, sendLabel: '↵', sound: false, ua: true, lang: PREF.lang, onLang: l => { PREF.lang = l; savePref(); } });
const MAX = { q: 160, a: 24, h: 80, g: 40 };
function typeKey(k) {
  const f = S.field; if (!f) return;
  let t = S.text[f];
  if (k === 'DEL') t = t.slice(0, -1);
  else if (k === 'SEND') return submitField();
  else if (k === ' ') { if (f === 'a') { SFX.nope(); return; } if (!t.length || t.endsWith(' ')) return; t += ' '; }
  else if (k.length >= 1 && k.length <= 2) t += k;
  else return;
  if (f === 'a') t = t.replace(/[^\p{L}\p{M}\p{N}'’\-.,]/gu, '');
  S.text[f] = t.slice(0, MAX[f]);
  SFX.key(); renderField();
}
function renderField() {
  const f = S.field, el = document.querySelector('#ctrl .field'); if (!f || !el) return;
  const t = S.text[f];
  const ph = { q: 'Напиши питання…', a: 'одне слово або число', h: 'маленька підказка…', g: 'правильна відповідь…' }[f];
  el.innerHTML = (t ? esc(t) : '<span class="ph">' + ph + '</span>') + '<span class="cur"></span>';
  const cnt = document.querySelector('#ctrl .count'); if (cnt) cnt.textContent = t.length + ' / ' + MAX[f];
  updateBuzz();
}
function submitField() {
  const f = S.field, t = S.text[f].trim();
  if (f === 'a') return pressBuzz();
  if (f === 'q') { if (t.length < 2) { SFX.nope(); toast('✍️ Спершу напиши питання'); return; } send({ t: 'ask', q: t }); SFX.click(); return; }
  if (f === 'h') { if (!t) return SFX.nope(); send({ t: 'hint', h: t }); S.text.h = ''; S.sub = ''; renderCtrl(true); return; }
  if (f === 'g') { send({ t: 'giveup', a: t }); S.text.g = ''; S.sub = ''; return; }
}
function pressBuzz() {
  const s = S.s, b = document.getElementById('buzz');
  if (!s || s.phase !== 'guess') return;
  if ((s.lock[S.me] || 0) > now()) { SFX.nope(); return; }
  const w = S.text.a.trim();
  if (!w) { SFX.nope(); toast('✍️ Спершу напиши відповідь, потім тисни!'); const f = document.querySelector('#ctrl .field'); if (f) { f.style.animation = 'shake .3s'; setTimeout(() => f.style.animation = '', 320); } return; }
  if (b) { b.classList.add('down'); setTimeout(() => b.classList.remove('down'), 160); }
  SFX.click(); S.pend = Date.now();
  send({ t: 'buzz', w });
}
function updateBuzz() {
  const b = document.getElementById('buzz'), s = S.s; if (!b || !s) return;
  if (s.phase === 'judge') {
    b.className = 'buzz taken'; b.style.setProperty('--tc', tagOf(s.buzzer));
    b.innerHTML = '<span>' + esc(s.buzzer) + '<br>⚡</span>'; return;
  }
  const lock = (s.lock[S.me] || 0) - now();
  if (lock > 0) { b.className = 'buzz off'; b.innerHTML = '<span>⏳<br>' + Math.ceil(lock / 1000) + '</span>'; return; }
  b.className = 'buzz' + (S.text.a ? '' : ' off'); b.style.pointerEvents = 'auto';
  b.innerHTML = '<span>ЖМИ!</span>';
}
function keyNow() {
  const s = S.s, me = S.me;
  if (!s || !NET.ws) return 'conn';
  const host = s.host === me;
  if (s.phase === 'ask') return host ? 'ask-host' : (s.paused ? 'pause' : 'ask-wait');
  if (s.paused && s.phase !== 'reveal') return host ? 'pause-host' : 'pause';
  if (s.phase === 'guess') return host ? 'guess-host-' + S.sub : 'guess';
  if (s.phase === 'judge') return host ? 'judge-host:' + s.answer : (s.buzzer === me ? 'judge-me:' + s.answer : 'judge-other:' + s.buzzer + s.answer);
  if (s.phase === 'reveal') return 'reveal:' + s.winner + s.reveal;
  return 'x';
}
function nm(n) { return '<span class="nm" style="color:' + tagOf(n) + '">' + esc(n) + '</span>'; }
function renderCtrl(force) {
  const k = keyNow();
  if (k === ctrlKey && !force) { updateBuzz(); return; }
  ctrlKey = k;
  const s = S.s, c = $('ctrl');
  let h = '', field = '';
  const answerRow = '<div class="fieldRow"><div class="field word"></div><button class="buzz" id="buzz"><span>ЖМИ!</span></button></div>';
  const code = S.code;
  if (k === 'conn') h = '<div class="say">📡 Verbinden met de studio<span class="dots"></span></div>';
  else if (k === 'pause') h = '<div class="say">⏳ Чекаємо на гравців<span class="dots"></span><small>Потрібно щонайменше двоє. Скажи друзям: ' + esc(epLong(code)) + ' (код ' + esc(code) + ')</small></div>';
  else if (k === 'pause-host') h = '<div class="say">⏳ Питання готове! Чекаємо на гравців<span class="dots"></span><small>Код епізоду: ' + esc(code) + ' · ' + esc(epLong(code)) + '</small></div>';
  else if (k === 'ask-host') {
    field = 'q';
    h = '<div class="fieldRow"><div class="field q"></div><button class="btn go" data-act="ask">✨<br>ЗАПИТАТИ</button></div><div class="count"></div>';
  } else if (k === 'ask-wait') h = '<div class="say">' + nm(s.host) + ' вигадує питання<span class="dots"></span><small>Приготуйся: напиши відповідь і тисни зелену кнопку!</small></div>';
  else if (k === 'guess') { field = 'a'; h = answerRow; }
  else if (k === 'guess-host-') h = '<div class="say">👀 Чекаємо на відповіді<span class="dots"></span><small>Ти питаєш, інші відповідають</small></div><div class="row"><button class="btn small" data-act="hint">💡 ПІДКАЗКА</button><button class="btn small" data-act="give">🏳 НІХТО НЕ ЗНАЄ</button></div>';
  else if (k === 'guess-host-hint') { field = 'h'; h = '<div class="fieldRow"><div class="field"></div><button class="btn small go" data-act="sendhint">ДАТИ</button><button class="btn small" data-act="cancel">✕</button></div>'; }
  else if (k === 'guess-host-give') { field = 'g'; h = '<div class="say" style="font-size:14px">Яка правильна відповідь?</div><div class="fieldRow"><div class="field"></div><button class="btn small go" data-act="sendgive">ПОКАЗАТИ</button><button class="btn small" data-act="cancel">✕</button></div>'; }
  else if (k.startsWith('judge-host')) h = '<div class="say">' + nm(s.buzzer) + ' каже:</div><div class="big-word">' + esc(s.answer) + '</div><div class="row"><button class="btn yes" data-act="yes">✓ ТАК!</button><button class="btn no" data-act="no">✗ НІ</button></div>';
  else if (k.startsWith('judge-me')) h = '<div class="say"><span class="drum">🥁</span> ' + nm(s.host) + ' перевіряє твою відповідь<span class="dots"></span></div><div class="big-word">' + esc(s.answer) + '</div>';
  else if (k.startsWith('judge-other')) { field = 'a'; h = '<div class="say" style="font-size:15px">' + nm(s.buzzer) + ' каже «' + esc(s.answer) + '» · ' + nm(s.host) + ' перевіряє <span class="drum">🥁</span></div>' + answerRow; }
  else if (k.startsWith('reveal')) {
    h = (s.winner ? '<div class="say">🏆 Бал отримує ' + nm(s.winner) + '!</div>' : '<div class="say">🏳 Ніхто не вгадав!<small>Правильна відповідь:</small></div>')
      + '<div class="big-word">' + esc(s.reveal || '🤫') + '</div><div class="row" style="align-items:center"><span class="say" style="font-size:14px" id="nextIn"></span><button class="btn small go" data-act="next">ДАЛІ ►</button></div>';
  }
  c.innerHTML = h;
  S.field = field;
  $('kbd').classList.toggle('show', !!field);
  if (field) kbd.setSendLabel(field === 'a' ? 'ЖМИ' : '↵');
  renderField(); updateBuzz(); tickCtrl();
}
$('ctrl').addEventListener('click', e => {
  const b = e.target.closest('[data-act]'); if (!b) return;
  SFX.click();
  const a = b.dataset.act;
  if (a === 'ask') submitField();
  else if (a === 'yes') send({ t: 'judge', ok: true });
  else if (a === 'no') send({ t: 'judge', ok: false });
  else if (a === 'hint') { S.sub = 'hint'; renderCtrl(); }
  else if (a === 'give') { S.sub = 'give'; renderCtrl(); }
  else if (a === 'cancel') { S.sub = ''; renderCtrl(); }
  else if (a === 'sendhint' || a === 'sendgive') submitField();
  else if (a === 'next') send({ t: 'next' });
});
$('ctrl').addEventListener('pointerdown', e => { if (e.target.closest('#buzz')) { e.preventDefault(); pressBuzz(); } });
function tickCtrl() {
  const s = S.s; if (!s || SCREEN !== 'app') return;
  updateBuzz();
  const ni = document.getElementById('nextIn');
  if (ni && s.phase === 'reveal') {
    const left = Math.max(0, Math.ceil((s.nextAt - now()) / 1000));
    const nx = nextHostName();
    ni.textContent = (nx ? 'Далі питає ' + nx + ' · ' : '') + left;
  }
}
setInterval(tickCtrl, 200);
function nextHostName() {
  const s = S.s; if (!s) return '';
  const on = s.online, seats = s.seats, i = seats.indexOf(s.host);
  for (let k = 1; k <= seats.length; k++) { const c = seats[(i + k) % seats.length]; if (on.includes(c)) return c; }
  return '';
}

// ---------- leaderboard and history ----------
function leadRows(names, scores, asked, online, looks) {
  if (!names.length) return '<div class="empty">Ще нікого. Хто перший?</div>';
  const medal = ['🥇', '🥈', '🥉'];
  return names.map((n, i) => '<div class="lead' + (i === 0 && scores[n] > 0 ? ' first' : '') + '"><span class="rk">' + (medal[i] || (i + 1)) + '</span><canvas data-av="' + esc(n) + '" data-seed="' + ((looks && looks[n]) || '') + '"></canvas>'
    + '<span class="nm">' + esc(n) + '<small>❓ ' + (asked[n] || 0) + ' питань' + (online.includes(n) ? ' · 🟢 тут' : '') + '</small></span><span class="pts">' + (scores[n] || 0) + '</span></div>').join('');
}
function renderPanels() {
  const s = S.s; if (!s) return;
  const lk = JSON.stringify([s.scores, s.asked, s.online]), hk = s.hist.length + ':' + (s.hist.length ? s.hist[s.hist.length - 1].t : 0);
  const key = S.tab === 'lead' ? 'L' + lk : 'H' + hk;
  if (key === S.histKey) return;
  S.histKey = key;
  let h;
  if (S.tab === 'lead') {
    const names = Object.keys(s.scores).sort((a, b) => (s.scores[b] - s.scores[a]) || ((s.asked[b] || 0) - (s.asked[a] || 0)));
    h = '<div class="empty" style="padding:0 0 8px">🏆 Рейтинг цього епізоду · ' + esc(epName(S.code)) + '</div>' + leadRows(names, s.scores, s.asked || {}, s.online, s.looks);
  } else {
    h = s.hist.length ? s.hist.slice().reverse().map(r => '<div class="rnd"><div class="top"><span>РАУНД ' + r.n + '</span><span>🎤 ' + esc(r.host) + '</span></div>'
      + '<div class="q">' + esc(r.q) + '</div>' + (r.hint ? '<div class="h">💡 ' + esc(r.hint) + '</div>' : '')
      + '<div class="gs">' + r.tries.map(t => tryChip(t, false)).join('') + '</div>'
      + '<div class="res">' + (r.winner ? '🏆 ' + esc(r.winner) + ' · «' + esc(r.a) + '»' : '🏳 ніхто' + (r.a ? ' · «' + esc(r.a) + '»' : '')) + '</div></div>').join('')
      : '<div class="empty">Ще жодного раунду. Перше питання — твоє? 🎤</div>';
  }
  for (const id of ['panelSide', 'panelSheet']) { const el = $(id); el.innerHTML = h; paintAvatars(el); }
}

// ---------- settings ----------
let installEv = null;
addEventListener('beforeinstallprompt', e => { e.preventDefault(); installEv = e; });
function openSettings(inStudio) {
  SFX.click();
  const sw = on => '<span class="sw' + (on ? ' on' : '') + '">' + (on ? 'УВІМК' : 'ВИМК') + '</span>';
  const standalone = matchMedia('(display-mode: standalone)').matches || matchMedia('(display-mode: fullscreen)').matches || navigator.standalone;
  dialog('<h3>⚙ Налаштування</h3>'
    + '<button class="opt" data-a="push"><span class="ico">🔔</span><span>Сповіщення<small>нове питання і нова відповідь у твоєму епізоді</small></span>' + sw(PREF.push && PUSH.sub) + '</button>'
    + '<button class="opt" data-a="voice"><span class="ico">🗣</span><span>Ведучий читає питання<small>вголос, українською (коли питання кирилицею)</small></span>' + sw(PREF.voice) + '</button>'
    + '<button class="opt" data-a="sound"><span class="ico">🔊</span><span>Звук<small>кнопки, фанфари, оплески</small></span>' + sw(SET.sound) + '</button>'
    + (standalone ? '' : '<button class="opt" data-a="install"><span class="ico">📲</span><span>Встановити як додаток<small>на головний екран, на весь екран</small></span></button>')
    + (inStudio ? '<button class="opt" data-a="reset"><span class="ico">🔄</span><span>Нова гра в цьому епізоді<small>рейтинг ' + esc(epName(S.code)) + ' на нуль</small></span></button>' : '')
    + '<button class="opt" data-a="adv"><span class="ico">🛠</span><span>Розширені<small>стерти епізод, сезон, гравців або все</small></span></button>'
    + '<button class="btn" data-a="close" style="margin-top:4px">ГОТОВО</button>', {
    push: () => { togglePush().then(() => openSettings(inStudio)); return 'keep'; },
    voice: () => { PREF.voice = !PREF.voice; savePref(); if (!PREF.voice) try { speechSynthesis.cancel(); } catch (e) {} else speakTest(); openSettings(inStudio); return 'keep'; },
    sound: () => { setSound(!SET.sound); openSettings(inStudio); return 'keep'; },
    install: () => {
      if (installEv) { installEv.prompt(); installEv = null; return; }
      const ios = /iP(hone|ad|od)/.test(navigator.userAgent) || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);
      setTimeout(() => dialog('<h3>📲</h3><p>' + (ios ? 'Safari: tik op Deel ⎙ en kies "Zet op beginscherm".' : 'Open het browsermenu ⋮ en kies "App installeren" of "Toevoegen aan startscherm".') + '</p><button class="btn go" data-a="ok">OK</button>'), 50);
    },
    adv: () => { setTimeout(() => openAdvanced(inStudio), 30); },
    reset: () => { setTimeout(() => dialog('<h3>Нова гра?</h3><p>Рейтинг і історія ' + esc(epName(S.code)) + ' стануть порожніми.</p><button class="btn no" data-a="yes">ТАК, ЗАНОВО</button> <button class="btn" data-a="no">НІ</button>', { yes: () => send({ t: 'reset' }) }), 50); },
  });
}
// ---------- advanced: erasing ----------
function openAdvanced(inStudio) {
  const opt = (a, ico, label, sub) => '<button class="opt" data-a="' + a + '"><span class="ico">' + ico + '</span><span>' + label + '<small>' + sub + '</small></span></button>';
  dialog('<h3>🛠 Розширені</h3><p>Стерти дані гри. Цього не можна скасувати.</p>'
    + opt('ep', '🧹', 'Стерти епізод або сезон', 'історія питань і рейтинг; код знову вільний')
    + opt('players', '👥', 'Стерти тільки гравців', 'вигляд гравців і рейтинги в усіх епізодах; питання лишаються')
    + opt('all', '💣', 'Стерти все', 'усі епізоди, історії, рейтинги і гравців')
    + '<button class="btn" data-a="back" style="margin-top:4px">◄ НАЗАД</button>', {
    ep: () => { setTimeout(() => eraseEpisodes(S.code || codeOf()), 30); },
    players: () => { setTimeout(() => eraseConfirm({ what: 'players' }, '👥 Стерти всіх гравців?', 'Вигляд гравців і рейтинги в усіх епізодах зникнуть. Питання й відповіді в історії лишаються. Хто зараз грає, повернеться в лобі.'), 30); },
    all: () => { setTimeout(() => eraseConfirm({ what: 'all' }, '💣 Стерти все?', 'Усі епізоди всіх сезонів, їхні історії й рейтинги, і всі гравці. Хто зараз грає, повернеться в лобі.'), 30); },
    back: () => { setTimeout(() => openSettings(inStudio), 30); },
  });
}
// pick an episode (or a whole season) to erase; the episodes that exist are listed
async function eraseEpisodes(code) {
  let eps = [];
  try { eps = (await (await fetch('/api/vgadai/episodes')).json()).episodes || []; } catch (e) {}
  let ss = +code.slice(0, 2) || 1, ee = +code.slice(2) || 1;
  const draw = () => {
    const c = String(ss).padStart(2, '0') + String(ee).padStart(2, '0'), inSeason = eps.filter(x => x.code.startsWith(c.slice(0, 2)));
    const exists = eps.some(x => x.code === c);
    const chips = eps.length ? eps.map(x => '<button class="chipEp' + (x.code === c ? ' on' : '') + '" data-ep="' + x.code + '">' + esc(epName(x.code)) + (x.n ? ' 🔴' : '') + '</button>').join('') : '<span class="empty" style="padding:4px">Ще немає жодного епізоду.</span>';
    dialog('<h3>🧹 Стерти епізод</h3>'
      + '<div class="steps"><span>СЕЗОН</span><button class="hbtn" data-a="s-">◄</button><b>' + String(ss).padStart(2, '0') + '</b><button class="hbtn" data-a="s+">►</button></div>'
      + '<div class="steps"><span>ЕПІЗОД</span><button class="hbtn" data-a="e-">◄</button><b>' + String(ee).padStart(2, '0') + '</b><button class="hbtn" data-a="e+">►</button></div>'
      + '<div class="eps">' + chips + '</div>'
      + '<button class="opt" data-a="one"' + (exists ? '' : ' disabled style="opacity:.45"') + '><span class="ico">🧹</span><span>Епізод ' + esc(epName(c)) + '<small>' + (exists ? 'історія, рейтинг, код ' + c + ' знову вільний' : 'такого епізоду немає') + '</small></span></button>'
      + '<button class="opt" data-a="season"' + (inSeason.length ? '' : ' disabled style="opacity:.45"') + '><span class="ico">🗂</span><span>Весь сезон ' + ss + '<small>' + inSeason.length + ' епізод(ів)</small></span></button>'
      + '<button class="btn" data-a="back" style="margin-top:4px">◄ НАЗАД</button>', {
      's-': () => { ss = ss > 1 ? ss - 1 : 99; draw(); return 'keep'; }, 's+': () => { ss = ss < 99 ? ss + 1 : 1; draw(); return 'keep'; },
      'e-': () => { ee = ee > 1 ? ee - 1 : 99; draw(); return 'keep'; }, 'e+': () => { ee = ee < 99 ? ee + 1 : 1; draw(); return 'keep'; },
      one: () => { if (!exists) return 'keep'; setTimeout(() => eraseConfirm({ what: 'episode', code: c }, '🧹 Стерти ' + epName(c) + '?', 'Історія питань і рейтинг цього епізоду зникнуть, а код ' + c + ' знову вільний для нового епізоду.'), 30); },
      season: () => { if (!inSeason.length) return 'keep'; setTimeout(() => eraseConfirm({ what: 'season', season: c.slice(0, 2) }, '🗂 Стерти сезон ' + ss + '?', 'Усі ' + inSeason.length + ' епізод(ів) сезону ' + ss + ': історії й рейтинги.'), 30); },
      back: () => { setTimeout(() => openAdvanced(SCREEN === 'app'), 30); },
    });
    $('dlg').querySelectorAll('[data-ep]').forEach(b => b.addEventListener('click', e => { e.stopPropagation(); SFX.tick(); ss = +b.dataset.ep.slice(0, 2); ee = +b.dataset.ep.slice(2); draw(); }));
  };
  draw();
}
function eraseConfirm(body, title, text) {
  dialog('<h3>' + esc(title) + '</h3><p>' + esc(text) + '</p>'
    + '<button class="btn no hold" data-hold="go">✋ ТРИМАЙ, ЩОБ СТЕРТИ</button><br><button class="btn" data-a="no" style="margin-top:10px">НІ</button>', {
    go: async () => {
      try {
        const r = await (await fetch('/api/vgadai/erase', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) })).json();
        toast(r.ok ? '🧹 Стерто' + (r.episodes ? ': епізодів ' + r.episodes : '') : 'Не вийшло');
        if (body.what === 'all' || body.what === 'players') { for (const k in SEEDS) delete SEEDS[k]; S.seed = 0; }
        if (SCREEN === 'lobby') { peek(); loadLive(); if (!HOONGY[S.me] && (body.what === 'all' || body.what === 'players')) enterLobby(); }
      } catch (e) { toast('Geen verbinding'); }
    },
  });
}
function speakTest() {
  if (!('speechSynthesis' in window)) { toast('🗣 Цей браузер не вміє говорити'); return; }
  speakQ('Вітаємо у шоу Вгадай що!');
}

// =====================================================================
// notifications: a new question or a new answer in your episode
// =====================================================================
const PUSH = { reg: null, key: null, sub: null };
function pushBytes(s) {
  s = s.replace(/-/g, '+').replace(/_/g, '/'); while (s.length % 4) s += '=';
  const b = atob(s), u = new Uint8Array(b.length); for (let i = 0; i < b.length; i++) u[i] = b.charCodeAt(i); return u;
}
async function pushPrepare() {
  try {
    if (!('serviceWorker' in navigator)) return;
    PUSH.reg = await navigator.serviceWorker.register('/sw.js');
    if (!('PushManager' in window)) return;
    PUSH.sub = await PUSH.reg.pushManager.getSubscription();
    if (!PUSH.key) PUSH.key = pushBytes((await (await fetch('/api/push')).json()).key);
  } catch (e) {}
}
async function togglePush() {
  if (PREF.push && PUSH.sub) {
    PREF.push = false; savePref();
    if (S.code) send({ t: 'push', on: false, sub: PUSH.sub.toJSON() }); else toast('🔕 Сповіщення вимкнено');
    return;
  }
  const ios = /iP(hone|ad|od)/.test(navigator.userAgent) || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);
  const app = matchMedia('(display-mode: standalone)').matches || matchMedia('(display-mode: fullscreen)').matches || navigator.standalone;
  if (!PUSH.reg || !('PushManager' in window) || !('Notification' in window)) {
    toast(ios && !app ? '📲 iPhone: спершу встанови гру на головний екран' : 'Сповіщення тут не працюють');
    return;
  }
  if (!PUSH.key) { toast('Even geduld… probeer het zo nog eens'); pushPrepare(); return; }
  try {
    // straight from the tap: this is what asks "allow notifications?"
    PUSH.sub = PUSH.sub || await PUSH.reg.pushManager.subscribe({ userVisibleOnly: true, applicationServerKey: PUSH.key });
    PREF.push = true; savePref();
    if (S.code) send({ t: 'push', on: true, sub: PUSH.sub.toJSON() });
    else toast('🔔 Сповіщення увімкнено: вони працюють в епізоді, де ти граєш');
  } catch (e) {
    toast(Notification.permission === 'denied' ? 'Сповіщення заблоковані в налаштуваннях телефону' : 'Не вийшло. Спробуй ще раз.');
  }
}
pushPrepare();

// =====================================================================
// the keyboard on a computer
// =====================================================================
addEventListener('keydown', e => {
  if (e.ctrlKey || e.metaKey || e.altKey) return;
  if (e.key === 'Escape') {
    e.preventDefault();
    if (dlgOpen()) { const wasMenu = MENU; closeDialog(); if (wasMenu) SFX.click(); }
    else if ($('sheet').classList.contains('show')) closeSheet();
    else openMenu();
    return;
  }
  if (dlgOpen()) return dlgKey(e);
  if ((e.key === 'f' || e.key === 'F') && fsOk() && !typingNow()) { e.preventDefault(); toggleFull(); return; }
  if (SCREEN === 'login') {
    if (e.key === 'Enter') { e.preventDefault(); loginKb.flash('SEND'); return loginKey('SEND'); }
    if (e.key === 'Backspace') { e.preventDefault(); return loginKey('DEL'); }
    if (e.key.length === 1) { e.preventDefault(); loginKb.flash(e.key); return loginKey(e.key); }
    return;
  }
  if (SCREEN === 'lobby') {
    // the dials with the arrow keys, Enter to join
    if (e.key === 'ArrowUp' || e.key === 'ArrowDown') { e.preventDefault(); dialStep(e.shiftKey ? 'S' : 'E', e.key === 'ArrowUp' ? 1 : -1); }
    else if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') { e.preventDefault(); dialStep('S', e.key === 'ArrowRight' ? 1 : -1); }
    else if (e.key === 'Enter') { e.preventDefault(); SFX.click(); goEpisode('join'); }
    return;
  }
  if (SCREEN !== 'app') return;
  const s = S.s;
  if (S.field) {
    if (e.key === 'Enter') { e.preventDefault(); return S.field === 'a' ? pressBuzz() : submitField(); }
    if (e.key === 'Backspace') { e.preventDefault(); return typeKey('DEL'); }
    if (e.key.length === 1) { e.preventDefault(); kbd.flash(e.key); return typeKey(e.key === ' ' ? ' ' : e.key); }
    return;
  }
  if (s && s.phase === 'judge' && s.host === S.me) {
    if (/^[yjt]$/i.test(e.key)) send({ t: 'judge', ok: true });
    if (/^n$/i.test(e.key)) send({ t: 'judge', ok: false });
  }
  if (s && s.phase === 'reveal' && e.key === 'Enter') send({ t: 'next' });
});

// =====================================================================
// THE STUDIO IN 3D
// a real perspective camera looks at the stage; the characters are drawn
// flat (like paper cut-outs) at their place in the room
// =====================================================================
const view = $('stage'), X = view.getContext('2d');
let VW = 2, VH = 2, DPR = 1;
function resize() {
  const r = $('stageWrap').getBoundingClientRect();
  DPR = Math.min(2, devicePixelRatio || 1);
  const w = Math.max(2, Math.round(r.width * DPR)), h = Math.max(2, Math.round(r.height * DPR));
  if (w !== VW || h !== VH) { VW = view.width = w; VH = view.height = h; }
}
addEventListener('resize', resize);
if (window.ResizeObserver) new ResizeObserver(resize).observe($('stageWrap'));

const INK = '#33241b';
const PZ = 100, WALLZ = 238, LIPZ = 34, CURZ = 40;
const POD = { hw: 17, d: 6, h: 21 }, RISER = 10;
const CAM = { x: 0, y: 46, z: -60, yaw: 0, pitch: 0.1, roll: 0, f: 600, cx: 0, cy: 0 };
const BASE = { x: 0, y: 46, z: -60, pitch: 0.1 };
const FOC = { x: 0, dolly: 0, y: 0 };
let cyw = 1, syw = 0, cpi = 1, spi = 0, cro = 1, sro = 0;
function camTrig() { cyw = Math.cos(CAM.yaw); syw = Math.sin(CAM.yaw); cpi = Math.cos(CAM.pitch); spi = Math.sin(CAM.pitch); cro = Math.cos(CAM.roll); sro = Math.sin(CAM.roll); }
// a point in the room → [screen x, screen y, pixels per unit, depth]
function P(x, y, z) {
  const dx = x - CAM.x, dy = y - CAM.y, dz = z - CAM.z;
  const x1 = dx * cyw - dz * syw, z1 = dx * syw + dz * cyw;
  const y2 = dy * cpi + z1 * spi, z2 = -dy * spi + z1 * cpi;
  const zz = Math.max(3, z2), k = CAM.f / zz, sx = x1 * k, sy = -y2 * k;
  return [CAM.cx + sx * cro - sy * sro, CAM.cy + sx * sro + sy * cro, k, zz];
}
const idT = () => X.setTransform(1, 0, 0, 1, 0, 0);
// draw flat things (people, signs) standing at a point in the room
function billboard(x, y, z, fn) {
  const p = P(x, y, z), k = p[2];
  X.setTransform(k * cro, k * sro, -k * sro, k * cro, p[0], p[1]);
  fn(k, p);
}
// draw on a flat surface in the room: o is its centre, ex / ey one step right / down
function plane(o, ex, ey, fn) {
  const h = 5, p0 = P(o[0], o[1], o[2]);
  const p1 = P(o[0] + ex[0] * h, o[1] + ex[1] * h, o[2] + ex[2] * h), p2 = P(o[0] + ey[0] * h, o[1] + ey[1] * h, o[2] + ey[2] * h);
  X.setTransform((p1[0] - p0[0]) / h, (p1[1] - p0[1]) / h, (p2[0] - p0[0]) / h, (p2[1] - p0[1]) / h, p0[0], p0[1]);
  fn(p0[2]);
}
function quad(pts, fill, lw, ink) {
  idT(); X.beginPath();
  pts.forEach((q, i) => { const p = P(q[0], q[1], q[2]); i ? X.lineTo(p[0], p[1]) : X.moveTo(p[0], p[1]); });
  X.closePath();
  if (fill) { X.fillStyle = fill; X.fill(); }
  if (lw) { X.strokeStyle = ink || INK; X.lineWidth = lw; X.lineJoin = 'round'; X.stroke(); }
}
function shade(hex, a) {
  const [r, g, b] = rgbOf(hex), f = v => clamp(Math.round(a < 0 ? v * (1 + a) : v + (255 - v) * a), 0, 255);
  return 'rgb(' + f(r) + ',' + f(g) + ',' + f(b) + ')';
}
function hsl(h, s, l, a) { return 'hsla(' + ((h % 360) + 360) % 360 + ',' + s + '%,' + l + '%,' + (a == null ? 1 : a) + ')'; }
const ROUND = '"M PLUS Rounded 1c", "Pixelify Sans", sans-serif', BUBBLE = '"Rubik Bubbles", "M PLUS Rounded 1c", sans-serif';
function text(t, x, y, size, fill, o) {
  o = o || {};
  X.font = (o.w || 900) + ' ' + size + 'px ' + (o.font || ROUND);
  X.textAlign = o.align || 'center'; X.textBaseline = 'middle';
  if (o.ink) { X.lineJoin = 'round'; X.strokeStyle = o.inkCol || INK; X.lineWidth = o.ink; X.strokeText(t, x, y); }
  X.fillStyle = fill; X.fillText(t, x, y);
}
function fitFont(t, size, maxW, font, w) {
  X.font = (w || 900) + ' ' + size + 'px ' + (font || ROUND);
  const m = X.measureText(t).width;
  return m > maxW ? size * maxW / m : size;
}

// ---------- per-player effects ----------
const FX = { by: {}, confetti: [], floats: [], shake: 0, rainbow: 0, redFlash: 0, applause: 0, qmarks: 0, sadT: 0 };
function fxOf(n) { return FX.by[n] || (FX.by[n] = {}); }
function confetti(n) {
  const cols = ['#ffe23a', '#ff6fae', '#5ad0ff', '#7af07a', '#ff9a4a', '#c39bff', '#ffffff'];
  for (let i = 0; i < n; i++) FX.confetti.push({
    x: (Math.random() - 0.5) * 260, y: 150 + Math.random() * 60, z: 50 + Math.random() * 170,
    vx: (Math.random() - 0.5) * 30, vy: -10 - Math.random() * 30, vz: (Math.random() - 0.5) * 20,
    r: Math.random() * 6, vr: (Math.random() - 0.5) * 12, c: cols[i % cols.length], w: 1.6 + Math.random() * 1.6, life: 6 + Math.random() * 2,
  });
}
function reactFx(by, e) {
  FX.floats.push({ by, e: REACT[e] || '❤️', t: T, dx: (Math.random() - 0.5) * 8 });
  const f = fxOf(by);
  if (e === 0) f.giggle = T + 2;
  else if (e === 3 || e === 5) f.clap = T + 1.4;
  else if (e === 4) f.love = T + 1.6;
  else f.wave = T + 1.2;
}

// ---------- the cast: who stands where ----------
function cast() {
  const s = S.s, seats = s ? s.seats : (S.me ? [S.me] : []);
  const n = Math.max(1, seats.length), SP = n > 4 ? 44 : 52;
  const xs = seats.map((_, i) => (i - (n - 1) / 2) * SP);
  const hostX = -(n - 1) / 2 * SP - 40;
  const mid = (hostX - 10 + (n - 1) / 2 * SP + 20) / 2;
  return {
    seats, n, SP,
    pods: seats.map((name, i) => ({ name, x: xs[i] - mid, z: PZ + Math.abs(xs[i] - mid) * 0.08 })),
    host: { x: hostX - mid, z: 84 },
    left: hostX - mid - 16, right: (n - 1) / 2 * SP - mid + 24,
  };
}
const podOf = (C, name) => C.pods.find(p => p.name === name);

// ---------- the camera: a person holding it, who follows the action ----------
let T = 0, lastTs = 0;
function camera(C, dt) {
  const s = S.s;
  // what is the camera looking at?
  let fx = 0, dolly = 0;
  const pod = n => podOf(C, n);
  if (!s || s.paused) { fx = 0; dolly = 0; }
  else if (VOICE.talking) { fx = C.host.x * 0.7; dolly = 24; }
  else if (s.phase === 'ask' && pod(s.host)) { fx = pod(s.host).x * 0.7; dolly = 12; }
  else if (s.phase === 'guess') { fx = Math.sin(T * 0.35) * 18; dolly = 6; }
  else if (s.phase === 'judge' && pod(s.buzzer)) {
    // a TV cut: the one who answered, now and then a glance at the one who asked
    const a = pod(s.buzzer), b = pod(s.host) || a;
    fx = (T % 5) < 3.4 ? a.x : b.x; dolly = 34;
  } else if (s.phase === 'reveal') {
    const w = s.winner && pod(s.winner);
    const since = T - ((w && fxOf(s.winner).win) ? fxOf(s.winner).win - 4 : 0);
    if (w && since < 2.6) { fx = w.x; dolly = 40; } else { fx = 0; dolly = -4; }
  }
  // a narrow (phone) picture doesn't zoom in as far, so nobody falls out of it
  if (VW < VH * 1.1) dolly *= 0.55;
  const k = 1 - Math.exp(-dt * 1.6);
  FOC.x = lerp(FOC.x, fx, k); FOC.dolly = lerp(FOC.dolly, dolly, k);
  // framing: everybody fits, from the sign over the stage down to the floor in front of the desks
  const D = PZ - BASE.z, needW = Math.max(150, C.right - C.left) + 16;
  const cp = Math.cos(BASE.pitch), sp = Math.sin(BASE.pitch);
  const vy = (y, z) => { const dy = y - BASE.y, dz = z - BASE.z; return -(dy * cp + dz * sp) / (-dy * sp + dz * cp); };
  const vTop = vy(MQ.y + MQ.h / 2 + 5, MQ.z), vBot = vy(-7, PZ - 16);
  CAM.f = Math.min(VW * D / needW, VH * 0.97 / (vBot - vTop));
  CAM.cx = VW / 2; CAM.cy = VH / 2 - CAM.f * (vTop + vBot) / 2;
  // the hands that hold the camera
  const hx = 1.5 * Math.sin(T * 0.53) + 0.7 * Math.sin(T * 1.31 + 2) + 0.25 * Math.sin(T * 3.7);
  const hy = 0.9 * Math.sin(T * 0.71 + 1) + 0.45 * Math.sin(T * 1.7) + 0.2 * Math.sin(T * 4.1 + 1);
  let shake = 0;
  if (T < FX.shake) shake = (FX.shake - T) * 8;
  CAM.x = BASE.x + FOC.x * 0.45 + hx + (Math.random() - 0.5) * shake;
  CAM.y = BASE.y + hy - FOC.dolly * 0.12 + (Math.random() - 0.5) * shake;
  CAM.z = BASE.z + FOC.dolly + 0.8 * Math.sin(T * 0.43);
  CAM.yaw = Math.atan2(FOC.x - CAM.x, PZ - CAM.z) * 0.55 + 0.004 * Math.sin(T * 0.37 + 0.5);
  CAM.pitch = BASE.pitch + 0.004 * Math.sin(T * 0.61 + 3) + FOC.dolly * 0.0004;
  CAM.roll = 0.007 * Math.sin(T * 0.43) + 0.003 * Math.sin(T * 1.27 + 1);
  // the opening shot: a crane comes down onto the stage
  const ik = ease((T - S.introT) / 3.4);
  if (ik < 1) { const u = 1 - ik; CAM.y += u * u * 85; CAM.z -= u * u * 70; CAM.pitch += u * u * 0.36; CAM.roll += u * 0.03; }
  camTrig();
}
// what the steady camera sees at a depth (to hang the curtains just at the edges)
function frustumAt(z) {
  const D = (z - BASE.z) / Math.cos(BASE.pitch);
  const halfW = (VW / 2) * D / CAM.f, halfH = (VH / 2) * D / CAM.f;
  const yc = BASE.y - (z - BASE.z) * Math.tan(BASE.pitch);
  return { halfW, top: yc + halfH, bottom: yc - halfH };
}

// ---------- drawing the studio ----------
const SPOTS = [{ sx: -150, sy: 240, sz: 40, tx: -40, tz: 100 }, { sx: 150, sy: 240, sz: 40, tx: 40, tz: 100 }, { sx: 0, sy: 280, sz: 150, tx: 0, tz: 110 }];
function drawStudio(dt) {
  const C = cast(), s = S.s;
  camera(C, dt);
  A.use(X);
  idT();
  X.fillStyle = '#140a28'; X.fillRect(0, 0, VW, VH);
  wall(C);
  marquee();
  floor(C);
  spots(C, dt);
  // everyone on stage, the far ones first
  const list = C.pods.map(p => ({ z: p.z, f: () => podium(C, p) }));
  list.push({ z: C.host.z, f: () => hostNpc(C) });
  list.sort((a, b) => b.z - a.z).forEach(o => o.f());
  bubbles(C);
  particles(C, dt);
  curtains(C, dt);
  idT();
  audience();
  overlay();
  idT(); A.paper(X, VW, VH, 0.32);
}

// the back wall: a turning sunburst and a big round screen
function wall(C) {
  const top = 300;
  idT();
  const g = X.createLinearGradient(0, P(0, top, WALLZ)[1], 0, P(0, 0, WALLZ)[1]);
  g.addColorStop(0, '#2a1258'); g.addColorStop(0.6, '#3d1b78'); g.addColorStop(1, '#2a1150');
  quad([[-420, 0, WALLZ], [420, 0, WALLZ], [420, top, WALLZ], [-420, top, WALLZ]], g);
  // sunburst rays
  const cx = 0, cy = 64, N = 32, R = 420, rot = T * 0.05;
  const red = s => s && s.phase === 'reveal' && s.winner;
  const c0 = P(cx, cy, WALLZ - 0.5);
  for (let i = 0; i < N; i += 2) {
    const a0 = rot + i / N * Math.PI * 2, a1 = rot + (i + 1) / N * Math.PI * 2;
    const p0 = P(cx + Math.cos(a0) * R, cy + Math.sin(a0) * R, WALLZ - 0.5), p1 = P(cx + Math.cos(a1) * R, cy + Math.sin(a1) * R, WALLZ - 0.5);
    idT(); X.beginPath(); X.moveTo(c0[0], c0[1]); X.lineTo(p0[0], p0[1]); X.lineTo(p1[0], p1[1]); X.closePath();
    X.fillStyle = red(S.s) ? hsl(T * 90 + i * 12, 90, 70, 0.13) : 'rgba(255,214,120,0.075)'; X.fill();
  }
  // twinkling stars on the wall
  for (let i = 0; i < 26; i++) {
    const x = (A.hash(i * 7 + 1) - 0.5) * 380, y = 10 + A.hash(i * 7 + 2) * 150, tw = 0.5 + 0.5 * Math.sin(T * 2.2 + i * 1.7);
    billboard(x, y, WALLZ - 1, () => { X.globalAlpha = 0.25 + tw * 0.6; A.sparkle(0, 0, 1.2 + tw * 1.4, '#fff6c8'); X.globalAlpha = 1; });
  }
  // the round screen: "?" while asking, the name when someone pressed, ✓ / ✗
  const s = S.s;
  plane([0, 58, WALLZ - 1], [1, 0, 0], [0, -1, 0], () => {
    const R1 = 25;
    X.beginPath(); X.arc(0, 0, R1 + 3, 0, Math.PI * 2); X.fillStyle = '#cf3a2a'; X.fill(); X.lineWidth = 0.9; X.strokeStyle = INK; X.stroke();
    X.beginPath(); X.arc(0, 0, R1, 0, Math.PI * 2); X.fillStyle = '#120826'; X.fill(); X.stroke();
    for (let i = 0; i < 28; i++) {
      const a = i / 28 * Math.PI * 2, lit = ((i + (T * 8 | 0)) % 4) === 0 || (s && s.phase === 'reveal' && ((i + (T * 16 | 0)) % 2));
      if (lit) A.glow(Math.cos(a) * (R1 + 1.5), Math.sin(a) * (R1 + 1.5), 3, '#ffe9a0', 0.8);
      A.circle(Math.cos(a) * (R1 + 1.5), Math.sin(a) * (R1 + 1.5), 0.85, lit ? '#fff6c0' : '#7a4a2a');
    }
    let g2 = '?', col = '#ffe23a', size = 34, font = BUBBLE;
    if (!s || s.paused) { g2 = '…'; col = '#8a7aa8'; }
    else if (s.phase === 'judge') { g2 = s.buzzer; col = tagOf(s.buzzer); size = fitFont(g2, 12, 40, BUBBLE, 400); }
    else if (s.phase === 'reveal') { g2 = s.winner ? '✓' : '🏳'; col = s.winner ? '#5af07a' : '#ffffff'; }
    else if (T < FX.redFlash) { g2 = '✗'; col = '#ff4a3a'; }
    const sp = s && (s.phase === 'ask' || s.phase === 'guess') ? Math.cos(T * 1.6) : 1;
    X.save(); X.scale(Math.abs(sp) < 0.08 ? 0.08 : sp, 1);
    A.glow(0, 0, 22, col, 0.35);
    text(g2, 0, 1, size, col, { font, w: 400, ink: 1.6 });
    X.restore();
  });
}
// the sign over the stage, with chasing bulbs, the title and the episode
const MQ = { y: 108, z: 206, w: 150, h: 34 };
function marquee() {
  const s = S.s, y0 = MQ.y, z = MQ.z, w = MQ.w, h = MQ.h;
  // cables to the ceiling
  for (const sx of [-60, 60]) { const a = P(sx, y0 + h / 2, z), b = P(sx, 260, z); idT(); X.beginPath(); X.moveTo(a[0], a[1]); X.lineTo(b[0], b[1]); X.strokeStyle = 'rgba(20,10,30,.8)'; X.lineWidth = 0.5 * a[2]; X.stroke(); }
  plane([0, y0, z], [1, 0, 0], [0, -1, 0], () => {
    A.glow(0, 0, 90, '#ff6fd0', 0.16);
    A.box(-w / 2, -h / 2, w, h, '#cf3a2a', 1, 3, 6);
    A.box(-w / 2 + 4, -h / 2 + 4, w - 8, h - 8, '#2a1446', 0.7, 4, 4);
    // bulbs all around
    const per = 2 * (w + h - 4), n = Math.floor(per / 5.2), fast = s && s.phase === 'reveal';
    for (let i = 0; i < n; i++) {
      let d = i / n * per, x, y; const W2 = w - 4, H2 = h - 4;
      if (d < W2) { x = -W2 / 2 + d; y = -H2 / 2; } else if ((d -= W2) < H2) { x = W2 / 2; y = -H2 / 2 + d; } else if ((d -= H2) < W2) { x = W2 / 2 - d; y = H2 / 2; } else { d -= W2; x = -W2 / 2; y = H2 / 2 - d; }
      const lit = ((i + Math.floor(T * (fast ? 16 : 7))) % 3) === 0;
      if (lit) A.glow(x, y, 3.2, '#ffe9a0', 0.9);
      A.circle(x, y, 0.95, lit ? '#fff7c8' : '#8a4a2a', 0.3);
    }
    // the title, every letter bouncing
    const title = 'Вгадай що?!', size = fitFont(title, 19, w - 16, BUBBLE, 400);
    X.font = '400 ' + size + 'px ' + BUBBLE;
    const tw = X.measureText(title).width;
    let x = -tw / 2;
    const cols = ['#ffe23a', '#ff9ec0', '#7fd3ff', '#b8e07a', '#ffb35a'];
    [...title].forEach((ch, i) => {
      const cw = X.measureText(ch).width;
      X.save(); X.translate(x + cw / 2, -1 + Math.sin(T * 3 + i * 0.7) * 1.1); X.rotate(Math.sin(T * 2 + i) * 0.06);
      text(ch, 0, 0, size, cols[i % 5], { font: BUBBLE, w: 400, ink: 2.2 });
      X.restore();
      x += cw;
    });
    // the plaque underneath: season and episode
    const ep = S.code ? epLong(S.code) : 'ТЕЛЕШОУ';
    A.box(-44, h / 2 - 2, 88, 10, '#ffe23a', 0.8, 6, 2.5);
    text(ep, 0, h / 2 + 3.1, fitFont(ep, 5.6, 80), INK);
  });
}
// the floor: LED tiles that light up in patterns
function floor(C) {
  const s = S.s, TS = 22, x0 = -264, nx = 24, zs = [LIPZ, 56, 78, 100, 122, 144, 166, 188, 212, WALLZ];
  quad([[-420, 0, LIPZ], [420, 0, LIPZ], [420, 0, WALLZ], [-420, 0, WALLZ]], '#160c2a');
  const bz = s && s.phase === 'judge' && podOf(C, s.buzzer), win = s && s.phase === 'reveal' && s.winner && podOf(C, s.winner);
  for (let j = 0; j < zs.length - 1; j++) {
    for (let i = 0; i < nx; i++) {
      const xa = x0 + i * TS + 0.9, xb = xa + TS - 1.8, za = zs[j] + 0.9, zb = zs[j + 1] - 0.9, cx = (xa + xb) / 2, cz = (za + zb) / 2;
      let col = (i + j) % 2 ? [46, 26, 82] : [37, 20, 63], L = 0, lc = [255, 214, 120];
      if (win) { const d = Math.hypot(cx - win.x, cz - win.z); L = 0.5 + 0.5 * Math.sin(d * 0.09 - T * 7); const c2 = rgbOf(shade2(hsl(d * 2 - T * 220, 90, 60))); lc = c2; }
      else if (bz) { const d = Math.hypot(cx - bz.x, cz - bz.z); L = Math.max(0, Math.sin(d * 0.12 - T * 6)) ** 3; lc = rgbOf(tagOf(s.buzzer)); }
      else if (T < FX.redFlash) { L = 0.7; lc = [255, 60, 50]; }
      else if (s && s.phase === 'guess') { L = A.hash(i * 31 + j * 7 + (T * 2.5 | 0) * 101) < 0.07 ? 0.8 : 0; lc = [140, 220, 255]; }
      else { const w = Math.sin(cx * 0.03 + cz * 0.025 - T * 1.4); L = w > 0.82 ? (w - 0.82) * 4 : 0; }
      const m = [0, 1, 2].map(q => Math.round(col[q] + (lc[q] - col[q]) * L * 0.75));
      quad([[xa, 0, za], [xb, 0, za], [xb, 0, zb], [xa, 0, zb]], 'rgb(' + m.join(',') + ')');
    }
  }
  // the edge of the stage with footlights
  const fr = frustumAt(LIPZ);
  quad([[-420, 0, LIPZ], [420, 0, LIPZ], [420, -16, LIPZ], [-420, -16, LIPZ]], '#0e0618');
  quad([[-420, 0, LIPZ], [420, 0, LIPZ], [420, -1.4, LIPZ], [-420, -1.4, LIPZ]], '#d4a020');
  for (let x = -Math.ceil(fr.halfW / 14) * 14 - 14; x < fr.halfW + 14; x += 14) {
    billboard(x, -4.5, LIPZ - 0.5, () => {
      const on = s && s.phase === 'reveal' ? ((x / 14 + (T * 10 | 0)) % 2 === 0) : true;
      A.glow(0, 0, 7, '#ffe9a0', on ? 0.5 : 0.15);
      A.ell(0, 0, 2.1, 1.5, 0, on ? '#fff6c0' : '#8a6a3a', 0.4);
    });
  }
}
function shade2(h) { // hsl string → hex (for the rainbow floor)
  const m = /hsla?\(([\d.]+),([\d.]+)%,([\d.]+)%/.exec(h); if (!m) return '#ffffff';
  const H = +m[1] / 360, Sx = +m[2] / 100, Lx = +m[3] / 100;
  const f = n => { const k = (n + H * 12) % 12, a = Sx * Math.min(Lx, 1 - Lx); return Math.round(255 * (Lx - a * Math.max(-1, Math.min(k - 3, 9 - k, 1)))); };
  return '#' + [f(0), f(8), f(4)].map(v => v.toString(16).padStart(2, '0')).join('');
}
// spotlights from above, following the action
function spots(C, dt) {
  const s = S.s, k = 1 - Math.exp(-dt * 2.2);
  const pod = n => podOf(C, n);
  let targets;
  if (!s || s.paused) targets = [[-30, 120], [30, 120], [0, 140]];
  else if (VOICE.talking) targets = [[C.host.x, C.host.z], [C.host.x, C.host.z], [0, 130]];
  else if (s.phase === 'ask' && pod(s.host)) { const p = pod(s.host); targets = [[p.x, p.z], [p.x, p.z], [C.host.x, C.host.z]]; }
  else if (s.phase === 'judge' && pod(s.buzzer)) { const p = pod(s.buzzer); targets = [[p.x, p.z], [p.x, p.z], [p.x, p.z]]; }
  else if (s.phase === 'reveal' && s.winner && pod(s.winner)) { const p = pod(s.winner); targets = [[p.x + Math.sin(T * 3) * 20, p.z], [p.x - Math.sin(T * 3) * 20, p.z], [p.x, p.z]]; }
  else targets = [[Math.sin(T * 0.7) * 90, 100 + Math.sin(T * 0.9) * 20], [Math.sin(T * 0.6 + 2) * 90, 100 + Math.cos(T * 0.8) * 20], [Math.sin(T * 0.45 + 4) * 70, 120]];
  X.save(); X.globalCompositeOperation = 'lighter';
  SPOTS.forEach((sp, i) => {
    sp.tx = lerp(sp.tx, targets[i][0], k); sp.tz = lerp(sp.tz, targets[i][1], k);
    const col = s && s.phase === 'reveal' && s.winner ? hsl(T * 120 + i * 120, 95, 65) : (T < FX.redFlash ? 'rgb(255,80,60)' : 'rgb(255,240,200)');
    const src = P(sp.sx, sp.sy, sp.sz), R = 22, pts = [];
    for (let a = 0; a < 16; a++) pts.push(P(sp.tx + Math.cos(a / 16 * Math.PI * 2) * R, 0.2, sp.tz + Math.sin(a / 16 * Math.PI * 2) * R));
    let lo = pts[0], hi = pts[0];
    for (const p of pts) { if (p[0] < lo[0]) lo = p; if (p[0] > hi[0]) hi = p; }
    const ctr = P(sp.tx, 0.2, sp.tz);
    idT();
    const g = X.createLinearGradient(src[0], src[1], ctr[0], ctr[1]);
    g.addColorStop(0, withA(col, 0));
    g.addColorStop(1, withA(col, 0.16));
    X.beginPath(); X.moveTo(src[0], src[1]); X.lineTo(lo[0], lo[1]); X.lineTo(hi[0], hi[1]); X.closePath(); X.fillStyle = g; X.fill();
    // the pool of light on the floor
    X.beginPath(); pts.forEach((p, j) => j ? X.lineTo(p[0], p[1]) : X.moveTo(p[0], p[1])); X.closePath();
    const rg = X.createRadialGradient(ctr[0], ctr[1], 0, ctr[0], ctr[1], Math.max(4, hi[0] - lo[0]) / 2);
    rg.addColorStop(0, withA(col, 0.38)); rg.addColorStop(1, withA(col, 0));
    X.fillStyle = rg; X.fill();
  });
  X.restore();
}
function withA(col, a) {
  if (col.startsWith('hsla')) return col.replace(/,[\d.]+\)$/, ',' + a + ')');
  if (col.startsWith('hsl(')) return col.replace('hsl(', 'hsla(').replace(')', ',' + a + ')');
  return col.replace('rgb(', 'rgba(').replace(')', ',' + a + ')');
}

// one contestant: the person behind a desk with their name and score
function podium(C, p) {
  const s = S.s, name = p.name, tag = tagOf(name), on = !s || s.online.includes(name), host = s && s.host === name;
  const { hw, d, h } = POD, x0 = p.x - hw, x1 = p.x + hw, zf = p.z - d, zb = p.z + d, f = fxOf(name);
  // light on the floor under the desk
  const fp = P(p.x, 0.3, p.z);
  idT(); X.save(); X.globalCompositeOperation = 'lighter';
  const rg = X.createRadialGradient(fp[0], fp[1], 0, fp[0], fp[1], 30 * fp[2]);
  rg.addColorStop(0, withA(hexRgb(tag), on ? 0.35 : 0.08)); rg.addColorStop(1, withA(hexRgb(tag), 0));
  X.fillStyle = rg; X.fillRect(fp[0] - 30 * fp[2], fp[1] - 12 * fp[2], 60 * fp[2], 24 * fp[2]); X.restore();
  // the BZZ! star behind the head
  if (T < f.bzz) billboard(p.x, RISER + 26, zb + 6, () => {
    const k = (f.bzz - T) / 1.3;
    X.save(); X.rotate(T * 3); X.scale(0.8 + (1 - k) * 0.6, 0.8 + (1 - k) * 0.6);
    const pts = []; for (let i = 0; i < 16; i++) { const r = i % 2 ? 9 : 19; pts.push([Math.cos(i / 16 * Math.PI * 2) * r, Math.sin(i / 16 * Math.PI * 2) * r]); }
    X.globalAlpha = Math.min(1, k * 2); A.poly(pts, '#ffe23a', 0.9, 3, 0.4); X.globalAlpha = 1; X.restore();
  });
  // the person
  const dir = host ? (p.x < 0 ? 1 : -1) : (s && podOf(C, s.host) ? (podOf(C, s.host).x >= p.x ? 1 : -1) : (p.x <= 0 ? 1 : -1));
  let anim = '', laugh = 0, blush = 0, talk = false, jump = 0;
  if (T < f.win) { anim = (T % 1.2) < 0.7 ? 'heart' : 'wave'; laugh = 2; }
  else if (T < f.clap || T < f.wave) anim = 'wave';
  else if (T < f.love) { anim = 'heart'; blush = 1; }
  if (T < f.giggle) laugh = 2;
  if (s && s.phase === 'judge' && s.buzzer === name) talk = (T * 7 | 0) % 2 === 0 && (T % 2) < 0.8;
  if (s && s.phase === 'ask' && host && !s.paused) talk = (T % 3) < 0.4 ? false : false;
  if (f.jump && T - f.jump < 0.6) jump = Math.abs(Math.sin((T - f.jump) * 10)) * 5 * (1 - (T - f.jump) / 0.6);
  billboard(p.x, RISER + jump, zb + 3, () => {
    A.friend(keyOf(name), 0, 0, { t: T + p.x * 0.01, dir, anim, laugh, blush, talk, alpha: on ? 1 : 0.3 });
  });
  // the desk: side, front, top
  const side = shade(tag, -0.38), top = shade(tag, 0.35), lw = 0.9 * P(p.x, h, zf)[2];
  if (CAM.x < x0) quad([[x0, 0, zf], [x0, 0, zb], [x0, h, zb], [x0, h, zf]], side, lw);
  if (CAM.x > x1) quad([[x1, 0, zf], [x1, h, zf], [x1, h, zb], [x1, 0, zb]], side, lw);
  quad([[x0, 0, zf], [x1, 0, zf], [x1, h, zf], [x0, h, zf]], tag, lw);
  const o = 1.6;
  quad([[x0 - o, h, zf - o], [x1 + o, h, zf - o], [x1 + o, h + 2.6, zf - o], [x0 - o, h + 2.6, zf - o]], shade(tag, -0.2), lw);
  quad([[x0 - o, h + 2.6, zf - o], [x1 + o, h + 2.6, zf - o], [x1 + o, h + 2.6, zb + o], [x0 - o, h + 2.6, zb + o]], top, lw);
  // the front: name plate, score, bulbs
  plane([p.x, h / 2, zf - 0.05], [1, 0, 0], [0, -1, 0], () => {
    X.fillStyle = 'rgba(255,255,255,.18)'; X.fillRect(-hw + 1, -h / 2 + 1, 3, h - 2);
    A.box(-13.5, -8.6, 27, 6.4, host ? '#ffd34a' : '#fbf4e2', 0.55, 7, 1.6);
    text(name, 0, -5.3, fitFont(name, 4.6, 24), INK);
    A.box(-8.5, -1.4, 17, 8.6, '#120a1e', 0.6, 8, 1.6);
    const sc = String(s ? (s.scores[name] || 0) : 0), flash = f.plus && T - f.plus < 1.4;
    X.shadowColor = tag; X.shadowBlur = 4 * P(p.x, h / 2, zf)[2];
    text(sc, 0, 3.1, 9.5, flash && (T * 8 | 0) % 2 ? '#ffffff' : tag, { font: 'VT323, monospace', w: 400 });
    X.shadowBlur = 0;
    for (let i = -3; i <= 3; i++) {
      const lit = T < f.win ? ((i + (T * 12 | 0)) % 2 === 0) : (((i + 3) + (T * 3 | 0)) % 7 === 0);
      if (lit) A.glow(i * 4, 8.4, 2.6, '#fff1b0', 0.7);
      A.circle(i * 4, 8.4, 0.8, lit ? '#fff7c8' : shade(tag, -0.45), 0.25);
    }
    if (!on) { X.fillStyle = 'rgba(20,10,30,.45)'; X.fillRect(-hw, -h / 2, hw * 2, h); }
  });
  // on the desk: the lamp that lights when you pressed, or the microphone of the one who asks
  billboard(p.x + 11, h + 2.6, p.z - 1, () => {
    if (host) {
      A.line(0, 0, 0.8, -6, '#3a3a40', 0.9); A.circle(1, -7, 1.8, '#4a4a52', 0.5); A.circle(0.6, -7.5, 0.6, '#9a9aa8');
    } else {
      const lit = s && s.phase === 'judge' && s.buzzer === name;
      A.ell(0, 0, 3.6, 1.2, 0, '#24242a', 0.5);
      if (lit) A.glow(0, -2, 10, '#ff5a3a', 0.6);
      X.beginPath(); X.arc(0, -0.2, 2.8, Math.PI, 0); X.closePath(); X.fillStyle = lit && (T * 8 | 0) % 2 ? '#ff8a6a' : (lit ? '#ff3a2a' : '#8a1e1a'); X.fill(); X.lineWidth = 0.5; X.strokeStyle = INK; X.stroke();
      A.ell(-0.9, -1.4, 0.8, 0.5, -0.4, 'rgba(255,255,255,.6)');
    }
  });
  // sleeping when away
  if (!on) billboard(p.x + 6, RISER + 40, zb + 3, () => {
    for (let i = 0; i < 3; i++) { const k = (T * 0.5 + i / 3) % 1; X.globalAlpha = 1 - k; text('z', k * 6, -k * 12, 4 + k * 3, '#e8e0ff', { ink: 0.5 }); }
    X.globalAlpha = 1;
  });
}
function hexRgb(hex) { const [r, g, b] = rgbOf(hex); return 'rgb(' + r + ',' + g + ',' + b + ')'; }

// the show host: walks a little, lifts the microphone and talks when reading a question
const HOSTW = { x: 0, dir: 1, tx: 0, wait: 0, phase: 0 };
function hostNpc(C) {
  const s = S.s, dt = 1 / 60;
  if (VOICE.talking || (s && s.phase === 'judge')) HOSTW.tx = 0;
  else if (T > HOSTW.wait) { HOSTW.tx = (Math.random() - 0.5) * 16; HOSTW.wait = T + 3 + Math.random() * 4; }
  const dx = HOSTW.tx - HOSTW.x, moving = Math.abs(dx) > 0.4;
  if (moving) { HOSTW.x += Math.sign(dx) * 12 * dt; HOSTW.dir = Math.sign(dx); HOSTW.phase += dt * 1.8; }
  else HOSTW.dir = 1;
  const talking = VOICE.talking, x = C.host.x + HOSTW.x, z = C.host.z;
  let anim = '', laugh = 0;
  const win = s && s.phase === 'reveal' && s.winner;
  if (talking) anim = 'wave';
  else if (win) { anim = (T % 1) < 0.5 ? 'wave' : 'heart'; laugh = 2; }
  else if (T < FX.redFlash + 1.2) laugh = 1;
  if (T < FX.sadT) laugh = 0;
  // a soft spotlight pool under his feet
  const fp = P(x, 0.3, z);
  idT(); X.save(); X.globalCompositeOperation = 'lighter';
  const rg = X.createRadialGradient(fp[0], fp[1], 0, fp[0], fp[1], 20 * fp[2]);
  rg.addColorStop(0, 'rgba(255,220,130,.35)'); rg.addColorStop(1, 'rgba(255,220,130,0)');
  X.fillStyle = rg; X.fillRect(fp[0] - 20 * fp[2], fp[1] - 8 * fp[2], 40 * fp[2], 16 * fp[2]); X.restore();
  billboard(x, 0, z, () => {
    const dir = moving ? HOSTW.dir : 1;
    A.friend('__HOST', 0, 0, { t: T, dir, moving, phase: HOSTW.phase, anim, laugh, talk: talking && (T * 9 | 0) % 2 === 0 });
    X.save(); X.scale(dir, 1);
    // bow tie and gold sparkles on the jacket
    A.shape([[0.2, -17.6], [3.6, -18.6], [3.6, -16.2]], '#d8282a', 0.4, 1, 0.05); A.shape([[3.2, -17.4], [6.4, -18.6], [6.4, -16.2]], '#d8282a', 0.4, 2, 0.05);
    for (let i = 0; i < 3; i++) { const tw = Math.sin(T * 4 + i * 2); if (tw > 0.3) A.sparkle(-2 + i * 2.6, -14 + (i % 2) * 4, 0.5 + tw * 0.5, '#fff6c0'); }
    // the microphone in his hand
    if (anim === 'wave' || anim === 'heart') { A.line(7.2, -24.2, 6.2, -20.4, '#2a2a30', 1); A.circle(6, -20, 1.3, '#5a5a64', 0.4); }
    else { A.line(4.6, -9.6, 6.6, -12.6, '#2a2a30', 1); A.circle(6.9, -13, 1.3, '#5a5a64', 0.4); }
    X.restore();
  });
}

// speech and thought bubbles above heads
function bubbles(C) {
  const s = S.s; if (!s || s.paused) return;
  const at = (name, txt, o) => { const p = podOf(C, name); if (p) bubble(p.x, RISER + 45, p.z + POD.d + 3, txt, o); };
  if (s.phase === 'ask') at(s.host, '…', { think: true, dots: true });
  else if (s.phase === 'guess') at(s.host, '?', { big: true });
  else if (s.phase === 'judge') { at(s.buzzer, s.answer, { col: tagOf(s.buzzer) }); at(s.host, '🤔', { think: true }); }
  else if (s.phase === 'reveal') {
    if (s.winner) at(s.winner, s.reveal, { col: '#5af07a', bg: '#d8ffd0' });
    else if (s.reveal) at(s.host, s.reveal, { bg: '#fff1b8' });
  }
  // the host says the question while reading it
  if (VOICE.talking) bubble(C.host.x + HOSTW.x + 6, 44, C.host.z, '🎤 …', { small: true });
}
function bubble(x, y, z, txt, o) {
  o = o || {};
  billboard(x, y + Math.sin(T * 2.4 + x) * 0.8, z, () => {
    const size = o.big ? 11 : o.small ? 4.6 : 6.4, font = o.big ? BUBBLE : ROUND;
    let t = txt;
    if (o.dots) t = '.'.repeat(1 + (T * 2.5 | 0) % 3);
    X.font = (o.big ? 400 : 900) + ' ' + size + 'px ' + font;
    const w = Math.max(o.big ? 12 : 10, X.measureText(o.dots ? '...' : t).width + 7), h = size + 5.5;
    if (o.think) {
      A.puffs([[-w / 2 + 3, 0, h / 2], [0, -1.5, h / 2 + 1], [w / 2 - 3, 0, h / 2], [-w / 4, 2, h / 2], [w / 4, 2, h / 2]], '#ffffff', 0.7);
      A.circle(-1, h / 2 + 3.4, 1.4, '#ffffff', 0.6); A.circle(-2.4, h / 2 + 6.4, 0.9, '#ffffff', 0.5);
    } else {
      A.box(-w / 2, -h / 2, w, h, o.bg || '#ffffff', 0.8, 9, 3.4);
      A.shape([[-2.6, h / 2 - 0.6], [2.6, h / 2 - 0.6], [-1.6, h / 2 + 4.6]], o.bg || '#ffffff', 0, 2, 0.05);
      A.line(-2.4, h / 2 - 0.1, -1.6, h / 2 + 4.6, INK, 0.8); A.line(-1.6, h / 2 + 4.6, 2.4, h / 2 - 0.1, INK, 0.8);
    }
    text(t, 0, o.big ? 0.6 : 0.3, size, o.col ? deep(o.col) : INK, { font, w: o.big ? 400 : 900 });
  });
}

// confetti, rain clouds, +1, floating emoji, question marks
function particles(C, dt) {
  const s = S.s;
  // confetti falls through the room (in 3D)
  for (let i = FX.confetti.length - 1; i >= 0; i--) {
    const c = FX.confetti[i];
    c.vy -= 22 * dt; c.vy *= 0.985; c.vx *= 0.99; c.x += (c.vx + Math.sin(T * 3 + i) * 6) * dt; c.y += c.vy * dt; c.z += c.vz * dt; c.r += c.vr * dt; c.life -= dt;
    if (c.life <= 0 || c.y < -2) { FX.confetti.splice(i, 1); continue; }
    billboard(c.x, c.y, c.z, () => { X.rotate(c.r); X.scale(1, Math.cos(c.r * 1.7)); X.fillStyle = c.c; X.fillRect(-c.w / 2, -0.7, c.w, 1.4); });
  }
  for (const p of C.pods) {
    const f = fxOf(p.name), hy = RISER + 34, z = p.z + POD.d + 3;
    // a little rain cloud after a wrong answer
    if (T < f.rain) billboard(p.x, hy + 12 + Math.sin(T * 3) * 0.6, z - 1, () => {
      const k = Math.min(1, (f.rain - T) * 1.5);
      X.globalAlpha = k;
      A.puffs([[-6, 0, 4.5], [0, -2, 5.5], [6, 0, 4.5], [-3, 2, 4], [3, 2, 4]], '#9aa0b8', 0.7, INK, 'rgba(90,96,120,.35)');
      for (let i = 0; i < 5; i++) { const q = (T * 2.2 + i / 5) % 1; A.ell(-6 + i * 3, 5 + q * 12, 0.5, 1.1, 0, '#7fcfff', 0.25); }
      if ((T * 3 | 0) % 4 === 0) A.poly([[1, 4], [-1, 8], [1, 8], [-1, 12]], '#ffe23a', 0.3, 1, 0);
      X.globalAlpha = 1;
    });
    // +1 rising from the score
    if (f.plus && T - f.plus < 1.8) billboard(p.x, POD.h + 6 + (T - f.plus) * 14, p.z - POD.d - 2, () => {
      const k = (T - f.plus) / 1.8;
      X.globalAlpha = 1 - k * k; X.scale(1 + k * 0.6, 1 + k * 0.6);
      text('+1', 0, 0, 12, '#ffe23a', { font: BUBBLE, w: 400, ink: 1.6 }); X.globalAlpha = 1;
    });
    if (T < f.win) for (let i = 0; i < 6; i++) {
      const a = i / 6 * Math.PI * 2 + T * 2.4, r = 16 + Math.sin(T * 5 + i) * 2;
      billboard(p.x + Math.cos(a) * r, hy + Math.sin(a) * r * 0.6, z - 2, () => A.sparkle(0, 0, 1.6 + Math.sin(T * 9 + i) * 0.6, i % 2 ? '#ffe070' : '#ffffff'));
    }
  }
  // emoji reactions float up from the person who sent them
  for (let i = FX.floats.length - 1; i >= 0; i--) {
    const e = FX.floats[i], k = (T - e.t) / 2.2;
    if (k > 1) { FX.floats.splice(i, 1); continue; }
    const p = podOf(C, e.by); if (!p) continue;
    billboard(p.x + e.dx + Math.sin(k * 9) * 2, RISER + 36 + k * 30, p.z + POD.d + 2, () => {
      X.globalAlpha = 1 - k * k; X.font = '10px sans-serif'; X.textAlign = 'center'; X.textBaseline = 'middle';
      X.fillText(e.e, 0, 0); X.globalAlpha = 1;
    });
  }
  // question marks drifting up while a question is new
  if (s && (s.phase === 'ask' || s.phase === 'guess')) for (let i = 0; i < 9; i++) {
    const k = (T * 0.12 + i / 9) % 1;
    billboard((A.hash(i * 3 + 1) - 0.5) * 240 + Math.sin(T + i) * 6, 6 + k * 150, 150 + A.hash(i * 3 + 2) * 70, () => {
      X.globalAlpha = Math.sin(k * Math.PI) * (T < FX.qmarks ? 0.75 : 0.22);
      text('?', 0, 0, 8 + A.hash(i) * 8, ['#ffe23a', '#ff9ec0', '#7fd3ff', '#b8e07a'][i % 4], { font: BUBBLE, w: 400 });
      X.globalAlpha = 1;
    });
  }
}

// the red curtains at the front of the stage (closed while waiting for players)
let CUR = 1;
function curtains(C, dt) {
  const s = S.s, want = !s || s.paused ? 1 : 0;
  CUR = lerp(CUR, want, 1 - Math.exp(-dt * (want ? 1.6 : 1.2)));
  const fr = frustumAt(CURZ), hw = fr.halfW + 30, top = fr.top + 14, bot = -20;
  const openEdge = fr.halfW + 7, edge = lerp(openEdge, -1, CUR);
  for (const side of [-1, 1]) {
    const xOut = side * hw, xIn = side * edge, w = Math.abs(xOut - xIn);
    plane([side * (edge + w / 2), (top + bot) / 2, CURZ], [side, 0, 0], [0, -1, 0], () => {
      // local: u from -w/2 (inner edge) to w/2 (outer), v from -H/2 (top) to H/2
      const H = top - bot, tie = (1 - CUR) * 0.4, tieV = H / 2 - (40 - bot);
      const edgeAt = v => { const k = Math.exp(-Math.pow((v - tieV) / 16, 2)); return -w / 2 + tie * w * 0.9 * k + (v > tieV ? (1 - CUR) * (v - tieV) * 0.08 : 0); };
      X.beginPath(); X.moveTo(w / 2 + 4, -H / 2 - 2);
      for (let v = -H / 2 - 2; v <= H / 2 + 2; v += 4) X.lineTo(edgeAt(v), v);
      X.lineTo(w / 2 + 4, H / 2 + 2); X.closePath();
      const g = X.createLinearGradient(-w / 2, 0, w / 2, 0);
      g.addColorStop(0, '#c4202e'); g.addColorStop(1, '#7a0e1a');
      X.fillStyle = g; X.fill(); X.lineWidth = 0.9; X.strokeStyle = INK; X.stroke();
      // folds
      X.save(); X.clip();
      for (let i = 1; i < 14; i++) {
        const u = i / 14;
        X.beginPath();
        for (let v = -H / 2; v <= H / 2; v += 4) { const e = edgeAt(v), x = e + (w / 2 + 4 - e) * u + Math.sin(v * 0.05 + i + T * 0.6) * 0.6; v === -H / 2 ? X.moveTo(x, v) : X.lineTo(x, v); }
        X.strokeStyle = i % 2 ? 'rgba(40,0,8,.35)' : 'rgba(255,150,150,.18)'; X.lineWidth = i % 2 ? 2.2 : 1.2; X.stroke();
      }
      X.restore();
      // the golden rope when the curtain is tied back
      if (CUR < 0.6) { const ex = edgeAt(tieV); A.ell(ex + 3, tieV, 4, 1.6, 0.3, '#e0b030', 0.6); A.circle(ex + 6, tieV + 3, 1.4, '#e0b030', 0.5); }
    });
  }
  // the valance across the top, with a golden fringe
  plane([0, top - 8, CURZ - 1], [1, 0, 0], [0, -1, 0], () => {
    const w = hw * 2, n = Math.ceil(w / 22);
    X.beginPath(); X.moveTo(-w / 2, -14);
    for (let i = 0; i < n; i++) { const xa = -w / 2 + i * w / n, xb = xa + w / n; X.lineTo(xa, 4); X.quadraticCurveTo((xa + xb) / 2, 12, xb, 4); }
    X.lineTo(w / 2, -14); X.closePath();
    X.fillStyle = '#a8182a'; X.fill(); X.lineWidth = 0.9; X.strokeStyle = INK; X.stroke();
    for (let i = 0; i < n; i++) { const xa = -w / 2 + i * w / n, xb = xa + w / n; X.beginPath(); X.moveTo(xa, 4); X.quadraticCurveTo((xa + xb) / 2, 12, xb, 4); X.strokeStyle = '#e8b830'; X.lineWidth = 1.6; X.stroke(); }
  });
  // while waiting: a sign on the closed curtains
  if (CUR > 0.75) {
    const a = (CUR - 0.75) * 4, yc = (fr.top + fr.bottom) / 2 + 4;
    plane([0, yc, CURZ - 2], [1, 0, 0], [0, -1, 0], () => {
      X.globalAlpha = a;
      A.line(-20, -40, -16, -15, '#d8b070', 0.6); A.line(20, -40, 16, -15, '#d8b070', 0.6);
      X.save(); X.rotate(Math.sin(T * 1.1) * 0.03);
      A.box(-36, -16, 72, 38, '#fbf4e2', 1, 5, 4);
      text('ЧЕКАЄМО', 0, -8, 10, '#cf3a2a', { font: BUBBLE, w: 400, ink: 1.2 });
      text('НА ГРАВЦІВ', 0, 1, 5.6, INK);
      text(S.code ? epName(S.code) : '…', 0, 9.6, 8, '#3a1d6e', { font: 'VT323, monospace', w: 400 });
      text(S.code ? 'КОД ' + S.code + ' · ' + (S.s ? S.s.online.length : 0) + ' в студії' : '', 0, 16.4, 4, '#8a6a50');
      X.restore();
      X.globalAlpha = 1;
    });
  }
}
// the audience in front of the camera (in the dark), cheering after a right answer
function audience() {
  const n = Math.ceil(VW / (VH * 0.07)) + 2, r = VH * 0.042, cheer = T < FX.applause, par = (CAM.x - BASE.x) * VH * 0.004;
  idT(); X.fillStyle = '#0c0616';
  for (let i = 0; i < n; i++) {
    const x = (i - 1) * VH * 0.07 + A.hash(i) * VH * 0.025 - par, bob = cheer ? Math.abs(Math.sin(T * 9 + i)) * r * 0.5 : Math.sin(T * 1.3 + i) * r * 0.05;
    const y = VH + r * 0.55 - bob - A.hash(i + 9) * r * 0.25;
    X.beginPath(); X.ellipse(x, y, r * 0.78, r, 0, 0, Math.PI * 2); X.fill();
    X.beginPath(); X.ellipse(x, y + r * 1.4, r * 1.4, r * 0.9, 0, 0, Math.PI * 2); X.fill();
    if (cheer && i % 2) {
      const up = Math.sin(T * 12 + i) * r * 0.3;
      X.beginPath(); X.ellipse(x - r * 0.9, y - r * 1.3 + up, r * 0.22, r * 0.5, -0.3, 0, Math.PI * 2); X.fill();
      X.beginPath(); X.ellipse(x + r * 0.9, y - r * 1.3 - up, r * 0.22, r * 0.5, 0.3, 0, Math.PI * 2); X.fill();
    }
  }
}
// the TV picture: "on air" and the channel logo in the corners
function overlay() {
  const s = S.s, u = VH / 100, fs = Math.max(9 * DPR, u * 4.2);
  idT();
  X.font = '900 ' + fs + 'px ' + ROUND; X.textBaseline = 'middle'; X.textAlign = 'left';
  const cheer = T < FX.applause;
  const lab = cheer ? '👏 ОПЛЕСКИ!' : 'В ЕФІРІ';
  const tw = X.measureText(lab).width, x = u * 3.2 + 6 * DPR, y = u * 3.2 + 4 * DPR, h = fs * 1.5;
  X.fillStyle = cheer ? ((T * 6 | 0) % 2 ? '#ffe23a' : '#ff9a3a') : 'rgba(12,6,22,.7)';
  rr(x, y, tw + fs * 1.9, h, h / 2); X.fill();
  X.fillStyle = cheer ? INK : '#ffffff'; X.fillText(lab, x + fs * 1.35, y + h / 2);
  if (!cheer) { X.fillStyle = (T % 1.2) < 0.8 ? '#ff2a2a' : '#6a1010'; X.beginPath(); X.arc(x + fs * 0.75, y + h / 2, fs * 0.33, 0, Math.PI * 2); X.fill(); }
  if (S.code) {
    const bug = epName(S.code).replace(' · ', ' ');
    X.font = '400 ' + fs * 1.15 + 'px VT323, monospace'; X.textAlign = 'right';
    X.fillStyle = 'rgba(255,255,255,.75)'; X.fillText(bug, VW - x, y + h / 2);
  }
  X.textAlign = 'left';
}
function rr(x, y, w, h, r) { X.beginPath(); X.moveTo(x + r, y); X.arcTo(x + w, y, x + w, y + h, r); X.arcTo(x + w, y + h, x, y + h, r); X.arcTo(x, y + h, x, y, r); X.arcTo(x, y, x + w, y, r); X.closePath(); }

// =====================================================================
// the main loop
// =====================================================================
function frame(ts) {
  const dt = Math.min(0.05, Math.max(0.001, (ts - (lastTs || ts)) / 1000));
  lastTs = ts; T += dt;
  if (SCREEN === 'app') { resize(); drawStudio(dt); }
  else if (SCREEN === 'lobby') drawMe(T);
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);

// start: fonts first (the canvas needs them), then the login
const fontsReady = document.fonts && document.fonts.load ? Promise.all([
  document.fonts.load('400 20px "Rubik Bubbles"', 'Вгадай'), document.fonts.load('900 20px "M PLUS Rounded 1c"', 'Вгадай'), document.fonts.load('20px VT323', '0'),
]).catch(() => {}) : Promise.resolve();
showLogin();
fontsReady.then(() => {
  const ep = new URLSearchParams(location.search).get('ep');
  // opened from a notification: log straight in with the saved name
  if (ep && typed && SCREEN === 'login') setTimeout(() => { if (!loginBusy) tryLogin(); }, 500);
});
globalThis.__VG = { S, CAM, FX, send, T: () => T };
})();
