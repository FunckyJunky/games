// SHOW.EXE engine checks:  node tools/lamp-show-test.mjs   (presets fit the IR budget, orders, fades, waves, sender)
import fs from 'fs';
const win = { location: { search: '' } };
globalThis.window = win; globalThis.location = win.location; globalThis.localStorage = { getItem(){return null}, setItem(){} };
new Function('window', fs.readFileSync(new URL('../public/lamp/lamp.js', import.meta.url), 'utf8'))(win);
globalThis.LAMP = win.LAMP;
new Function('window', fs.readFileSync(new URL('../public/lamp/show/engine.js', import.meta.url), 'utf8'))(win);
const E = win.ShowEngine, L = win.LAMP;
let bad = 0;
const ok = (c, m) => { console.log((c ? '  [ok] ' : '  [FAIL] ') + m); if (!c) bad++; };

// follow test: run the sender against the sim board for 60 s, compare the sim lamp with the plan
function follow(s, seed = 3, secs = 60) {
  const plan = new E.Plan(s, seed), sim = new E.SimOut(), snd = new E.Sender(sim);
  const dt = 0.04; let match = 0, n = 0, cols = 0, colsOk = 0, sent = 0;
  for (let t = 0; t < secs; t += dt) {
    sim.step(t);
    const w = plan.want(t + 0.15);
    if (snd.tick(t, w)) sent++;
    if (t < 2) continue;
    const want = plan.want(t), st = sim.state;
    const same = want.on === st.on && (!want.on || (want.mode !== 'color' ? st.mode === want.mode : st.key === want.key && st.mode === 'color' && (want.level == null || st.level === want.level)));
    n++; if (same) match++;
    if (want.on && want.mode === 'color') { cols++; if (st.on && st.key === want.key) colsOk++; }
  }
  return { match: match / n, colour: cols ? colsOk / cols : 1, kps: sent / secs, load: E.estimate(plan, 0, secs) };
}
console.log('presets:');
for (const p of E.PRESETS) {
  const r = follow(p.s);
  console.log(`  ${p.n.padEnd(10)} load ${(r.load*100).toFixed(0).padStart(4)}%  keys/s ${r.kps.toFixed(2)}  exact ${(r.match*100).toFixed(0)}%  colour ${(r.colour*100).toFixed(0)}%`);
  ok(r.load < 1.05, p.n + ' fits the IR budget');
}
// surprise shows stay within budget
let worst = 0;
const rnd = E.mulberry32(42);
for (let i = 0; i < 40; i++) { const s = E.surprise(rnd); const l = E.estimate(new E.Plan(s, 1), 0, 40); worst = Math.max(worst, l); }
ok(worst <= 0.95, 'SURPRISE ME shows fit (worst ' + (worst*100).toFixed(0) + '%)');

