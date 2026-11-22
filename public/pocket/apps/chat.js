// ERROR-CHAT for pocket.DOS - the error.blue chat, on the little screen.
// One button: TAP = next, HOLD = choose / send a quick message. A Bluetooth keyboard (or the PC keyboard
// in the browser): type, ENTER = send, UP/DOWN = scroll. Same channels as error.blue/apps/error-chat.
// When someone in the channel shares their camera (in the browser), WATCH it here: the picture comes
// as tiny 4-bit grey frames, drawn in the screen colour.
var BG = 0x031208, FG = 0x39ff7a, DIM = 0x1b7a3c, HI = 0xb8ffd0, DARK = 0x0a2a14, WARN = 0xffb43a;
(function () {                                           // the colours chosen in SETUP
  var s = typeof sys == 'function' ? sys() : null, t = s && s.theme;
  if (t) { FG = t.fg; DIM = t.dim; BG = t.bg; HI = t.hi; DARK = t.dark; }
})();
var ST = 0, T = 0, sel = 0, items = [], name = '', code = '';
var pick = '', pickFor = '', pickI = 0;                  // entering a code / name letter by letter
var lines = [], last = 0, who = [], pollAt = 0, busy = false, err = '', scroll = 0;
var typed = '', phrase = 0, sent = '', sentT = 0;
var downAt = -1, holdDone = false, keyT = -99;
var cams = [], camId = '', camName = '', fr = null, frBusy = false, frAt = 0, half = false, frN = 0, fps = 0, fpsT = 0;
var frMsg = '', offT = 0, sub = '', subT = 0, watchT = -999;
var FAST = typeof img4 != 'function';                    // no img4 (older pocket.DOS): small pictures, drawn in JS
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
  ST = 2; lines = []; last = 0; who = []; err = ''; scroll = 0; pollAt = 0; typed = ''; cams = []; phrase = 0;
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
      if (x.kind == 'msg') { add(x.name, x.text); if (last) { sub = latin(x.name) + ': ' + latin(x.text); subT = 120; } }
      else if (x.kind == 'join' || x.kind == 'leave') add('*', x.name + (x.kind == 'join' ? ' IS HERE' : ' LEFT'));
      if (x.kind == 'msg' && x.name != name && last) { led(FG); ledT = 4; }
    }
    if (j.last) last = Math.max(last, j.last);
    who = j.who || who;
    if (j.cams) setCams(j.cams);
  });
  if (!busy) err = 'BUSY...';
}
var ledT = 0;
// cameras in the channel: they come first in the TAP list
function setCams(c) {
  for (var i = 0; i < c.length; i++) {
    var known = false;
    for (var k = 0; k < cams.length; k++) if (cams[k].id == c[i].id) known = true;
    if (!known && ST == 2) {
      add('*', c[i].name + ' SHARES A CAMERA - HOLD TO WATCH');
      phrase = i; led(FG); ledT = 10;
    }
  }
  var had = cams.length;
  cams = c;
  if (ST == 2) {
    if (phrase >= cams.length && phrase < had) phrase = cams.length;   // a camera went away
    if (phrase >= cams.length + PHRASES.length) phrase = 0;
  }
}
function choice(i) { return i < cams.length ? '[WATCH ' + latin(cams[i].name).toUpperCase() + ']' : PHRASES[i - cams.length]; }
function choices() { return cams.length + PHRASES.length; }

