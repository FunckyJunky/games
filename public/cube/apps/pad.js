// PHONEPAD: the cube and the phone talk. Open the cube on error.blue/cube, APPS tab:
// what you type there shows up here; tapping a pad here lights it up on the phone.
var msgs = ['OPEN ERROR.BLUE/CUBE', 'ON YOUR PHONE'], lit = -1, litAt = 0;
var PADS = ['A', 'B', 'C', 'D'];

function onmsg(d) {
  if (d && d.text) { msgs.push(String(d.text).toUpperCase().slice(0, 28)); if (msgs.length > 4) msgs.shift(); beep('msg'); }
}
function draw() {
  cls();
  text('PHONEPAD', 240, 24, FG, 2, 1);
  box(20, 80, 440, 150, FG);
  for (var i = 0; i < msgs.length; i++) text(msgs[i], 40, 96 + i * 32, i == msgs.length - 1 ? FG : DIM, 1, 0);
  for (var p = 0; p < 4; p++) button(PADS[p], 28 + p * 108, 260, 96, 150, p == lit && ms() - litAt < 300);
  text('TAP A PAD', 240, 430, DIM, 1, 1, false);
}
function touch(type, x, y) {
  if (type != 'down' || y < 250 || y > 420) return;
  var p = Math.floor((x - 28) / 108);
  if (p < 0 || p > 3) return;
  lit = p; litAt = ms(); tone(440 + p * 110, 80);
  send({ pad: PADS[p] });
}