// determinism + weights
const s = { w: Object.assign(Object.fromEntries(E.ALL.map(k=>[k,0])), { r: 9, b: 3 }), order: 'random', bpm: 120 };
const a = new E.Plan(s, 5), b = new E.Plan(s, 5);
const seqA = [], seqB = [];
for (let t = 0; t < 100; t += 0.5) { seqA.push(a.want(t).key); seqB.push(b.want(t).key); }
ok(seqA.join() === seqB.join(), 'same seed, same show');
// with 2 colours random alternates (no repeats) -> 50/50 regardless of weights; use 3 colours to test weights
const s3 = { w: Object.assign(Object.fromEntries(E.ALL.map(k=>[k,0])), { r: 9, g: 1, b: 1 }), order: 'random', bpm: 240 };
const p3 = new E.Plan(s3, 9); const cnt = {};
for (let i = 0; i < 4000; i++) { const k = p3.want(i * 0.25 + 0.01).key; cnt[k] = (cnt[k]||0)+1; }
ok(cnt.r > cnt.g * 2.5 && cnt.r > cnt.b * 2.5, 'heavier slider = more often ' + JSON.stringify(cnt));
// cycle order + reps
const sc = { w: Object.assign(Object.fromEntries(E.ALL.map(k=>[k,0])), { r: 1, g: 5, b: 9 }), order: 'cycle', bpm: 60 };
const pc = new E.Plan(sc, 1); const sq = []; for (let i = 0; i < 12; i++) sq.push(pc.want(i + 0.5).key);
ok(sq.join(' ') === 'r g g b b b r g g b b b', 'cycle: ' + sq.join(' '));
const sb = { w: Object.assign(Object.fromEntries(E.ALL.map(k=>[k,0])), { r: 1, g: 1, b: 1, w: 1 }), order: 'bounce', bpm: 60 };
const pb = new E.Plan(sb, 1); const bq = []; for (let i = 0; i < 9; i++) bq.push(pb.want(i + 0.5).key);
ok(bq.join(' ') === 'r g b w b g r g b', 'bounce: ' + bq.join(' '));
// walk stays near on the wheel
const sw = { w: Object.assign(Object.fromEntries(E.ALL.map(k=>[k,0])), Object.fromEntries(E.WHEEL.map(k=>[k,5]))), order: 'walk', bpm: 60 };
const pw = new E.Plan(sw, 2); let far = 0, prevK = null;
for (let i = 0; i < 500; i++) { const k = pw.want(i + 0.5).key; if (prevK) { const d = Math.abs(E.WHEEL.indexOf(k) - E.WHEEL.indexOf(prevK)); if (Math.min(d, 15 - d) > 3) far++; } prevK = k; }
ok(far < 25, 'walk mostly moves to neighbours (' + far + '/500 jumps > 3)');
// rhythm: slider 9 every slot
const sr = { w: Object.assign(Object.fromEntries(E.ALL.map(k=>[k,0])), { r: 9, b: 7 }), order: 'rhythm', bpm: 60 };
const pr = new E.Plan(sr, 4); const rq = []; for (let i = 0; i < 9; i++) rq.push(pr.want(i + 0.5).key);
ok(rq.every((k) => k === 'r'), 'rhythm: the fastest colour wins every beat it fires: ' + rq.join(' '));
const sr2 = { w: Object.assign(Object.fromEntries(E.ALL.map(k=>[k,0])), { r: 6, b: 8 }), order: 'rhythm', bpm: 60 };
const pr2 = new E.Plan(sr2, 4); const rq2 = []; for (let i = 0; i < 12; i++) rq2.push(pr2.want(i + 0.5).key);
console.log('    rhythm r every 4, b every 2:', rq2.join(' '));
// fades
const fd = { w: Object.assign(Object.fromEntries(E.ALL.map(k=>[k,0])), { r: 5 }), bpm: 30, fade: 'decay', fdepth: 4, flen: 100, bright: 7 };
const pf = new E.Plan(fd, 1); const lv = []; for (let x = 0; x < 2; x += 0.25) lv.push(pf.want(x).level);
ok(lv.join() === '7,7,6,6,5,5,4,4' || lv[0] === 7 && lv[lv.length-1] <= 4, 'decay goes down 4 steps over the beat: ' + lv.join());
const fb = Object.assign({}, fd, { fade: 'blink', flen: 50 }); const pfb = new E.Plan(fb, 1);
ok(pfb.want(0.5).on && !pfb.want(1.5).on, 'blink: dark in the second half');
const deep = Object.assign({}, fd, { fade: 'decay', fdepth: 8 }); const pd = new E.Plan(deep, 1);
ok(!pd.want(1.99).on, 'a fade deeper than the dimmest step turns the lamp OFF');
const mg = { w: Object.assign(Object.fromEntries(E.ALL.map(k=>[k,0])), { r: 5 }), bpm: 60, mod: 'gate', mdepth: 5 };
const pg = new E.Plan(mg, 1); let dark = 0; for (let i = 0; i < 1000; i++) if (!pg.want(i + 0.5).on) dark++;
ok(dark > 400 && dark < 600, 'gate 5 = about half the beats dark (' + dark + '/1000)');
const ms = { w: Object.assign(Object.fromEntries(E.ALL.map(k=>[k,0])), { r: 5 }), bpm: 60, mod: 'sine', mdepth: 3, mrate: 8 };
const pm = new E.Plan(ms, 1); const ml = []; for (let i = 0; i <= 8; i++) ml.push(pm.want(i).level);
ok(ml[0] === 7 && ml[4] === 4 && ml[8] === 7, 'sine wave 3 steps over 8 beats: ' + ml.join());
// effects
const sf = { w: Object.assign(Object.fromEntries(E.ALL.map(k=>[k,0])), { r: 5, g: 5 }), bpm: 60, fxp: 100, fx: ['smooth'], fxlen: 4 };
const pfx = new E.Plan(sf, 1); const fq = []; for (let i = 0; i < 10; i++) { const w = pfx.want(i + 0.5); fq.push(w.mode === 'color' ? w.key : w.mode); }
ok(fq[0] !== 'smooth' && fq.slice(1, 5).every((m) => m === 'smooth'), 'effects: ' + fq.join(' '));
// live rebuild keeps time going
const p0 = new E.Plan(s, 3, 50); ok(p0.want(50.1).on && p0.slotAt(50.1).i === 0, 'a plan rebuilt mid-show starts at its origin');
// long run: slots trimmed, still answers
const plong = new E.Plan({ bpm: 240 }, 1); for (let t = 0; t < 3600; t += 0.7) plong.want(t); ok(plong.slots.length <= 3000 && plong.want(3600.3).on !== undefined, 'an hour at 240 bpm: ' + plong.slots.length + ' slots kept');
// sender calibration with sim
const sim = new E.SimOut(), snd = new E.Sender(sim); snd.calibrate(0, false);
for (let t = 0; t < 4; t += 0.04) sim.step(t);
ok(sim.state.level === 0 && sim.state.on && sim.inflight === 0, 'calibrate: on + 8 x down -> dimmest step, all confirmed');
console.log(bad ? bad + ' FAILED' : 'ALL ENGINE TESTS PASSED');