// ------------------------------------------------------------------ watching a camera
function watch(c) {
  ST = 3; camId = c.id; camName = latin(c.name).toUpperCase(); fr = null; frMsg = 'CONNECTING...'; offT = 0;
  frN = 0; fps = 0; fpsT = T; sub = ''; subT = 0; half = FAST; watchT = T; glitch(6);
}
function pollFrame() {
  if (frBusy) return;
  var path = '/api/chat/' + code + '/frame?id=' + camId + '&name=' + encodeURIComponent(name) + (half ? '&s=2' : '');
  frBusy = get(path, function (status, text) {
    frBusy = false; frAt = T + 1;
    if (status != 200) { frMsg = status < 0 ? 'NO CONNECTION' : 'ERROR ' + status; frAt = T + 30; return; }
    var j; try { j = JSON.parse(text); } catch (e) { frMsg = 'BAD PICTURE'; return; }
    if (j.cams) cams = j.cams;
    if (j.off) { frMsg = 'CAMERA OFF'; fr = null; if (!offT) offT = T; frAt = T + 15; return; }
    offT = 0;
    if (j.name) camName = latin(j.name).toUpperCase();
    if (j.id) camId = j.id;
    if (j.wait) { frMsg = 'WAITING FOR ' + camName + '...'; frAt = T + 8; return; }
    if (!fr || j.seq != fr.seq) frN++;
    fr = j; frMsg = '';
  });
  if (!frBusy) frAt = T + 4;
}
// img4 in JS (older pocket.DOS): 4-bit pixels as k x k boxes
var B64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
function img4js(s, x0, y0, w, h, k) {
  var pal = [], i, acc = 0, bits = 0, px = 0, n = w * h;
  for (i = 0; i < 16; i++) pal.push(mix(BG, FG, i / 15));
  for (i = 0; i < s.length && px < n; i++) {
    var v = B64.indexOf(s.charAt(i)); if (v < 0) continue;
    acc = ((acc << 6) | v) & 0xffff; bits += 6;
    if (bits < 8) continue;
    bits -= 8;
    var b = (acc >> bits) & 255;
    rect(x0 + (px % w) * k, y0 + ((px / w) | 0) * k, k, k, pal[b >> 4]); px++;
    if (px < n) { rect(x0 + (px % w) * k, y0 + ((px / w) | 0) * k, k, k, pal[b & 15]); px++; }
  }
}
function mix(a, b, f) {
  var r = (a >> 16) & 255, g = (a >> 8) & 255, l = a & 255;
  return (Math.round(r + (((b >> 16) & 255) - r) * f) << 16) | (Math.round(g + (((b >> 8) & 255) - g) * f) << 8) | Math.round(l + ((b & 255) - l) * f);
}
function nextCam(d) {
  if (cams.length < 2) return;
  var i = 0;
  for (var k = 0; k < cams.length; k++) if (cams[k].id == camId) i = k;
  watch(cams[(i + d + cams.length) % cams.length]);
}

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
  if (ST == 3) {                                          // watching a camera
    if (T >= frAt) pollFrame();
    if (T >= pollAt) { poll(); pollAt = T + 90; }         // new messages show at the bottom
    if (subT > 0) subT--;
    if (T - fpsT >= 30) { fps = frN; frN = 0; fpsT = T; }
    if (offT && T - offT > 90) { ST = 2; add('*', 'CAMERA OFF'); return; }
    while (k) {
      if (k == 'ESC' || k == 'BACK' || k == 'ENTER') { ST = 2; return; }
      if (k == 'RIGHT' || k == 'DOWN') nextCam(1);
      if (k == 'LEFT' || k == 'UP') nextCam(-1);
      if (k == ' ' && !FAST) half = !half;
      k = key();
    }
    b = button();
    if (b == 1) { if (cams.length > 1) nextCam(1); else if (!FAST) half = !half; }
    if (b == 2) ST = 2;
    return;
  }
  // in a channel
  if (T >= pollAt) { poll(); pollAt = T + 75; }
  while (k) {
    if (k == 'ENTER') { if (typed) { say(typed); typed = ''; } else pickChoice(); }
    else if (k == 'BACK') typed = typed.substr(0, typed.length - 1);
    else if (k == 'UP') scroll = Math.min(scroll + 1, Math.max(0, lines.length - 7));
    else if (k == 'DOWN') scroll = Math.max(0, scroll - 1);
    else if (k.length == 1 && typed.length < 120) typed += k;
    else if (k.length > 1 && k.charCodeAt(0) > 127 && typed.length < 120) typed += k;
    k = key();
  }
  b = button();
  if (b == 1) phrase = (phrase + 1) % choices();
  if (b == 2) pickChoice();
}
function pickChoice() {
  if (phrase < cams.length) watch(cams[phrase]);
  else say(PHRASES[phrase - cams.length]);
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
  } else if (ST == 3) {
    drawVideo();
  } else {
    var first = Math.max(0, lines.length - 7 - scroll);
    for (var l = 0; l < 7 && first + l < lines.length; l++) {
      var L = lines[first + l];
      text(L.t, 2, 11 + l * 9, L.sys ? DIM : L.me ? HI : FG);
    }
    if (scroll > 0) text('v', W - 7, 11, WARN);
    rect(0, 74, W, 12, DARK);
    var cam = !typed && phrase < cams.length;
    var c = typed ? '>' + latin(typed).slice(-24) + ((T >> 3) & 1 ? '_' : '') : '>' + choice(phrase);
    if (sentT > 0) c = 'SENT: ' + latin(sent).substr(0, 20);
    if (err && (T >> 4) & 1) c = err;
    text(c, 2, 77, typed ? HI : err ? WARN : cam ? ((T >> 4) & 1 ? HI : WARN) : FG);
    if (cams.length && !cam && !typed) text('CAM', W - 2 - textw('CAM'), 77, (T >> 4) & 1 ? WARN : DIM);
    if (downAt >= 0 && !holdDone) rect(0, 74, Math.min(1, (T - downAt) / 18) * W | 0, 1, HI);
    if (!typed && T < 200) { rect(14, 30, 132, 20, BG); box(14, 30, 132, 20, DIM); ct('TAP: NEXT MESSAGE', 33, FG); ct('HOLD: SEND IT', 42, FG); }
  }
  if (T < 8) glitch(8 - T);
}
function drawVideo() {
  cls(BG);
  if (fr && fr.data) {
    var k = Math.max(1, Math.min(4, Math.floor(Math.min(W / fr.w, H / fr.h))));
    var x = (W - fr.w * k) >> 1, y = (H - fr.h * k) >> 1;
    if (FAST) img4js(fr.data, x, y, fr.w, fr.h, k);
    else img4(fr.data, x, y, fr.w, fr.h, BG, FG, k);
  } else {
    for (var i = 0; i < 40; i++) rect(irnd(W), 9 + irnd(H - 9), 2 + irnd(14), 1, irnd(3) ? DARK : DIM);   // snow
    ct(frMsg || 'CONNECTING...', 40, (T >> 4) & 1 ? FG : DIM);
  }
  // the label: who, live, how many pictures a second
  var lab = camName.substr(0, 14);
  rect(0, 0, textw(lab) + 5, 9, FG); text(lab, 2, 1, BG);
  if (fr && !frMsg) {
    var r = (half ? 'FAST ' : '') + fps + '/S';
    rect(W - textw(r) - 12, 0, textw(r) + 12, 9, BG);
    if ((T >> 4) & 1) circ(W - textw(r) - 7, 4, 2, WARN);
    text(r, W - textw(r) - 2, 1, FG);
  }
  if (subT > 0 && sub) { rect(0, H - 10, W, 10, BG); text(sub.substr(0, 26), 2, H - 8, HI); }
  if (T - watchT < 150) {
    rect(0, H - 10, W, 10, BG);
    ct(cams.length > 1 ? 'TAP: NEXT CAMERA  HOLD: BACK' : FAST ? 'HOLD: BACK TO CHAT' : 'TAP: SHARP/FAST  HOLD: BACK', H - 8, DIM);
  }
  if (frMsg && (T & 63) < 3) glitch(2);
}
