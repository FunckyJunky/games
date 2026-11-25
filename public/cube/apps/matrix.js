// MATRIX: digital rain in the scheme's colour. Tap = faster / slower.
var cols = [], speed = 1;
function init() {
  fps(20);
  for (var i = 0; i < 32; i++) cols.push({ y: Math.random() * -30, v: 0.4 + Math.random() * 0.8, s: [] });
}
function update(dt) {
  for (var i = 0; i < cols.length; i++) {
    var c = cols[i];
    c.y += c.v * speed * dt / 60;
    if (c.y > 20 + Math.random() * 10) { c.y = -Math.random() * 8; c.v = 0.4 + Math.random() * 0.8; }
  }
}
function draw() {
  cls(BLACK);
  for (var i = 0; i < cols.length; i++) {
    var c = cols[i], head = Math.floor(c.y);
    for (var k = 0; k < 14; k++) {
      var row = head - k;
      if (row < 0 || row > 15) continue;
      var ch = String.fromCharCode(48 + ((row * 7 + i * 13 + Math.floor(c.y * 3) * (k == 0 ? 1 : 0)) % 43));
      text(ch, i * 15, row * 30, k == 0 ? WHITE : k < 4 ? FG : DIM, 1, 0, k < 4);
    }
  }
}
function touch(type) { if (type == 'tap') { speed = speed >= 3 ? 0.5 : speed * 2; beep('move'); } }
