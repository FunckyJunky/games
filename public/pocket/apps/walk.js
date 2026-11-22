// DOT WALK for pocket.DOS - a night walk through a little town, drawn like a dot-matrix sign.
// Tilt forward to walk (back = a step back), left/right to turn. TAP = the town map. HOLD 3 s = back to the menu.
// Find the 8 places: the clock tower, the fountain, the old oak, the lighthouse, the radio mast, the statue,
// the night cafe and the cat. Calibrate once on the title (like NEON FLIGHT). The dots' colour is on the title too.
// No motion sensor: it walks you round the town by itself.
var MK = typeof motion == 'function' ? motion() : 0;     // 0 none, 1 keys, 2 sensor
var MAP = [
  'HHHHHHHHHHHHHH##########OOOOOOOOOOOOOOOO',
  'H......................................O',
  'H.HHH.HHH.HHH..############..OOOO.fffffO',
  'H.HHHtHHHtHHH..############..OOOO...M..O',
  'H............t.############..OOOO.f....O',
  'H..............###....#####..OOOO......O',
  'H.HHH.HHH.HHH..###.T..#####......l.....O',
  'H.HHHtHHHtHHH..###....#####........OOO.O',
  'H...................l........OOOO..OOO.O',
  'H..............l..........l..OOOO..OOO.O',
  'H............CC..............OOOO..OOO.O',
  'H............CC..............OOOO......O',
  'H...t.....t..CC.....F........OOOO......O',
  'H.t.....t........................l.OOO.O',
  'H...........f......................OOO.O',
  'H...k.....t.f...........S....OOOO..OOO.O',
  'H.t.........f................OOOO..OOO.O',
  'H.....Q.k...f..l..........l..OOOO..OOO.O',
  'H...........f................OOOO......O',
  'H..t.....t.....####....####..OOOO......O',
  'H..............####....####............O',
  'H....t.....t...........................O',
  'H....l......l.......P......l...k..l..K.O',
  'H......................................O',
  'wwwwwwwwwwwwwwwwwww..wwwwwwwwwwwwwwwwwww',
  'wwwwwwwwwwwwwwwwwww..wwwwwwwwwwwwwwwwwww',
  'wwwwwwwwwwwwwwwwwwwL.wwwwwwwwwwwwwwwwwww',
  'wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww'
];
var MW = 40, MH = 28;
// cell types: 0 street, 1 old house, 2 house, 3 office, 4 clock tower, 5 cafe, 6 low fence, 7 the sea
var TCH = '.#HOTCfw', HGT = [0, 1.8, 1.1, 3.2, 6, 1.3, 0.35, 0], BASE = [0, 4, 4, 3.6, 5, 4, 3, 0], LITP = [0, 0.3, 0.4, 0.35, 0, 1, 0, 0];
var TY = [], PASS = [], SP = [], PLACES = [], RUNS = [], SPC = [];
var PW = MW + 4, PT = [], PH = [];                    // the map with 2 cells of 'outside' round it, for the rays
// the screen is 53 x 28 dots, each dot 2x2 pixels with a 1 pixel gap
var COLS = 53, ROWS = 28, FOC = 40, EYE = 0.5, MAXD = 15, PAL = [];
var FD = [], FL = [], FN = [], CLIP = 28;           // per column: the walls the ray drew (distance, top)
// detail: one ray for every 2 columns of dots (RS); when the board can't keep up: a shorter view, then 1 ray for 3 columns
var lod = 0, lodN = 0, lodT = 0, lodSlow = 0, RS = 2, CW = 6, DARKW = 7;
var THEMES = [['GREEN', 0x39ff7a], ['AMBER', 0xffb43a], ['CYAN', 0x22f0ff], ['RED', 0xff3355], ['PINK', 0xff2bd6], ['WHITE', 0xe4ecff]];
var LV = [0.06, 0.2, 0.34, 0.5, 0.66, 0.82, 1, 1];
var theme = 0;
// you
var odo = 0, px = 20.5, py = 22.5, yaw = -1.5708, dirX = 0, dirY = -1, plX = 1, plY = 0, hz = 13.5, ph = 0, spd = 0, steps = 0, stepsSaved = 0;
var ready = 0, ST = 0, T = 0, found = 0, nFound = 0, sel = 0, items = [], downT = -1, picked = false, flash = 0;
var tick = '', tickX = 0, tickQ = [], rain = [], raining = false, fw = [], stars = [], flies = [];
var path = [], pathI = 0, goal = -1, tour = 0, stuck = 0;
// calibration (the same idea as NEON FLIGHT): level, right, nose down; then "level" = how you hold it now
var CN = [0, 0, 1], CR = [1, 0, 0], CD = [0, 1, 0], N = [0, 0, 1], RR = [1, 0, 0], DD = [0, 1, 0];
var calOk = false, calStep = 0, calWarn = 0, sx = 0, sy = 0;

