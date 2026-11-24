/* FUNDA WATCH: the neighbours section (WOZ values of the houses within 150 m) and the 3D map.
   Uses the helpers of the main page script ($, esc, eur, num, sign, niceTicks, API, DATA ...).
   Data: GET /api/funda/<id>/buren?n=N (src/buren.js). */
"use strict";

const NB = { data: null, n: 10, year: null, loading: false, gen: 0 };
const NB_CHOICES = [5, 10, 20, 50, 100];
try { const v = +localStorage.getItem("funda.n"); if (NB_CHOICES.includes(v)) NB.n = v; } catch {}

const kEur = (v) => v == null ? "–" : (v < 0 ? "−" : "") + "€" + Math.round(Math.abs(v) / 1000) + "k";
const sEur = (v) => (v > 0 ? "+" : v < 0 ? "−" : "±") + "€" + Math.round(Math.abs(v) / 1000) + "k";
const pct = (v, d = 1) => (v > 0 ? "+" : v < 0 ? "−" : "±") + Math.abs(v).toLocaleString("en-GB", { maximumFractionDigits: d }) + "%";
const cssv = (n) => getComputedStyle(document.documentElement).getPropertyValue(n).trim();
const short = (s, n) => (s.length > n ? s.slice(0, n - 1) + "…" : s);
const wozMap = (h) => new Map(h.woz || []);
const median = (a) => { const b = a.slice().sort((x, y) => x - y); const m = b.length >> 1; return b.length ? (b.length % 2 ? b[m] : (b[m - 1] + b[m]) / 2) : null; };

// ---------- loading ----------
function nbButtons() {
  $("nbN").innerHTML = NB_CHOICES.map((v) => `<button type="button" data-n="${v}" aria-pressed="${v === NB.n}">${v === 100 ? "All (150 m)" : v + " houses"}</button>`).join("");
}
nbButtons();
$("nbN").addEventListener("click", (e) => {
  const b = e.target.closest("button"); if (!b) return;
  NB.n = +b.dataset.n;
  try { localStorage.setItem("funda.n", NB.n); } catch {}
  nbButtons();
  loadBuren();
});
$("nbYear").addEventListener("change", () => { NB.year = +$("nbYear").value; drawBuren(); });

async function loadBuren() {
  const gen = ++NB.gen;
  NB.loading = true;
  $("nbProg").textContent = "Finding the houses around it…";
  for (let round = 0; round < 8; round++) {
    let d;
    try {
      const r = await fetch(API + "/buren?n=" + NB.n, { cache: "no-store" });
      d = await r.json();
      if (!r.ok) throw new Error(d.error || "HTTP " + r.status);
    } catch (e) {
      if (gen !== NB.gen) return;
      $("nbProg").textContent = "Couldn't load the neighbours: " + e.message;
      NB.loading = false;
      if (!NB.data) { $("mapMsg").textContent = "No neighbours yet."; }
      return;
    }
    if (gen !== NB.gen) return;
    NB.data = d;
    drawBuren();
    if (d.pending > 0 && !d.stopped) {
      const have = d.houses.filter((h) => h.woz).length;
      $("nbProg").textContent = `Looking up WOZ values… ${have} of ${d.houses.length} houses`;
      await new Promise((r) => setTimeout(r, 700));
      continue;
    }
    break;
  }
  NB.loading = false;
  const d = NB.data;
  if (!d) return;
  const got = d.houses.filter((h) => h.woz && h.woz.length).length;
  const none = d.houses.filter((h) => h.woz && !h.woz.length).length;
  $("nbProg").innerHTML = `${got} neighbours compared` + (none ? ` <span class="muted">(${none} without a WOZ value, e.g. a shop or garage)</span>` : "")
    + ` · <span class="muted">${d.all} addresses within ${d.radius} m</span>`
    + (d.pending ? ` · <span class="muted">${d.pending} still to look up (try again in a minute)</span>` : "")
    + (d.errors && d.errors.length && d.pending ? `<br><span class="muted">${esc(d.errors[0])}</span>` : "");
}

// ---------- the numbers ----------
function nbSet() {
  const d = NB.data; if (!d || !d.self) return null;
  const self = d.self;
  const houses = d.houses.filter((h) => h.woz && h.woz.length);
  const years = [...new Set([...(self.woz || []).map((x) => x[0]), ...houses.flatMap((h) => h.woz.map((x) => x[0]))])].sort((a, b) => a - b);
  if (!years.length) return null;
  if (!NB.year || !years.includes(NB.year)) NB.year = years[years.length - 1];
  const Y = NB.year;
  const sv = wozMap(self).get(Y) ?? null;
  const rows = houses.map((h) => ({ h, v: wozMap(h).get(Y) ?? null })).filter((r) => r.v != null);
  return { d, self, houses, years, Y, sv, rows, med: median(rows.map((r) => r.v)) };
}

function drawBuren() {
  const S = nbSet();
  const sel = $("nbYear");
  if (!S) { $("nbTiles").innerHTML = ""; drawMap(null); return; }
  const want = S.years.slice().reverse().map((y) => `<option value="${y}" ${y === S.Y ? "selected" : ""}>${y}</option>`).join("");
  if (sel.innerHTML !== want) sel.innerHTML = want;
  tiles(S);
  drawMap(S);
  rankChart(S); diffChart(S); yearsChart(S); growthChart(S); heatChart(S); m2Chart(S); scatterChart(S); distChart(S);
}

