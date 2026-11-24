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
$("nbYear").addEventListener("change", () => { NB.year = +$("nbYear").value; if (MAP.playing) stopPlay(); MAP.year = NB.year; drawBuren(); });

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
  scoreCharts(S);
  rankChart(S); diffChart(S); yearsChart(S); growthChart(S); heatChart(S); m2Chart(S); scatterChart(S); distChart(S);
}

// this house's asking price right now (the newest Funda reading), drawn as a dashed blue outline everywhere
const ASK_NAME = "Asking price now";
const askNow = () => (typeof DATA !== "undefined" && DATA && readings().length ? readings().at(-1).price : null);
const ASK_STYLE = 'stroke="var(--s1)" stroke-width="1.5" stroke-dasharray="3 2"';
const ASK_FILL = "var(--s1-soft)";

function tiles(S) {
  const { sv, rows, med, Y } = S;
  const all = rows.map((r) => r.v).concat(sv != null ? [sv] : []).sort((a, b) => b - a);
  const rank = sv != null ? all.indexOf(sv) + 1 : null;
  const ask = askNow();
  const askRank = ask ? rows.filter((r) => r.v > ask).length + 1 : null;
  const t = (k, v, d) => `<div class="tile"><div class="k">${k}</div><div class="v">${v}</div><div class="d">${d || ""}</div></div>`;
  $("nbTiles").innerHTML =
    t("This house · WOZ " + Y, sv != null ? eur(sv) : "–", rank ? `#${rank} of ${all.length} · ${sv != null && med != null ? sEur(sv - med) : ""} vs median` : "") +
    t("Asking price now", ask ? eur(ask) : "–", ask ? `would be #${askRank} of ${rows.length + 1} · ${med != null ? sEur(ask - med) : ""} vs median` : "") +
    t("Neighbours' median WOZ", med != null ? eur(Math.round(med)) : "–", rows.length ? `${rows.length} houses · ${kEur(Math.min(...rows.map((r) => r.v)))} – ${kEur(Math.max(...rows.map((r) => r.v)))}` : "") +
    t("Asking vs own WOZ", ask && sv ? sEur(ask - sv) : "–", ask && sv ? `${pct((ask / sv - 1) * 100)} on top of the ${Y} WOZ` : "");
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
  const ask = askNow();
  const items = S.rows.map((r) => ({ name: r.h.name, v: r.v, h: r.h }))
    .concat(S.sv != null ? [{ name: S.self.name, v: S.sv, me: true }] : [], ask ? [{ name: ASK_NAME, v: ask, ask: true }] : [])
    .sort((a, b) => b.v - a.v);
  if (items.length < 2) return empty(box, "Waiting for the neighbours' WOZ values.");
  const w = W(box), dense = items.length > 30, rh = dense ? 13 : 20, bh = dense ? 9 : 13, lw = Math.min(150, w * 0.36), top = 18;
  const x0 = lw + 6, x1 = w - (dense ? 8 : 46);
  const max = Math.max(...items.map((i) => i.v)), X = (v) => x0 + (v / max) * (x1 - x0);
  let g = "";
  items.forEach((it, i) => {
    const y = top + i * rh, hl = it.me || it.ask;
    const tip = it.ask
      ? `<b>Asking price now</b><br><b>${eur(it.v)}</b>` + (S.sv ? `<br>${sEur(it.v - S.sv)} on top of its WOZ ${S.Y}` : "") + `<br>Higher than ${S.rows.filter((r) => r.v < it.v).length} of ${S.rows.length} neighbours' WOZ`
      : `<b>${esc(it.name)}</b>${it.me ? " (this house)" : ""}<br>WOZ ${S.Y}: <b>${eur(it.v)}</b>` + (!it.me && S.sv ? `<br>${sEur(it.v - S.sv)} vs this house` : "") + (!it.me && ask ? `<br>${sEur(it.v - ask)} vs the asking price` : "") + (it.h && it.h.dist != null ? `<br>${Math.round(it.h.dist)} m away` : "");
    g += T(lw, y + bh / 2, short(it.name, dense ? 24 : 22), { a: "end", db: "middle", fill: hl ? "var(--ink)" : "var(--ink-2)", w: hl ? 700 : null, fs: dense ? 10 : 11.5 });
    g += hbar(x0, X(it.v), y, bh, it.ask ? ASK_FILL : it.me ? "var(--s1)" : "var(--nb)", tip, it.ask ? ASK_STYLE : "");
    if (!dense || hl) g += T(X(it.v) + 5, y + bh / 2, kEur(it.v), { db: "middle", fill: hl ? "var(--ink)" : "var(--ink-2)", w: hl ? 700 : null });
  });
  const mx = X(S.med);
  g += `<line x1="${mx}" x2="${mx}" y1="${top - 6}" y2="${top + items.length * rh}" stroke="var(--ink-2)" stroke-dasharray="3 3"/>` + T(mx, top - 8, "median " + kEur(S.med), { a: "middle", fill: "var(--ink-2)" });
  put(box, `<svg viewBox="0 0 ${w} ${top + items.length * rh + 4}" height="${top + items.length * rh + 4}">${g}</svg>`);
  const rank = items.filter((i) => !i.ask).findIndex((i) => i.me) + 1;
  const askRank = ask ? items.findIndex((i) => i.ask) + 1 : null;
  $("nbRankNote").textContent = `WOZ value ${S.Y} of each house, highest first. This house is #${rank} of ${items.filter((i) => !i.ask).length}` + (ask ? `; at its asking price of ${eur(ask)} it would be #${askRank} (dashed bar). WOZ values date from 1 January ${S.Y}, so today's market prices are higher for every house.` : ".");
}