function h2(a, b, c) { var n = (a * 7919 + b * 104729 + c * 1299709) | 0; n = (n ^ (n >>> 13)) | 0; n = (n * 1103 + 12345) | 0; return ((n >>> 7) & 1023) / 1024; }
function setTheme(i) {
  theme = i;
  var c = THEMES[i][1], r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
  for (var k = 0; k < 8; k++) {
    var f = LV[k], m = k == 7 ? 0.55 : 0, R = Math.min(255, (r * f + (255 - r * f) * m) | 0), G = Math.min(255, (g * f + (255 - g * f) * m) | 0), B = Math.min(255, (b * f + (255 - b * f) * m) | 0);
    PAL[k] = Math.max(16, (R << 16) | (G << 8) | B);
  }
}

function init() {
  var x, y, i, c, t;
  calOk = load('cal', 0) == 1;
  if (calOk) for (i = 0; i < 3; i++) { CN[i] = load('n' + i, CN[i]); CR[i] = load('r' + i, CR[i]); CD[i] = load('d' + i, CD[i]); }
  setTheme(load('theme', 0) % THEMES.length);
  found = load('found', 0); steps = stepsSaved = load('steps', 0);
  for (y = 0; y < MH; y++) for (x = 0; x < MW; x++) {
    c = MAP[y].charAt(x); t = TCH.indexOf(c); if (t < 0) t = 0;
    TY.push(t); PASS.push(t == 0 && 'tlkFQMSLK'.indexOf(c) < 0);
    if ('tlkFQMSLK'.indexOf(c) >= 0) SP.push({ k: c, x: x + 0.5, y: y + 0.5, r: { t: 0.42, l: 0.12, k: 0.4, F: 0.75, Q: 0.9, M: 0.3, S: 0.35, L: 0.45, K: 0.22 }[c],
      cr: { t: 0.3, l: 0.15, k: 0.3, F: 0.75, Q: 0.5, M: 0.3, S: 0.4, L: 0.45, K: 0 }[c], d: 0, sc: 0 });
  }
  for (y = -2; y < MH + 2; y++) for (x = -2; x < MW + 2; x++) {
    t = x < 0 || y < 0 || x >= MW || y >= MH ? -1 : TY[y * MW + x];
    PT.push(t); PH.push(t < 0 ? -2 : t == 7 ? -1 : HGT[t]);
  }
  for (i = 0; i < MW * MH; i++) SPC.push(-1);
  for (i = 0; i < SP.length; i++) if (SP[i].cr) SPC[(SP[i].y | 0) * MW + (SP[i].x | 0)] = i;
  // the 8 places: where they are, and a free cell next to them to walk to
  var P = [['THE FOUNTAIN', 20.5, 12.5], ['THE STATUE', 24.5, 15.5], ['THE CLOCK TOWER', 19.5, 7.3], ['THE NIGHT CAFE', 15.3, 11.5],
    ['THE OLD OAK', 6.5, 17.5], ['THE LIGHTHOUSE', 19.6, 26.3], ['THE CAT', 37.5, 22.5], ['THE RADIO MAST', 36.5, 3.5]];
  for (i = 0; i < P.length; i++) PLACES.push({ n: P[i][0], x: P[i][1], y: P[i][2] });
  for (i = 0; i < 8; i++) if (found & (1 << i)) nFound++;
  for (i = 0; i < 34; i++) stars.push([rnd(6.2832), 0.18 + rnd(0.5), irnd(3)]);
  for (i = 0; i < 7; i++) flies.push({ x: 3 + rnd(8), y: 13 + rnd(8), h: 0.3 + rnd(0.6), p: rnd(6.28) });
  for (i = 0; i < 34; i++) rain.push([rnd(COLS), rnd(ROWS), 0.6 + rnd(0.5)]);
  // the map screen, row by row: runs of the same kind of cell
  for (y = 0; y < MH; y++) { x = 0; while (x < MW) { t = TY[y * MW + x]; var e = x; while (e + 1 < MW && TY[y * MW + e + 1] == t) e++; if (t) RUNS.push([y, x, e, t]); x = e + 1; } }
  menu(); face();
}
function menu() {
  items = [nFound > 0 && nFound < 8 ? 'GO ON' : 'WALK'];
  if (nFound > 0) items.push('NEW WALK');
  if (MK == 2) items.push('CALIBRATE');
  items.push('COLOR');
  items.push('EXIT');
  if (sel >= items.length) sel = 0;
}
function face() { dirX = Math.cos(yaw); dirY = Math.sin(yaw); plX = -dirY; plY = dirX; }
function say(s) { tickQ.push(s); }