function tiles(S) {
  const { sv, rows, med, Y } = S;
  const all = rows.map((r) => r.v).concat(sv != null ? [sv] : []).sort((a, b) => b - a);
  const rank = sv != null ? all.indexOf(sv) + 1 : null;
  const cheaper = rows.filter((r) => r.v < sv).length;
  const t = (k, v, d) => `<div class="tile"><div class="k">${k}</div><div class="v">${v}</div><div class="d">${d || ""}</div></div>`;
  const ask = DATA && readings().length ? readings().at(-1).price : null;
  $("nbTiles").innerHTML =
    t("This house · WOZ " + Y, sv != null ? eur(sv) : "–", ask && sv ? `Asking price is ${pct((ask / sv - 1) * 100, 0)} on top` : "") +
    t("Neighbours' median", med != null ? eur(Math.round(med)) : "–", rows.length ? `${rows.length} houses · ${kEur(Math.min(...rows.map((r) => r.v)))} – ${kEur(Math.max(...rows.map((r) => r.v)))}` : "") +
    t("Difference", sv != null && med != null ? sEur(sv - med) : "–", sv != null && med ? `${pct((sv / med - 1) * 100)} vs the median` : "") +
    t("Rank", rank ? `#${rank} <span class="muted" style="font-size:15px;font-weight:600">of ${all.length}</span>` : "–", rank ? `${cheaper} neighbour${cheaper === 1 ? " is" : "s are"} valued lower` : "");
}

// ---------- tooltips for every chart: any element with data-tip ----------
function tipHost(box) {
  if (box._tip) return box._tip;
  const tip = document.createElement("div"); tip.className = "tip"; box.appendChild(tip);
  const show = (e) => {
    const t = e.target.closest && e.target.closest("[data-tip]");
    if (!t || !box.contains(t)) { tip.style.opacity = 0; return; }
    tip.innerHTML = t.getAttribute("data-tip");
    tip.style.opacity = 1;
    const r = box.getBoundingClientRect(), tw = tip.offsetWidth;
    tip.style.left = Math.min(r.width - tw / 2 - 2, Math.max(tw / 2 + 2, e.clientX - r.left)) + "px";
    tip.style.top = Math.max(tip.offsetHeight + 4, e.clientY - r.top - 12) + "px";
  };
  box.addEventListener("pointermove", show);
  box.addEventListener("pointerdown", show);
  box.addEventListener("pointerleave", () => { tip.style.opacity = 0; });
  box._tip = tip;
  return tip;
}
function put(box, svg) {
  const tip = box._tip;
  box.innerHTML = svg;
  if (tip) box.appendChild(tip); else tipHost(box);
}
const T = (x, y, txt, o = {}) => `<text x="${x}" y="${y}" font-size="${o.fs || 11.5}" fill="${o.fill || "var(--muted)"}" text-anchor="${o.a || "start"}" ${o.w ? `font-weight="${o.w}"` : ""} ${o.db ? `dominant-baseline="${o.db}"` : ""}>${esc(txt)}</text>`;
const empty = (box, msg) => put(box, `<div class="chart"><div class="empty">${esc(msg)}</div></div>`);
const W = (box) => Math.max(260, box.clientWidth || 320);

// rounded-end horizontal bar (square at the base side)
function hbar(x0, x1, y, h, fill, tip, extra = "") {
  const l = Math.min(x0, x1), w = Math.abs(x1 - x0), r = Math.min(4, h / 2, w);
  const toRight = x1 >= x0;
  const d = w < 0.5 ? "" : toRight
    ? `M${l},${y}H${l + w - r}Q${l + w},${y} ${l + w},${y + r}V${y + h - r}Q${l + w},${y + h} ${l + w - r},${y + h}H${l}Z`
    : `M${l + w},${y}H${l + r}Q${l},${y} ${l},${y + r}V${y + h - r}Q${l},${y + h} ${l + r},${y + h}H${l + w}Z`;
  return `<path d="${d}" fill="${fill}" ${extra}></path><rect x="${l - 2}" y="${y - 1}" width="${Math.max(w + 4, 14)}" height="${h + 2}" fill="transparent" data-tip="${esc(tip)}"></rect>`;
}

// ---------- 1. ranking ----------
function rankChart(S) {
  const box = $("cRank");
  const items = S.rows.map((r) => ({ name: r.h.name, v: r.v, h: r.h })).concat(S.sv != null ? [{ name: S.self.name, v: S.sv, me: true }] : []).sort((a, b) => b.v - a.v);
  if (items.length < 2) return empty(box, "Waiting for the neighbours' WOZ values.");
  const w = W(box), dense = items.length > 30, rh = dense ? 13 : 20, bh = dense ? 9 : 13, lw = Math.min(150, w * 0.36), top = 18;
  const x0 = lw + 6, x1 = w - (dense ? 8 : 46);
  const max = Math.max(...items.map((i) => i.v)), X = (v) => x0 + (v / max) * (x1 - x0);
  let g = "";
  items.forEach((it, i) => {
    const y = top + i * rh;
    const tip = `<b>${esc(it.name)}</b>${it.me ? " (this house)" : ""}<br>WOZ ${S.Y}: <b>${eur(it.v)}</b>` + (!it.me && S.sv ? `<br>${sEur(it.v - S.sv)} vs this house` : "") + (it.h && it.h.dist != null ? `<br>${Math.round(it.h.dist)} m away` : "");
    g += T(lw, y + bh / 2, short(it.name, dense ? 22 : 20), { a: "end", db: "middle", fill: it.me ? "var(--ink)" : "var(--ink-2)", w: it.me ? 700 : null, fs: dense ? 10 : 11.5 });
    g += hbar(x0, X(it.v), y, bh, it.me ? "var(--s1)" : "var(--nb)", tip);
    if (!dense || it.me) g += T(X(it.v) + 5, y + bh / 2, kEur(it.v), { db: "middle", fill: it.me ? "var(--ink)" : "var(--ink-2)", w: it.me ? 700 : null });
  });
  const mx = X(S.med);
  g += `<line x1="${mx}" x2="${mx}" y1="${top - 6}" y2="${top + items.length * rh}" stroke="var(--ink-2)" stroke-dasharray="3 3"/>` + T(mx, top - 8, "median " + kEur(S.med), { a: "middle", fill: "var(--ink-2)" });
  put(box, `<svg viewBox="0 0 ${w} ${top + items.length * rh + 4}" height="${top + items.length * rh + 4}">${g}</svg>`);
  const rank = items.findIndex((i) => i.me) + 1;
  $("nbRankNote").textContent = `WOZ value ${S.Y} of each house, highest first. This house is #${rank} of ${items.length}.`;
}

