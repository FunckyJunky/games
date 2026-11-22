// ERROR-CHAT for pocket.DOS - the error.blue chat, on the little screen.
// One button: TAP = next, HOLD = choose / send a quick message. A Bluetooth keyboard (or the PC keyboard
// in the browser): type, ENTER = send, UP/DOWN = scroll. Same channels as error.blue/apps/error-chat.
var BG = 0x031208, FG = 0x39ff7a, DIM = 0x1b7a3c, HI = 0xb8ffd0, DARK = 0x0a2a14, WARN = 0xffb43a;
var ST = 0, T = 0, sel = 0, items = [], name = '', code = '';
var pick = '', pickFor = '', pickI = 0;                  // entering a code / name letter by letter
var lines = [], last = 0, who = [], pollAt = 0, busy = false, err = '', scroll = 0;
var typed = '', phrase = 0, sent = '', sentT = 0;
var downAt = -1, holdDone = false, keyT = -99;
var ABC = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';
var PHRASES = ['HI!', 'HELLO FROM POCKET.DOS', 'OK', 'YES', 'NO', 'LOL', 'GG', 'BRB', 'WHO IS HERE?', '<3', ':)', 'BYE'];
// Cyrillic letters -> Latin (the little font only has Latin)
var CYR = 'абвгґдеєжзиіїйклмнопрстуфхцчшщьюяыэёъ', LAT = ['a', 'b', 'v', 'h', 'g', 'd', 'e', 'ye', 'zh', 'z', 'y', 'i', 'yi', 'y', 'k', 'l', 'm', 'n', 'o', 'p', 'r', 's', 't', 'u', 'f', 'kh', 'ts', 'ch', 'sh', 'shch', '', 'yu', 'ya', 'y', 'e', 'yo', ''];
function latin(s) {
  var o = '', i, c, k, lc;
  for (i = 0; i < s.length; i++) {
    c = s.charAt(i); lc = c.toLowerCase(); k = CYR.indexOf(lc);
    if (k >= 0) { var l = LAT[k]; o += c != lc && l ? l.charAt(0).toUpperCase() + l.substr(1) : l; }
    else o += s.charCodeAt(i) < 127 ? c : '?';
  }
  return o;
}

function init() {
  name = load('name', '');
  if (!name) { name = 'POCKET' + (10 + irnd(90)); save('name', name); }
  code = load('code', '');
  lobby();
}
function lobby() {
  ST = 0; items = [];
  if (code) items.push('JOIN #' + code);
  items.push('NEW CHANNEL', 'ENTER A CODE', 'NAME: ' + name, 'EXIT');
  if (sel >= items.length) sel = 0;
}
function newCode() { var s = ''; for (var i = 0; i < 5; i++) s += ABC.charAt(irnd(26)); return s; }
function join(c) {
  code = c; save('code', code);
  ST = 2; lines = []; last = 0; who = []; err = ''; scroll = 0; pollAt = 0; typed = '';
  add('*', 'CHANNEL #' + code + ' - SHARE THE CODE');
}