// ------------------------------------------------------------------ the tilt (see NEON FLIGHT)
function dot(a, b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
function norm(a) { var l = Math.sqrt(dot(a, a)) || 1; a[0] /= l; a[1] /= l; a[2] /= l; return a; }
function flat(v, n) { var d = dot(v, n); return norm([v[0] - n[0] * d, v[1] - n[1] * d, v[2] - n[2] * d]); }
function up() { return [ax(), ay(), az()]; }
function level() {
  if (MK == 0) return;
  N = norm(up()); RR = flat(CR, N);
  var d = flat(CD, N), k = dot(d, RR);
  DD = norm([d[0] - RR[0] * k, d[1] - RR[1] * k, d[2] - RR[2] * k]);
}
function dz(v) { v = v > 0 ? Math.max(0, v - 0.08) : Math.min(0, v + 0.08); return Math.max(-1, Math.min(1, v / 0.92)); }
function stick() { if (MK == 0) return; var g = up(); sx = dz(dot(g, RR) / 0.45); sy = dz(dot(g, DD) / 0.4); }

// ------------------------------------------------------------------ walking
function solid(x, y) { if (x < 0 || y < 0 || x >= MW || y >= MH) return true; return TY[(y | 0) * MW + (x | 0)] != 0; }
function free(x, y) {
  var r = 0.2;
  if (solid(x - r, y - r) || solid(x + r, y - r) || solid(x - r, y + r) || solid(x + r, y + r)) return false;
  for (var j = -1; j <= 1; j++) for (var i = -1; i <= 1; i++) {
    var cx = (x | 0) + i, cy = (y | 0) + j; if (cx < 0 || cy < 0 || cx >= MW || cy >= MH) continue;
    var k = SPC[cy * MW + cx]; if (k < 0) continue;
    var s = SP[k], dx = x - s.x, dy = y - s.y, rr = s.cr + 0.15; if (dx * dx + dy * dy < rr * rr) return false;
  }
  return true;
}
function walk(v) {
  var nx = px + dirX * v, ny = py + dirY * v;
  if (free(nx, py)) px = nx;
  if (free(px, ny)) py = ny;
  spd += (Math.abs(v) - spd) * 0.3; odo += v;
  var o = Math.sin(ph); ph += spd * 4; if (o > 0 && Math.sin(ph) <= 0) { steps += 2; if (steps - stepsSaved >= 100) { save('steps', steps); stepsSaved = steps; } }
  hz = 13.5 + Math.sin(ph) * 0.45 * Math.min(1, spd * 18);
}
function update() {
  T++;
  if (++lodN == 30) {                                 // how fast are we really? (t() = seconds)
    var tn = t();
    if (lodT > 0 && T > 90 && ST != 3) { if ((tn - lodT) / 30 > 0.04) { if (++lodSlow >= 2 && lod < 2) { lod++; lodSlow = 0; MAXD = lod > 1 ? 9 : 11; DARKW = 0; if (lod > 1) { RS = 3; CW = 9; } } } else lodSlow = 0; }
    lodT = tn; lodN = 0;
  }
  if (flash > 0 && --flash == 0) led(0);
  if (calWarn > 0) calWarn--;
  var k = key(), i;
  if (!tick && tickQ.length) { tick = tickQ.shift(); tickX = 159; }
  if (tick) { tickX -= 3; if (tickX < -textw(tick, 3)) tick = ''; }
  for (i = 0; i < flies.length; i++) { var f = flies[i]; f.p += 0.03; f.x += Math.cos(f.p * 1.3) * 0.01; f.y += Math.sin(f.p) * 0.01; }
  if (raining) for (i = 0; i < rain.length; i++) { var d = rain[i]; d[1] += d[2]; d[0] -= 0.08; if (d[1] >= ROWS) { d[1] = -1 - rnd(4); d[0] = rnd(COLS + 6); } }
  for (i = fw.length - 1; i >= 0; i--) { var p = fw[i]; p[0] += p[2]; p[1] += p[3]; p[3] += 0.03; if (--p[4] <= 0) fw.splice(i, 1); }

  if (ST == 0) { title(k); autoWalk(0.045); return; }
  if (ST == 1) { calib(k); return; }
  if (ST == 3) { if (btnp() || k == 'ENTER' || k == ' ') { ST = 2; level(); } return; }

  // walking (after a moment to hold it level: that's what "level" means from then on)
  if (ready > 0) { if (--ready == 0) level(); return; }
  if (btnp() || k == 'ENTER' || k == 'm' || k == 'M') { ST = 3; save('steps', steps); stepsSaved = steps; return; }
  if (MK == 0) autoWalk(0.05);
  else {
    stick();
    yaw += sx * 0.055; face();
    walk(sy > 0 ? sy * 0.075 : sy * 0.035);
  }
  for (i = 0; i < 8; i++) {
    if (found & (1 << i)) continue;
    var P = PLACES[i], dx = px - P.x, dy = py - P.y;
    if (dx * dx + dy * dy < 3.2) {
      found |= 1 << i; nFound++; save('found', found); led(PAL[6]); flash = 25;
      say(P.n + ' ' + nFound + '/8');
      if (nFound == 8) say('ALL 8 FOUND!');
      goal = -1;
    }
  }
  if (nFound == 8 && T % 40 == 0) {                    // fireworks over the town
    var c = 8 + rnd(37), r = 3 + rnd(5), n = 12;
    for (i = 0; i < n; i++) { var a = i * 6.2832 / n; fw.push([c, r, Math.cos(a) * 0.5, Math.sin(a) * 0.35, 18 + irnd(8)]); }
  }
}

// the town walks you round: to a place, along the streets (a breadth-first search on the map)
var PREV = [], QQ = [];
function route(tx, ty) {
  var s = (py | 0) * MW + (px | 0), g = (ty | 0) * MW + (tx | 0), i, h, t;
  for (i = 0; i < MW * MH; i++) PREV[i] = -2;
  PREV[s] = -1; QQ.length = 0; QQ.push(s); h = 0; t = -1;
  while (h < QQ.length) {
    var c = QQ[h++], cx = c % MW, cy = (c / MW) | 0;
    if (Math.abs(cx - (g % MW)) <= 1 && Math.abs(cy - ((g / MW) | 0)) <= 1) { t = c; break; }
    var nb = [c - 1, c + 1, c - MW, c + MW];
    for (i = 0; i < 4; i++) {
      var n = nb[i];
      if (n < 0 || n >= MW * MH || PREV[n] != -2 || !PASS[n]) continue;
      if ((i < 2) && ((n / MW) | 0) != cy) continue;
      PREV[n] = c; QQ.push(n);
    }
  }
  path = [];
  while (t >= 0) { path.unshift(t); t = PREV[t]; }
  pathI = 1;
}
function autoWalk(v) {
  if (goal < 0 || pathI >= path.length) {
    if (ST == 2 && nFound < 8) { for (var i = 0; i < 8; i++) { var j = (tour + i) % 8; if (!(found & (1 << j))) { goal = j; break; } } }
    else goal = (goal + 1 + irnd(7)) % 8;
    tour = goal;
    route(PLACES[goal].x, PLACES[goal].y);
    if (path.length < 2) { goal = -1; return; }
  }
  var c = path[pathI], tx = (c % MW) + 0.5, ty = ((c / MW) | 0) + 0.5, dx = tx - px, dy = ty - py;
  if (dx * dx + dy * dy < 0.12) { pathI++; return; }
  var e = Math.atan2(dy, dx) - yaw; e = Math.atan2(Math.sin(e), Math.cos(e));
  yaw += Math.max(-0.05, Math.min(0.05, e)); face();
  var ox = px, oy = py;
  walk(Math.abs(e) < 0.5 ? v : 0.008);
  if (Math.abs(px - ox) + Math.abs(py - oy) < 0.001 && Math.abs(e) < 0.5) { if (++stuck > 20) { goal = -1; stuck = 0; } } else stuck = 0;
}

// ------------------------------------------------------------------ title + calibration
function title(k) {
  if (btnp()) { downT = T; picked = false; }
  if (btn() && downT >= 0 && !picked && T - downT > 18) { picked = true; pick(); }
  if (btnr()) { if (downT >= 0 && !picked) sel = (sel + 1) % items.length; downT = -1; }
  if (k == 'DOWN' || k == 'RIGHT') sel = (sel + 1) % items.length;
  if (k == 'UP' || k == 'LEFT') sel = (sel + items.length - 1) % items.length;
  if (k == 'ENTER') pick();
}
function pick() {
  var it = items[sel];
  if (it == 'WALK' || it == 'GO ON') {
    if (MK == 2 && !calOk) { ST = 1; calStep = 0; return; }
    start();
  } else if (it == 'NEW WALK') { found = 0; nFound = 0; save('found', 0); menu(); sel = 0; }
  else if (it == 'CALIBRATE') { ST = 1; calStep = 0; }
  else if (it == 'COLOR') { setTheme((theme + 1) % THEMES.length); save('theme', theme); }
  else if (it == 'EXIT') exit();
}
function start() {
  ST = 2; px = 20.5; py = 22.5; yaw = -1.5708; face(); goal = -1; tour = 0; spd = 0; fw = [];
  raining = rnd() < 0.35; tick = ''; tickQ = [];
  ready = MK > 0 ? 40 : 1;
  if (nFound == 0) say('FIND 8 PLACES');
  else if (nFound < 8) say((8 - nFound) + ' MORE TO FIND');
}
function calib(k) {
  var g = norm(up());
  if (btnp() || k == 'ENTER') {
    if (calStep == 0) { CN = g; calStep = 1; }
    else {
      var d = flat(g, CN);
      if (dot(g, CN) > 0.97) { calWarn = 40; return; }    // not tilted enough
      if (calStep == 1) { CR = d; calStep = 2; }
      else {
        var q = dot(d, CR); CD = norm([d[0] - CR[0] * q, d[1] - CR[1] * q, d[2] - CR[2] * q]);
        for (var i = 0; i < 3; i++) { save('n' + i, CN[i]); save('r' + i, CR[i]); save('d' + i, CD[i]); }
        save('cal', 1); calOk = true; menu(); start();
      }
    }
  }
}

// ------------------------------------------------------------------ drawing: dots
function dcol(c, a, b, l, m) {                       // one column of dots, rows a..b (b not included)
  a = Math.floor(a + 0.5); b = Math.floor(b + 0.5);
  if (b <= a) { if (!m) return; b = a + 1; }
  if (a < 0) a = 0; if (b > ROWS) b = ROWS;
  if (b > a) rect(c * 3, 1 + a * 3, 3, (b - a) * 3, PAL[l]);
}
function fog(b, d) { var l = Math.round(b - d * 0.16); return l < 1 ? 1 : l; }
function mask() {                                    // the gaps between the dots
  var i;
  for (i = 2; i < 160; i += 3) rect(i, 0, 1, 86, 0);
  rect(159, 0, 1, 86, 0);
  for (i = 0; i < 86; i += 3) rect(0, i, 160, 1, 0);
  rect(0, 85, 160, 1, 0);
}
function big(s, r, l) { var w = textw(s, 3), c = Math.max(0, Math.floor((COLS - (w + 3) / 3) / 2)); text(s, c * 3, 1 + r * 3, PAL[l], 3); }
function small(s, x, y, c) { var w = textw(s); rect(x - 2, y - 2, w + 4, 11, 0); text(s, x, y, c); }

function draw() {
  if (ST == 1) { drawCal(); return; }
  if (ST == 3) { drawMap(); return; }
  world();
  if (ST == 0) {
    rect(0, 1 + 3, 160, 27, PAL[0]); big('DOT WALK', 2, 6);
    rect(0, 1 + 17 * 3, 160, 27, PAL[0]);
    var it = items[sel];
    big(it == 'COLOR' ? THEMES[theme][0] : it, 18, (T >> 3) & 1 ? 7 : 6);
  } else if (tick) { rect(0, 0, 160, 28, PAL[0]); text(tick, tickX, 4, PAL[6], 3); }
  mask();
  if (ST == 2 && ready > 0 && MK > 0) small('HOLD IT LEVEL ...', 80 - (textw('HOLD IT LEVEL ...') >> 1), 66, PAL[(T >> 2) & 1 ? 7 : 5]);
  if (ST == 0) {
    var hint = { WALK: 'HOLD: START', 'GO ON': 'HOLD: GO ON', 'NEW WALK': 'HOLD: FORGET ALL', CALIBRATE: 'HOLD: SET TILT', COLOR: 'HOLD: NEXT COLOR', EXIT: 'HOLD: TO MENU' }[items[sel]];
    small('FOUND ' + nFound + '/8   STEPS ' + steps, 4, 34, PAL[5]);
    small(hint, 4, 76, PAL[6]);
    small('TAP: NEXT', 160 - 2 - textw('TAP: NEXT'), 76, PAL[4]);
  }
}
function world() {
  var c, i, j, n, d, t, h, top, bot, lim, u, l, s, f;
  cls(PAL[0]);
  // the sky: stars, the moon (and fireworks)
  for (i = 0; i < stars.length; i++) {
    s = stars[i]; d = s[0] - yaw; d = d - Math.floor((d + 3.1416) / 6.2832) * 6.2832;
    if (d < -0.75 || d > 0.75) continue;
    c = COLS / 2 + d * (1 + d * d * 0.4) * FOC; if (c < 0 || c >= COLS) continue;    // about tan(d)
    dcol(c | 0, hz - s[1] * FOC, hz - s[1] * FOC + 1, s[2] + ((T + i * 7) % 50 < 3 ? 0 : 1), 1);
  }
  d = 0.9 - yaw; d = d - Math.floor((d + 3.1416) / 6.2832) * 6.2832;
  var moon = -99;
  if (d > -0.8 && d < 0.8) { moon = COLS / 2 + Math.tan(d) * FOC; t = hz - 0.42 * FOC; for (i = -2; i <= 2; i++) { h = i * i > 1 ? 1 : 2; dcol((moon + i) | 0, t - h, t + h, i == 2 ? 4 : 6, 1); } }
  for (i = 0; i < fw.length; i++) { f = fw[i]; dcol(f[0] | 0, f[1], f[1] + 1, f[4] > 10 ? 7 : f[4] > 5 ? 5 : 3, 1); }
  // the ground near you
  f = odo - Math.floor(odo);                           // the street: lines across it, 1 apart, coming toward you
  for (i = 1; i < 10; i++) { d = i - f; if (d < 0.4) continue; t = Math.floor(hz + EYE * FOC / d + 0.5); if (t < ROWS) rect(0, 1 + t * 3, 160, 3, PAL[d < 2.5 ? 3 : d < 5 ? 2 : 1]); }

  // the town: one ray per column, through every wall it can still see over. A ray stops as soon as nothing
  // further away could show above what it has drawn (only the clock tower is taller than the offices).
  d = (19.5 - px) * dirX + (6.5 - py) * dirY;
  var tcol = d > 0.3 ? COLS / 2 + ((19.5 - px) * plX + (6.5 - py) * plY) / d * FOC : -99, thw = d > 0.3 ? 0.8 * FOC / d + 1 : 0;
  for (c = 0; c < COLS; c += RS) {
    var k = (c + RS / 2 - COLS / 2) / FOC, rdx = dirX + plX * k, rdy = dirY + plY * k, mx = px | 0, my = py | 0, x3 = c * 3, nf = 0, fo = c * 6;
    var ddx = rdx == 0 ? 1e9 : Math.abs(1 / rdx), ddy = rdy == 0 ? 1e9 : Math.abs(1 / rdy), stx, sty, sdx, sdy, side = 0, prev = 0;
    var hm = (c + 0.5 - tcol) * (c + 0.5 - tcol) < thw * thw ? 5.5 * FOC : 2.7 * FOC, a, b;
    if (rdx < 0) { stx = -1; sdx = (px - mx) * ddx; } else { stx = 1; sdx = (mx + 1 - px) * ddx; }
    if (rdy < 0) { sty = -1; sdy = (py - my) * ddy; } else { sty = 1; sdy = (my + 1 - py) * ddy; }
    lim = ROWS;
    var ix = (my + 2) * PW + mx + 2, sy3 = sty * PW, dstop = MAXD;    // dstop: past this nothing can show any more
    for (n = 0; n < 40; n++) {
      if (sdx < sdy) { d = sdx; sdx += ddx; ix += stx; side = 0; } else { d = sdy; sdy += ddy; ix += sy3; side = 1; }
      if (d > dstop) break;
      h = PH[ix];
      if (h <= prev) { prev = h < 0 ? 0 : h; if (h == -2) break; if (h != -1) continue; }
      if (h < 0) {
        if (h == -2) break;                              // off the map
        if (d > 0.3 && rdy > 0) {                        // the sea: the shore line, the moon on the water
          bot = hz + EYE * FOC / d;
          if (bot < lim) {
            a = (bot + 0.5) | 0; if (a < ROWS) rect(x3, 1 + a * 3, CW, 3, PAL[2]);
            for (i = 0; i < 2; i++) {
              s = (((c * 7 + i * 13 + (T >> 3) * 5) * 2654435761) >>> 20) & 255;
              b = a - 1 - (s & 15) % Math.max(1, a - ((hz + 1) | 0));
              if (b > hz && (s < 60 || (c - moon) * (c - moon) < 2.5)) rect(x3, 1 + b * 3, CW, 3, PAL[(c - moon) * (c - moon) < 2.5 ? 5 : 2]);
            }
          }
          break;                                         // nothing but sea from here on
        }
        prev = 0; continue;
      }
      t = PT[ix]; mx = (ix % PW) - 2; my = ((ix / PW) | 0) - 2;
      {
        top = hz - (h - EYE) * FOC / d; bot = hz + EYE * FOC / d;
        if (bot > lim) bot = lim;
        if (top < bot) {
          l = BASE[t] - side * 0.8 - d * 0.16; l = l < 1 ? 1 : (l + 0.5) | 0;
          a = (top + 0.5) | 0; b = (bot + 0.5) | 0; if (a < 0) a = 0; if (b > ROWS) b = ROWS;
          if (b > a) rect(x3, 1 + a * 3, CW, (b - a) * 3, PAL[l]);
          if (d < 12 && LITP[t] > 0) {                   // windows: some lit, some dark
            u = side == 0 ? py + d * rdy : px + d * rdx; u -= Math.floor(u);
            var wi = t == 3 ? (u > 0.12 && u < 0.42 ? 0 : u > 0.58 && u < 0.88 ? 1 : -1) : t == 2 ? (u > 0.33 && u < 0.67 ? 0 : -1) : t == 5 ? (u > 0.1 && u < 0.9 ? 0 : -1) : (u > 0.28 && u < 0.72 ? 0 : -1);
            if (wi >= 0) {
              if (t == 5) { span2(c, 0.2, 0.75, 5, d, top, bot); span2(c, 0.95, 1.12, 7, d, top, bot); }
              else {
                var seed = (mx * 7919 + my * 104729 + side * 31 + wi * 977) | 0, lw = d < 5 ? PAL[6] : PAL[5], fy = FOC / d;
                for (var w = 0; 0.48 + w * 0.5 < h - 0.05; w++) {
                  var hh = (((seed + w * 1299709) * 2654435761) >>> 22) & 1023, lit = hh < LITP[t] * 1024;
                  if (!lit && d > DARKW) continue;            // dark windows only close by
                  a = hz - (w * 0.5 - 0.02) * fy; b = hz - (w * 0.5 - 0.2) * fy;     // 0.3 + w * 0.5 + (0..0.18) - EYE
                  if (a < top) a = top; if (b > bot) b = bot;
                  if (b < a - 0.5) continue;
                  a = (a + 0.5) | 0; b = (b + 0.5) | 0; if (b <= a) b = a + 1; if (a < 0) a = 0; if (b > ROWS) b = ROWS;
                  if (b > a) rect(x3, 1 + a * 3, CW, (b - a) * 3, lit ? lw : PAL[1]);
                }
              }
            }
          }
          if (t == 4) {                                   // the clock
            u = side == 0 ? py + d * rdy : px + d * rdx; u -= Math.floor(u); u -= 0.5;
            if (u * u < 0.1) { s = Math.sqrt(0.1 - u * u); span2(c, 5 - s, 5 + s, 5, d, top, bot); if (u * u < 0.004) span2(c, 4.95, 5.25, 1, d, top, bot); }
            if (u * u < 0.03) span2(c, 5.8, 6, (T >> 4) & 1 ? 7 : 3, d, top, bot);
          }
        }
        if (top < lim) { lim = top; if (lim <= 0) dstop = 0; else if (lim < hz) { dstop = hm / (hz - lim); if (dstop > MAXD) dstop = MAXD; } }
        if (nf < 6) { FD[fo + nf] = d; FL[fo + nf] = lim; nf++; }
      }
      prev = h;
    }
    FN[c] = nf;
    for (j = 1; j < RS && c + j < COLS; j++) { FN[c + j] = nf; for (i = 0; i < nf; i++) { FD[fo + 6 * j + i] = FD[fo + i]; FL[fo + 6 * j + i] = FL[fo + i]; } }
  }
  sprites();
  if (raining) for (i = 0; i < rain.length; i++) { s = rain[i]; if (s[1] >= 0 && s[0] < COLS) dcol(s[0] | 0, s[1], s[1] + 1, 2, 1); }
}
function span2(c, y0, y1, l, d, top, bot) {        // a part of a wall, kept inside what we see of it
  var a = hz - (y1 - EYE) * FOC / d, b = hz - (y0 - EYE) * FOC / d;
  if (a < top) a = top; if (b > bot) b = bot;
  if (b < a - 0.5) return;
  a = (a + 0.5) | 0; b = (b + 0.5) | 0; if (b <= a) b = a + 1; if (a < 0) a = 0; if (b > ROWS) b = ROWS;
  if (b > a) rect(c * 3, 1 + a * 3, CW, (b - a) * 3, PAL[l]);
}

// ------------------------------------------------------------------ the things in the street
var VIS = [];
var DW = 3;
function dr(x, a, b, col) {                          // rows a..b of dots in the column at pixel x (DW wide), cut at CLIP
  if (b > CLIP) b = CLIP;
  a = (a + 0.5) | 0; b = (b + 0.5) | 0; if (b <= a) b = a + 1;
  if (a < 0) a = 0; if (b > CLIP) b = CLIP;
  if (b > a) rect(x, 1 + a * 3, DW, (b - a) * 3, col);
}
function sprites() {
  var i, j, s, dx, dy, dep, hw, sc, c, c0, c1, u, a, f, y0, q, l2, l3, n;
  VIS.length = 0;
  n = SP.length + flies.length;
  for (i = 0; i < n; i++) {
    s = i < SP.length ? SP[i] : flies[i - SP.length];
    dx = s.x - px; dy = s.y - py; dep = dx * dirX + dy * dirY;
    if (dep < 0.25 || dep > 15) continue;
    sc = COLS / 2 + (dx * plX + dy * plY) / dep * FOC; hw = (s.r || 0.05) * FOC / dep;
    if (sc + hw < 0 || sc - hw > COLS) continue;
    s.d = dep; s.sc = sc; s.hw = hw;
    for (j = VIS.length; j > 0 && VIS[j - 1].d < dep; j--) VIS[j] = VIS[j - 1];
    VIS[j] = s;
  }
  for (i = 0; i < VIS.length; i++) {
    s = VIS[i]; dep = s.d; f = FOC / dep; y0 = hz + EYE * f;       // y0 = the row of the ground under it
    l2 = PAL[fog(3, dep)]; l3 = PAL[fog(4, dep)];
    c0 = Math.floor(s.sc - s.hw); c1 = Math.floor(s.sc + s.hw);
    var st = 1;                                       // wide things: in steps of RS columns, like the walls
    if (s.hw < 0.5) c0 = c1 = Math.floor(s.sc);
    else if (RS > 1 && s.hw > 1.2) { st = RS; c0 = Math.floor(c0 / RS) * RS; }
    DW = st * 3;
    if (c0 < 0) c0 = 0; if (c1 >= COLS) c1 = COLS - 1;
    for (c = c0; c <= c1; c += st) {
      CLIP = ROWS;
      for (j = 0; j < FN[c] && FD[c * 6 + j] < dep; j++) CLIP = FL[c * 6 + j];
      if (CLIP <= 0) continue;
      u = s.hw < 0.5 ? 0 : (c + st * 0.5 - s.sc) / s.hw; a = u < 0 ? -u : u;
      var x = c * 3;
      switch (s.k) {                                  // heights in the world (0 = the street) times f = rows
        case 't':                                     // a tree
          if (a < 0.2) dr(x, y0 - 0.6 * f, y0, l2);
          if (a < 1) { q = Math.sqrt(1 - a * a) * 0.42 * f; dr(x, y0 - 0.95 * f - q, y0 - 0.95 * f + q, l3); }
          break;
        case 'Q':                                     // the old oak, with a few dots of light in it
          if (a < 0.22) dr(x, y0 - f, y0, l2);
          if (a < 1) { q = Math.sqrt(1 - a * a) * 0.85 * f; dr(x, y0 - 1.65 * f - q, y0 - 1.65 * f + q, l3);
            if (((c * 7 + (T >> 4) * 3) % 11) == 0) dr(x, y0 - 1.6 * f, y0 - 1.5 * f, PAL[5]); }
          break;
        case 'l':                                     // a street lamp
          dr(x, y0 - 1.3 * f, y0, l2); dr(x, y0 - 1.42 * f, y0 - 1.3 * f, PAL[7]);
          break;
        case 'k':                                     // a bench
          dr(x, y0 - 0.26 * f, y0 - 0.18 * f, l3); if (u < -0.3) dr(x, y0 - 0.45 * f, y0 - 0.3 * f, l2); if (a > 0.75) dr(x, y0 - 0.18 * f, y0, l2);
          break;
        case 'F':                                     // the fountain
          if (a < 1) { dr(x, y0 - 0.25 * f, y0, l3); dr(x, y0 - 0.3 * f, y0 - 0.25 * f, PAL[fog(5, dep)]); }
          if (a < 0.18) dr(x, y0 - (0.85 + Math.sin(T * 0.3) * 0.05) * f, y0 - 0.3 * f, PAL[5]);
          else if (a < 0.7) { q = (0.85 - ((T * 0.04 + a) % 0.6) * a * 1.2) * f; dr(x, y0 - q, y0 - q + 0.05 * f, PAL[4]); }
          break;
        case 'S':                                     // the statue with a torch
          if (a < 1) dr(x, y0 - 0.4 * f, y0, l2);
          if (a < 0.45) dr(x, y0 - 1.05 * f, y0 - 0.4 * f, l3);
          if (a < 0.28) dr(x, y0 - 1.3 * f, y0 - 1.08 * f, l3);
          if (u > 0.45 && u < 0.75) { dr(x, y0 - 1.42 * f, y0 - 0.95 * f, l3); dr(x, y0 - 1.55 * f, y0 - 1.42 * f, PAL[(T >> 2) & 1 ? 7 : 6]); }
          break;
        case 'M':                                     // the radio mast
          if (a < 0.25) { dr(x, y0 - 5 * f, y0, l2); dr(x, y0 - 5.18 * f, y0 - 5 * f, PAL[(T % 30) < 15 ? 7 : 1]); }
          else if (a > 0.6) dr(x, y0 - 1.6 * f, y0, l2);
          if (a < 0.6) { dr(x, y0 - 1.68 * f, y0 - 1.6 * f, l2); dr(x, y0 - 3.28 * f, y0 - 3.2 * f, l2); }
          break;
        case 'L':                                     // the lighthouse: its light turns round
          if (a < 0.8) {
            dr(x, y0 - 2.4 * f, y0, l2); dr(x, y0 - 1.1 * f, y0 - 0.8 * f, PAL[fog(4, dep)]); dr(x, y0 - 1.9 * f, y0 - 1.6 * f, PAL[fog(4, dep)]);
            q = Math.cos(T * 0.06 - Math.atan2(py - s.y, px - s.x));
            dr(x, y0 - 2.75 * f, y0 - 2.4 * f, PAL[q > 0.85 ? 7 : q > 0.3 ? 5 : 2]);
          }
          break;
        case 'K':                                     // the cat: mostly two eyes in the dark
          if (a < 0.8) dr(x, y0 - 0.18 * f, y0, l2);
          if (u > 0.2) dr(x, y0 - 0.34 * f, y0 - 0.15 * f, l2);
          if ((u > 0.1 && u < 0.3) || u > 0.7) dr(x, y0 - 0.41 * f, y0 - 0.34 * f, l2);
          if (((u > 0.3 && u < 0.45) || (u > 0.6 && u < 0.75) || s.hw < 1.5) && (T % 110) > 4) dr(x, y0 - 0.28 * f, y0 - 0.24 * f, PAL[6]);
          break;
        default:                                      // a firefly
          if ((T + (s.p * 10 | 0)) % 40 < 26) { q = (s.h + Math.sin(s.p * 3) * 0.1) * f; dr(x, y0 - q - 0.05 * f, y0 - q, PAL[5]); }
      }
    }
  }
}

// ------------------------------------------------------------------ the map and the calibration
function drawMap() {
  var i, r, c, x0 = 6;
  cls(PAL[0]);
  for (i = 0; i < RUNS.length; i++) { r = RUNS[i]; rect((x0 + r[1]) * 3, 1 + r[0] * 3, (r[2] - r[1] + 1) * 3, 3, PAL[r[3] == 7 ? 1 : r[3] == 6 ? 2 : r[3] == 3 || r[3] == 4 ? 4 : 3]); }
  for (i = 0; i < SP.length; i++) { r = SP[i]; if (r.k == 't' || r.k == 'l') dcol(x0 + (r.x | 0), r.y | 0, (r.y | 0) + 1, r.k == 'l' ? 5 : 3, 1); }
  for (i = 0; i < 8; i++) if (found & (1 << i)) dcol(x0 + (PLACES[i].x | 0), PLACES[i].y | 0, (PLACES[i].y | 0) + 1, (T >> 3) & 1 ? 7 : 5, 1);
  c = x0 + (px | 0); r = py | 0;
  if ((T >> 2) & 1) dcol(c, r, r + 1, 7, 1);
  dcol(x0 + ((px + dirX * 1.2) | 0), (py + dirY * 1.2) | 0, ((py + dirY * 1.2) | 0) + 1, 6, 1);
  mask();
  small(nFound + '/8', 160 - textw(nFound + '/8') - 2, 3, PAL[6]);
  small('TAP', 160 - textw('TAP') - 2, 74, PAL[4]);
}
function drawCal() {
  cls(PAL[0]);
  var g = norm(up()), w = ['LEVEL', 'RIGHT', 'FORWARD'][calStep];
  big(w, 3, 6);
  var bx = 26 + dot(g, [1, 0, 0]) * 20, by = 18 + dot(g, [0, 1, 0]) * 8;      // a live dot: the board's tilt
  for (var i = -1; i <= 1; i++) dcol((bx + i) | 0, by - 1, by + 1, 6, 1);
  mask();
  small(['HOLD IT THE WAY YOU WALK', 'TILT IT TO THE RIGHT', 'TILT THE TOP AWAY FROM YOU'][calStep], 4, 58, PAL[6]);
  small(calWarn > 0 ? 'TILT IT MORE' : 'THEN TAP  (' + (calStep + 1) + '/3)', 4, 72, calWarn > 0 ? PAL[7] : PAL[5]);
}