// ---------- 2. difference with this house (diverging) ----------
function diffChart(S) {
  const box = $("cDiff");
  if (S.sv == null || !S.rows.length) return empty(box, "Waiting for the neighbours' WOZ values.");
  const items = S.rows.map((r) => ({ name: r.h.name, d: r.v - S.sv, v: r.v, h: r.h })).sort((a, b) => b.d - a.d);
  const w = W(box), dense = items.length > 30, rh = dense ? 13 : 20, bh = dense ? 9 : 13, lw = Math.min(150, w * 0.34), top = 22;
  const x0 = lw + (dense ? 10 : 52), x1 = w - (dense ? 10 : 48); // room for the value labels on both sides
  const m = Math.max(1000, ...items.map((i) => Math.abs(i.d)));
  const zero = x0 + (x1 - x0) / 2, X = (v) => zero + (v / m) * ((x1 - x0) / 2);
  let g = T(zero - 6, 10, "← lower", { a: "end", fill: "var(--ink-2)" }) + T(zero + 6, 10, "higher →", { fill: "var(--ink-2)" });
  items.forEach((it, i) => {
    const y = top + i * rh;
    const tip = `<b>${esc(it.name)}</b><br>WOZ ${S.Y}: ${eur(it.v)}<br><b>${sEur(it.d)}</b> (${pct((it.v / S.sv - 1) * 100)}) vs this house`;
    g += T(lw, y + bh / 2, short(it.name, dense ? 22 : 20), { a: "end", db: "middle", fill: "var(--ink-2)", fs: dense ? 10 : 11.5 });
    g += hbar(zero, X(it.d), y, bh, it.d > 0 ? "var(--hi)" : "var(--lo)", tip);
    if (!dense) g += T(it.d >= 0 ? X(it.d) + 5 : X(it.d) - 5, y + bh / 2, sEur(it.d), { a: it.d >= 0 ? "start" : "end", db: "middle", fill: "var(--ink-2)" });
  });
  g += `<line x1="${zero}" x2="${zero}" y1="${top - 4}" y2="${top + items.length * rh}" stroke="var(--s1)" stroke-width="2"/>`;
  g += T(zero, top + items.length * rh + 14, "this house " + kEur(S.sv), { a: "middle", fill: "var(--ink)", w: 650 });
  const H = top + items.length * rh + 20;
  put(box, `<svg viewBox="0 0 ${w} ${H}" height="${H}">${g}</svg>`);
}

// ---------- 3. WOZ over the years (lines) ----------
function yearsChart(S) {
  const box = $("cYears");
  const ys = S.years; if (ys.length < 2) return empty(box, "Needs more than one year.");
  const w = W(box), H = 240, m = { l: 52, r: 56, t: 10, b: 24 };
  const series = S.houses.map((h) => ({ h, pts: h.woz.filter(([y]) => ys.includes(y)) }));
  const selfPts = (S.self.woz || []);
  const all = series.flatMap((s) => s.pts.map((p) => p[1])).concat(selfPts.map((p) => p[1]));
  const yt = niceTicks(0, Math.max(...all), 4), hi = yt[yt.length - 1];
  const X = (y) => m.l + ((y - ys[0]) / (ys[ys.length - 1] - ys[0])) * (w - m.l - m.r), Y = (v) => m.t + (H - m.t - m.b) * (1 - v / hi);
  const path = (pts) => pts.map(([y, v], i) => `${i ? "L" : "M"}${X(y).toFixed(1)},${Y(v).toFixed(1)}`).join("");
  let g = "";
  for (const v of yt) g += `<line x1="${m.l}" x2="${w - m.r}" y1="${Y(v)}" y2="${Y(v)}" stroke="${v ? "var(--grid)" : "var(--axis)"}"/>` + T(m.l - 6, Y(v) + 4, kEur(v), { a: "end" });
  const step = Math.ceil(ys.length / Math.max(2, Math.floor((w - m.l - m.r) / 44)));
  ys.forEach((y, i) => { if ((ys.length - 1 - i) % step === 0) g += T(X(y), H - 6, String(y), { a: "middle" }); });
  for (const s of series) g += `<path d="${path(s.pts)}" fill="none" stroke="var(--nb)" stroke-width="1.2" opacity=".8"/>`;
  const medPts = ys.map((y) => [y, median(S.houses.map((h) => wozMap(h).get(y)).filter((v) => v != null))]).filter((p) => p[1] != null);
  g += `<path d="${path(medPts)}" fill="none" stroke="var(--ink-2)" stroke-width="2" stroke-dasharray="5 4"/>`;
  g += `<path d="${path(selfPts)}" fill="none" stroke="var(--s1)" stroke-width="2.5" stroke-linejoin="round"/>`;
  const ls = selfPts.at(-1), lm = medPts.at(-1);
  if (ls) g += `<circle cx="${X(ls[0])}" cy="${Y(ls[1])}" r="4" fill="var(--s1)" stroke="var(--surface)" stroke-width="2"/>` + T(X(ls[0]) + 7, Y(ls[1]) + (lm && ls[1] >= lm[1] ? -4 : 12), "this house", { fill: "var(--ink)", w: 650 });
  if (lm) g += T(X(lm[0]) + 7, Y(lm[1]) + (ls && ls[1] >= lm[1] ? 12 : -4), "median", { fill: "var(--ink-2)" });
  // hover: one band per year
  ys.forEach((y, i) => {
    const vals = S.houses.map((h) => wozMap(h).get(y)).filter((v) => v != null);
    const sv = wozMap(S.self).get(y);
    const x = X(y), half = (w - m.l - m.r) / (ys.length - 1) / 2;
    const tip = `<b>${y}</b><br>This house: <b>${sv != null ? eur(sv) : "–"}</b><br>Median: ${vals.length ? eur(Math.round(median(vals))) : "–"}<br>Range: ${vals.length ? kEur(Math.min(...vals)) + " – " + kEur(Math.max(...vals)) : "–"}`;
    g += `<rect x="${x - half}" y="${m.t}" width="${half * 2}" height="${H - m.t - m.b}" fill="transparent" data-tip="${esc(tip)}"/>`;
  });
  put(box, `<svg viewBox="0 0 ${w} ${H}" height="${H}">${g}</svg>`);
}

