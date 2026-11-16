// Туса У Берліні — three friends, one favourite place in Berlin: Hoongy.
'use strict';
(() => {
const A = ART;
const $ = id => document.getElementById(id);
const view = $('view'), vctx = view.getContext('2d');
const low = document.createElement('canvas'), lctx = low.getContext('2d');
const stage = $('stage'), dlg = $('dlg');
let LW = 240, LH = 180, SCALE = 2, DPR = 1, CW = 480, CH = 360;

// =====================================================================
// words: Dutch and Ukrainian, mixed at random
// =====================================================================
const pick = a => a[Math.random() * a.length | 0];
const mix = (nl, uk) => (Math.random() < 0.5 ? nl : uk);
const JOKES = [
  'Ik heb een grap over de Deutsche Bahn... maar die komt later.',
  'Мій борщ сильніший за мій вайфай.',
  'Mijn dieet? Ik eet alleen op dagen die eindigen op -dag.',
  'Кава — це не напій. Це стратегія.',
  'Berlijn is zo cool, zelfs de duiven dragen een zonnebril.',
  'Я не лінивий. Я в режимі енергозбереження.',
  'Hoe noem je een slapende dinosaurus? Een dino-snurk!',
  'Мій будильник і я — у складних стосунках.',
  'De Mango-Lassi fluistert tegen mij: "drink mij"...',
  'Їжак пішов у спортзал. Тепер він їжачок-качок!',
  'Mijn Oekraïens is perfect: борщ, вареники, смачного!',
  'Ik zei: één visje graag. Hij bracht een vis... met de hele familie.',
  'Чому кіт сидить на ноутбуці? Стежить за мишкою!',
  'Ik ben niet te laat. Iedereen anders is gewoon te vroeg.',
  'Скільки треба друзів, щоб замовити піцу? Троє: один замовляє, двоє сперечаються.',
  'Waarom staat Oleksiy\'s snor nooit stil? Hij heeft een eigen wind.',
  'Віка каже, що не їсть після шостої. Тому вона їсть о 18:01.',
  'Vito lacht zo breed, de tram stopt om ruimte te maken.',
  'Мій улюблений спорт — сидіти в Hoongy. Олімпійський рівень!',
  'Weet je wat Berlijn en mijn koelkast gemeen hebben? Altijd open, altijd leeg om 3 uur \'s nachts.',
];
const LAUGH = [
  ['hehe', 'хі-хі', 'haha!', 'ги-ги'],
  ['HAHAHA!', 'ХА-ХА-ХА!', 'Ik lach me rot!', 'Ой, не можу!'],
  ['Ik lig dubbel!', 'Помираю від сміху!', 'Ik huil van het lachen!', 'Сльози з очей!'],
  ['*rolt over de vloer*', '*качається по підлозі*', 'HAHAHAHA STOP!', 'Я ЗАРАЗ ЛОПНУ!'],
];
const HUGS = ['Knuffel!', 'Обійми!', 'Aww!', 'Мур-мур!', 'Lekker knuffelen!', 'Іди сюди!'];
const THANKS = ['Dank je!', 'Дякую!', 'Ooh, lief!', 'Серденько!', 'Wat schattig!', 'Ой, як мило!'];
const HELLOS = ['Привіт, Берлін!', 'Hallo Berlijn!', 'Daar ben ik!', 'Я тут!', 'Gezellig!', 'Туса починається!'];
const CHILL = ['♪ ♫', '♪', '♫ ♪ ♫'];
const AIR = ['*knuffelt de lucht*', '*обіймає повітря*'];
const FOOD = {
  lassi: { ico: '🥭', nl: 'Mango-Lassi', uk: 'Манго-ласі', d: mix('exotische mango-cocktail', 'екзотичний манговий коктейль'), p: '5,90' },
  coffee: { ico: '☕', nl: 'Vietnamese koffie', uk: "В'єтнамська кава", d: mix('sterk en zoet, met gecondenseerde melk', 'міцна й солодка, зі згущенкою'), p: '4,50' },
  shrimp: { ico: '🍤', nl: 'Garnalen', uk: 'Креветки', d: mix('knapperig, met chili en limoen', 'хрусткі, з чилі та лаймом'), p: '12,90' },
  fish: { ico: '🐟', nl: 'Vis', uk: 'Риба', d: mix('hele vis met gember', 'ціла риба з імбиром'), p: '15,50' },
};
const NAMES = {
  VITO: ['VITO'],
  VIKA: ['VIKA', 'VICA', 'VICTORIA', 'VIKTORIA', 'VIKTORIIA', 'VOCTORIA', 'VIKTORIJA', 'VIKUSYA'],
  OLEKSIY: ['OLEKSIY', 'OLEKSII', 'OLEKSIJ', 'OLEKSI', 'ALEKSEY', 'OLEXIY', 'OLEKSIJ'],
};
const HOME = {
  VITO: { title: 'CAMP LION', sub: 'România · Румунія', draw: A.campLion, door: 75, exitDir: 1, W: 470, sign: 'CAMP LION' },
  VIKA: { title: 'ALTENGRABOW', sub: 'Sachsen-Anhalt · Німеччина', draw: A.altengrabow, door: 79, exitDir: 1, W: 420, sign: 'ALTENGRABOW' },
  OLEKSIY: { title: 'LEHNIN', sub: 'Brandenburg · Німеччина', draw: A.lehnin, door: 62, exitDir: 1, W: 400, sign: 'LEHNIN' },
};
const ARRIVE = { VITO: 'taxi', VIKA: 'amarok', OLEKSIY: 'scooter' };
const TAG = { VITO: '#7fd3ff', VIKA: '#ff9ec0', OLEKSIY: '#b8e07a' };

// =====================================================================
// settings shared with ERROR-DOS (sound on/off)
// =====================================================================
let SET = { sound: true };
try { Object.assign(SET, JSON.parse(localStorage.getItem('errordos-settings') || '{}')); } catch (e) {}

// =====================================================================
// sound: synthesized effects
// =====================================================================
let ac = null, master = null;
function audio() {
  if (!SET.sound) return null;
  if (!ac) {
    try { ac = new (window.AudioContext || window.webkitAudioContext)(); master = ac.createGain(); master.gain.value = 0.9; master.connect(ac.destination); } catch (e) { return null; }
  }
  if (ac.state === 'suspended') ac.resume();
  return ac;
}
function tone(f, d, v, type, when, f2) {
  const a = audio(); if (!a) return;
  const t = a.currentTime + (when || 0), o = a.createOscillator(), g = a.createGain();
  o.type = type || 'square'; o.frequency.setValueAtTime(f, t); if (f2) o.frequency.exponentialRampToValueAtTime(f2, t + d);
  g.gain.setValueAtTime(v, t); g.gain.exponentialRampToValueAtTime(0.0001, t + d);
  o.connect(g).connect(master); o.start(t); o.stop(t + d + 0.03);
}
let nbuf = null;
function noise(d, v, f, q, when) {
  const a = audio(); if (!a) return;
  if (!nbuf) { nbuf = a.createBuffer(1, a.sampleRate, a.sampleRate); const x = nbuf.getChannelData(0); for (let i = 0; i < x.length; i++) x[i] = Math.random() * 2 - 1; }
  const t = a.currentTime + (when || 0), s = a.createBufferSource(), bf = a.createBiquadFilter(), g = a.createGain();
  s.buffer = nbuf; bf.type = 'bandpass'; bf.frequency.value = f; bf.Q.value = q;
  g.gain.setValueAtTime(v, t); g.gain.exponentialRampToValueAtTime(0.0001, t + d);
  s.connect(bf).connect(g).connect(master); s.start(t, Math.random() * 0.5); s.stop(t + d + 0.03);
}
const SFX = {
  click() { noise(0.02, 0.2, 3600, 0.8); },
  joke() { tone(150, 0.12, 0.12, 'sine', 0, 90); tone(120, 0.14, 0.12, 'sine', 0.16, 70); noise(0.35, 0.1, 7000, 0.6, 0.34); },
  laugh(char) {
    const base = { VITO: 300, VIKA: 520, OLEKSIY: 230 }[char] || 400;
    for (let i = 0; i < 4; i++) tone(base * (1 + (i % 2) * 0.15), 0.08, 0.035, 'triangle', i * 0.12, base * 0.8);
  },
  heart() { tone(880, 0.09, 0.05, 'sine'); tone(1318, 0.16, 0.05, 'sine', 0.09); },
  hug() { tone(392, 0.3, 0.04, 'sine', 0, 523); tone(523, 0.35, 0.03, 'sine', 0.2, 440); },
  door() { tone(1568, 0.25, 0.04, 'sine'); tone(1175, 0.4, 0.04, 'sine', 0.18); },
  bell() { tone(1200, 0.5, 0.05, 'triangle'); tone(1200, 0.5, 0.05, 'triangle', 0.28); },
  car() { noise(0.9, 0.05, 240, 0.7); },
  plane() { noise(2.2, 0.06, 500, 0.5); },
  eat() { noise(0.05, 0.12, 1800, 1.2); noise(0.05, 0.1, 1400, 1.2, 0.09); },
  pop() { tone(660, 0.06, 0.05, 'square', 0, 990); },
  step() { noise(0.03, 0.03, 700, 1); },
};

// =====================================================================
// layout
// =====================================================================
const coarse = matchMedia('(pointer: coarse)').matches || 'ontouchstart' in window;
if (!coarse) { $('pad').classList.add('hidden'); $('keys').style.display = 'block'; }
function layout() {
  const sw = stage.clientWidth, sh = stage.clientHeight;
  if (sh > sw * 0.9) {
    // portrait phone: keep about 215 pixels across and show extra height above
    SCALE = sw / 215;
    LW = Math.round(sw / SCALE);
    LH = Math.max(180, Math.min(300, Math.floor(sh / SCALE)));
  } else {
    let ch = Math.min(sh, sw * 0.78);
    let cw = Math.min(sw, ch * 430 / 180);
    if (cw < ch * 1.2) ch = Math.min(sh, cw / 1.2);
    SCALE = ch / 180;
    LW = Math.max(180, Math.round(cw / SCALE)); LH = 180;
  }
  DPR = Math.min(2, window.devicePixelRatio || 1);
  CW = Math.round(LW * SCALE); CH = Math.round(LH * SCALE);
  view.style.width = CW + 'px'; view.style.height = CH + 'px';
  view.width = Math.round(CW * DPR); view.height = Math.round(CH * DPR);
  low.width = LW; low.height = LH;
}
addEventListener('resize', layout);

// =====================================================================
// game state
// =====================================================================
const G = {
  scene: 'login', me: null, t: 0,
  p: { x: 0, y: 150, dir: 1, phase: 0, moving: false, anim: '', animUntil: 0, seat: -1, angle: -Math.PI / 2, idle: 0 },
  order: null, menuShownFor: 0,
  camX: 0, camY: 0,
  base: null, travel: null, arrival: null, trans: null,
  npcs: [], cars: [], leaves: [], hearts: [], notes: [],
  bubbles: {}, jokes: {}, blush: {}, hugs: {},
  others: {},
  nextPed: 0, nextCar: 0, nextTram: 8, nextSoldier: 0,
  prompt: '', closedShown: false,
};

// ---------- network ----------
const NET = { ws: null, ok: false, offset: 0, g: { clockStart: 0, radio: { ch: 0, since: 0 } }, lastSent: 0, lastKey: '', want: false, retry: null };
const snow = () => Date.now() + NET.offset;
function connect(arrive, x) {
  NET.want = true;
  clearTimeout(NET.retry);
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  const ws = new WebSocket(proto + '//' + location.host + '/api/tusa?char=' + G.me + '&arrive=' + (arrive || '') + '&x=' + Math.round(x || G.p.x));
  NET.ws = ws;
  ws.onopen = () => { NET.ok = true; NET.ping = setInterval(() => ws.readyState === 1 && ws.send('{"t":"ping"}'), 25000); };
  ws.onmessage = ev => { let m; try { m = JSON.parse(ev.data); } catch (e) { return; } onNet(m); };
  ws.onclose = () => {
    clearInterval(NET.ping);
    if (NET.ws !== ws) return;
    NET.ok = false;
    if (NET.want && (G.scene === 'street' || G.scene === 'inside' || G.scene === 'enter')) NET.retry = setTimeout(() => connect('', G.p.x), 2500);
  };
}
function disconnect() {
  NET.want = false; NET.ok = false;
  if (NET.ws) { try { NET.ws.close(1000, 'bye'); } catch (e) {} }
  NET.ws = null; G.others = {};
  radioStop();
}
function send(obj) { if (NET.ok && NET.ws && NET.ws.readyState === 1) NET.ws.send(JSON.stringify(obj)); }
function sendState(force) {
  const now = performance.now();
  if (!force && now - NET.lastSent < 110) return;
  const s = { t: 'st', x: Math.round(G.p.x * 10) / 10, y: Math.round(G.p.y * 10) / 10, dir: G.p.dir, scene: G.scene === 'inside' ? 'inside' : 'street', anim: G.p.anim || (G.p.moving ? 'walk' : '') };
  const key = JSON.stringify(s);
  if (!force && key === NET.lastKey) return;
  NET.lastKey = key; NET.lastSent = now;
  send(s);
}
function onNet(m) {
  if (m.now) NET.offset = m.now - Date.now();
  if (m.type === 'welcome') {
    G.others = {};
    for (const p of m.players) addOther(p, true);
    NET.g = m.g;
    radioSync();
    sendState(true);
  } else if (m.type === 'p') {
    if (m.p.char === G.me) { G.p.seat = m.p.seat; G.order = m.p.order; return; }
    addOther(m.p, false);
  } else if (m.type === 'gone') {
    const o = G.others[m.char];
    if (o) { bubble(m.char, mix('Tot ziens!', 'Бувай!'), 2); delete G.others[m.char]; }
  } else if (m.type === 'emote') {
    handleEmote(m);
  } else if (m.type === 'g') {
    const was = NET.g.clockStart;
    NET.g = m.g;
    if (!was && m.g.clockStart) { bubble(G.me, mix('De klok tikt! 18:00', 'Годинник пішов! 18:00'), 2.5); SFX.bell(); }
    radioSync();
  } else if (m.type === 'taken') {
    bubble(G.me, mix('Bezet!', 'Зайнято!'), 1.5);
  } else if (m.type === 'kicked') {
    NET.want = false;
    showDialog({ title: 'ERROR', text: mix('Je bent ergens anders ingelogd.', 'Ти увійшов з іншого пристрою.'), opts: [{ ico: '↩', label: 'OK', act: () => location.reload() }] });
  }
}
function addOther(p, initial) {
  let o = G.others[p.char];
  const fresh = !o;
  if (!o) o = G.others[p.char] = { char: p.char, x: p.x, y: p.y, phase: 0 };
  Object.assign(o, { tx: p.x, ty: p.y, dir: p.dir, scene: p.scene, anim: p.anim, seat: p.seat, order: p.order, arrive: p.arrive, arriveAt: p.arriveAt });
  if (fresh) {
    o.x = p.x; o.y = p.y;
    if (!initial && p.scene === 'street') setTimeout(() => bubble(p.char, pick(HELLOS), 2.5), arrivingFor(o) ? 2600 : 200);
  }
}
function arrivingFor(o) { return o.arrive && snow() - o.arriveAt < 6500; }

// ---------- clock ----------
const CLOSE_MS = 60 * 60 * 1000; // 4 game hours at 4x
function clockInfo() {
  if (!NET.g.clockStart) return null;
  const ms = snow() - NET.g.clockStart;
  const gameMin = 18 * 60 + Math.floor(ms / 1000 * 4 / 60);
  return { closed: ms >= CLOSE_MS, h: Math.floor(Math.min(gameMin, 22 * 60) / 60), m: Math.min(gameMin, 22 * 60) % 60 };
}
function updateClock() {
  const el = $('clock');
  const at = G.scene === 'street' || G.scene === 'inside' || G.scene === 'enter';
  if (G.scene === 'login') { el.textContent = 'ТУСА.EXE'; return; }
  if (!at) { el.innerHTML = (HOME[G.me] ? HOME[G.me].title : 'TUSA') + ' <small>' + (G.scene === 'travel' ? '→ BERLIN' : '') + '</small>'; return; }
  const c = clockInfo();
  if (!c) G.closedShown = false;
  if (!c) { el.innerHTML = '18:00 <small>' + (Math.floor(G.t / 4) % 2 ? 'wachten op vrienden…' : 'чекаємо друзів…') + '</small>'; return; }
  const hh = String(c.h).padStart(2, '0') + ':' + String(c.m).padStart(2, '0');
  el.innerHTML = hh + (c.closed ? ' <small>' + mix('GESLOTEN', 'ЗАЧИНЕНО') + '</small>' : '');
}

// =====================================================================
// login (DOS screen with the ERROR keyboard)
// =====================================================================
const term = $('term');
let typed = '', loginLines = [], loginBusy = false;
function termRender() {
  term.innerHTML = loginLines.join('\n') + (loginBusy ? '' : '\nLOGIN: ' + esc(typed) + '<span class="cur"> </span>');
  term.scrollTop = term.scrollHeight;
}
function esc(s) { return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;'); }
function loginKey(k) {
  if (G.scene !== 'login' || loginBusy) return;
  unlock();
  if (k === 'DEL') typed = typed.slice(0, -1);
  else if (k === 'SEND') return tryLogin();
  else if (k.length >= 1 && typed.length < 16 && !/^(SHIFT|SYM|ABC)$/.test(k)) typed += k;
  termRender();
}
const kb = ErrorKbd.mount($('loginKbd'), { onKey: loginKey, sendLabel: 'ENTER', sound: false });
function whoIs(name) {
  const n = name.trim().toUpperCase().replace(/[^A-Z]/g, '');
  for (const k in NAMES) if (NAMES[k].includes(n)) return k;
  return null;
}
function tryLogin() {
  const who = whoIs(typed);
  loginLines.push('LOGIN: ' + esc(typed));
  if (!who) {
    loginLines.push('<span class="err">' + mix('TOEGANG GEWEIGERD', 'ДОСТУП ЗАБОРОНЕНО') + '. Bad command or file name.</span>');
    loginLines.push('<span class="dim">' + mix('Alleen voor', 'Тільки для') + ': VIKA · VITO · OLEKSIY</span>');
    typed = ''; SFX.pop(); termRender(); return;
  }
  try { localStorage.setItem('tusa-name', typed.trim()); } catch (e) {}
  loginBusy = true;
  loginLines.push('');
  loginLines.push(mix('Welkom', 'Ласкаво просимо') + ', ' + who + '!');
  termRender();
  let i = 0;
  const bar = () => {
    i++;
    loginLines[loginLines.length - 1] = 'LOADING TUSA.EXE [' + '#'.repeat(i) + '.'.repeat(Math.max(0, 16 - i)) + ']';
    termRender();
    if (i < 16) setTimeout(bar, 55); else setTimeout(() => startBase(who), 300);
  };
  loginLines.push('');
  setTimeout(bar, 300);
}
function showLogin() {
  G.scene = 'login';
  $('login').classList.add('show');
  try { typed = localStorage.getItem('tusa-name') || ''; } catch (e) { typed = ''; }
  loginLines = [
    '<span class="dim">ERROR-DOS 1.0 · C:\\ONLINE\\TUSA.EXE</span>',
    '',
    '<span class="big">Туса У Берліні</span>',
    '<span class="dim">(c) 2026 Hoongy &amp; vrienden · друзі</span>',
    '',
    mix('Drie vrienden. Eén café in Berlijn.', 'Троє друзів. Одне кафе в Берліні.'),
    mix('Wie ben jij?', 'Хто ти?'),
    '',
  ];
  loginBusy = false;
  termRender();
}

// =====================================================================
// scenes: base
// =====================================================================
function startBase(who) {
  G.me = who;
  $('login').classList.remove('show');
  G.scene = 'base';
  const H = HOME[who];
  G.base = { who, H, t0: G.t, out: false };
  G.p = { x: H.door, y: 152, dir: 1, phase: 0, moving: false, anim: '', animUntil: 0, seat: -1, angle: -Math.PI / 2, idle: 0 };
  G.npcs = [];
  for (let i = 0; i < 3; i++) spawnSoldier(true);
  G.camX = 0;
  closeDialog();
  radioStop();
}
function spawnSoldier(anywhere) {
  const B = G.base; if (!B) return;
  const dir = Math.random() < 0.5 ? 1 : -1, W = B.H.W;
  const x = anywhere ? 40 + Math.random() * (W - 80) : (dir > 0 ? -20 : W + 20);
  const camo = { VITO: ['#6b6a45', '#4a4a30'], VIKA: ['#6e7350', '#3f4a2c'], OLEKSIY: ['#5d6b44', '#3a462a'] }[B.who];
  G.npcs.push({ kind: 'soldier', x, y: 152 + Math.random() * 3, dir, speed: (Math.random() < 0.25 ? 34 : 15) + Math.random() * 6, phase: Math.random(), wait: 0,
    look: { skin: pick(['#e0b090', '#c8946a', '#f0c8a0', '#8a5a3a']), top: camo[0], camo: camo[1], legs: camo[0], shoe: '#2a2018', hat: pick(['helmet', 'beret', 'cap']), hatc: pick([camo[1], '#7a1f2b', '#3a462a']), seed: Math.random() * 20 | 0 } });
}
function updateBase(dt) {
  const B = G.base, H = B.H;
  if (G.t - B.t0 < 0.9) { G.p.x = H.door + (G.t - B.t0) * 18; G.p.moving = true; G.p.phase += dt * 2; G.p.dir = 1; return; }
  if (!B.out) { B.out = true; bubble(G.me, pick(CHILL) + ' ' + mix('Zo, vrij!', 'Нарешті вихідний!'), 2.5); }
  movePlayerSide(dt, 30, H.W + 30);
  if (G.p.x > H.W + 10 && !dialogOpen()) {
    if (G.me === 'VITO') {
      showDialog({
        title: mix('Vliegen naar Berlijn?', 'Летіти до Берліна?'),
        text: 'Бухарест (OTP) → Berlin (BER) · ✈ 2u05',
        opts: [
          { ico: '✈', label: mix('Vliegen!', 'Летимо!'), key: 'A', act: () => startTravel('VITO') },
          { ico: '↩', label: mix('Nog even chillen', 'Ще трохи почилити'), key: 'B', act: () => { G.p.x = H.W - 20; G.p.dir = -1; } },
        ],
      });
    } else startTravel(G.me);
  }
  // soldiers wander, sometimes salute
  G.nextSoldier -= dt;
  if (G.nextSoldier <= 0 && G.npcs.length < 5) { spawnSoldier(false); G.nextSoldier = 4 + Math.random() * 6; }
  for (const n of G.npcs) {
    if (n.wait > 0) { n.wait -= dt; n.look.salute = n.wait > 0.2 && Math.abs(n.x - G.p.x) < 26; continue; }
    n.x += n.dir * n.speed * dt; n.phase += dt * n.speed / 14;
    if (Math.random() < dt * 0.06) { n.wait = 1 + Math.random() * 2; }
  }
  G.npcs = G.npcs.filter(n => n.x > -60 && n.x < H.W + 60);
  G.camX = clamp(G.p.x - LW * 0.4, 0, Math.max(0, H.W + 30 - LW));
}
function drawBase() {
  const B = G.base, H = B.H;
  A.use(lctx);
  topFill({ VITO: '#5b8fd1', VIKA: '#6f9fd8', OLEKSIY: '#7aa6d6' }[B.who]);
  lctx.save(); lctx.translate(0, sideOY());
  const r = H.draw(LW, 180, G.camX, G.t);
  B.signX = r.signX; B.signY = r.signY; B.signW = r.signW; B.signH = r.signH;
  const list = G.npcs.map(n => ({ y: n.y, d: () => A.person(n.x - G.camX, n.y, n.look, n.phase, n.wait <= 0, n.dir, G.t) }));
  list.push({ y: G.p.y, d: () => drawMe(G.p.x - G.camX, G.p.y) });
  list.sort((a, b) => a.y - b.y).forEach(o => o.d());
  // door: open while stepping out
  if (G.t - B.t0 < 1.2 && G.me === 'VITO') A.R(H.door - 5 - G.camX, 133, 3, 16, '#9aa1a6');
  lctx.restore();
}

// =====================================================================
// scenes: the trip to Berlin
// =====================================================================
const TRIPS = {
  VITO: [
    { k: 'europe', d: 5, cap: () => 'Бухарест → Берлін · Boekarest → Berlijn' },
    { k: 'airport', d: 3.6, cap: () => 'BER · ' + mix('Welkom in Berlijn!', 'Вітаємо в Берліні!') },
    { k: 'city', d: 3, cap: () => 'TAXI → Hoongy' },
  ],
  VIKA: [
    { k: 'mapDrive', d: 5, cap: () => 'Altengrabow → Berlin · A2 · 🚙 Amarok' },
    { k: 'drive', d: 3.6, cap: () => mix('Nog 42 km!', 'Ще 42 км!') },
    { k: 'city', d: 2.6, cap: () => 'Berlin · Берлін' },
  ],
  OLEKSIY: [
    { k: 'mapTrain', d: 5, cap: () => 'Lehnin → Brück → Berlin Hbf · RE7' },
    { k: 'train', d: 3.6, cap: () => 'RE7 · ' + mix('Volgende halte: Berlin Hbf', 'Наступна зупинка: Berlin Hbf') },
    { k: 'hbf', d: 2.6, cap: () => 'Berlin Hbf · 🛴 → Hoongy' },
  ],
};
function startTravel(who) {
  closeDialog();
  G.scene = 'travel';
  G.travel = { steps: TRIPS[who], i: 0, t0: G.t };
  G.npcs = [];
  if (who === 'VITO') SFX.plane(); else SFX.car();
}
function updateTravel() {
  const T = G.travel, st = T.steps[T.i];
  if (G.t - T.t0 >= st.d) nextTravel();
}
function nextTravel() {
  const T = G.travel;
  T.i++; T.t0 = G.t;
  if (T.i >= T.steps.length) arriveBerlin();
  else if (T.steps[T.i].k === 'airport') SFX.plane();
}
function drawTravel() {
  const T = G.travel, st = T.steps[T.i], p = Math.min(1, (G.t - T.t0) / st.d), t = G.t;
  A.use(lctx);
  const side = !/^(europe|mapDrive|mapTrain)$/.test(st.k);
  G.travelShift = side ? sideOY() : 0;
  if (side) { topFill({ airport: '#0b1530', drive: '#5e8ccc', train: '#6c96cf', hbf: '#0d1a33', city: '#0b1324' }[st.k]); lctx.save(); lctx.translate(0, sideOY()); }
  const LH = side ? 180 : globalThis.__LH;
  if (st.k === 'europe') { const r = A.europeMap(LW, LH, ease(p), t); G.mapPins = [['Бухарест · Boekarest', r.from], ['Берлін · Berlijn', r.to]]; }
  else if (st.k === 'mapDrive' || st.k === 'mapTrain') {
    const route = st.k === 'mapDrive'
      ? [[12.18, 52.18], [12.28, 52.27], [12.55, 52.33], [12.95, 52.36], [13.1, 52.39], [13.3, 52.48], [13.42, 52.53]]
      : [[12.74, 52.32], [12.76, 52.2], [12.95, 52.3], [13.06, 52.39], [13.2, 52.45], [13.37, 52.525]];
    const r = A.brandenburgMap(LW, LH, route, ease(p), t);
    const pins = st.k === 'mapDrive' ? [['Altengrabow', 12.18, 52.18], ['Berlin', 13.42, 52.53]] : [['Lehnin', 12.74, 52.32], ['Brück', 12.76, 52.2], ['Berlin Hbf', 13.37, 52.525]];
    G.mapPins = pins.map(([n, lo, la]) => [n, r.P(lo, la)]);
    lctx.save(); lctx.translate(r.at[0], r.at[1]);
    if (st.k === 'mapDrive') { A.R(-4, -2, 8, 4, '#4f8a45'); A.R(-1, -3, 4, 2, '#9cc7ff'); }
    else { A.R(-5, -2, 10, 4, '#e8212b'); A.R(-4, -1, 8, 1, '#fff'); }
    lctx.restore();
  } else if (st.k === 'airport') {
    const px = -40 + p * (LW * 0.75), py = p < 0.6 ? 70 + p / 0.6 * 70 : 140;
    A.airport(LW, LH, t, px, py);
  } else if (st.k === 'drive') {
    A.roadTrip(LW, LH, t, 'day');
    if (p > 0.6) A.skyline(LW, 140, t, (p - 0.6) / 0.4);
    A.amarok(LW * 0.45, 158, 1, t, 'VIKA');
    A.R(LW * 0.8, 112, 34, 14, '#1d5a2a'); A.R(LW * 0.8 + 16, 126, 2, 14, '#888');
  } else if (st.k === 'train') {
    A.trainTrip(LW, LH, t, 'OLEKSIY');
  } else if (st.k === 'hbf') {
    A.hbf(LW, LH, t);
    A.friend('OLEKSIY', LW * 0.2 + p * LW * 0.6, 160, { dir: 1, moving: true, phase: t * 2, t });
  } else if (st.k === 'city') {
    A.use(lctx);
    lctx.fillStyle = '#0b1324'; lctx.fillRect(0, 0, LW, LH);
    A.skyline(LW, 140, t, 1);
    A.R(0, 140, LW, 40, '#232328'); for (let x = -((t * 120) % 40); x < LW; x += 40) A.R(x, 160, 20, 1, '#ddd');
    if (G.me === 'VITO') A.taxi(LW * 0.5, 160, 1, t);
    else A.amarok(LW * 0.5, 160, 1, t, 'VIKA');
  }
  if (side) lctx.restore();
}
// side views are 180 high; on tall screens they sit at the bottom with extra sky above
function sideOY() { return LH - 180; }
function topFill(kind, cx) {
  const oy = sideOY(); if (oy <= 0) return;
  if (kind === 'street') {
    lctx.fillStyle = '#3b3a3f'; lctx.fillRect(0, 0, LW, oy + 2);
    for (let row = 1; row * 34 <= oy + 30; row++) for (let i = -2; i < 30; i++) {
      const x = i * 34 + 6 - (cx % 34), y = oy - row * 34 + 2;
      lctx.fillStyle = '#262a33'; lctx.fillRect(x, y, 18, 22);
      if (A.hash(i + row * 31 + Math.floor(cx / 34)) > 0.6) { lctx.fillStyle = '#e8b35a'; lctx.fillRect(x + 1, y + 1, 16, 20); lctx.fillStyle = '#6b5030'; lctx.fillRect(x + 8, y + 1, 1, 20); }
      lctx.fillStyle = '#4a4950'; lctx.fillRect(x - 2, y + 22, 22, 2);
    }
  } else {
    lctx.fillStyle = kind; lctx.fillRect(0, 0, LW, oy + 2);
    if (kind === '#0b1530' || kind === '#0b1324' || kind === '#0d1a33') for (let i = 0; i < 40; i++) { lctx.fillStyle = '#cfd8ff'; lctx.fillRect(A.hash(i + 70) * LW, A.hash(i + 71) * oy, 1, 1); }
  }
}
const ease = p => p < 0.5 ? 2 * p * p : 1 - Math.pow(-2 * p + 2, 2) / 2;

// =====================================================================
// scenes: Berlin street (shared)
// =====================================================================
const S = A.STREET;
const ARR = { taxi: { from: -80, stop: 318, y: 170, leave: S.W + 120 }, amarok: { from: S.W + 80, stop: 420, y: 170, leave: -120 }, scooter: { from: -40, stop: 520, y: 150 } };
function arriveBerlin() {
  G.scene = 'street';
  const kind = ARRIVE[G.me], a = ARR[kind];
  G.p = { x: a.stop + (kind === 'scooter' ? -6 : 4), y: S.feet, dir: kind === 'amarok' ? -1 : 1, phase: 0, moving: false, anim: '', animUntil: 0, seat: -1, angle: -Math.PI / 2, idle: 0 };
  G.arrival = { kind, t0: G.t };
  G.order = null;
  G.npcs = []; G.cars = []; G.leaves = [];
  for (let i = 0; i < 4; i++) spawnPed(true);
  G.camX = clamp(G.p.x - LW / 2, 0, S.W - LW);
  connect(kind, G.p.x);
  setTimeout(() => { if (G.scene === 'street') bubble(G.me, pick(HELLOS), 2.5); }, 2800);
}
function arrivalState(kind, t0, now) {
  // returns { vx, show (character visible), vehicle visible }
  const a = ARR[kind], el = now - t0;
  if (kind === 'scooter') {
    if (el < 3) return { vx: a.from + (a.stop - a.from) * ease(el / 3), riding: true, show: false, parked: false };
    return { vx: a.stop, riding: false, show: true, parked: true };
  }
  if (el < 2.2) return { vx: a.from + (a.stop - a.from) * ease(el / 2.2), show: false, veh: true };
  if (el < 2.8) return { vx: a.stop, show: el > 2.5, veh: true };
  if (el < 5) return { vx: a.stop + (a.leave - a.stop) * ((el - 2.8) / 2.2) ** 2, show: true, veh: true };
  return { show: true, veh: false };
}
function spawnPed(anywhere) {
  const dir = Math.random() < 0.5 ? 1 : -1;
  const x = anywhere ? Math.random() * S.W : (dir > 0 ? -20 : S.W + 20);
  const tops = ['#5a4a7a', '#7a3a3a', '#2f5a6a', '#8a6a3a', '#3a3a3a', '#6a2a4a', '#2a4a3a', '#c86a3a'];
  G.npcs.push({ kind: 'ped', x, y: 146 + Math.random() * 7, dir, speed: 12 + Math.random() * 14, phase: Math.random(),
    look: { skin: pick(['#f0c8a0', '#e0b090', '#c8946a', '#8a5a3a', '#f4d4b8']), top: pick(tops), legs: pick(['#2a2a3a', '#3a3a2a', '#1a1a1a', '#4a3a5a']), shoe: '#1a1a1a', hair: pick(['#1a1a1a', '#5a3a1a', '#d8b860', '#8a2a1a', '#c8c8c8']), long: Math.random() < 0.4, hat: Math.random() < 0.25 ? 'beanie' : null, hatc: pick(['#c83a3a', '#e8c040', '#3a6ac8']), bag: Math.random() < 0.3 ? pick(['#e8e0c8', '#3a6a3a']) : null, dog: Math.random() < 0.12 ? pick(['#c8a070', '#3a3a3a', '#f0f0f0']) : null } });
}
function updateStreet(dt) {
  const arr = G.arrival;
  const now = G.t;
  if (arr) {
    const st = arrivalState(arr.kind, arr.t0, now);
    if (arr.kind === 'taxi' && now - arr.t0 > 2.2 && !arr.sounded) { arr.sounded = true; SFX.door(); }
    if (st.show && now - arr.t0 > (arr.kind === 'scooter' ? 3.1 : 2.9)) G.arrival = null;
  } else if (!dialogOpen()) {
    movePlayerSide(dt, -20, S.W + 20);
    // walked off the street: home or stay?
    if (G.p.x < -8 || G.p.x > S.W + 8) {
      showDialog({
        title: mix('Terug naar de basis?', 'Назад на базу?'),
        text: mix('Of blijf je nog even gezellig?', 'Чи ще трохи потусуєш?'),
        opts: [
          { ico: '🏠', label: mix('Naar huis', 'Додому, на базу'), key: 'A', act: () => { disconnect(); startBase(G.me); } },
          { ico: '🍜', label: mix('Blijven!', 'Залишаюсь!'), key: 'B', act: () => { G.p.x = clamp(G.p.x, 12, S.W - 12); G.p.dir *= -1; } },
        ],
      });
    }
  }
  // traffic, people, leaves
  G.nextPed -= dt; if (G.nextPed <= 0) { spawnPed(false); G.nextPed = 2.5 + Math.random() * 5; }
  for (const n of G.npcs) { n.x += n.dir * n.speed * dt; n.phase += dt * n.speed / 14; }
  G.npcs = G.npcs.filter(n => n.x > -60 && n.x < S.W + 60);
  G.nextCar -= dt;
  if (G.nextCar <= 0) {
    const dir = Math.random() < 0.5 ? 1 : -1;
    G.cars.push({ kind: 'car', x: dir > 0 ? -50 : S.W + 50, y: dir > 0 ? 168 : 178, dir, speed: 70 + Math.random() * 40, col: pick(['#c8323a', '#2a4a8a', '#e8e8e8', '#1a1a1a', '#5a7a3a', '#d8a020', '#7a7a8a']) });
    G.nextCar = 3 + Math.random() * 6; if (onScreen(S.W / 2)) SFX.car();
  }
  G.nextTram -= dt;
  if (G.nextTram <= 0) {
    const dir = Math.random() < 0.5 ? 1 : -1;
    G.cars.push({ kind: 'tram', x: dir > 0 ? -170 : S.W + 170, y: 172, dir, speed: 55, rang: false });
    G.nextTram = 28 + Math.random() * 20;
  }
  for (const v of G.cars) {
    v.x += v.dir * v.speed * dt;
    if (v.kind === 'tram' && !v.rang && Math.abs(v.x - G.p.x) < 160) { v.rang = true; SFX.bell(); }
  }
  G.cars = G.cars.filter(v => v.x > -260 && v.x < S.W + 260);
  // falling ginkgo and linden leaves, carried by the wind
  const wind = 9 + Math.sin(G.t * 0.4) * 7 + (Math.sin(G.t * 1.7) > 0.9 ? 18 : 0);
  if (Math.random() < dt * 7) {
    const tx = pick(A.treesAt);
    G.leaves.push({ x: tx + (Math.random() - 0.5) * 50, y: 40 + Math.random() * 25, vx: 0, vy: 6 + Math.random() * 6, rot: Math.random() * 6, col: pick(['#f2b400', '#ffcf3a', '#e8891c', '#d8641a', '#f5d24a']), life: 1, ground: 148 + Math.random() * 10 });
  }
  if (Math.random() < dt * 1.5) G.leaves.push({ x: G.camX - 10, y: 20 + Math.random() * 100, vx: 0, vy: 4, rot: 0, col: pick(['#f2b400', '#ffcf3a', '#e8891c']), life: 1, ground: 150 + Math.random() * 8 });
  for (const l of G.leaves) {
    if (l.y < l.ground) { l.vx += (wind - l.vx) * dt * 0.8; l.x += (l.vx + Math.sin(G.t * 3 + l.rot) * 8) * dt; l.y += (l.vy + Math.cos(G.t * 4 + l.rot) * 3) * dt; l.rot += dt * 5; }
    else { l.life -= dt * 0.15; l.x += (wind > 20 ? 6 : 0) * dt; }
  }
  G.leaves = G.leaves.filter(l => l.life > 0 && l.x < S.W + 40);
  if (G.leaves.length > 160) G.leaves.splice(0, G.leaves.length - 160);
  // the door
  G.prompt = '';
  if (!G.arrival && Math.abs(G.p.x - S.door) < 14) {
    const c = clockInfo();
    G.prompt = c && c.closed ? '🔒 ' + mix('Gesloten tot morgen', 'Зачинено до завтра') : 'A ▲ ' + mix('naar binnen', 'всередину');
  }
  G.camX = clamp(G.p.x - LW / 2, 0, S.W - LW);
  sendState(false);
}
function onScreen(x) { return x > G.camX - 40 && x < G.camX + LW + 40; }
function drawStreet() {
  A.use(lctx);
  const t = G.t, cx = G.camX;
  topFill('street', cx);
  lctx.save(); lctx.translate(0, sideOY());
  A.street(LW, 180, cx, t, { doorOpen: G.doorUntil > t });
  A.outdoorTables(cx, t);
  // the parked e-scooter while Oleksiy is here
  const olek = G.me === 'OLEKSIY' ? (G.arrival ? null : true) : (G.others.OLEKSIY && G.others.OLEKSIY.scene && !arrivingFor(G.others.OLEKSIY));
  if (olek) A.scooter(528 - cx, S.feet, -1, null, 0);
  const list = [];
  for (const n of G.npcs) list.push({ y: n.y, d: () => A.person(n.x - cx, n.y, n.look, n.phase, true, n.dir, t) });
  // friends
  for (const ch of presentChars()) {
    const o = ch === G.me ? null : G.others[ch];
    if (o && o.scene !== 'street') continue;
    if (ch === G.me) {
      if (G.arrival && !arrivalState(G.arrival.kind, G.arrival.t0, t).show) continue;
      list.push({ y: G.p.y + 0.5, d: () => drawMe(G.p.x - cx, G.p.y) });
    } else {
      if (arrivingFor(o) && !arrivalState(o.arrive, (o.arriveAt - snow()) / 1000 + t, t).show) continue;
      list.push({ y: o.y + 0.4, d: () => drawOther(o, o.x - cx, o.y) });
    }
  }
  list.sort((a, b) => a.y - b.y).forEach(o => o.d());
  A.streetFront(cx, t);
  // arrivals (mine and friends')
  const arrivals = [];
  if (G.arrival) arrivals.push([G.me, G.arrival.kind, G.arrival.t0]);
  for (const ch in G.others) { const o = G.others[ch]; if (arrivingFor(o)) arrivals.push([ch, o.arrive, (o.arriveAt - snow()) / 1000 + t]); }
  // traffic
  for (const v of G.cars.filter(v => v.y < 172)) drawVehicle(v);
  for (const [ch, kind, t0] of arrivals) {
    const st = arrivalState(kind, t0, t), a = ARR[kind];
    if (kind === 'scooter' && st.riding) { A.scooter(st.vx - cx, a.y, 1, ch, t); A.friend(ch, st.vx - cx - 1, a.y - 3, { dir: 1, t }); }
    else if (st.veh && kind === 'taxi') A.taxi(st.vx - cx, a.y, 1, t);
    else if (st.veh && kind === 'amarok') A.amarok(st.vx - cx, a.y, -1, t, (t - t0) < 2.5 ? ch : null);
  }
  for (const v of G.cars.filter(v => v.y >= 172)) drawVehicle(v);
  // leaves on top of everything
  for (const l of G.leaves) {
    if (!onScreen(l.x)) continue;
    lctx.globalAlpha = Math.min(1, l.life * 2);
    const flip = Math.sin(l.rot) > 0;
    A.R(l.x - cx, l.y, flip ? 2 : 1, flip ? 1 : 2, l.col);
    lctx.globalAlpha = 1;
  }
  lctx.restore();
}
function drawVehicle(v) {
  const x = v.x - G.camX;
  if (x < -200 || x > LW + 200) return;
  if (v.kind === 'tram') A.tram(x, v.y, v.dir, G.t); else A.car(x, v.y, v.col, v.dir, G.t, true);
}

// =====================================================================
// entering and leaving Hoongy
// =====================================================================
function enterHoongy() {
  const c = clockInfo();
  if (c && c.closed) { bubble(G.me, mix('Gesloten! Morgen weer.', 'Зачинено! Завтра знову.'), 2); return; }
  G.scene = 'enter'; G.trans = { t0: G.t, dir: 'in' };
  G.doorUntil = G.t + 1.2; SFX.door();
}
function updateEnter() {
  const p = (G.t - G.trans.t0) / 1.7;
  if (p >= 1) {
    G.scene = 'inside';
    G.p.x = A.IN.door.x; G.p.y = A.IN.H - 24; G.p.angle = -Math.PI / 2; G.p.seat = -1;
    sendState(true);
    setTimeout(() => { if (G.scene === 'inside') bubble(G.me, mix('Wat gezellig hier!', 'Як тут затишно!'), 2.2); }, 300);
  }
}
function drawEnter() {
  const p = (G.t - G.trans.t0) / 1.7;
  if (p < 0.55) {
    // the camera slides into the front window
    const k = ease(p / 0.55), zoom = 1 + k * 3.2;
    const wx = (S.shop0 + S.shop1) / 2 - G.camX, wy = 96 + sideOY();
    drawStreet();
    const img = lctx.getImageData(0, 0, LW, LH);
    const tmp = document.createElement('canvas'); tmp.width = LW; tmp.height = LH; tmp.getContext('2d').putImageData(img, 0, 0);
    lctx.fillStyle = '#000'; lctx.fillRect(0, 0, LW, LH);
    lctx.save();
    lctx.translate(LW / 2, LH / 2); lctx.scale(zoom, zoom); lctx.translate(-(wx + (LW / 2 - wx) * (1 - k)), -(wy + (LH / 2 - wy) * (1 - k)));
    lctx.drawImage(tmp, 0, 0);
    lctx.restore();
    lctx.fillStyle = 'rgba(255,220,140,' + (k * 0.5) + ')'; lctx.fillRect(0, 0, LW, LH);
  } else {
    // ...and rises up: the view turns to look down from the ceiling
    const k = ease((p - 0.55) / 0.45);
    const endY = clamp(A.IN.H - 24 - LH / 2, 0, A.IN.H - LH);
    G.camX = clamp(A.IN.door.x - LW / 2, Math.min(0, (A.IN.W - LW) / 2), Math.max(0, A.IN.W - LW));
    G.camY = (A.IN.H - LH) + 60 * (1 - k) - ((A.IN.H - LH) - endY) * k;
    drawInsideWorld();
    lctx.fillStyle = 'rgba(255,220,140,' + (0.5 * (1 - k)) + ')'; lctx.fillRect(0, 0, LW, LH);
  }
}
function leaveHoongy() {
  if (G.p.seat >= 0) send({ t: 'stand' });
  G.scene = 'street';
  G.p.x = S.door; G.p.y = S.feet; G.p.dir = 1; G.p.seat = -1; G.order = null;
  G.doorUntil = G.t + 1; SFX.door();
  G.fadeUntil = G.t + 0.5;
  sendState(true);
}

// =====================================================================
// inside Hoongy (top-down)
// =====================================================================
const IN = A.IN;
function insideBlocked(x, y) {
  if (x < 18 || x > IN.W - 18 || y < 54 || y > IN.H - 16) return true;
  for (const tb of IN.tables) if (Math.abs(x - tb.x) < tb.w / 2 + 4 && Math.abs(y - tb.y) < tb.h / 2 + 4) return true;
  return false;
}
function seatTakenMap() {
  const m = {};
  for (const ch in G.others) { const o = G.others[ch]; if (o.scene === 'inside' && o.seat >= 0) m[o.seat] = ch; }
  if (G.p.seat >= 0) m[G.p.seat] = G.me;
  return m;
}
function nearSeat() {
  const taken = seatTakenMap();
  let best = -1, bd = 14;
  IN.seats.forEach((s, i) => { const d = Math.hypot(s.x - G.p.x, s.y - G.p.y); if (d < bd && !taken[i]) { bd = d; best = i; } });
  return best;
}
function updateInside(dt) {
  const c = clockInfo();
  if (c && c.closed && !G.closedShown) {
    G.closedShown = true;
    showDialog({ title: '22:00 · Hoongy ' + mix('sluit!', 'зачиняється!'), text: mix('Dank jullie wel en tot morgen! Doei!', 'Дякуємо, до завтра! Па-па!'), opts: [{ ico: '🚪', label: 'OK', key: 'A', act: leaveHoongy }] });
  }
  if (G.p.seat >= 0) {
    const s = IN.seats[G.p.seat];
    G.p.x = s.x; G.p.y = s.y; G.p.angle = s.face; G.p.moving = false;
    // the waiter brings the menu
    const o = G.order;
    if (o && o.ph === 'wait' && snow() >= o.t + 3800 && G.menuShownFor !== o.t && !dialogOpen()) { G.menuShownFor = o.t; openMenu(); }
    if (!o) G.prompt = '';
    else if (o.ph === 'wait') G.prompt = snow() < o.t + 3800 ? '🧑‍🍳 ' + mix('de ober komt eraan…', 'офіціант іде…') : 'A 📋 ' + mix('menukaart', 'меню');
    else if (o.ph === 'cook' && snow() < o.t + 10500) G.prompt = '🍳 ' + mix('wordt bereid…', 'готується…');
    else if (o.ph === 'cook' && (o.bites || 0) < 5) G.prompt = 'A 😋 ' + mix('eten!', 'їсти!') + '   B ' + mix('opstaan', 'встати');
    else G.prompt = 'A ' + mix('nog iets bestellen', 'замовити ще') + '   B ' + mix('opstaan', 'встати');
  } else {
    movePlayerTop(dt);
    const ns = nearSeat();
    G.hiSeat = ns;
    G.prompt = ns >= 0 ? 'A 🪑 ' + mix('gaan zitten', 'сісти') : '';
    if (Math.abs(G.p.x - IN.door.x) < 14 && G.p.y > IN.H - 30) G.prompt = 'A 🚪 ' + mix('naar buiten', 'вийти');
    if (Math.abs(G.p.x - IN.door.x) < 12 && G.p.y >= IN.H - 17 && G.inputDown) leaveHoongy();
  }
  G.camX = IN.W <= LW ? (IN.W - LW) / 2 : clamp(G.p.x - LW / 2, 0, IN.W - LW);
  G.camY = clamp(G.p.y - LH / 2, 0, IN.H - LH);
  sendState(false);
}
function drawInsideWorld() {
  A.use(lctx);
  lctx.fillStyle = '#120c08'; lctx.fillRect(0, 0, LW, LH);
  lctx.save(); lctx.translate(-Math.round(G.camX), -Math.round(G.camY));
  const taken = seatTakenMap();
  A.inside(G.t, { seatTaken: taken, hiSeat: G.scene === 'inside' && G.p.seat < 0 ? G.hiSeat : -1, doorOpen: G.doorUntil > G.t });
  // dishes on the tables
  const diners = [];
  if (G.p.seat >= 0) diners.push([G.p.seat, G.order]);
  for (const ch in G.others) { const o = G.others[ch]; if (o.scene === 'inside' && o.seat >= 0) diners.push([o.seat, o.order]); }
  for (const [seat, o] of diners) {
    if (!o) continue;
    const s = IN.seats[seat], tb = IN.tables[s.table];
    const dx = s.x + (tb.x - s.x) * 0.55, dy = s.y;
    if (o.ph === 'cook' && snow() >= o.t + 10500) A.dish(o.item, dx, dy, 1 - (o.bites || 0) / 5, G.t);
    if (o.ph === 'wait' && snow() >= o.t + 3800) A.menuCard(dx, dy);
  }
  // people: two regulars at the bar, the waiter, the friends
  const list = [];
  list.push({ y: 46, d: () => A.friendTop('WAITER', 142, 48, { angle: -Math.PI / 2, t: G.t }) });
  const w = waiterPos();
  list.push({ y: w.y, d: () => { A.friendTop('WAITER', w.x, w.y, { angle: w.a, moving: w.moving, phase: G.t * 2, t: G.t }); if (w.carry) A.dish(w.carry, w.x + Math.cos(w.a) * 6, w.y + Math.sin(w.a) * 6, 1, G.t); } });
  for (const ch of presentChars()) {
    if (ch === G.me) { if (G.scene === 'inside') list.push({ y: G.p.y, d: () => drawMeTop(G.p.x, G.p.y) }); continue; }
    const o = G.others[ch];
    if (o.scene !== 'inside') continue;
    list.push({ y: o.y, d: () => drawOtherTop(o) });
  }
  list.sort((a, b) => a.y - b.y).forEach(o => o.d());
  A.insideTop(G.t);
  lctx.restore();
}
// the waiter: where is he right now?
function waiterPos() {
  const now = snow(), home = IN.bar;
  const tasks = [];
  const add = (seat, o) => {
    if (!o) return;
    const s = IN.seats[seat], tb = IN.tables[s.table];
    const at = { x: tb.x, y: tb.y - tb.h / 2 - 7 };
    if (o.ph === 'wait') tasks.push({ t: o.t + 600, legs: [[home, at, 3200, null], [at, at, 999999, null]] });
    if (o.ph === 'cook') tasks.push({ t: o.t, legs: [[at, IN.kitchen, 3000, null], [IN.kitchen, IN.kitchen, 4000, null], [IN.kitchen, at, 3500, o.item], [at, at, 1200, null], [at, home, 2500, null]] });
  };
  if (G.p.seat >= 0) add(G.p.seat, G.order);
  for (const ch in G.others) { const o = G.others[ch]; if (o.scene === 'inside' && o.seat >= 0) add(o.seat, o.order); }
  tasks.sort((a, b) => b.t - a.t);
  for (const tk of tasks) {
    if (now < tk.t) continue;
    let el = now - tk.t;
    for (const [from, to, d, carry] of tk.legs) {
      if (el < d) {
        const k = Math.min(1, el / Math.min(d, 3500));
        const x = from.x + (to.x - from.x) * k, y = from.y + (to.y - from.y) * k;
        const moving = (from !== to) && k < 1;
        return { x, y, a: moving ? Math.atan2(to.y - from.y, to.x - from.x) : Math.PI / 2, moving, carry };
      }
      el -= d;
    }
  }
  return { x: home.x + Math.sin(G.t * 0.3) * 30, y: home.y + 4, a: Math.PI / 2, moving: true };
}
function openMenu() {
  SFX.pop();
  showDialog({
    title: '📋 HOONGY · ' + mix('Menukaart', 'Меню'),
    text: mix('Wat mag het zijn?', 'Що бажаєте?') + ' 🙂',
    opts: Object.keys(FOOD).map(k => ({
      ico: FOOD[k].ico, label: FOOD[k].nl + ' · ' + FOOD[k].uk, small: FOOD[k].d + ' — ' + FOOD[k].p + ' €',
      act: () => { send({ t: 'order', item: k }); if (!NET.ok) G.order = { ph: 'cook', item: k, t: snow(), bites: 0 }; bubble(G.me, mix('Eén ' + FOOD[k].nl + ', graag!', FOOD[k].uk + ', будь ласка!'), 2.2); },
    })).concat([{ ico: '⏳', label: mix('Nog even kijken', 'Ще подумаю'), key: 'B', act: () => {} }]),
  });
}

// =====================================================================
// moving
// =====================================================================
const keys = { left: false, right: false, up: false, down: false };
function movePlayerSide(dt, minX, maxX) {
  const dx = (keys.right ? 1 : 0) - (keys.left ? 1 : 0);
  const an = animFor(G.me).anim;
  if (an === 'rofl' || an === 'hug') { G.p.moving = false; return; }
  if (dx) {
    G.p.x = clamp(G.p.x + dx * 46 * dt, minX, maxX); G.p.dir = dx; G.p.moving = true; G.p.phase += dt * 2.4; G.p.idle = 0;
    if ((G.p.phase % 0.5) < dt * 2.4) SFX.step();
  } else { G.p.moving = false; G.p.idle += dt; }
  G.inputDown = keys.down;
}
function movePlayerTop(dt) {
  const dx = (keys.right ? 1 : 0) - (keys.left ? 1 : 0), dy = (keys.down ? 1 : 0) - (keys.up ? 1 : 0);
  G.inputDown = keys.down;
  if (!dx && !dy) { G.p.moving = false; G.p.idle += dt; return; }
  const l = Math.hypot(dx, dy), sp = 40 * dt;
  const nx = G.p.x + dx / l * sp, ny = G.p.y + dy / l * sp;
  if (!insideBlocked(nx, G.p.y)) G.p.x = nx;
  if (!insideBlocked(G.p.x, ny)) G.p.y = ny;
  G.p.angle = Math.atan2(dy, dx); G.p.moving = true; G.p.phase += dt * 2.4; G.p.idle = 0;
  if (dx) G.p.dir = dx;
}
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));

// =====================================================================
// friends together: hugs, hearts and jokes
// =====================================================================
function presentChars() {
  const list = [];
  if (G.me && (G.scene === 'street' || G.scene === 'inside' || G.scene === 'enter')) list.push(G.me);
  for (const ch in G.others) list.push(ch);
  return list;
}
function posOf(ch) {
  if (ch === G.me) return { x: G.p.x, y: G.p.y, scene: G.scene === 'inside' ? 'inside' : 'street' };
  const o = G.others[ch]; return o ? { x: o.x, y: o.y, scene: o.scene } : null;
}
function nearestFriend(maxD) {
  const me = posOf(G.me); let best = null, bd = maxD || 1e9;
  for (const ch in G.others) {
    const p = posOf(ch); if (!p || p.scene !== me.scene) continue;
    const d = Math.hypot(p.x - me.x, (p.y - me.y) * (me.scene === 'inside' ? 1 : 0.3));
    if (d < bd) { bd = d; best = ch; }
  }
  return best;
}
function emote(kind) {
  if (!(G.scene === 'street' || G.scene === 'inside' || G.scene === 'base')) return;
  unlock();
  const to = nearestFriend(kind === 'hug' ? 46 : 1e9) || '';
  const j = Math.random() * JOKES.length | 0;
  if (NET.ok) send({ t: 'emote', kind, to, j });
  else handleEmote({ from: G.me, kind, to, j, t: snow() });
}
function laughLevel(ch) {
  const list = (G.jokes[ch] || []).filter(t => snow() - t < 25000);
  G.jokes[ch] = list;
  return Math.min(4, list.length);
}
function lastJoke(ch) { const l = G.jokes[ch] || []; return l.length ? snow() - l[l.length - 1] : 1e9; }
function handleEmote(e) {
  const from = e.from;
  if (e.kind === 'joke') {
    bubble(from, JOKES[e.j % JOKES.length], 4.2, 'joke');
    SFX.joke();
    for (const ch of presentChars()) {
      if (ch === from) continue;
      (G.jokes[ch] = G.jokes[ch] || []).push(e.t || snow());
      const lv = laughLevel(ch);
      setTimeout(() => { bubble(ch, pick(LAUGH[Math.max(0, lv - 1)]), 2.4); SFX.laugh(ch); }, 1500 + Math.random() * 900);
    }
    if (presentChars().length < 2) setTimeout(() => bubble(from, mix('...niemand lacht? 🦗', '...ніхто не сміється? 🦗'), 2), 2200);
  } else if (e.kind === 'heart') {
    SFX.heart();
    const a = posOf(from), b = e.to ? posOf(e.to) : null;
    if (a) G.hearts.push({ x: a.x, y: a.y - (scenePlace() === 'inside' ? 6 : 30), tx: b ? b.x : a.x, ty: b ? b.y - (scenePlace() === 'inside' ? 6 : 26) : a.y - 70, t0: G.t, place: a.scene });
    setAnim(from, 'heart', 0.8);
    if (e.to) { G.blush[e.to] = G.t + 3; setTimeout(() => bubble(e.to, pick(THANKS), 2), 900); }
  } else if (e.kind === 'hug') {
    if (e.to) {
      SFX.hug();
      setAnim(from, 'hug', 2.2); setAnim(e.to, 'hug', 2.2);
      G.hugs[from] = { with: e.to, until: G.t + 2.2 }; G.hugs[e.to] = { with: from, until: G.t + 2.2 };
      const a = posOf(from), b = posOf(e.to);
      if (from === G.me && b) { G.p.dir = b.x >= G.p.x ? 1 : -1; if (G.scene === 'street') G.p.x = b.x - G.p.dir * 11; }
      if (e.to === G.me && a) { G.p.dir = a.x >= G.p.x ? 1 : -1; }
      bubble(from, pick(HUGS), 2); setTimeout(() => bubble(e.to, pick(HUGS), 2), 600);
      if (a && b) for (let i = 0; i < 5; i++) G.hearts.push({ x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 - (a.scene === 'inside' ? 4 : 26), tx: (a.x + b.x) / 2 + (Math.random() - 0.5) * 30, ty: (a.y + b.y) / 2 - 60, t0: G.t + i * 0.12, place: a.scene, small: true });
    } else { bubble(from, pick(AIR), 2); setAnim(from, 'hug', 1.2); }
  }
}
function scenePlace() { return G.scene === 'inside' ? 'inside' : 'street'; }
function setAnim(ch, anim, dur) {
  if (ch === G.me) { G.p.anim = anim; G.p.animUntil = G.t + dur; }
  else if (G.others[ch]) { G.others[ch].localAnim = anim; G.others[ch].localAnimUntil = G.t + dur; }
}
function bubble(ch, text, dur, kind) { if (ch) G.bubbles[ch] = { text, until: G.t + (dur || 2.5), kind }; }

function animFor(ch) {
  const lv = laughLevel(ch), recent = lastJoke(ch) < 9000;
  const isMe = ch === G.me;
  const a = isMe ? (G.p.animUntil > G.t ? G.p.anim : '') : ((G.others[ch].localAnimUntil || 0) > G.t ? G.others[ch].localAnim : '');
  if (lv >= 4 && recent && a !== 'hug') return { anim: 'rofl', laugh: 4 };
  return { anim: a, laugh: recent ? lv : 0 };
}
function drawMe(x, y) {
  const st = animFor(G.me);
  A.friend(G.me, x, y, { dir: G.p.dir, phase: G.p.phase, moving: G.p.moving, anim: st.anim, laugh: st.laugh, blush: G.blush[G.me] > G.t, t: G.t, talk: bubbleTalking(G.me) });
}
function drawOther(o, x, y) {
  const st = animFor(o.char);
  A.friend(o.char, x, y, { dir: o.dir, phase: o.phase, moving: o.moving, anim: st.anim, laugh: st.laugh, blush: G.blush[o.char] > G.t, t: G.t, talk: bubbleTalking(o.char) });
}
function drawMeTop(x, y) {
  const st = animFor(G.me);
  A.friendTop(G.me, x, y, { angle: G.p.angle, moving: G.p.moving, phase: G.p.phase, laugh: st.laugh, t: G.t });
}
function drawOtherTop(o) {
  const st = animFor(o.char);
  const ang = o.seat >= 0 ? IN.seats[o.seat].face : (o.dir > 0 ? 0 : Math.PI);
  A.friendTop(o.char, o.x, o.y, { angle: o.angleLive != null ? o.angleLive : ang, moving: o.moving, phase: o.phase, laugh: st.laugh, t: G.t });
}
function bubbleTalking(ch) { const b = G.bubbles[ch]; return b && b.until > G.t && b.kind === 'joke' && (G.t * 8 | 0) % 2 === 0; }

// =====================================================================
// the radio: everyone hears the same station
// =====================================================================
let STATIONS = [];
fetch('radio/stations.json', { cache: 'no-store' }).then(r => r.json()).then(j => { STATIONS = j; radioSync(); }).catch(() => {});
const radioEl = new Audio(); radioEl.loop = true; radioEl.preload = 'auto'; radioEl.crossOrigin = 'anonymous';
let radioNode = null, radioFilter = null, radioGain = null, radioCh = -1;
function radioGraph() {
  const a = audio(); if (!a || radioNode) return;
  try {
    radioNode = a.createMediaElementSource(radioEl);
    radioFilter = a.createBiquadFilter(); radioFilter.type = 'lowpass'; radioFilter.frequency.value = 900;
    radioGain = a.createGain(); radioGain.gain.value = 0.5;
    radioNode.connect(radioFilter).connect(radioGain).connect(master);
  } catch (e) {}
}
function radioSync() {
  const at = G.scene === 'street' || G.scene === 'inside' || G.scene === 'enter';
  const ch = NET.g.radio ? NET.g.radio.ch : 0;
  if (!at || !ch || !STATIONS[ch - 1]) { radioStop(); return; }
  radioGraph();
  const st = STATIONS[ch - 1];
  const start = () => {
    const off = ((snow() - NET.g.radio.since) / 1000) % (radioEl.duration || 1e9);
    try { radioEl.currentTime = Math.max(0, off); } catch (e) {}
    radioEl.play().catch(() => {});
  };
  if (radioCh !== ch) {
    radioCh = ch;
    radioEl.src = 'radio/' + st.file;
    radioEl.onloadedmetadata = start;
    radioEl.load();
  } else if (radioEl.paused) start();
}
function radioStop() { radioCh = -1; try { radioEl.pause(); } catch (e) {} }
function radioMix() {
  if (!radioFilter || !ac) return;
  const inside = G.scene === 'inside';
  radioFilter.frequency.setTargetAtTime(inside ? 18000 : 700, ac.currentTime, 0.3);
  radioGain.gain.setTargetAtTime(inside ? 0.55 : 0.35, ac.currentTime, 0.3);
}
function openRadio() {
  unlock();
  const at = G.scene === 'street' || G.scene === 'inside';
  if (!at) { showDialog({ title: '📻 RADIO', text: mix('De radio speelt in Hoongy. Eerst naar Berlijn!', 'Радіо грає в Hoongy. Спершу до Берліна!'), opts: [{ ico: '👍', label: 'OK', key: 'A', act: () => {} }] }); return; }
  const cur = NET.g.radio ? NET.g.radio.ch : 0;
  showDialog({
    title: '📻 RADIO HOONGY',
    text: mix('Iedereen hoort dezelfde zender.', 'Усі слухають ту саму хвилю.'),
    opts: [{ ico: '⏻', label: 'OFF · ВИМК', sel: cur === 0, act: () => setRadio(0) }]
      .concat(STATIONS.map((s, i) => ({ ico: '♫', label: s.name, small: s.note || '', sel: cur === i + 1, act: () => setRadio(i + 1) }))),
  });
}
function setRadio(ch) {
  if (NET.ok) send({ t: 'radio', ch });
  else { NET.g.radio = { ch, since: snow() }; radioSync(); }
}

// =====================================================================
// dialogs (DOS style)
// =====================================================================
let dlgOpts = [], dlgSel = 0;
function showDialog(d) {
  dlgOpts = d.opts; dlgSel = Math.max(0, d.opts.findIndex(o => o.sel));
  dlg.innerHTML = '<h3>' + esc(d.title) + '</h3>' + (d.text ? '<p>' + esc(d.text) + '</p>' : '') +
    '<div class="opts">' + d.opts.map((o, i) =>
      '<button class="opt' + (i === dlgSel ? ' sel' : '') + '" data-i="' + i + '"><span class="ico">' + (o.ico || '') + '</span><span>' + esc(o.label) +
      (o.small ? '<small>' + esc(o.small) + '</small>' : '') + '</span>' + (o.key ? '<span class="k">' + o.key + '</span>' : '') + '</button>').join('') + '</div>';
  dlg.classList.add('show');
}
function closeDialog() { dlg.classList.remove('show'); dlgOpts = []; }
function dialogOpen() { return dlg.classList.contains('show'); }
function chooseDialog(i) {
  const o = dlgOpts[i]; closeDialog(); SFX.click();
  if (o && o.act) o.act();
}
dlg.addEventListener('click', e => { const b = e.target.closest('.opt'); if (b) chooseDialog(+b.dataset.i); });
function dialogKey(k) {
  if (k === 'up') dlgSel = (dlgSel + dlgOpts.length - 1) % dlgOpts.length;
  else if (k === 'down') dlgSel = (dlgSel + 1) % dlgOpts.length;
  else if (k === 'a') return chooseDialog(dlgSel);
  else if (k === 'b') { const bi = dlgOpts.findIndex(o => o.key === 'B'); if (bi >= 0) return chooseDialog(bi); closeDialog(); return; }
  dlg.querySelectorAll('.opt').forEach((b, i) => b.classList.toggle('sel', i === dlgSel));
  SFX.click();
}

// =====================================================================
// input
// =====================================================================
let unlocked = false;
function unlock() { if (!unlocked) { unlocked = true; audio(); radioSync(); } }
function press(k) {
  unlock();
  if (G.scene === 'login') return;
  if (dialogOpen()) return dialogKey(k);
  if (k === 'hug' || k === 'heart' || k === 'joke') return emote(k);
  if (k === 'radio') return openRadio();
  if (G.scene === 'travel') { if (k === 'a' || k === 'b') nextTravel(); return; }
  if (G.scene === 'base' && k === 'a' && G.me === 'VITO' && G.p.x > G.base.H.W - 30) return startTravel('VITO');
  if (G.scene === 'street' && !G.arrival) {
    if ((k === 'a' || k === 'up') && Math.abs(G.p.x - S.door) < 14) return enterHoongy();
  }
  if (G.scene === 'inside') {
    if (G.p.seat >= 0) {
      const o = G.order;
      if (k === 'b' || k === 'left' || k === 'right' || k === 'up' || k === 'down') { standUp(); return; }
      if (k === 'a') {
        if (o && o.ph === 'wait' && snow() >= o.t + 3800) openMenu();
        else if (o && o.ph === 'cook' && snow() >= o.t + 10500 && (o.bites || 0) < 5) { send({ t: 'eat' }); if (!NET.ok) o.bites = (o.bites || 0) + 1; SFX.eat(); if ((o.bites || 0) >= 4) setTimeout(() => bubble(G.me, mix('Heerlijk! 😋', 'Смакота! 😋'), 2), 200); }
        else if (o && o.ph === 'cook' && (o.bites || 0) >= 5) { send({ t: 'more' }); if (!NET.ok) G.order = { ph: 'wait', t: snow() }; }
      }
      return;
    }
    if (k === 'a') {
      if (Math.abs(G.p.x - IN.door.x) < 14 && G.p.y > IN.H - 30) return leaveHoongy();
      const s = nearSeat();
      if (s >= 0) sitDown(s);
    }
  }
}
function sitDown(s) {
  G.p.seat = s; G.order = { ph: 'wait', t: snow() };
  const st = IN.seats[s]; G.p.x = st.x; G.p.y = st.y; G.p.angle = st.face;
  send({ t: 'sit', seat: s }); sendState(true);
  bubble(G.me, mix('Hè hè, zitten!', 'Ох, нарешті сісти!'), 1.8);
}
function standUp() {
  const s = IN.seats[G.p.seat];
  G.p.seat = -1; G.order = null; send({ t: 'stand' });
  if (s) { G.p.x = s.x + (s.face === 0 ? -10 : 10); if (insideBlocked(G.p.x, G.p.y)) G.p.y += 14; }
  sendState(true);
}
const KEYMAP = { ArrowLeft: 'left', KeyA: 'left', ArrowRight: 'right', KeyD: 'right', ArrowUp: 'up', KeyW: 'up', ArrowDown: 'down', KeyS: 'down', Enter: 'a', Space: 'a', KeyE: 'a', Escape: 'b', Backspace: 'b', KeyQ: 'b', KeyH: 'hug', KeyL: 'heart', KeyJ: 'joke', KeyR: 'radio' };
addEventListener('keydown', e => {
  if (G.scene === 'login') {
    unlock();
    if (e.key === 'Enter') { e.preventDefault(); kb.flash('SEND'); return tryLogin(); }
    if (e.key === 'Backspace') { e.preventDefault(); return loginKey('DEL'); }
    if (e.key.length === 1 && !e.ctrlKey && !e.metaKey) { e.preventDefault(); kb.flash(e.key); return loginKey(e.key); }
    return;
  }
  const k = KEYMAP[e.code]; if (!k) return;
  e.preventDefault();
  if (k in keys) { if (!keys[k] && (dialogOpen() || (G.scene === 'inside' && G.p.seat >= 0) || k === 'up')) press(k); keys[k] = true; return; }
  if (!e.repeat) press(k);
});
addEventListener('keyup', e => { const k = KEYMAP[e.code]; if (k in keys) keys[k] = false; });
// touch pad
document.querySelectorAll('#pad .pb').forEach(b => {
  const k = b.dataset.k;
  const down = e => { e.preventDefault(); b.classList.add('on'); if (k in keys) { if (dialogOpen() || (G.scene === 'inside' && G.p.seat >= 0) || k === 'up') press(k); keys[k] = true; } else press(k); };
  const up = () => { b.classList.remove('on'); if (k in keys) keys[k] = false; };
  b.addEventListener('pointerdown', down); b.addEventListener('pointerup', up); b.addEventListener('pointercancel', up); b.addEventListener('pointerleave', up);
});
// tap the picture inside to walk or sit
view.addEventListener('pointerdown', e => {
  unlock();
  if (G.scene !== 'inside' || dialogOpen()) return;
  const r = view.getBoundingClientRect();
  const wx = (e.clientX - r.left) / SCALE + G.camX, wy = (e.clientY - r.top) / SCALE + G.camY;
  const taken = seatTakenMap();
  const s = IN.seats.findIndex((st, i) => !taken[i] && Math.hypot(st.x - wx, st.y - wy) < 9);
  if (s >= 0 && G.p.seat < 0 && Math.hypot(IN.seats[s].x - G.p.x, IN.seats[s].y - G.p.y) < 40) sitDown(s);
});
$('dosBtn').addEventListener('click', () => {
  unlock();
  showDialog({
    title: mix('Terug naar ERROR-DOS?', 'Назад до ERROR-DOS?'),
    opts: [
      { ico: '◄', label: mix('Ja, afsluiten', 'Так, вийти'), key: 'A', act: () => { disconnect(); try { sessionStorage.setItem('errordos-ran', 'TUSA'); } catch (e) {} location.href = '/'; } },
      { ico: '🍜', label: mix('Nee, blijven', 'Ні, залишаюсь'), key: 'B', act: () => {} },
    ],
  });
});
$('radioBtn').addEventListener('click', openRadio);
// game controller
const padPrev = {};
function pollPad() {
  if (!navigator.getGamepads) return;
  for (const p of navigator.getGamepads()) {
    if (!p) continue;
    const b = i => !!(p.buttons[i] && p.buttons[i].pressed), ax = p.axes || [];
    const st = { left: b(14) || ax[0] < -0.5, right: b(15) || ax[0] > 0.5, up: b(12) || ax[1] < -0.5, down: b(13) || ax[1] > 0.5, a: b(0), b: b(1), joke: b(2), heart: b(3), hug: b(4) || b(5), radio: b(9) };
    const prev = padPrev[p.index] || {};
    for (const k in st) {
      if (k in keys) { if (st[k] && !prev[k] && (dialogOpen() || (G.scene === 'inside' && G.p.seat >= 0) || k === 'up')) press(k); keys[k] = st[k] || (prev[k] && !st[k] ? false : keys[k]); if (!st[k] && prev[k]) keys[k] = false; }
      else if (st[k] && !prev[k]) press(k);
    }
    padPrev[p.index] = st;
  }
}

// =====================================================================
// screen-space text: names, bubbles, prompts, captions
// =====================================================================
function S2(x) { return x * SCALE * DPR; }
function drawOverlay() {
  const c = vctx, k = SCALE * DPR;
  const oy = (G.scene === 'base' || G.scene === 'street') ? sideOY() : 0;
  c.textBaseline = 'alphabetic';
  // captions
  if (G.scene === 'base') {
    const H = G.base.H;
    capText(H.title, H.sub);
    if (G.base.signX != null) {
      c.font = '600 ' + (5.5 * k) + 'px "Pixelify Sans", monospace'; c.fillStyle = '#f4ead0'; c.textAlign = 'center';
      c.fillText(H.sign, S2(G.base.signX + G.base.signW / 2), S2(G.base.signY + G.base.signH * 0.72 + oy));
      c.textAlign = 'left';
    }
    if (G.p.x > H.W - 70) hint('► ' + mix('Naar Berlijn', 'До Берліна') + ' ►');
  }
  if (G.scene === 'travel') {
    const st = G.travel.steps[G.travel.i];
    capText(st.cap(), mix('A: overslaan', 'A: пропустити'));
    if (G.mapPins && (st.k === 'mapDrive' || st.k === 'mapTrain' || st.k === 'europe')) {
      c.font = (6 * k) + 'px "Pixelify Sans", monospace'; c.fillStyle = '#9cff9c';
      for (const [n, [x, y]] of G.mapPins) { const w = c.measureText(n).width; c.fillText(n, Math.min(S2(x + 3), view.width - w - 4), S2(y - 2)); }
    }
  }
  if (G.scene === 'street' || G.scene === 'inside') {
    for (const ch of presentChars()) {
      const p = posOf(ch); if (!p) continue;
      if ((G.scene === 'inside') !== (p.scene === 'inside')) continue;
      const sx = p.x - G.camX, sy = p.y - (G.scene === 'inside' ? G.camY : -oy);
      if (ch !== G.me && G.others[ch] && arrivingFor(G.others[ch]) && G.scene === 'street') {
        const o = G.others[ch], t0 = (o.arriveAt - snow()) / 1000 + G.t;
        if (!arrivalState(o.arrive, t0, G.t).show) continue;
      }
      if (ch === G.me && G.arrival && !arrivalState(G.arrival.kind, G.arrival.t0, G.t).show) continue;
      const top = G.scene === 'inside' ? sy - 9 : sy - 34;
      nameTag(ch, sx, top);
      const b = G.bubbles[ch];
      if (b && b.until > G.t) speech(b.text, sx, top - 4, b.kind === 'joke');
    }
  }
  if (G.scene === 'base') {
    const b = G.bubbles[G.me];
    if (b && b.until > G.t) speech(b.text, G.p.x - G.camX, G.p.y - 38 + oy);
    nameTag(G.me, G.p.x - G.camX, G.p.y - 34 + oy);
  }
  // hearts
  for (const h of G.hearts) {
    const el = G.t - h.t0; if (el < 0) continue;
    const kk = Math.min(1, el / 0.9);
    const hy = G.scene === 'inside' ? G.camY : -oy;
    const x = h.x + (h.tx - h.x) * kk - G.camX, y = h.y + (h.ty - h.y) * kk - Math.sin(kk * Math.PI) * 18 - hy;
    c.globalAlpha = Math.max(0, 1 - Math.max(0, el - 1.1) * 2);
    c.font = ((h.small ? 7 : 10) * k) + 'px sans-serif'; c.textAlign = 'center';
    c.fillText('❤️', S2(x), S2(y));
    c.globalAlpha = 1; c.textAlign = 'left';
  }
  G.hearts = G.hearts.filter(h => G.t - h.t0 < 1.8);
  // chill notes when idle
  if ((G.scene === 'street' || G.scene === 'base') && G.p.idle > 6 && !G.arrival) {
    c.font = (7 * k) + 'px sans-serif'; c.fillStyle = '#ffe9a8'; c.textAlign = 'center';
    const ph = (G.t * 0.7) % 1;
    c.globalAlpha = 1 - ph; c.fillText(ph < 0.5 ? '♪' : '♫', S2(G.p.x - G.camX + 6 + ph * 6), S2(G.p.y - 32 - ph * 14 + oy));
    c.globalAlpha = 1; c.textAlign = 'left';
  }
  if (G.prompt && !dialogOpen()) hint(G.prompt);
  // fade after leaving
  if (G.fadeUntil > G.t) { c.fillStyle = 'rgba(0,0,0,' + ((G.fadeUntil - G.t) * 2) + ')'; c.fillRect(0, 0, view.width, view.height); }
}
function capText(a, b) {
  const c = vctx, k = SCALE * DPR;
  c.fillStyle = 'rgba(0,0,0,.55)'; c.fillRect(0, 0, view.width, 15 * k);
  c.font = '600 ' + (8 * k) + 'px "Pixelify Sans", monospace'; c.fillStyle = '#ffe23a'; c.textAlign = 'left';
  c.fillText(a, 5 * k, 10 * k);
  const aw = c.measureText(a).width;
  if (b) { c.font = (6 * k) + 'px "Pixelify Sans", monospace'; if (aw + c.measureText(b).width + 16 * k < view.width) { c.fillStyle = '#e8e0c8'; c.textAlign = 'right'; c.fillText(b, view.width - 5 * k, 10 * k); c.textAlign = 'left'; } }
}
function hint(txt) {
  const c = vctx, k = SCALE * DPR;
  c.font = '600 ' + (7 * k) + 'px "Pixelify Sans", monospace';
  const w = c.measureText(txt).width + 10 * k;
  const x = (view.width - w) / 2, y = view.height - 15 * k;
  c.fillStyle = 'rgba(3,18,8,.85)'; c.fillRect(x, y, w, 11 * k);
  c.strokeStyle = '#39ff7a'; c.lineWidth = k * 0.6; c.strokeRect(x, y, w, 11 * k);
  c.fillStyle = '#c8ffd4'; c.fillText(txt, x + 5 * k, y + 8 * k);
}
function nameTag(ch, x, y) {
  const c = vctx, k = SCALE * DPR;
  c.font = '600 ' + (5.5 * k) + 'px "Pixelify Sans", monospace'; c.textAlign = 'center';
  c.fillStyle = 'rgba(0,0,0,.5)'; c.fillText(ch, S2(x) + k * 0.5, S2(y) + k * 0.5);
  c.fillStyle = TAG[ch] || '#fff'; c.fillText(ch, S2(x), S2(y));
  c.textAlign = 'left';
}
function speech(text, x, y, joke) {
  const c = vctx, k = SCALE * DPR;
  const fs = 6.2 * k, maxW = Math.min(110, LW * 0.62) * k;
  c.font = (joke ? '600 ' : '') + fs + 'px "Pixelify Sans", monospace';
  const words = text.split(' '), lines = []; let line = '';
  for (const w of words) { const tt = line ? line + ' ' + w : w; if (c.measureText(tt).width > maxW && line) { lines.push(line); line = w; } else line = tt; }
  if (line) lines.push(line);
  const w = Math.max(...lines.map(l => c.measureText(l).width)) + 8 * k, h = lines.length * fs * 1.15 + 6 * k;
  let bx = S2(x) - w / 2; bx = Math.max(2 * k, Math.min(view.width - w - 2 * k, bx));
  const by = Math.max(16 * k, S2(y) - h - 4 * k);
  c.fillStyle = joke ? '#fff7c8' : '#ffffff'; c.strokeStyle = '#1a1a1a'; c.lineWidth = k * 0.7;
  roundRect(c, bx, by, w, h, 3 * k); c.fill(); c.stroke();
  c.beginPath(); c.moveTo(S2(x) - 3 * k, by + h); c.lineTo(S2(x), by + h + 4 * k); c.lineTo(S2(x) + 3 * k, by + h); c.closePath(); c.fill();
  c.fillStyle = '#1a1a1a'; c.textAlign = 'left';
  lines.forEach((l, i) => c.fillText(l, bx + 4 * k, by + 3 * k + fs * (i + 0.9) * 1.1));
}
function roundRect(c, x, y, w, h, r) { c.beginPath(); c.moveTo(x + r, y); c.arcTo(x + w, y, x + w, y + h, r); c.arcTo(x + w, y + h, x, y + h, r); c.arcTo(x, y + h, x, y, r); c.arcTo(x, y, x + w, y, r); c.closePath(); }

// =====================================================================
// main loop
// =====================================================================
let last = performance.now(), lastClock = 0;
function frame(now) {
  const dt = Math.min(0.05, (now - last) / 1000);
  last = now; G.t += dt;
  pollPad();
  // smooth other players towards their last known spot
  for (const ch in G.others) {
    const o = G.others[ch];
    const dx = o.tx - o.x, dy = o.ty - o.y, d = Math.hypot(dx, dy);
    if (d > 80) { o.x = o.tx; o.y = o.ty; }
    else { o.x += dx * Math.min(1, dt * 10); o.y += dy * Math.min(1, dt * 10); }
    o.moving = d > 0.6 || o.anim === 'walk';
    if (o.moving) { o.phase += dt * 2.4; if (Math.abs(dx) + Math.abs(dy) > 0.3) o.angleLive = Math.atan2(dy, dx); }
  }
  if (G.scene === 'base') updateBase(dt);
  else if (G.scene === 'travel') updateTravel();
  else if (G.scene === 'street') updateStreet(dt);
  else if (G.scene === 'enter') { updateStreet(0); updateEnter(); }
  else if (G.scene === 'inside') updateInside(dt);
  radioMix();
  // draw
  lctx.imageSmoothingEnabled = false;
  if (G.scene === 'base') drawBase();
  else if (G.scene === 'travel') drawTravel();
  else if (G.scene === 'street') drawStreet();
  else if (G.scene === 'enter') drawEnter();
  else if (G.scene === 'inside') drawInsideWorld();
  else { lctx.fillStyle = '#000'; lctx.fillRect(0, 0, LW, LH); }
  vctx.imageSmoothingEnabled = false;
  vctx.drawImage(low, 0, 0, view.width, view.height);
  drawOverlay();
  if (now - lastClock > 250) { updateClock(); lastClock = now; }
  requestAnimationFrame(frame);
}

// =====================================================================
// start
// =====================================================================
layout();
globalThis.__LH = LH;
addEventListener('resize', () => { globalThis.__LH = LH; });
showLogin();
requestAnimationFrame(frame);
if ('serviceWorker' in navigator) addEventListener('load', () => navigator.serviceWorker.register('/sw.js').catch(() => {}));
addEventListener('beforeunload', () => { NET.want = false; });
// test hook: ?as=VITO&skip=1 jumps straight to Berlin (used for quick checks)
const q = new URLSearchParams(location.search);
if (q.get('as') && whoIs(q.get('as'))) {
  const who = whoIs(q.get('as'));
  setTimeout(() => { startBase(who); if (q.get('skip')) { startTravel(who); G.travel.i = G.travel.steps.length - 1; nextTravel(); } }, 50);
}
window.__tusa = G;
})();