// ------------------------------------------------------------------ messages
function add(n, text) {
  var s = n == '*' ? '* ' + latin(text) : latin(n) + ': ' + latin(text);
  while (s.length > 26) {                                // wrap at 26 letters, at a space if there is one
    var cut = s.lastIndexOf(' ', 26);
    if (cut < 10) cut = 26;
    lines.push({ t: s.substr(0, cut), me: n == name, sys: n == '*' });
    s = '  ' + s.substr(cut).replace(/^ /, '');
  }
  lines.push({ t: s, me: n == name, sys: n == '*' });
  if (lines.length > 80) lines.splice(0, lines.length - 80);
  if (scroll > 0) scroll++;
}
function poll() {
  if (busy) return;
  var path = '/api/chat/' + code + '/log?after=' + last + '&name=' + encodeURIComponent(name) + (last ? '' : '&limit=20');
  busy = get(path, function (status, text) {
    busy = false;
    if (status != 200) { err = status < 0 ? 'NO CONNECTION' : 'ERROR ' + status; return; }
    err = '';
    var j; try { j = JSON.parse(text); } catch (e) { err = 'BAD ANSWER'; return; }
    var m = j.msgs || [];
    for (var i = m.length - 1; i >= 0; i--) {             // they come newest first
      var x = m[i];
      if (x.id <= last) continue;
      if (x.kind == 'msg') add(x.name, x.text);
      else if (x.kind == 'join' || x.kind == 'leave') add('*', x.name + (x.kind == 'join' ? ' IS HERE' : ' LEFT'));
      if (x.kind == 'msg' && x.name != name && last) { led(FG); ledT = 4; }
    }
    if (j.last) last = Math.max(last, j.last);
    who = j.who || who;
  });
  if (!busy) err = 'BUSY...';
}
var ledT = 0;
function say(text) {
  text = String(text).substr(0, 200);
  if (!text) return;
  var ok = post('/api/chat/' + code + '/say', { name: name, text: text }, function (status) {
    if (status == 200) { pollAt = 0; } else err = status == 429 ? 'SLOW DOWN' : 'NOT SENT (' + status + ')';
  });
  if (ok) { sent = text; sentT = 40; } else err = 'BUSY, AGAIN';
}

// ------------------------------------------------------------------ input
function button() {                                      // tap / hold from the one button
  var r = 0;
  if (T - keyT < 8) return 0;                            // a keyboard key also "presses" it
  if (btnp()) { downAt = T; holdDone = false; }
  if (btn() && downAt >= 0 && !holdDone && T - downAt > 18) { holdDone = true; r = 2; }
  if (btnr()) { if (downAt >= 0 && !holdDone) r = 1; downAt = -1; }
  return r;
}
function update() {
  T++;
  if (ledT > 0 && --ledT == 0) led(0);
  if (sentT > 0) sentT--;
  var k = key(), b;
  if (k) keyT = T;
  if (ST == 0) {                                          // the lobby
    b = button();
    if (k == 'DOWN' || k == 'RIGHT' || b == 1) sel = (sel + 1) % items.length;
    if (k == 'UP' || k == 'LEFT') sel = (sel + items.length - 1) % items.length;
    if (k == 'ENTER' || b == 2) {
      var it = items[sel];
      if (it.indexOf('JOIN') == 0) join(code);
      else if (it == 'NEW CHANNEL') join(newCode());
      else if (it == 'ENTER A CODE') { ST = 1; pickFor = 'code'; pick = ''; pickI = 0; }
      else if (it.indexOf('NAME') == 0) { ST = 1; pickFor = 'name'; pick = ''; pickI = 0; }
      else exit();
    }
    return;
  }
  if (ST == 1) {                                          // letters: TAP = next letter, HOLD = take it
    var n = ABC.length + 2, max = pickFor == 'code' ? 8 : 12;
    b = button();
    while (k) {
      if (k == 'ENTER') { done(); return; }
      if (k == 'BACK') pick = pick.substr(0, pick.length - 1);
      else if (k == 'ESC') { lobby(); return; }
      else if (k.length == 1 && /[A-Za-z0-9]/.test(k) && pick.length < max) pick += k.toUpperCase();
      k = key();
    }
    if (b == 1) pickI = (pickI + 1) % n;
    if (b == 2) {
      if (pickI == ABC.length) pick = pick.substr(0, pick.length - 1);       // DEL
      else if (pickI == ABC.length + 1) { done(); return; }                  // OK
      else if (pick.length < max) pick += ABC.charAt(pickI);
    }
    return;
  }
  // in a channel
  if (T >= pollAt) { poll(); pollAt = T + 75; }
  while (k) {
    if (k == 'ENTER') { if (typed) { say(typed); typed = ''; } else say(PHRASES[phrase]); }
    else if (k == 'BACK') typed = typed.substr(0, typed.length - 1);
    else if (k == 'UP') scroll = Math.min(scroll + 1, Math.max(0, lines.length - 7));
    else if (k == 'DOWN') scroll = Math.max(0, scroll - 1);
    else if (k.length == 1 && typed.length < 120) typed += k;
    else if (k.length > 1 && k.charCodeAt(0) > 127 && typed.length < 120) typed += k;
    k = key();
  }
  b = button();
  if (b == 1) phrase = (phrase + 1) % PHRASES.length;
  if (b == 2) say(PHRASES[phrase]);
}
function done() {
  if (pickFor == 'code') { if (pick.length >= 3) { join(pick); return; } }
  else if (pick.length >= 2) { name = pick; save('name', name); }
  lobby();
}