// ---------- 4. growth (lollipop) ----------
function growthChart(S) {
  const box = $("cGrowth");
  const sm = wozMap(S.self), from = S.years.find((y) => sm.has(y) && S.houses.some((h) => wozMap(h).has(y)));
  if (from == null || from >= S.Y) return empty(box, "Needs WOZ values from more than one year.");
  const g0 = (h) => { const mm = wozMap(h); return mm.has(from) && mm.has(S.Y) ? (mm.get(S.Y) / mm.get(from) - 1) * 100 : null; };
  const items = S.houses.map((h) => ({ name: h.name, g: g0(h) })).filter((i) => i.g != null).concat([{ name: S.self.name, g: g0(S.self), me: true }]).filter((i) => i.g != null).sort((a, b) => b.g - a.g);
  $("nbGrowthNote").textContent = `How much each WOZ value grew from ${from} to ${S.Y}.`;
  const w = W(box), dense = items.length > 30, rh = dense ? 12 : 19, lw = Math.min(130, w * 0.36), top = 6;
  const x0 = lw + 8, x1 = w - 46;
  const lo = Math.min(0, ...items.map((i) => i.g)), hi = Math.max(...items.map((i) => i.g));
  const X = (v) => x0 + ((v - lo) / (hi - lo || 1)) * (x1 - x0);
  let g = `<line x1="${X(0)}" x2="${X(0)}" y1="${top}" y2="${top + items.length * rh}" stroke="var(--axis)"/>`;
  items.forEach((it, i) => {
    const y = top + i * rh + rh / 2;
    const c = it.me ? "var(--s1)" : "var(--nb)";
    g += T(lw, y, short(it.name, 18), { a: "end", db: "middle", fill: it.me ? "var(--ink)" : "var(--ink-2)", w: it.me ? 700 : null, fs: dense ? 10 : 11.5 });
    g += `<line x1="${X(0)}" x2="${X(it.g)}" y1="${y}" y2="${y}" stroke="${c}" stroke-width="2"/>`;
    g += `<circle cx="${X(it.g)}" cy="${y}" r="${it.me ? 5 : 4}" fill="${c}" stroke="var(--surface)" stroke-width="1.5"/>`;
    if (!dense || it.me) g += T(X(it.g) + 8, y, pct(it.g, 0), { db: "middle", fill: it.me ? "var(--ink)" : "var(--ink-2)", w: it.me ? 650 : null });
    g += `<rect x="${x0 - 4}" y="${y - rh / 2}" width="${x1 - x0 + 8}" height="${rh}" fill="transparent" data-tip="${esc(`<b>${esc(it.name)}</b>${it.me ? " (this house)" : ""}<br>${from} → ${S.Y}: <b>${pct(it.g)}</b>`)}"/>`;
  });
  const H = top + items.length * rh + 6;
  put(box, `<svg viewBox="0 0 ${w} ${H}" height="${H}">${g}</svg>`);
}

// ---------- 5. yearly change heatmap ----------
const RAMP = ["#cde2fb", "#9ec5f4", "#6da7ec", "#3987e5", "#256abf", "#184f95", "#0d366b"];
function heatColor(p) {
  if (p == null) return "transparent";
  if (p < 0) return p < -5 ? "#e34948" : "#f3a5a4";
  const i = Math.min(RAMP.length - 1, Math.floor(p / 4)); // 0-4%, 4-8%, ... 24%+
  return RAMP[i];
}
function heatChart(S) {
  const box = $("cHeat");
  const ys = S.years.slice(1);
  if (!ys.length) return empty(box, "Needs WOZ values from more than one year.");
  const list = [{ name: S.self.name, h: S.self, me: true }].concat(S.rows.slice().sort((a, b) => b.v - a.v).map((r) => ({ name: r.h.name, h: r.h })));
  const w = W(box), lw = Math.min(118, w * 0.34), top = 18, cw = (w - lw - 8) / ys.length, ch = list.length > 30 ? 10 : 16;
  let g = "";
  const step = Math.ceil(ys.length / Math.max(2, Math.floor((w - lw) / 34)));
  ys.forEach((y, j) => { if ((ys.length - 1 - j) % step === 0) g += T(lw + 8 + j * cw + cw / 2, 11, "’" + String(y).slice(2), { a: "middle" }); });
  list.forEach((it, i) => {
    const mm = wozMap(it.h), y = top + i * ch + (i ? 6 : 0);
    g += T(lw, y + ch / 2, short(it.name, 16), { a: "end", db: "middle", fill: it.me ? "var(--ink)" : "var(--ink-2)", w: it.me ? 700 : null, fs: ch < 12 ? 9.5 : 11 });
    ys.forEach((yr, j) => {
      const a = mm.get(yr - 1), b = mm.get(yr);
      const p = a && b ? (b / a - 1) * 100 : null;
      g += `<rect x="${lw + 8 + j * cw + 1}" y="${y + 1}" width="${Math.max(1, cw - 2)}" height="${ch - 2}" rx="2" fill="${heatColor(p)}" ${p == null ? 'stroke="var(--grid)"' : ""} data-tip="${esc(`<b>${esc(it.name)}</b><br>${yr - 1} → ${yr}: <b>${p == null ? "no value" : pct(p)}</b>${p != null ? `<br>${kEur(a)} → ${kEur(b)}` : ""}`)}"/>`;
    });
    if (it.me) g += `<rect x="${lw + 7}" y="${y}" width="${cw * ys.length + 2}" height="${ch}" rx="3" fill="none" stroke="var(--s1)" stroke-width="1.5"/>`;
  });
  const H = top + list.length * ch + 30;
  const ly = H - 12;
  g += T(lw + 8, ly + 4, "less", { fill: "var(--ink-2)" });
  RAMP.forEach((c, i) => { g += `<rect x="${lw + 38 + i * 16}" y="${ly - 5}" width="14" height="10" rx="2" fill="${c}"/>`; });
  g += T(lw + 42 + RAMP.length * 16, ly + 4, "more", { fill: "var(--ink-2)" });
  put(box, `<svg viewBox="0 0 ${w} ${H}" height="${H}">${g}</svg>`);
}