// ---------- 2. difference with this house (diverging) ----------
function diffChart(S) {
  const box = $("cDiff");
  if (S.sv == null || !S.rows.length) return empty(box, "Waiting for the neighbours' WOZ values.");
  const ask = askNow();
  const items = S.rows.map((r) => ({ name: r.h.name, d: r.v - S.sv, v: r.v, h: r.h }))
    .concat(ask ? [{ name: ASK_NAME, d: ask - S.sv, v: ask, ask: true }] : []).sort((a, b) => b.d - a.d);
  const w = W(box), dense = items.length > 30, rh = dense ? 13 : 20, bh = dense ? 9 : 13, lw = Math.min(150, w * 0.34), top = 22;
  const x0 = lw + (dense ? 10 : 52), x1 = w - (dense ? 10 : 48); // room for the value labels on both sides
  const m = Math.max(1000, ...items.map((i) => Math.abs(i.d)));
  const zero = x0 + (x1 - x0) / 2, X = (v) => zero + (v / m) * ((x1 - x0) / 2);
  let g = T(zero - 6, 10, "← lower", { a: "end", fill: "var(--ink-2)" }) + T(zero + 6, 10, "higher →", { fill: "var(--ink-2)" });
  items.forEach((it, i) => {
    const y = top + i * rh;
    const tip = it.ask
      ? `<b>Asking price now</b><br>${eur(it.v)}<br><b>${sEur(it.d)}</b> (${pct((it.v / S.sv - 1) * 100)}) on top of its own WOZ ${S.Y}`
      : `<b>${esc(it.name)}</b><br>WOZ ${S.Y}: ${eur(it.v)}<br><b>${sEur(it.d)}</b> (${pct((it.v / S.sv - 1) * 100)}) vs this house` + (ask ? `<br>${sEur(it.v - ask)} vs the asking price` : "");
    g += T(lw, y + bh / 2, short(it.name, dense ? 24 : 22), { a: "end", db: "middle", fill: it.ask ? "var(--ink)" : "var(--ink-2)", w: it.ask ? 700 : null, fs: dense ? 10 : 11.5 });
    g += hbar(zero, X(it.d), y, bh, it.ask ? ASK_FILL : it.d > 0 ? "var(--hi)" : "var(--lo)", tip, it.ask ? ASK_STYLE : "");
    if (!dense || it.ask) g += T(it.d >= 0 ? X(it.d) + 5 : X(it.d) - 5, y + bh / 2, sEur(it.d), { a: it.d >= 0 ? "start" : "end", db: "middle", fill: it.ask ? "var(--ink)" : "var(--ink-2)", w: it.ask ? 700 : null });
  });
  g += `<line x1="${zero}" x2="${zero}" y1="${top - 4}" y2="${top + items.length * rh}" stroke="var(--s1)" stroke-width="2"/>`;
  g += T(zero, top + items.length * rh + 14, "this house's WOZ " + kEur(S.sv), { a: "middle", fill: "var(--ink)", w: 650 });
  if (ask) {
    const ax = X(ask - S.sv);
    g += `<line x1="${ax}" x2="${ax}" y1="${top - 4}" y2="${top + items.length * rh}" stroke="var(--s1)" stroke-width="1.5" stroke-dasharray="3 3" opacity=".7"/>`;
  }
  const H = top + items.length * rh + 20;
  put(box, `<svg viewBox="0 0 ${w} ${H}" height="${H}">${g}</svg>`);
}