// ------------------------------------------------------------------ drawing (ERROR-DOS green)
function ct(s, y, c, k) { k = k || 1; text(s, (W - textw(s, k)) >> 1, y, c, k); }
function draw() {
  cls(BG);
  rect(0, 0, W, 9, FG);
  if (ST == 2) {
    text('#' + code, 2, 1, BG);
    var w = who.length + ' ON';
    text(w, W - 2 - textw(w), 1, BG);
  } else text('ERROR-CHAT', 2, 1, BG);
  if (ST == 0) {
    text('C:\\>CHAT', 3, 12, DIM);
    text('YOU ARE ' + latin(name), 3, 21, HI);
    for (var i = 0; i < items.length; i++) {
      var y = 32 + i * 9;
      if (i == sel) { rect(2, y - 1, W - 4, 9, FG); if (downAt >= 0 && !holdDone) rect(2, y - 1, Math.min(1, (T - downAt) / 18) * (W - 4) | 0, 9, HI); }
      text(items[i], 6, y, i == sel ? BG : FG);
    }
    rect(0, 77, W, 1, DIM);
    ct('TAP: NEXT   HOLD: OK', 79, DIM);
  } else if (ST == 1) {
    text(pickFor == 'code' ? 'CHANNEL CODE (3-8)' : 'YOUR NAME', 3, 13, DIM);
    rect(3, 24, W - 6, 13, DARK);
    var shown = pick + ((T >> 3) & 1 ? '_' : ' ');
    text(shown, 6, 26, HI, 1);
    // the letter wheel
    var n = ABC.length + 2;
    for (var d = -5; d <= 5; d++) {
      var j = (pickI + d + n) % n, s = j < ABC.length ? ABC.charAt(j) : j == ABC.length ? 'DEL' : 'OK';
      var x = 80 + d * 14 - (textw(s) >> 1);
      if (d == 0) { rect(x - 3, 44, textw(s) + 6, 13, FG); text(s, x, 47, BG); }
      else text(s, x, 47, Math.abs(d) < 3 ? FG : DIM);
    }
    rect(0, 66, W, 1, DIM);
    ct('TAP: NEXT LETTER', 69, DIM);
    ct('HOLD: TAKE IT (OR TYPE)', 78, DIM);
  } else {
    var first = Math.max(0, lines.length - 7 - scroll);
    for (var l = 0; l < 7 && first + l < lines.length; l++) {
      var L = lines[first + l];
      text(L.t, 2, 11 + l * 9, L.sys ? DIM : L.me ? HI : FG);
    }
    if (scroll > 0) text('v', W - 7, 11, WARN);
    rect(0, 74, W, 12, DARK);
    var c = typed ? '>' + latin(typed).slice(-24) + ((T >> 3) & 1 ? '_' : '') : '>' + PHRASES[phrase];
    if (sentT > 0) c = 'SENT: ' + latin(sent).substr(0, 20);
    if (err && (T >> 4) & 1) c = err;
    text(c, 2, 77, typed ? HI : err ? WARN : FG);
    if (downAt >= 0 && !holdDone) rect(0, 74, Math.min(1, (T - downAt) / 18) * W | 0, 1, HI);
    if (!typed && T < 200) { rect(14, 30, 132, 20, BG); box(14, 30, 132, 20, DIM); ct('TAP: NEXT MESSAGE', 33, FG); ct('HOLD: SEND IT', 42, FG); }
  }
  if (T < 8) glitch(8 - T);
}