// ---------- 6. WOZ per m² ----------
function m2Chart(S) {
  const box = $("cM2");
  const items = S.rows.filter((r) => r.h.area > 15).map((r) => ({ name: r.h.name, v: r.v / r.h.area, r }))
    .concat(S.sv != null && S.self.area > 15 ? [{ name: S.self.name, v: S.sv / S.self.area, me: true }] : []).sort((a, b) => b.v - a.v);
  if (items.length < 2) return empty(box, "No living areas known (BAG) for these houses.");
  const ask = DATA && readings().length ? readings().at(-1).price : null;
  const askM2 = ask && S.self.area ? ask / S.self.area : null;
  const w = W(box), dense = items.length > 30, rh = dense ? 12 : 19, bh = dense ? 8 : 12, lw = Math.min(130, w * 0.36), top = 18;
  const x0 = lw + 6, x1 = w - 46, max = Math.max(...items.map((i) => i.v), askM2 || 0), X = (v) => x0 + (v / max) * (x1 - x0);
  let g = "";
  items.forEach((it, i) => {
    const y = top + i * rh;
    g += T(lw, y + bh / 2, short(it.name, 18), { a: "end", db: "middle", fill: it.me ? "var(--ink)" : "var(--ink-2)", w: it.me ? 700 : null, fs: dense ? 10 : 11.5 });
    g += hbar(x0, X(it.v), y, bh, it.me ? "var(--s1)" : "var(--nb)", `<b>${esc(it.name)}</b><br><b>${eur(Math.round(it.v))}</b> per m²<br>${eur(it.me ? S.sv : it.r.v)} / ${it.me ? S.self.area : it.r.h.area} m²`);
    if (!dense || it.me) g += T(X(it.v) + 5, y + bh / 2, "€" + nf.format(Math.round(it.v)), { db: "middle", fill: it.me ? "var(--ink)" : "var(--ink-2)", w: it.me ? 650 : null });
  });
  if (askM2) {
    const ax = X(askM2);
    g += `<line x1="${ax}" x2="${ax}" y1="${top - 6}" y2="${top + items.length * rh}" stroke="var(--s1)" stroke-dasharray="4 3" stroke-width="1.5"/>` + T(Math.min(ax, w - 60), top - 8, "asking €" + nf.format(Math.round(askM2)), { a: "middle", fill: "var(--ink)", w: 650 });
  }
  const H = top + items.length * rh + 4;
  put(box, `<svg viewBox="0 0 ${w} ${H}" height="${H}">${g}</svg>`);
  const med = median(items.filter((i) => !i.me).map((i) => i.v));
  $("nbM2Note").textContent = `WOZ ${S.Y} ÷ living area. Neighbours' median €${nf.format(Math.round(med))}/m²` + (askM2 ? `; this house's asking price is €${nf.format(Math.round(askM2))}/m².` : ".");
}

// ---------- 7. living area vs WOZ (scatter + trend) ----------
function scatterChart(S) {
  const box = $("cScatter");
  const pts = S.rows.filter((r) => r.h.area > 15).map((r) => ({ x: r.h.area, y: r.v, name: r.h.name }));
  if (pts.length < 3) return empty(box, "Needs living areas (BAG) for at least 3 neighbours.");
  const ask = DATA && readings().length ? readings().at(-1).price : null;
  const me = S.self.area && S.sv != null ? { x: S.self.area, y: S.sv } : null;
  const w = W(box), H = 240, m = { l: 50, r: 14, t: 12, b: 34 };
  const xs = pts.map((p) => p.x).concat(me ? [me.x] : []), ysv = pts.map((p) => p.y).concat(me ? [me.y] : [], ask ? [ask] : []);
  const xt = niceTicks(Math.min(...xs) * 0.95, Math.max(...xs) * 1.03, 4), yt = niceTicks(Math.min(...ysv) * 0.95, Math.max(...ysv) * 1.03, 4);
  const X = (v) => m.l + ((v - xt[0]) / (xt.at(-1) - xt[0])) * (w - m.l - m.r), Y = (v) => m.t + (H - m.t - m.b) * (1 - (v - yt[0]) / (yt.at(-1) - yt[0]));
  let g = "";
  for (const v of yt) g += `<line x1="${m.l}" x2="${w - m.r}" y1="${Y(v)}" y2="${Y(v)}" stroke="var(--grid)"/>` + T(m.l - 6, Y(v) + 4, kEur(v), { a: "end" });
  for (const v of xt) g += T(X(v), H - 18, v + " m²", { a: "middle" });
  g += T((m.l + w - m.r) / 2, H - 3, "living area", { a: "middle", fill: "var(--ink-2)" });
  // least-squares trend
  const n = pts.length, sx = pts.reduce((a, p) => a + p.x, 0), sy = pts.reduce((a, p) => a + p.y, 0);
  const sxx = pts.reduce((a, p) => a + p.x * p.x, 0), sxy = pts.reduce((a, p) => a + p.x * p.y, 0);
  const den = n * sxx - sx * sx, b = den ? (n * sxy - sx * sy) / den : 0, a = (sy - b * sx) / n;
  if (den) {
    const xa = xt[0], xb = xt.at(-1);
    g += `<line x1="${X(xa)}" y1="${Y(a + b * xa)}" x2="${X(xb)}" y2="${Y(a + b * xb)}" stroke="var(--ink-2)" stroke-dasharray="5 4" stroke-width="1.5"/>`;
  }
  for (const p of pts) g += `<circle cx="${X(p.x)}" cy="${Y(p.y)}" r="4.5" fill="var(--nb)" stroke="var(--surface)" stroke-width="1.5" data-tip="${esc(`<b>${esc(p.name)}</b><br>${p.x} m² · WOZ ${eur(p.y)}`)}"/>`;
  if (me) g += `<circle cx="${X(me.x)}" cy="${Y(me.y)}" r="6" fill="var(--s1)" stroke="var(--surface)" stroke-width="2" data-tip="${esc(`<b>This house</b><br>${me.x} m² · WOZ ${eur(me.y)}`)}"/>`;
  if (me && ask) {
    const cx = X(me.x), cy = Y(ask), r = 7;
    g += `<line x1="${cx}" x2="${cx}" y1="${Y(me.y) - 6}" y2="${cy + r}" stroke="var(--s1)" stroke-width="1.5" stroke-dasharray="2 2"/>`;
    g += `<path d="M${cx},${cy - r}L${cx + r},${cy}L${cx},${cy + r}L${cx - r},${cy}Z" fill="var(--surface)" stroke="var(--s1)" stroke-width="2" data-tip="${esc(`<b>Asking price</b><br>${eur(ask)} for ${me.x} m²`)}"/>`;
    g += T(cx + 10, cy + 4, "asking " + kEur(ask), { fill: "var(--ink)", w: 650 });
  }
  put(box, `<svg viewBox="0 0 ${w} ${H}" height="${H}">${g}</svg>`);
  if (den && me) {
    const exp = a + b * me.x;
    $("nbScatNote").textContent = `For ${me.x} m², the neighbours' trend (dashed) gives a WOZ of about ${kEur(exp)}; this house has ${kEur(me.y)}. The diamond is the asking price.`;
  }
}