// ---------- 3. WOZ over the years (lines) + the asking price today ----------
function yearsChart(S) {
  const box = $("cYears");
  const ys = S.years; if (ys.length < 2) return empty(box, "Needs more than one year.");
  const ask = askNow();
  const now = new Date(), yNow = now.getFullYear() + (now - new Date(now.getFullYear(), 0, 1)) / (365.25 * DAY);
  const info = (DATA && DATA.info) || {};
  const listed = nlDate(info.onFunda) || (info.published ? Date.parse(info.published) : null);
  const orig = readings().map((r) => r.orig).find((v) => v != null);
  const yListed = listed ? new Date(listed).getFullYear() + (listed - new Date(new Date(listed).getFullYear(), 0, 1)) / (365.25 * DAY) : null;
  const xMax = ask ? Math.max(ys[ys.length - 1], yNow) : ys[ys.length - 1];
  const w = W(box), H = 250, m = { l: 52, r: 74, t: 14, b: 24 };
  const series = S.houses.map((h) => ({ h, pts: h.woz.filter(([y]) => ys.includes(y)) }));
  const selfPts = (S.self.woz || []);
  const all = series.flatMap((s) => s.pts.map((p) => p[1])).concat(selfPts.map((p) => p[1]), ask ? [ask] : [], orig ? [orig] : []);
  const yt = niceTicks(0, Math.max(...all), 4), hi = yt[yt.length - 1];
  const X = (y) => m.l + ((y - ys[0]) / (xMax - ys[0])) * (w - m.l - m.r), Y = (v) => m.t + (H - m.t - m.b) * (1 - v / hi);
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
  // from the last WOZ value to today's asking price (and the original one when it was listed)
  if (ask && ls) {
    const pts = [[ls[0], ls[1]]];
    if (orig && yListed && yListed > ls[0] && yListed < yNow) pts.push([yListed, orig]);
    pts.push([yNow, ask]);
    g += `<path d="${path(pts)}" fill="none" stroke="var(--s1)" stroke-width="2" stroke-dasharray="4 3"/>`;
    if (orig && yListed && yListed > ls[0] && yListed < yNow) g += `<circle cx="${X(yListed)}" cy="${Y(orig)}" r="4" fill="var(--surface)" stroke="var(--s1)" stroke-width="2" data-tip="${esc(`<b>Listed on Funda</b><br>${eur(orig)}<br>${fDateY(listed)}`)}"/>`;
    const cx = X(yNow), cy = Y(ask), r = 6;
    g += `<path d="M${cx},${cy - r}L${cx + r},${cy}L${cx},${cy + r}L${cx - r},${cy}Z" fill="var(--surface)" stroke="var(--s1)" stroke-width="2" data-tip="${esc(`<b>Asking price now</b><br>${eur(ask)}<br>${sEur(ask - ls[1])} on top of its WOZ ${ls[0]}`)}"/>`;
    g += T(cx + 9, cy - 2, "asking", { fill: "var(--ink)", w: 650 }) + T(cx + 9, cy + 11, kEur(ask), { fill: "var(--ink)", w: 650 });
    g += T(cx, H - 6, "now", { a: "middle", fill: "var(--ink-2)" });
  }
  if (ls) g += `<circle cx="${X(ls[0])}" cy="${Y(ls[1])}" r="4" fill="var(--s1)" stroke="var(--surface)" stroke-width="2"/>` + T(X(ls[0]) - 6, Y(ls[1]) - 8, "this house", { a: "end", fill: "var(--ink)", w: 650 });
  if (lm) g += T(X(lm[0]) + 7, Y(lm[1]) + 12, "median", { fill: "var(--ink-2)" });
  // hover: one band per year
  ys.forEach((y, i) => {
    const vals = S.houses.map((h) => wozMap(h).get(y)).filter((v) => v != null);
    const sv = wozMap(S.self).get(y);
    const x = X(y), half = (X(ys[1]) - X(ys[0])) / 2;
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
  const ask = askNow();
  const items = S.houses.map((h) => ({ name: h.name, g: g0(h) })).filter((i) => i.g != null)
    .concat([{ name: S.self.name, g: g0(S.self), me: true }], ask && sm.has(from) ? [{ name: ASK_NAME, g: (ask / sm.get(from) - 1) * 100, ask: true }] : [])
    .filter((i) => i.g != null).sort((a, b) => b.g - a.g);
  $("nbGrowthNote").textContent = `How much each WOZ value grew from ${from} to ${S.Y}.` + (ask && sm.has(from) ? ` Dashed: from this house's ${from} WOZ (${kEur(sm.get(from))}) to its asking price now.` : "");
  const w = W(box), dense = items.length > 30, rh = dense ? 12 : 19, lw = Math.min(140, w * 0.38), top = 6;
  const x0 = lw + 8, x1 = w - 46;
  const lo = Math.min(0, ...items.map((i) => i.g)), hi = Math.max(...items.map((i) => i.g));
  const X = (v) => x0 + ((v - lo) / (hi - lo || 1)) * (x1 - x0);
  let g = `<line x1="${X(0)}" x2="${X(0)}" y1="${top}" y2="${top + items.length * rh}" stroke="var(--axis)"/>`;
  items.forEach((it, i) => {
    const y = top + i * rh + rh / 2;
    const hl = it.me || it.ask, c = hl ? "var(--s1)" : "var(--nb)";
    g += T(lw, y, short(it.name, it.ask ? 24 : 18), { a: "end", db: "middle", fill: hl ? "var(--ink)" : "var(--ink-2)", w: hl ? 700 : null, fs: dense ? 10 : it.ask ? 10.5 : 11.5 });
    g += `<line x1="${X(0)}" x2="${X(it.g)}" y1="${y}" y2="${y}" stroke="${c}" stroke-width="2" ${it.ask ? 'stroke-dasharray="4 3"' : ""}/>`;
    g += it.ask
      ? `<path d="M${X(it.g)},${y - 6}L${X(it.g) + 6},${y}L${X(it.g)},${y + 6}L${X(it.g) - 6},${y}Z" fill="var(--surface)" stroke="var(--s1)" stroke-width="2"/>`
      : `<circle cx="${X(it.g)}" cy="${y}" r="${it.me ? 5 : 4}" fill="${c}" stroke="var(--surface)" stroke-width="1.5"/>`;
    if (!dense || hl) g += T(X(it.g) + 9, y, pct(it.g, 0), { db: "middle", fill: hl ? "var(--ink)" : "var(--ink-2)", w: hl ? 650 : null });
    g += `<rect x="${x0 - 4}" y="${y - rh / 2}" width="${x1 - x0 + 8}" height="${rh}" fill="transparent" data-tip="${esc(it.ask ? `<b>Asking price now</b><br>${from} WOZ → ${eur(ask)}: <b>${pct(it.g)}</b>` : `<b>${esc(it.name)}</b>${it.me ? " (this house)" : ""}<br>${from} → ${S.Y}: <b>${pct(it.g)}</b>`)}"/>`;
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
  const ask = askNow();
  const askM2 = ask && S.self.area > 15 ? ask / S.self.area : null;
  const items = S.rows.filter((r) => r.h.area > 15).map((r) => ({ name: r.h.name, v: r.v / r.h.area, r }))
    .concat(S.sv != null && S.self.area > 15 ? [{ name: S.self.name, v: S.sv / S.self.area, me: true }] : [], askM2 ? [{ name: ASK_NAME, v: askM2, ask: true }] : [])
    .sort((a, b) => b.v - a.v);
  if (items.length < 2) return empty(box, "No living areas known (BAG) for these houses.");
  const w = W(box), dense = items.length > 30, rh = dense ? 12 : 19, bh = dense ? 8 : 12, lw = Math.min(130, w * 0.36), top = 18;
  const x0 = lw + 6, x1 = w - 46, max = Math.max(...items.map((i) => i.v), askM2 || 0), X = (v) => x0 + (v / max) * (x1 - x0);
  let g = "";
  items.forEach((it, i) => {
    const y = top + i * rh;
    const hl = it.me || it.ask;
    g += T(lw, y + bh / 2, short(it.name, it.ask ? 24 : 18), { a: "end", db: "middle", fill: hl ? "var(--ink)" : "var(--ink-2)", w: hl ? 700 : null, fs: dense ? 10 : it.ask ? 10.5 : 11.5 });
    const tip = it.ask ? `<b>Asking price now</b><br><b>${eur(Math.round(it.v))}</b> per m²<br>${eur(ask)} / ${S.self.area} m² (BAG)` : `<b>${esc(it.name)}</b><br><b>${eur(Math.round(it.v))}</b> per m²<br>${eur(it.me ? S.sv : it.r.v)} / ${it.me ? S.self.area : it.r.h.area} m²`;
    g += hbar(x0, X(it.v), y, bh, it.ask ? ASK_FILL : it.me ? "var(--s1)" : "var(--nb)", tip, it.ask ? ASK_STYLE : "");
    if (!dense || hl) g += T(X(it.v) + 5, y + bh / 2, "€" + nf.format(Math.round(it.v)), { db: "middle", fill: hl ? "var(--ink)" : "var(--ink-2)", w: hl ? 650 : null });
  });
  const H = top + items.length * rh + 4;
  put(box, `<svg viewBox="0 0 ${w} ${H}" height="${H}">${g}</svg>`);
  const med = median(items.filter((i) => !i.me && !i.ask).map((i) => i.v));
  const fundaM2 = DATA && DATA.info && DATA.info.living;
  $("nbM2Note").textContent = `WOZ ${S.Y} ÷ living area from the BAG, the same source for every house; the dashed bar is the asking price now. Neighbours' median €${nf.format(Math.round(med))}/m²`
    + (askM2 ? `; asking price €${nf.format(Math.round(askM2))}/m².` : ".")
    + (fundaM2 && S.self.area && Math.abs(fundaM2 - S.self.area) > 2 ? ` (The BAG gives this house ${S.self.area} m², Funda ${fundaM2} m².)` : "");
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
  const askD = askNow() ? askNow() - S.sv : 0;
  const lim = Math.max(1000, Math.abs(askD) * 1.15, ...pts.map((p) => Math.abs(p.y)));
  const yt = niceTicks(-lim, lim, 4), xt = niceTicks(0, Math.max(150, ...pts.map((p) => p.x)), 5);
  const X = (v) => m.l + (v / xt.at(-1)) * (w - m.l - m.r), Y = (v) => m.t + (H - m.t - m.b) * (1 - (v - yt[0]) / (yt.at(-1) - yt[0]));
  let g = "";
  for (const v of yt) g += `<line x1="${m.l}" x2="${w - m.r}" y1="${Y(v)}" y2="${Y(v)}" stroke="${v === 0 ? "var(--s1)" : "var(--grid)"}" ${v === 0 ? 'stroke-width="2"' : ""}/>` + T(m.l - 6, Y(v) + 4, v === 0 ? "same" : sEur(v), { a: "end", fill: v === 0 ? "var(--ink)" : "var(--muted)" });
  for (const v of xt) g += T(X(v), H - 18, v + " m", { a: "middle" });
  g += T((m.l + w - m.r) / 2, H - 3, "distance from this house", { a: "middle", fill: "var(--ink-2)" });
  const ask = askNow();
  if (ask && ask - S.sv > yt[0] && ask - S.sv < yt.at(-1)) {
    const ay = Y(ask - S.sv);
    g += `<line x1="${m.l}" x2="${w - m.r}" y1="${ay}" y2="${ay}" stroke="var(--s1)" stroke-width="1.5" stroke-dasharray="5 4"/>` + T(w - m.r, ay - 5, "asking price now " + kEur(ask), { a: "end", fill: "var(--ink)", w: 650 });
  }
  for (const p of pts) g += `<circle cx="${X(p.x)}" cy="${Y(p.y)}" r="4.5" fill="${p.y > 0 ? "var(--hi)" : "var(--lo)"}" stroke="var(--surface)" stroke-width="1.5" data-tip="${esc(`<b>${esc(p.name)}</b><br>${Math.round(p.x)} m away<br>WOZ ${eur(p.v)} (${sEur(p.y)})`)}"/>`;
  put(box, `<svg viewBox="0 0 ${w} ${H}" height="${H}">${g}</svg>`);
}

// ---------- value score: every house against all the chosen neighbours ----------
// Per factor a house gets its place among the others (0 = worst, 1 = best: the mid-rank percentile),
// then the factors are mixed with the weights below. Factors a house has no data for are left out of its mix.
const SCORE_F = [
  { k: "m2", name: "Price per m²", hint: "WOZ value ÷ living area (BAG). Lower is better.", better: -1, w: 35,
    val: (h, y) => { const v = wozAt(h, y); return v != null && h.area > 15 ? v / h.area : null; }, fmt: (v) => "€" + nf.format(Math.round(v)) + "/m²" },
  { k: "area", name: "Living area", hint: "More m² is better.", better: 1, w: 20, val: (h) => (h.area > 15 ? h.area : null), fmt: (v) => Math.round(v) + " m²" },
  { k: "plot", name: "Plot size", hint: "Land with the house (WOZ). Bigger is better.", better: 1, w: 15, val: (h) => (h.ground > 0 ? h.ground : null), fmt: (v) => Math.round(v) + " m²" },
  { k: "growth", name: "Value growth", hint: "How much the WOZ value grew since the first year. More is better.", better: 1, w: 15,
    val: (h, y, base) => growthAt(h, y, base), fmt: (v) => pct(v * 100, 0) },
  { k: "built", name: "Build year", hint: "Newer is better.", better: 1, w: 15, val: (h) => (h.built > 1000 ? h.built : null), fmt: (v) => String(Math.round(v)) },
];
const W_STD = Object.fromEntries(SCORE_F.map((f) => [f.k, f.w]));
let SW = { ...W_STD };
try { const x = JSON.parse(localStorage.getItem("funda.w") || "null"); if (x) for (const k in W_STD) if (typeof x[k] === "number") SW[k] = x[k]; } catch {}
const askPrice = () => askNow();

function scoreSet(S, y) {
  const base = mapScales(S).base;
  const hs = [S.self, ...S.houses].filter((h) => h && h.woz && h.woz.length && wozAt(h, y) != null);
  const raw = {}, sorted = {};
  for (const f of SCORE_F) { raw[f.k] = hs.map((h) => f.val(h, y, base)); sorted[f.k] = raw[f.k].filter((v) => v != null); }
  const place = (f, x) => {
    const arr = sorted[f.k];
    if (x == null || arr.length < 2) return null;
    let lt = 0, eq = 0;
    for (const v of arr) { if (v < x - 1e-9) lt++; else if (Math.abs(v - x) <= 1e-9) eq++; }
    const p = (lt + 0.5 * eq) / arr.length;
    return f.better < 0 ? 1 - p : p;
  };
  const mixIt = (xs) => {
    let acc = 0, sw = 0; const parts = {};
    SCORE_F.forEach((f, i) => { const x = xs[i], p = place(f, x); parts[f.k] = { x, p }; const w = SW[f.k] || 0; if (p != null && w > 0) { acc += w * p; sw += w; } });
    return { score: sw ? (100 * acc) / sw : null, parts };
  };
  const list = hs.map((h, i) => ({ h, ...mixIt(SCORE_F.map((f) => raw[f.k][i])) })).filter((e) => e.score != null);
  list.sort((a, b) => b.score - a.score);
  list.forEach((e, i) => { e.rank = i + 1; });
  const byNid = new Map(list.map((e) => [e.h.nid, e]));
  // this house at its asking price (asking price ÷ living area instead of WOZ ÷ living area)
  const ask = askPrice();
  let askE = null;
  if (ask && S.self.area > 15) askE = mixIt(SCORE_F.map((f) => (f.k === "m2" ? ask / S.self.area : f.val(S.self, y, base))));
  return { list, byNid, ask, askE, n: list.length, raw, sorted, base };
}

function weightsUI() {
  const box = $("wSliders");
  if (box.children.length) return;
  box.innerHTML = SCORE_F.map((f) => `<div class="wrow"><label for="w-${f.k}">${f.name}</label><output id="wo-${f.k}"></output><small>${f.hint}</small>
    <input type="range" id="w-${f.k}" min="0" max="100" step="5" value="${SW[f.k]}"></div>`).join("");
  const show = () => { const tot = SCORE_F.reduce((a, f) => a + (SW[f.k] || 0), 0) || 1; SCORE_F.forEach((f) => { $("wo-" + f.k).textContent = SW[f.k] ? Math.round((100 * SW[f.k]) / tot) + "% of the score" : "not counted"; }); };
  show();
  let t = 0;
  box.addEventListener("input", (e) => {
    const k = e.target.id.replace("w-", ""); if (!(k in SW)) return;
    SW[k] = +e.target.value; show();
    try { localStorage.setItem("funda.w", JSON.stringify(SW)); } catch {}
    clearTimeout(t); t = setTimeout(() => { const S = nbSet(); if (S) { scoreCharts(S); refreshMap(); } }, 60);
  });
  $("wReset").addEventListener("click", () => {
    SW = { ...W_STD }; try { localStorage.removeItem("funda.w"); } catch {}
    SCORE_F.forEach((f) => { $("w-" + f.k).value = SW[f.k]; }); show();
    const S = nbSet(); if (S) { scoreCharts(S); refreshMap(); }
  });
}

function scoreCharts(S) {
  weightsUI();
  const R = scoreSet(S, S.Y);
  const me = R.byNid.get(S.self.nid);
  const best = R.list.find((e) => e.h !== S.self);
  const t = (k, v, d) => `<div class="tile"><div class="k">${k}</div><div class="v">${v}</div><div class="d">${d || ""}</div></div>`;
  $("scoreTiles").innerHTML =
    t("This house · " + S.Y, me ? Math.round(me.score) + '<span class="muted" style="font-size:14px;font-weight:600"> / 100</span>' : "–", me ? `#${me.rank} of ${R.n} houses` : "") +
    t("At its asking price", R.askE ? Math.round(R.askE.score) + '<span class="muted" style="font-size:14px;font-weight:600"> / 100</span>' : "–", R.askE ? `${eur(R.ask)} instead of the WOZ value` : "") +
    t("Neighbours' median", R.n > 1 ? String(Math.round(median(R.list.filter((e) => e.h !== S.self).map((e) => e.score)))) : "–", "the middle score around it") +
    t("Best value nearby", best ? Math.round(best.score) + "" : "–", best ? esc(best.h.name) + (best.h.dist != null ? ` · ${Math.round(best.h.dist)} m` : "") : "");
  $("scoreNote").textContent = `Scores for ${S.Y}` + (R.askE && me ? `. The dashed bar is this house at its asking price now: ${Math.round(R.askE.score)} instead of ${Math.round(me.score)}, because the asking price is a ${R.ask > (wozAt(S.self, S.Y) || 0) ? "higher" : "lower"} price per m² than its WOZ value. WOZ values lag behind the market, so most houses would score lower at today's prices too.` : ".");
  scoreRankChart(S, R);
  factorChart(S, R);
}

function partsTip(e) {
  return SCORE_F.filter((f) => SW[f.k] > 0).map((f) => { const q = e.parts[f.k]; return q && q.p != null ? `${f.name}: ${f.fmt(q.x)} → ${Math.round(q.p * 100)}` : `${f.name}: –`; }).join("<br>");
}
function scoreRankChart(S, R) {
  const box = $("cScore");
  if (R.n < 2) return empty(box, "Waiting for the neighbours' WOZ values.");
  const rows = R.list.map((e) => ({ e, name: e.h.name, score: e.score, me: e.h === S.self }))
    .concat(R.askE ? [{ e: R.askE, name: ASK_NAME, score: R.askE.score, ask: true }] : []).sort((a, b) => b.score - a.score);
  const n = rows.length;
  const w = W(box), dense = n > 30, rh = dense ? 13 : 20, bh = dense ? 9 : 13, lw = Math.min(150, w * 0.36), top = 18;
  const x0 = lw + 6, x1 = w - (dense ? 10 : 34), X = (v) => x0 + (v / 100) * (x1 - x0);
  let g = "";
  for (const v of [0, 25, 50, 75, 100]) g += `<line x1="${X(v)}" x2="${X(v)}" y1="${top - 4}" y2="${top + n * rh}" stroke="var(--grid)"/>` + T(X(v), top - 7, String(v), { a: "middle" });
  rows.forEach((r, i) => {
    const y = top + i * rh, hl = r.me || r.ask;
    const tip = r.ask ? `<b>This house at its asking price now</b><br>${eur(R.ask)}<br>Score <b>${Math.round(r.score)}</b><br>${partsTip(r.e)}`
      : `<b>${esc(r.name)}</b>${r.me ? " (this house, WOZ)" : ""}<br>Score <b>${Math.round(r.score)}</b> · #${r.e.rank} of ${R.n}<br>${partsTip(r.e)}`;
    g += T(lw, y + bh / 2, short(r.name, dense ? 24 : 22), { a: "end", db: "middle", fill: hl ? "var(--ink)" : "var(--ink-2)", w: hl ? 700 : null, fs: dense ? 10 : r.ask ? 10.5 : 11.5 });
    g += hbar(x0, X(r.score), y, bh, r.ask ? ASK_FILL : r.me ? "var(--s1)" : "var(--nb)", tip, r.ask ? ASK_STYLE : "");
    if (!dense || hl) g += T(X(r.score) + 5, y + bh / 2, String(Math.round(r.score)), { db: "middle", fill: hl ? "var(--ink)" : "var(--ink-2)", w: hl ? 700 : null });
  });
  const H = top + n * rh + 4;
  put(box, `<svg viewBox="0 0 ${w} ${H}" height="${H}">${g}</svg>`);
}
function factorChart(S, R) {
  const box = $("cFactors");
  const me = R.byNid.get(S.self.nid);
  if (!me || R.n < 2) return empty(box, "Waiting for the neighbours' WOZ values.");
  const w = W(box), rowH = 64, top = 4, x0 = 14, x1 = w - 14;
  let g = "", row = 0;
  for (const f of SCORE_F) {
    const vals = R.sorted[f.k];
    const my = me.parts[f.k];
    const y = top + row * rowH;
    const wt = SW[f.k] || 0, tot = SCORE_F.reduce((a, q) => a + (SW[q.k] || 0), 0) || 1;
    g += T(x0, y + 12, f.name + (wt ? ` · ${Math.round((100 * wt) / tot)}%` : " · not counted"), { fill: wt ? "var(--ink)" : "var(--muted)", w: 650, fs: 12.5 });
    if (my && my.p != null) g += T(x1, y + 12, `better than ${Math.round(my.p * 100)}%`, { a: "end", fill: "var(--ink)", w: 650, fs: 12 });
    if (vals.length < 2 || !my || my.x == null) { g += T(x1, y + 12, "no data", { a: "end", fs: 11.5 }); row++; continue; }
    let lo = Math.min(...vals), hi = Math.max(...vals);
    const askX = f.k === "m2" && R.askE ? R.askE.parts.m2.x : null;
    if (askX != null) { lo = Math.min(lo, askX); hi = Math.max(hi, askX); }
    if (hi === lo) { lo -= 1; hi += 1; }
    // right is always better: flip the axis when lower is better
    const X = (v) => (f.better < 0 ? x1 - ((v - lo) / (hi - lo)) * (x1 - x0) : x0 + ((v - lo) / (hi - lo)) * (x1 - x0));
    const ly = y + 34;
    g += `<line x1="${x0}" x2="${x1}" y1="${ly}" y2="${ly}" stroke="var(--axis)"/>`;
    const med = median(vals);
    g += `<line x1="${X(med)}" x2="${X(med)}" y1="${ly - 9}" y2="${ly + 9}" stroke="var(--ink-2)" stroke-width="1.5"/>`;
    // neighbours: small dots, nudged up/down when they would overlap
    const used = [];
    R.list.forEach((e) => {
      const q = e.parts[f.k]; if (e.h === S.self || !q || q.x == null) return;
      const cx = X(q.x); let k = 0;
      while (used.some((u) => Math.abs(u[0] - cx) < 6 && u[1] === k) && k < 4) k++;
      used.push([cx, k]);
      const cy = ly + (k % 2 ? 1 : -1) * Math.ceil(k / 2) * 6;
      g += `<circle cx="${cx}" cy="${cy}" r="3.5" fill="var(--nb)" data-tip="${esc(`<b>${esc(e.h.name)}</b><br>${f.name}: ${f.fmt(q.x)}`)}"/>`;
    });
    if (askX != null) {
      const cx = X(askX), r = 6;
      g += `<path d="M${cx},${ly - r}L${cx + r},${ly}L${cx},${ly + r}L${cx - r},${ly}Z" fill="var(--surface)" stroke="var(--s1)" stroke-width="2" data-tip="${esc(`<b>This house at its asking price</b><br>${f.fmt(askX)}`)}"/>`;
    }
    g += `<circle cx="${X(my.x)}" cy="${ly}" r="6" fill="var(--s1)" stroke="var(--surface)" stroke-width="2" data-tip="${esc(`<b>This house</b><br>${f.name}: ${f.fmt(my.x)}<br>Neighbours' median: ${f.fmt(med)}<br>Better than ${Math.round(my.p * 100)}% of them`)}"/>`;
    const left = f.better < 0 ? hi : lo, right = f.better < 0 ? lo : hi;
    g += T(x0, ly + 21, f.fmt(left), { fs: 10.5 }) + T(x1, ly + 21, f.fmt(right) + " →", { a: "end", fs: 10.5 });
    g += T((x0 + x1) / 2, ly + 21, "this house " + f.fmt(my.x), { a: "middle", fs: 11, fill: "var(--ink)", w: 600 });
    row++;
  }
  const H = top + row * rowH + 4;
  put(box, `<svg viewBox="0 0 ${w} ${H}" height="${H}">${g}</svg>`);
}

// ---------- the 3D map (MapLibre, PDOK map / aerial photo, extruded BAG buildings) ----------
const MAP = { map: null, loading: null, base: "map", height: "flat", spin: true, S: null, ready: false, mode: "diff", year: null, playing: false };
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
// ----- colours: difference (green - grey - red), WOZ value (orange ramp), growth (violet ramp) -----
const mix = (a, b, k) => a.map((v, i) => Math.round(v + (b[i] - v) * k));
const rgbS = (c) => `rgb(${c.join(",")})`;
const clamp01 = (k) => Math.max(0, Math.min(1, k));
function diffColor(d, lim) {
  const lo = [27, 175, 122], mid = [216, 214, 206], hi = [227, 73, 72];
  const t = Math.max(-1, Math.min(1, d / lim));
  return rgbS(mix(mid, t < 0 ? lo : hi, Math.abs(t)));
}
const RAMPS = {
  value: [[253, 230, 210], [235, 104, 52], [110, 36, 8]],
  growth: [[230, 224, 250], [98, 80, 214], [34, 20, 104]],
  score: [[221, 243, 235], [27, 175, 122], [6, 78, 52]],
};
function rampColor(name, k) {
  const r = RAMPS[name]; k = clamp01(k);
  return rgbS(k < 0.5 ? mix(r[0], r[1], k * 2) : mix(r[1], r[2], (k - 0.5) * 2));
}
const GREY = "#d9d8d2";

// WOZ of a house at a (fractional) year: a straight line between the yearly values
function wozAt(h, y) {
  const w = h && h.woz; if (!w || !w.length) return null;
  if (y < w[0][0] - 1e-6) return null;
  for (let i = w.length - 1; i >= 0; i--) {
    if (w[i][0] <= y + 1e-6) {
      const b = w[i + 1];
      return b && b[0] > w[i][0] ? w[i][1] + (b[1] - w[i][1]) * ((y - w[i][0]) / (b[0] - w[i][0])) : w[i][1];
    }
  }
  return null;
}

// the scales stay the same for every year, so playing through the years shows the change
function mapScales(S) {
  if (S._scales) return S._scales;
  const all = [S.self, ...S.houses].filter((h) => h && h.woz && h.woz.length);
  const base = (S.self.woz && S.self.woz.length ? S.self.woz[0][0] : S.years[0]);
  const last = S.years[S.years.length - 1];
  const vals = all.flatMap((h) => h.woz.map((x) => x[1]));
  let lim = 15000, gmax = 0.01;
  for (const y of S.years) {
    const sv = wozAt(S.self, y);
    for (const h of all) {
      const v = wozAt(h, y); if (v == null) continue;
      if (sv != null && h !== S.self) lim = Math.max(lim, Math.abs(v - sv));
      const b0 = wozAt(h, base); if (b0) gmax = Math.max(gmax, v / b0 - 1);
    }
  }
  const ask = askPrice();
  // sized on the latest year: older years simply show as (much) lower than today's asking price
  const lastVals = all.map((h) => wozAt(h, last)).filter((v) => v != null);
  const askLim = ask ? Math.max(15000, ...lastVals.map((v) => Math.abs(v - ask))) : 50000;
  return (S._scales = { base, first: S.years[0], last, vmin: Math.min(...vals), vmax: Math.max(...vals), lim, gmax, all, ask, askLim });
}
const growthAt = (h, y, base) => { const v = wozAt(h, y), b = wozAt(h, base); return v != null && b ? v / b - 1 : null; };

const heightProp = () => ["get", MAP.height === "woz" ? "hWoz" : MAP.height === "score" ? "hScore" : "hFlat"];
function mapFeatures(S) {
  const d = NB.data;
  const byPand = new Map();
  const add = (h, isSelf, cmp) => { if (!h.pand) return; if (!byPand.has(h.pand)) byPand.set(h.pand, []); byPand.get(h.pand).push({ h, isSelf, cmp }); };
  if (d.self) add(d.self, true, true);
  for (const h of d.houses) add(h, false, true);
  for (const h of d.others || []) add(h, false, false);
  const sc = S ? mapScales(S) : null;
  const Y = MAP.year != null ? MAP.year : S ? S.Y : null;
  const sv = S ? wozAt(S.self, Y) : null;
  const R = S && (MAP.mode === "score" || MAP.height === "score") ? scoreSet(S, Y) : null;
  MAP.R = R;
  const feats = d.panden.map((p) => {
    const list = byPand.get(p.id) || [];
    const me = list.some((x) => x.isSelf);
    const known = list.filter((x) => x.cmp && x.h.woz && x.h.woz.length);
    const vs = S ? known.map((x) => wozAt(x.h, Y)).filter((v) => v != null) : [];
    const gs = S ? known.map((x) => growthAt(x.h, Y, sc.base)).filter((v) => v != null) : [];
    const v = vs.length ? vs.reduce((a, b) => a + b, 0) / vs.length : null;
    const g = gs.length ? gs.reduce((a, b) => a + b, 0) / gs.length : null;
    const ss = R ? known.map((x) => R.byNid.get(x.h.nid)).filter(Boolean).map((e) => e.score) : [];
    const score = ss.length ? ss.reduce((a, b) => a + b, 0) / ss.length : null;
    let color = GREY;
    if (S && MAP.mode === "diff") color = me ? (cssv("--s1") || "#2a78d6") : v != null && sv != null ? diffColor(v - sv, sc.lim) : GREY;
    else if (S && MAP.mode === "value") color = v != null ? rampColor("value", (v - sc.vmin) / (sc.vmax - sc.vmin || 1)) : GREY;
    else if (S && MAP.mode === "growth") color = g != null ? rampColor("growth", g / sc.gmax) : GREY;
    else if (S && MAP.mode === "ask") color = v != null && sc.ask ? diffColor(v - sc.ask, sc.askLim) : GREY;
    else if (S && MAP.mode === "score") color = score != null ? rampColor("score", score / 100) : GREY;
    const flatH = 6 + Math.min(6, Math.max(0, list.length - 1)) * 1.2;
    const wozH = v != null && sc ? 3 + 30 * (v / sc.vmax) : 2;
    const names = list.map((x) => x.h.name);
    return {
      type: "Feature",
      properties: { id: p.id, color, me, hFlat: me ? flatH + 1 : flatH, hWoz: wozH, hScore: score != null ? 2 + 0.3 * score : 2, v: v || 0, g: g == null ? -99 : g, s: score == null ? -1 : score, n: list.length, known: vs.length, built: p.built || 0, names: JSON.stringify(names.slice(0, 6)) },
      geometry: { type: "Polygon", coordinates: [p.ring] },
    };
  });
  return { type: "FeatureCollection", features: feats };
}

// the 3 houses whose WOZ grew most (up to the year on the slider): labels on the map in "Most changed"
function topGrowth(S, n = 3) {
  const sc = mapScales(S);
  return sc.all.map((h) => ({ h, g: growthAt(h, MAP.year, sc.base) })).filter((x) => x.g != null && x.h.lon != null).sort((a, b) => b.g - a.g).slice(0, n);
}
function updatePins() {
  MAP.pins = MAP.pins || [];
  const S = MAP.S;
  const top = !S || !MAP.ready ? [] : MAP.mode === "growth" ? topGrowth(S).map((t) => ({ h: t.h, txt: pct(t.g * 100, 0) }))
    : MAP.mode === "score" && MAP.R ? MAP.R.list.filter((e) => e.h.lon != null).slice(0, 3).map((e) => ({ h: e.h, txt: Math.round(e.score) + " pts" })) : [];
  while (MAP.pins.length < top.length) {
    const el = document.createElement("div"); el.className = "chg-pin";
    MAP.pins.push({ el, m: new maplibregl.Marker({ element: el, anchor: "bottom", offset: [0, -14] }) });
  }
  MAP.pins.forEach((pn, i) => {
    const t = top[i];
    if (!t) { pn.m.remove(); pn.on = false; return; }
    pn.el.innerHTML = `<b>#${i + 1}</b> ${t.txt}`;
    pn.el.style.background = MAP.mode === "score" ? "#06604a" : "#3b2a9e";
    pn.el.title = t.h.name;
    pn.m.setLngLat([t.h.lon, t.h.lat]);
    if (!pn.on) { pn.m.addTo(MAP.map); pn.on = true; }
  });
}

function refreshMap() {
  if (!MAP.map) return;
  if (!MAP.ready) { if (!MAP.map.isStyleLoaded()) return; MAP.ready = true; const m = $("mapMsg"); if (m) m.remove(); spin(); }
  MAP.map.getSource("panden").setData(mapFeatures(MAP.S));
  updatePins();
  legendMap(MAP.S);
  const yi = Math.round(MAP.year || 0);
  $("mapYearBig").textContent = MAP.year != null ? String(MAP.playing ? Math.floor(MAP.year + 1e-6) : yi) : "";
}

async function drawMap(S) {
  MAP.S = S;
  const d = NB.data;
  if (!d || !d.center) return;
  const box = $("map3d");
  if (!d.panden || !d.panden.length) { $("mapMsg").textContent = "No building outlines found for this spot."; return; }
  if (S) setupSlider(S);
  if (MAP.map) { refreshMap(); return; }
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
        panden: { type: "geojson", data: mapFeatures(MAP.S) },
      },
      layers: [
        { id: "bg", type: "background", paint: { "background-color": "#e9e8e3" } },
        { id: "base", type: "raster", source: "base", paint: { "raster-opacity": 0.95 } },
        { id: "ring-fill", type: "fill", source: "ring", paint: { "fill-color": "#2a78d6", "fill-opacity": 0.05 } },
        { id: "ring-line", type: "line", source: "ring", paint: { "line-color": "#2a78d6", "line-width": 2, "line-dasharray": [2, 2], "line-opacity": 0.8 } },
        { id: "b3d", type: "fill-extrusion", source: "panden", paint: {
          "fill-extrusion-color": ["get", "color"],
          "fill-extrusion-height": heightProp(),
          "fill-extrusion-base": 0, "fill-extrusion-opacity": 0.92, "fill-extrusion-vertical-gradient": true } },
      ],
    },
  });
  MAP.map = map;
  map.addControl(new maplibregl.NavigationControl({ visualizePitch: true }), "top-right");
  const pin = document.createElement("div"); pin.className = "me-pin"; pin.textContent = "This house";
  new maplibregl.Marker({ element: pin, anchor: "bottom", offset: [0, -24] }).setLngLat(d.self ? [d.self.lon, d.self.lat] : c).addTo(map);
  // ready as soon as the style is there (the "load" event can wait forever when base-map tiles fail)
  const onReady = () => {
    if (MAP.ready || !map.isStyleLoaded()) return;
    MAP.ready = true;
    const m = $("mapMsg"); if (m) m.remove();
    refreshMap(); spin();
  };
  map.on("load", onReady); map.on("idle", onReady); map.on("styledata", () => setTimeout(onReady, 0));
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
    const yr = Math.round(MAP.year || (S2 && S2.Y) || 0);
    const lines = [`<b>${esc(names[0] || "Building")}</b>${p.n > 1 ? ` <span style="color:#52514e">+ ${p.n - 1} more address${p.n > 2 ? "es" : ""}</span>` : ""}`];
    if (p.known && S2) {
      const sc = mapScales(S2);
      const v = p.v, svy = wozAt(S2.self, yr);
      lines.push(`WOZ ${yr}: <b>${eur(Math.round(p.v / 1000) * 1000)}</b>${p.known > 1 ? " (average)" : ""}${p.me ? " · this house" : ""}`);
      if (!p.me && svy) lines.push(`${sEur(v - svy)} (${pct((v / svy - 1) * 100)}) vs this house`);
      if (sc.ask) lines.push(`${sEur(v - sc.ask)} vs the asking price of this house`);
      if (p.g > -50) lines.push(`Grew ${pct(p.g * 100, 0)} since ${sc.base}`);
      const R = MAP.R || scoreSet(S2, yr);
      const e = names.length ? R.list.find((x) => x.h.name === names[0]) : null;
      if (e) lines.push(`Value score <b>${Math.round(e.score)}</b> · #${e.rank} of ${R.n}`);
    } else lines.push(`<span style="color:#52514e">Not compared. Choose more houses above to include it.</span>`);
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
const sq = (bg) => `<i style="background:${bg};display:inline-block;width:11px;height:11px;border-radius:3px;vertical-align:-1px"></i>`;
function legendMap(S) {
  const el = $("mapScale");
  if (!S || !S.rows.length) { el.innerHTML = ""; return; }
  const sc = mapScales(S), yr = Math.round(MAP.year != null ? MAP.year : S.Y);
  let h = "";
  if (MAP.mode === "diff") {
    h = `<span>WOZ ${yr} vs this house:</span><span>${sEur(-sc.lim)}</span><span class="bar" style="background:linear-gradient(90deg, ${diffColor(-sc.lim, sc.lim)}, ${diffColor(0, sc.lim)}, ${diffColor(sc.lim, sc.lim)})"></span><span>${sEur(sc.lim)}</span><span>${sq("var(--s1)")} this house</span>`;
  } else if (MAP.mode === "value") {
    h = `<span>WOZ value ${yr}:</span><span>${kEur(sc.vmin)}</span><span class="bar" style="background:linear-gradient(90deg, ${rampColor("value", 0)}, ${rampColor("value", 0.5)}, ${rampColor("value", 1)})"></span><span>${kEur(sc.vmax)}</span>`;
  } else if (MAP.mode === "ask") {
    h = sc.ask ? `<span>WOZ ${yr} vs this house's asking price ${kEur(sc.ask)}:</span><span>${sEur(-sc.askLim)}</span><span class="bar" style="background:linear-gradient(90deg, ${diffColor(-sc.askLim, sc.askLim)}, ${diffColor(0, sc.askLim)}, ${diffColor(sc.askLim, sc.askLim)})"></span><span>${sEur(sc.askLim)}</span>` : "<span>No asking price yet.</span>";
  } else if (MAP.mode === "score") {
    const R = MAP.R || scoreSet(S, MAP.year != null ? MAP.year : S.Y);
    const me = R.byNid.get(S.self.nid);
    h = `<span>Value score ${yr}:</span><span>0</span><span class="bar" style="background:linear-gradient(90deg, ${rampColor("score", 0)}, ${rampColor("score", 0.5)}, ${rampColor("score", 1)})"></span><span>100</span>`
      + `<span style="flex-basis:100%">${me ? `This house: <b>${Math.round(me.score)}</b> (#${me.rank} of ${R.n}) · ` : ""}Best: ${R.list.slice(0, 3).map((e, i) => `<b>#${i + 1}</b> ${esc(e.h.name)}${e.h === S.self ? " (this house)" : ""} ${Math.round(e.score)}`).join(" · ")}</span>`;
  } else {
    const top = topGrowth(S);
    h = `<span>Growth ${sc.base} → ${yr}:</span><span>0%</span><span class="bar" style="background:linear-gradient(90deg, ${rampColor("growth", 0)}, ${rampColor("growth", 0.5)}, ${rampColor("growth", 1)})"></span><span>${pct(sc.gmax * 100, 0)}</span>`
      + (top.length ? `<span style="flex-basis:100%">Most changed: ${top.map((t, i) => `<b>#${i + 1}</b> ${esc(t.h.name)}${t.h === S.self ? " (this house)" : ""} ${pct(t.g * 100, 0)}`).join(" · ")}</span>` : "");
  }
  el.innerHTML = h + `<span>${sq(GREY)} not compared</span>`;
}

// ----- time slider + play -----
function setupSlider(S) {
  const sc = mapScales(S);
  const r = $("mapYear");
  if (+r.min !== sc.first || +r.max !== sc.last) { r.min = sc.first; r.max = sc.last; }
  if (MAP.year == null || MAP.year < sc.first || MAP.year > sc.last) MAP.year = sc.last;
  if (!MAP.playing) r.value = MAP.year;
  $("mapYearMin").textContent = sc.first; $("mapYearMax").textContent = sc.last;
  $("timebar").hidden = sc.first === sc.last;
}
function setYear(y) {
  MAP.year = y;
  $("mapYear").value = y;
  refreshMap();
}
function stopPlay() {
  MAP.playing = false;
  cancelAnimationFrame(MAP.raf);
  $("mapPlay").textContent = "▶"; $("mapPlay").setAttribute("aria-label", "Play the years");
  if (MAP.year != null) setYear(Math.round(MAP.year));
}
function play() {
  const S = MAP.S; if (!S) return;
  const sc = mapScales(S);
  if (MAP.year >= sc.last - 0.01) MAP.year = sc.first;
  MAP.playing = true;
  $("mapPlay").textContent = "❚❚"; $("mapPlay").setAttribute("aria-label", "Pause");
  let last = performance.now(), drawn = 0;
  const PER_YEAR = 1100; // ms
  const step = (now) => {
    if (!MAP.playing) return;
    MAP.year = Math.min(sc.last, MAP.year + (now - last) / PER_YEAR);
    last = now;
    $("mapYear").value = MAP.year;
    if (now - drawn > 45) { drawn = now; refreshMap(); } // about 20 updates a second
    if (MAP.year >= sc.last) { stopPlay(); return; }
    MAP.raf = requestAnimationFrame(step);
  };
  MAP.raf = requestAnimationFrame(step);
}
$("mapPlay").addEventListener("click", () => (MAP.playing ? stopPlay() : play()));
$("mapYear").addEventListener("input", () => { if (MAP.playing) stopPlay(); MAP.year = +$("mapYear").value; refreshMap(); });
$("mapYear").addEventListener("change", () => setYear(Math.round(+$("mapYear").value)));
$("mapMode").addEventListener("click", (e) => {
  const b = e.target.closest("button"); if (!b) return;
  MAP.mode = b.dataset.v;
  $("mapMode").querySelectorAll("button").forEach((x) => x.setAttribute("aria-pressed", x === b));
  refreshMap();
});

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
  if (MAP.map && MAP.ready) { MAP.map.setPaintProperty("b3d", "fill-extrusion-height", heightProp()); refreshMap(); }
});

// start when the section comes near the screen (keeps the first load light)
new IntersectionObserver((es, ob) => { if (es.some((x) => x.isIntersecting)) { ob.disconnect(); loadBuren(); } }, { rootMargin: "600px" }).observe($("nbPanel"));
let nbRz = 0;
addEventListener("resize", () => { clearTimeout(nbRz); nbRz = setTimeout(() => { if (NB.data) { const S = nbSet(); if (S) { scoreCharts(S); rankChart(S); diffChart(S); yearsChart(S); growthChart(S); heatChart(S); m2Chart(S); scatterChart(S); distChart(S); } } }, 150); });