// ---------- 8. distance vs difference ----------
function distChart(S) {
  const box = $("cDist");
  if (S.sv == null) return empty(box, "Waiting for WOZ values.");
  const pts = S.rows.filter((r) => r.h.dist != null).map((r) => ({ x: r.h.dist, y: r.v - S.sv, name: r.h.name, v: r.v }));
  if (pts.length < 2) return empty(box, "Waiting for the neighbours' WOZ values.");
  const w = W(box), H = 210, m = { l: 50, r: 14, t: 12, b: 34 };
  const lim = Math.max(1000, ...pts.map((p) => Math.abs(p.y)));
  const yt = niceTicks(-lim, lim, 4), xt = niceTicks(0, Math.max(150, ...pts.map((p) => p.x)), 5);
  const X = (v) => m.l + (v / xt.at(-1)) * (w - m.l - m.r), Y = (v) => m.t + (H - m.t - m.b) * (1 - (v - yt[0]) / (yt.at(-1) - yt[0]));
  let g = "";
  for (const v of yt) g += `<line x1="${m.l}" x2="${w - m.r}" y1="${Y(v)}" y2="${Y(v)}" stroke="${v === 0 ? "var(--s1)" : "var(--grid)"}" ${v === 0 ? 'stroke-width="2"' : ""}/>` + T(m.l - 6, Y(v) + 4, v === 0 ? "same" : sEur(v), { a: "end", fill: v === 0 ? "var(--ink)" : "var(--muted)" });
  for (const v of xt) g += T(X(v), H - 18, v + " m", { a: "middle" });
  g += T((m.l + w - m.r) / 2, H - 3, "distance from this house", { a: "middle", fill: "var(--ink-2)" });
  for (const p of pts) g += `<circle cx="${X(p.x)}" cy="${Y(p.y)}" r="4.5" fill="${p.y > 0 ? "var(--hi)" : "var(--lo)"}" stroke="var(--surface)" stroke-width="1.5" data-tip="${esc(`<b>${esc(p.name)}</b><br>${Math.round(p.x)} m away<br>WOZ ${eur(p.v)} (${sEur(p.y)})`)}"/>`;
  put(box, `<svg viewBox="0 0 ${w} ${H}" height="${H}">${g}</svg>`);
}

// ---------- the 3D map (MapLibre, PDOK map / aerial photo, extruded BAG buildings) ----------
const MAP = { map: null, loading: null, base: "map", height: "flat", spin: true, S: null, ready: false };
// each base map has a list of tile addresses: when one keeps failing, the next one is tried
const BASES = {
  map: [
    { tiles: ["https://service.pdok.nl/brt/achtergrondkaart/wmts/v2_0/grijs/EPSG:3857/{z}/{x}/{y}.png"], maxzoom: 19, attribution: "Kaart © Kadaster / PDOK" },
    { tiles: ["https://service.pdok.nl/brt/achtergrondkaart/wmts/v2_0?SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&LAYER=grijs&STYLE=default&FORMAT=image/png&TILEMATRIXSET=EPSG:3857&TILEMATRIX={z}&TILEROW={y}&TILECOL={x}"], maxzoom: 19, attribution: "Kaart © Kadaster / PDOK" },
    { tiles: ["https://tile.openstreetmap.org/{z}/{x}/{y}.png"], maxzoom: 19, attribution: "© OpenStreetMap contributors" },
  ],
  photo: [
    { tiles: ["https://service.pdok.nl/hwh/luchtfotorgb/wmts/v1_0/Actueel_orthoHR/EPSG:3857/{z}/{x}/{y}.jpeg"], maxzoom: 21, attribution: "Luchtfoto © Beeldmateriaal Nederland / PDOK" },
    { tiles: ["https://service.pdok.nl/hwh/luchtfotorgb/wmts/v1_0?SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0&LAYER=Actueel_orthoHR&STYLE=default&FORMAT=image/jpeg&TILEMATRIXSET=EPSG:3857&TILEMATRIX={z}&TILEROW={y}&TILECOL={x}"], maxzoom: 21, attribution: "Luchtfoto © Beeldmateriaal Nederland / PDOK" },
    { tiles: ["https://service.pdok.nl/hwh/luchtfotorgb/wmts/v1_0/Actueel_ortho25/EPSG:3857/{z}/{x}/{y}.jpeg"], maxzoom: 19, attribution: "Luchtfoto © Beeldmateriaal Nederland / PDOK" },
  ],
};
MAP.try = { map: 0, photo: 0 };
MAP.fails = 0;
const baseSrc = () => ({ type: "raster", tileSize: 256, ...BASES[MAP.base][Math.min(MAP.try[MAP.base], BASES[MAP.base].length - 1)] });
function setBase() {
  const map = MAP.map; if (!map || !MAP.ready) return;
  MAP.fails = 0;
  map.removeLayer("base"); map.removeSource("base");
  map.addSource("base", baseSrc());
  map.addLayer({ id: "base", type: "raster", source: "base", paint: { "raster-opacity": 0.95 } }, "ring-fill");
}
function loadMapLib() {
  if (window.maplibregl) return Promise.resolve();
  if (MAP.loading) return MAP.loading;
  MAP.loading = new Promise((ok, bad) => {
    const css = document.createElement("link"); css.rel = "stylesheet"; css.href = "/other/funda/maplibre/maplibre-gl.css"; document.head.appendChild(css);
    const s = document.createElement("script"); s.src = "/other/funda/maplibre/maplibre-gl.js"; s.onload = ok; s.onerror = () => bad(new Error("map library didn't load")); document.head.appendChild(s);
  });
  return MAP.loading;
}
function circle(c, r, n = 72) {
  const out = [];
  for (let i = 0; i <= n; i++) {
    const a = (i / n) * 2 * Math.PI;
    out.push([c[0] + (r * Math.cos(a)) / (111320 * Math.cos(c[1] * Math.PI / 180)), c[1] + (r * Math.sin(a)) / 111320]);
  }
  return out;
}
// colour of a building: WOZ difference with this house (green lower, red higher); this house blue; grey unknown
function diffColor(d, lim) {
  const lo = [27, 175, 122], mid = [216, 214, 206], hi = [227, 73, 72];
  const t = Math.max(-1, Math.min(1, d / lim));
  const to = t < 0 ? lo : hi, k = Math.abs(t);
  const c = mid.map((v, i) => Math.round(v + (to[i] - v) * k));
  return `rgb(${c.join(",")})`;
}
function mapFeatures(S) {
  const d = NB.data;
  const byPand = new Map();
  const add = (h, isSelf) => { if (!h.pand) return; if (!byPand.has(h.pand)) byPand.set(h.pand, []); byPand.get(h.pand).push({ h, isSelf }); };
  if (d.self) add(d.self, true);
  for (const h of d.houses) add(h, false);
  for (const h of d.others || []) add(h, false);
  const lim = S && S.sv != null && S.rows.length ? Math.max(15000, ...S.rows.map((r) => Math.abs(r.v - S.sv))) : 50000;
  const maxV = S ? Math.max(S.sv || 0, ...S.rows.map((r) => r.v)) : 1;
  const feats = d.panden.map((p) => {
    const list = byPand.get(p.id) || [];
    const me = list.some((x) => x.isSelf);
    const vals = list.map((x) => (S && x.h.woz ? wozMap(x.h).get(S.Y) : null)).filter((v) => v != null);
    const avg = vals.length ? vals.reduce((a, b) => a + b, 0) / vals.length : null;
    const color = me ? cssv("--s1") || "#2a78d6" : avg != null && S && S.sv != null ? diffColor(avg - S.sv, lim) : "#d9d8d2";
    const flatH = 6 + Math.min(6, Math.max(0, list.length - 1)) * 1.2;
    const wozH = avg != null ? 3 + 30 * (avg / maxV) : me && S && S.sv ? 3 + 30 * (S.sv / maxV) : 2;
    const names = list.map((x) => x.h.name);
    return {
      type: "Feature",
      properties: { id: p.id, color, me, hFlat: me ? flatH + 1 : flatH, hWoz: wozH, avg: avg || 0, n: list.length, known: vals.length, built: p.built || 0, names: JSON.stringify(names.slice(0, 6)), more: Math.max(0, names.length - 6) },
      geometry: { type: "Polygon", coordinates: [p.ring] },
    };
  });
  return { type: "FeatureCollection", features: feats };
}
async function drawMap(S) {
  MAP.S = S;
  const d = NB.data;
  if (!d || !d.center) return;
  const box = $("map3d");
  if (!d.panden || !d.panden.length) { $("mapMsg").textContent = "No building outlines found for this spot."; return; }
  if (MAP.map) { if (MAP.ready) MAP.map.getSource("panden").setData(mapFeatures(S)); legendMap(S); return; }
  if (MAP.starting) return;
  MAP.starting = true;
  try { await loadMapLib(); } catch (e) { $("mapMsg").textContent = "The map couldn't load (" + e.message + ")."; MAP.starting = false; return; }
  const gl = (() => { try { const cv = document.createElement("canvas"); return !!(cv.getContext("webgl2") || cv.getContext("webgl")); } catch { return false; } })();
  if (!gl) { $("mapMsg").textContent = "This browser can't show 3D maps (WebGL is off)."; MAP.starting = false; return; }
  const c = d.center;
  const wpx = box.clientWidth || 360;
  const zoom = Math.log2((wpx * 156543.03 * Math.cos(c[1] * Math.PI / 180)) / (2 * d.radius * 1.05)) - 0.45;
  const map = new maplibregl.Map({
    container: box, center: c, zoom, pitch: 55, bearing: -25, maxPitch: 75, attributionControl: { compact: true },
    style: {
      version: 8,
      sources: {
        base: baseSrc(),
        ring: { type: "geojson", data: { type: "Feature", properties: {}, geometry: { type: "Polygon", coordinates: [circle(c, d.radius)] } } },
        panden: { type: "geojson", data: mapFeatures(S) },
      },
      layers: [
        { id: "bg", type: "background", paint: { "background-color": "#e9e8e3" } },
        { id: "base", type: "raster", source: "base", paint: { "raster-opacity": 0.95 } },
        { id: "ring-fill", type: "fill", source: "ring", paint: { "fill-color": "#2a78d6", "fill-opacity": 0.05 } },
        { id: "ring-line", type: "line", source: "ring", paint: { "line-color": "#2a78d6", "line-width": 2, "line-dasharray": [2, 2], "line-opacity": 0.8 } },
        { id: "b3d", type: "fill-extrusion", source: "panden", paint: {
          "fill-extrusion-color": ["get", "color"],
          "fill-extrusion-height": ["get", MAP.height === "woz" ? "hWoz" : "hFlat"],
          "fill-extrusion-base": 0, "fill-extrusion-opacity": 0.92, "fill-extrusion-vertical-gradient": true } },
      ],
    },
  });
  MAP.map = map;
  map.addControl(new maplibregl.NavigationControl({ visualizePitch: true }), "top-right");
  const pin = document.createElement("div"); pin.className = "me-pin"; pin.textContent = "This house";
  new maplibregl.Marker({ element: pin, anchor: "bottom", offset: [0, -24] }).setLngLat(d.self ? [d.self.lon, d.self.lat] : c).addTo(map);
  map.on("load", () => { MAP.ready = true; $("mapMsg").remove(); map.getSource("panden").setData(mapFeatures(MAP.S)); legendMap(MAP.S); spin(); });
  map.on("error", (e) => {
    if (!e || e.sourceId !== "base") return;
    // tiles of this base map keep failing: try its next address
    if (++MAP.fails >= 4 && MAP.try[MAP.base] < BASES[MAP.base].length - 1) { MAP.try[MAP.base]++; setBase(); }
  });
  map.on("mouseenter", "b3d", () => { map.getCanvas().style.cursor = "pointer"; });
  map.on("mouseleave", "b3d", () => { map.getCanvas().style.cursor = ""; });
  map.on("click", "b3d", (e) => {
    const f = e.features && e.features[0]; if (!f) return;
    const p = f.properties, S2 = MAP.S;
    let names = []; try { names = JSON.parse(p.names); } catch {}
    const lines = [];
    lines.push(`<b>${esc(names[0] || "Building")}</b>${p.n > 1 ? ` <span style="color:#52514e">+ ${p.n - 1} more address${p.n > 2 ? "es" : ""}</span>` : ""}`);
    if (p.me) lines.push("This house" + (S2 && S2.sv ? `: WOZ ${S2.Y} <b>${eur(S2.sv)}</b>` : ""));
    else if (p.known && S2) lines.push(`WOZ ${S2.Y}: <b>${eur(Math.round(p.avg))}</b>${p.known > 1 ? " (average)" : ""}` + (S2.sv ? `<br>${sEur(p.avg - S2.sv)} (${pct((p.avg / S2.sv - 1) * 100)}) vs this house` : ""));
    else lines.push(`<span style="color:#52514e">Not compared. Choose more houses above to include it.</span>`);
    if (p.built) lines.push(`Built ${p.built}`);
    new maplibregl.Popup({ closeButton: true, maxWidth: "240px" }).setLngLat(e.lngLat).setHTML(lines.join("<br>")).addTo(map);
  });
  ["mousedown", "touchstart", "wheel", "dragstart"].forEach((ev) => map.on(ev, () => { MAP.spin = false; }));
  MAP.starting = false;
}
function spin() {
  const map = MAP.map; if (!map || !MAP.spin || matchMedia("(prefers-reduced-motion: reduce)").matches) return;
  map.easeTo({ bearing: map.getBearing() + 24, duration: 6000, easing: (t) => t });
  map.once("moveend", () => { if (MAP.spin) spin(); });
}
function legendMap(S) {
  if (!S || S.sv == null || !S.rows.length) { $("mapScale").innerHTML = ""; return; }
  const lim = Math.max(15000, ...S.rows.map((r) => Math.abs(r.v - S.sv)));
  $("mapScale").innerHTML = `<span>WOZ ${S.Y} vs this house:</span><span>${sEur(-lim)}</span>`
    + `<span class="bar" style="background:linear-gradient(90deg, ${diffColor(-lim, lim)}, ${diffColor(0, lim)}, ${diffColor(lim, lim)})"></span><span>${sEur(lim)}</span>`
    + `<span><i class="sq" style="background:var(--s1);display:inline-block;width:11px;height:11px;border-radius:3px;vertical-align:-1px"></i> this house</span>`
    + `<span><i class="sq" style="background:#d9d8d2;display:inline-block;width:11px;height:11px;border-radius:3px;vertical-align:-1px"></i> not compared</span>`;
}
$("mapBase").addEventListener("click", (e) => {
  const b = e.target.closest("button"); if (!b) return;
  MAP.base = b.dataset.v;
  $("mapBase").querySelectorAll("button").forEach((x) => x.setAttribute("aria-pressed", x === b));
  setBase();
});
$("mapH").addEventListener("click", (e) => {
  const b = e.target.closest("button"); if (!b) return;
  MAP.height = b.dataset.v;
  $("mapH").querySelectorAll("button").forEach((x) => x.setAttribute("aria-pressed", x === b));
  if (MAP.map && MAP.ready) MAP.map.setPaintProperty("b3d", "fill-extrusion-height", ["get", MAP.height === "woz" ? "hWoz" : "hFlat"]);
});

// start when the section comes near the screen (keeps the first load light)
new IntersectionObserver((es, ob) => { if (es.some((x) => x.isIntersecting)) { ob.disconnect(); loadBuren(); } }, { rootMargin: "600px" }).observe($("nbPanel"));
let nbRz = 0;
addEventListener("resize", () => { clearTimeout(nbRz); nbRz = setTimeout(() => { if (NB.data) { const S = nbSet(); if (S) { rankChart(S); diffChart(S); yearsChart(S); growthChart(S); heatChart(S); m2Chart(S); scatterChart(S); distChart(S); } } }, 150); });
