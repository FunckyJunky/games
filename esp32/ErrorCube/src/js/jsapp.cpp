// jsapp.cpp: see jsapp.h. One app at a time, its JavaScript heap in PSRAM.
#include "jsapp.h"
#include "duktape.h"
#include "../ui/ui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef ARDUINO
#include <esp_heap_caps.h>
static void* jsA(void*, duk_size_t n) { return heap_caps_malloc(n ? n : 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
static void* jsR(void*, void* p, duk_size_t n) { return heap_caps_realloc(p, n ? n : 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
static void jsF(void*, void* p) { heap_caps_free(p); }
#else
static void* jsA(void*, duk_size_t n) { return malloc(n ? n : 1); }
static void* jsR(void*, void* p, duk_size_t n) { return realloc(p, n ? n : 1); }
static void jsF(void*, void* p) { free(p); }
#endif

static duk_context* ctx = nullptr;
static char appId[24] = "";
static bool wantExit = false;
static int fpsWant = 30;
static char crash[160] = "";

static void fatal(void*, const char* msg) {
  // Duktape can't go on: there's no way back from here but a restart
  printf("JS FATAL: %s\n", msg ? msg : "?");
  sysRestart();
  for (;;) {}
}

// ------------------------------------------------------------------ helpers
static int I(duk_context* c, int i, int d) { return duk_is_number(c, i) ? (int)floor(duk_get_number(c, i)) : d; }
static double D(duk_context* c, int i, double d) { return duk_is_number(c, i) ? duk_get_number(c, i) : d; }
static uint16_t C(duk_context* c, int i) { return duk_is_number(c, i) ? rgb((uint32_t)duk_get_number(c, i)) : C_FG; }

// ------------------------------------------------------------------ drawing
static duk_ret_t j_cls(duk_context* c) { gfxClearTo(duk_is_number(c, 0) ? C(c, 0) : C_BG); return 0; }
static duk_ret_t j_rect(duk_context* c) { fillRect(I(c, 0, 0), I(c, 1, 0), I(c, 2, 0), I(c, 3, 0), C(c, 4)); return 0; }
static duk_ret_t j_frame(duk_context* c) { rect(I(c, 0, 0), I(c, 1, 0), I(c, 2, 0), I(c, 3, 0), C(c, 4)); return 0; }
static duk_ret_t j_line(duk_context* c) { line(I(c, 0, 0), I(c, 1, 0), I(c, 2, 0), I(c, 3, 0), C(c, 4)); return 0; }
static duk_ret_t j_thick(duk_context* c) { thickLine(I(c, 0, 0), I(c, 1, 0), I(c, 2, 0), I(c, 3, 0), I(c, 4, 3), C(c, 5)); return 0; }
static duk_ret_t j_circ(duk_context* c) { fillCircle(I(c, 0, 0), I(c, 1, 0), I(c, 2, 1), C(c, 3)); return 0; }
static duk_ret_t j_ring(duk_context* c) { ring(I(c, 0, 0), I(c, 1, 0), I(c, 2, 1), I(c, 4, 2), C(c, 3)); return 0; }
static duk_ret_t j_pset(duk_context* c) { pset(I(c, 0, 0), I(c, 1, 0), C(c, 2)); return 0; }
static duk_ret_t j_fade(duk_context* c) { blendRect(I(c, 0, 0), I(c, 1, 0), I(c, 2, 0), I(c, 3, 0), C(c, 4), (int)(D(c, 5, 0.5) * 32)); return 0; }
static duk_ret_t j_text(duk_context* c) {
  const char* s = duk_safe_to_string(c, 0);
  int size = I(c, 4, 1), f = size >= 3 ? FH : size == 2 ? FB : FT, align = I(c, 5, 0);
  int x = I(c, 1, 0), y = I(c, 2, 0);
  bool glow = duk_is_boolean(c, 6) ? duk_get_boolean(c, 6) : true;
  int w = textW(f, s);
  if (align == 1) x -= w / 2; else if (align == 2) x -= w;
  drawText(f, x, y, s, C(c, 3), glow ? TX_GLOW : TX_FLAT);
  duk_push_int(c, w);
  return 1;
}
static duk_ret_t j_textw(duk_context* c) { int size = I(c, 1, 1); duk_push_int(c, textW(size >= 3 ? FH : size == 2 ? FB : FT, duk_safe_to_string(c, 0))); return 1; }
static duk_ret_t j_box(duk_context* c) { dosBox(I(c, 0, 0), I(c, 1, 0), I(c, 2, 0), I(c, 3, 0), C(c, 4)); return 0; }
static duk_ret_t j_button(duk_context* c) {
  const char* s = duk_safe_to_string(c, 0);
  int x = I(c, 1, 0), y = I(c, 2, 0), w = I(c, 3, 120), h = I(c, 4, 50);
  bool on = duk_is_boolean(c, 5) && duk_get_boolean(c, 5);
  if (on) { invBar(x, y, w, h); drawTextC(FT, x + w / 2, y + (h - LH) / 2, s, C_BG, TX_FLAT); }
  else { rect(x, y, w, h, C_FG); rect(x + 1, y + 1, w - 2, h - 2, C_FG); drawTextC(FT, x + w / 2, y + (h - LH) / 2, s, C_FG); }
  return 0;
}
static duk_ret_t j_bar(duk_context* c) { meter(I(c, 0, 0), I(c, 1, 0), I(c, 2, 100), I(c, 3, 30), I(c, 4, 0), true); return 0; }

// ------------------------------------------------------------------ sensors, sound, time, system
static duk_ret_t j_tilt(duk_context* c) {
  float a[3] = {0, 0, 9.81f}, g[3];
  imuRead(a, g);
  duk_idx_t arr = duk_push_array(c);
  for (int i = 0; i < 3; i++) { duk_push_number(c, a[i] / 9.81); duk_put_prop_index(c, arr, i); }
  return 1;
}
static duk_ret_t j_gyro(duk_context* c) {
  float a[3], g[3] = {0, 0, 0};
  imuRead(a, g);
  duk_idx_t arr = duk_push_array(c);
  for (int i = 0; i < 3; i++) { duk_push_number(c, g[i]); duk_put_prop_index(c, arr, i); }
  return 1;
}
static duk_ret_t j_tone(duk_context* c) { tone(I(c, 0, 440), I(c, 1, 100), I(c, 2, 100)); return 0; }
static duk_ret_t j_beep(duk_context* c) {
  const char* n = duk_is_string(c, 0) ? duk_get_string(c, 0) : "beep";
  static const char* N[] = {"key", "move", "beep", "thud", "crackle", "disk", "ok", "err", "msg", "alarm"};
  for (int i = 0; i < 10; i++) if (!strcmp(n, N[i])) { snd(i); return 0; }
  snd(SND_BEEP);
  return 0;
}
static duk_ret_t j_ms(duk_context* c) { duk_push_number(c, msNow()); return 1; }
static duk_ret_t j_clock(duk_context* c) {
  struct tm t;
  if (!sysLocalTime(t)) { duk_push_null(c); return 1; }
  duk_idx_t o = duk_push_object(c);
  duk_push_int(c, t.tm_hour); duk_put_prop_string(c, o, "h");
  duk_push_int(c, t.tm_min); duk_put_prop_string(c, o, "m");
  duk_push_int(c, t.tm_sec); duk_put_prop_string(c, o, "s");
  duk_push_int(c, t.tm_mday); duk_put_prop_string(c, o, "day");
  duk_push_int(c, t.tm_mon + 1); duk_put_prop_string(c, o, "mon");
  duk_push_int(c, t.tm_year + 1900); duk_put_prop_string(c, o, "year");
  duk_push_int(c, t.tm_wday); duk_put_prop_string(c, o, "wday");
  return 1;
}
static duk_ret_t j_battery(duk_context* c) { duk_push_int(c, S.battery ? S.batPct : -1); return 1; }
static duk_ret_t j_exit(duk_context*) { wantExit = true; return 0; }
static duk_ret_t j_fps(duk_context* c) { int f = I(c, 0, 30); fpsWant = f < 1 ? 1 : f > 60 ? 60 : f; return 0; }
static duk_ret_t j_print(duk_context* c) { printf("[%s] %s\n", appId, duk_safe_to_string(c, 0)); return 0; }
static duk_ret_t j_load(duk_context* c) {
  char p[64]; snprintf(p, sizeof(p), "/c/appdata/%s.json", appId);
  char* buf = (char*)jsA(nullptr, 8193); size_t n = 0;
  if (buf && fsRead(p, (uint8_t*)buf, 8192, n)) { buf[n] = 0; duk_push_string(c, buf); } else duk_push_null(c);
  if (buf) jsF(nullptr, buf);
  return 1;
}
static duk_ret_t j_save(duk_context* c) {
  char p[64]; snprintf(p, sizeof(p), "/c/appdata/%s.json", appId);
  const char* s = duk_safe_to_string(c, 0);
  size_t n = strlen(s);
  duk_push_boolean(c, n <= 8192 && fsWrite(p, (const uint8_t*)s, n));
  return 1;
}
static duk_ret_t j_get(duk_context* c) { netFetch(I(c, 0, 0), duk_safe_to_string(c, 1)); return 0; }
static duk_ret_t j_send(duk_context* c) { netAppSend(appId, duk_safe_to_string(c, 0)); return 0; }

static const duk_function_list_entry FUNCS[] = {
  {"cls", j_cls, DUK_VARARGS}, {"rect", j_rect, DUK_VARARGS}, {"frame", j_frame, DUK_VARARGS}, {"line", j_line, DUK_VARARGS},
  {"thick", j_thick, DUK_VARARGS}, {"circ", j_circ, DUK_VARARGS}, {"ring", j_ring, DUK_VARARGS}, {"pset", j_pset, DUK_VARARGS},
  {"fade", j_fade, DUK_VARARGS}, {"text", j_text, DUK_VARARGS}, {"textw", j_textw, DUK_VARARGS}, {"box", j_box, DUK_VARARGS},
  {"button", j_button, DUK_VARARGS}, {"bar", j_bar, DUK_VARARGS},
  {"tilt", j_tilt, 0}, {"gyro", j_gyro, 0}, {"tone", j_tone, DUK_VARARGS}, {"beep", j_beep, DUK_VARARGS},
  {"ms", j_ms, 0}, {"clock", j_clock, 0}, {"battery", j_battery, 0}, {"exit", j_exit, 0}, {"fps", j_fps, 1}, {"print", j_print, 1},
  {"__load", j_load, 0}, {"__save", j_save, 1}, {"__get", j_get, 2}, {"__send", j_send, 1},
  {nullptr, nullptr, 0}};

// what every app gets before its own code: storage, get(), send(), 3D lines (as in pocket.DOS)
static const char PRELUDE[] = R"JS(
var W = 480, H = 480;
var __st = {}; try { __st = JSON.parse(__load() || '{}') || {}; } catch (e) { __st = {}; }
function save(k, v) { __st[k] = v; return __save(JSON.stringify(__st)); }
function load(k, d) { return (k in __st) ? __st[k] : d; }
var __cb = {}, __cbn = 0;
function get(path, cb) { var id = ++__cbn; __cb[id] = cb; __get(id, String(path)); }
function __done(id, status, text) { var f = __cb[id]; delete __cb[id]; if (f) f(status, text); }
function send(o) { __send(JSON.stringify(o)); }
function __msg(s) { if (typeof onmsg == 'function') { var o; try { o = JSON.parse(s); } catch (e) { o = s; } onmsg(o); } }
var P3 = { x: 0, y: 0, z: 0, cy: 1, sy: 0, cp: 1, sp: 0, cr: 1, sr: 0, f: 240, cx: 240, cyy: 240, ox: 0, oy: 0, oz: 0, oc: 1, os: 0, ok: 1, X: 0, Y: 0, Z: 0 };
function cam3(x, y, z, yaw, pitch, roll, f, cx, cy) {
  P3.x = x; P3.y = y; P3.z = z; P3.cy = Math.cos(yaw || 0); P3.sy = Math.sin(yaw || 0); P3.cp = Math.cos(pitch || 0); P3.sp = Math.sin(pitch || 0);
  P3.cr = Math.cos(roll || 0); P3.sr = Math.sin(roll || 0); P3.f = f || 240; P3.cx = cx === undefined ? 240 : cx; P3.cyy = cy === undefined ? 240 : cy;
  P3.ox = P3.oy = P3.oz = 0; P3.oc = 1; P3.os = 0; P3.ok = 1;
}
function xf3(x, y, z, a, k) { P3.ox = x || 0; P3.oy = y || 0; P3.oz = z || 0; P3.oc = Math.cos(a || 0); P3.os = Math.sin(a || 0); P3.ok = k === undefined ? 1 : k; }
function __tc3(x, y, z) {
  var lx = x * P3.oc + z * P3.os, lz = z * P3.oc - x * P3.os;
  var dx = P3.ox + lx * P3.ok - P3.x, dy = P3.oy + y * P3.ok - P3.y, dz = P3.oz + lz * P3.ok - P3.z;
  var x1 = dx * P3.cy - dz * P3.sy, z1 = dx * P3.sy + dz * P3.cy, y2 = dy * P3.cp - z1 * P3.sp;
  P3.Z = dy * P3.sp + z1 * P3.cp; P3.X = x1 * P3.cr - y2 * P3.sr; P3.Y = x1 * P3.sr + y2 * P3.cr;
}
function line3(ax, ay, az, bx, by, bz, c) {
  __tc3(ax, ay, az); var X0 = P3.X, Y0 = P3.Y, Z0 = P3.Z; __tc3(bx, by, bz); var X1 = P3.X, Y1 = P3.Y, Z1 = P3.Z, f;
  if (Z0 < 1 && Z1 < 1) return;
  if (Z0 < 1) { f = (1 - Z0) / (Z1 - Z0); X0 += (X1 - X0) * f; Y0 += (Y1 - Y0) * f; Z0 = 1; }
  else if (Z1 < 1) { f = (1 - Z1) / (Z0 - Z1); X1 += (X0 - X1) * f; Y1 += (Y0 - Y1) * f; Z1 = 1; }
  line(P3.cx + X0 * P3.f / Z0, P3.cyy - Y0 * P3.f / Z0, P3.cx + X1 * P3.f / Z1, P3.cyy - Y1 * P3.f / Z1, c);
}
function proj3(x, y, z) { __tc3(x, y, z); return P3.Z < 0.05 ? null : [P3.cx + P3.X * P3.f / P3.Z, P3.cyy - P3.Y * P3.f / P3.Z, P3.Z]; }
)JS";

static uint32_t to888(uint16_t c) { return ((c >> 11) * 255 / 31) << 16 | (((c >> 5) & 63) * 255 / 63) << 8 | ((c & 31) * 255 / 31); }

static void setErr(char* err, size_t n, const char* what) { snprintf(err, n, "%s", what); snprintf(crash, sizeof(crash), "%s", what); }

// call a global function of the app, if it has one. false = it threw
static bool call(const char* fn, int nargs, char* err, size_t errLen, bool* returned = nullptr) {
  // the arguments are already on the stack
  duk_get_global_string(ctx, fn);
  if (!duk_is_function(ctx, -1)) { duk_pop_n(ctx, nargs + 1); if (returned) *returned = false; return true; }
  duk_insert(ctx, -(nargs + 1));
  if (duk_pcall(ctx, nargs) != 0) {
    char m[160]; snprintf(m, sizeof(m), "%s(): %s", fn, duk_safe_to_string(ctx, -1));
    duk_pop(ctx);
    if (err) setErr(err, errLen, m); else snprintf(crash, sizeof(crash), "%s", m);
    return false;
  }
  if (returned) *returned = duk_to_boolean(ctx, -1);
  duk_pop(ctx);
  return true;
}

bool jsStart(const char* id, const char* src, size_t n, char* err, size_t errLen) {
  jsStop();
  snprintf(appId, sizeof(appId), "%s", id);
  wantExit = false; fpsWant = 30; crash[0] = 0;
  ctx = duk_create_heap(jsA, jsR, jsF, nullptr, fatal);
  if (!ctx) { setErr(err, errLen, "NO MEMORY FOR JAVASCRIPT"); return false; }
  duk_push_global_object(ctx);
  duk_put_function_list(ctx, -1, FUNCS);
  struct { const char* n; uint32_t v; } K[] = {{"FG", to888(C_FG)}, {"DIM", to888(C_DIM)}, {"BG", to888(C_BG)},
                                               {"RED", 0xff5a6a}, {"WARN", 0xffb43a}, {"WHITE", 0xffffff}, {"BLACK", 0}};
  for (auto& k : K) { duk_push_number(ctx, k.v); duk_put_prop_string(ctx, -2, k.n); }
  duk_push_string(ctx, id); duk_put_prop_string(ctx, -2, "APP");
  duk_push_string(ctx, CUBE_VERSION); duk_put_prop_string(ctx, -2, "VERSION");
  duk_pop(ctx);
  if (duk_peval_string(ctx, PRELUDE) != 0) { setErr(err, errLen, duk_safe_to_string(ctx, -1)); jsStop(); return false; }
  duk_pop(ctx);
  duk_push_lstring(ctx, src, n);
  duk_push_string(ctx, id);
  if (duk_pcompile(ctx, 0) != 0) {
    char m[160]; snprintf(m, sizeof(m), "%s", duk_safe_to_string(ctx, -1));
    setErr(err, errLen, m); jsStop(); return false;
  }
  if (duk_pcall(ctx, 0) != 0) {
    char m[160]; snprintf(m, sizeof(m), "%s", duk_safe_to_string(ctx, -1));
    setErr(err, errLen, m); jsStop(); return false;
  }
  duk_pop(ctx);
  if (!call("init", 0, err, errLen)) { jsStop(); return false; }
  return true;
}
void jsStop() { if (ctx) { duk_destroy_heap(ctx); ctx = nullptr; } }
bool jsRunning() { return ctx != nullptr; }
bool jsWantsExit() { return wantExit; }
int jsFps() { return fpsWant; }

bool jsFrame(uint32_t dt, char* err, size_t errLen) {
  if (!ctx) return false;
  duk_push_number(ctx, dt);
  if (!call("update", 1, err, errLen)) return false;
  if (!call("draw", 0, err, errLen)) return false;
  if (crash[0]) { setErr(err, errLen, crash); return false; }
  return true;
}
void jsTouch(const char* type, int x, int y) {
  if (!ctx) return;
  duk_push_string(ctx, type); duk_push_int(ctx, x); duk_push_int(ctx, y);
  call("touch", 3, nullptr, 0);
}
void jsSwipe(const char* dir) { if (!ctx) return; duk_push_string(ctx, dir); call("swipe", 1, nullptr, 0); }
bool jsKey(const char* k) {
  if (!ctx) return false;
  bool r = false;
  duk_push_string(ctx, k);
  call("key", 1, nullptr, 0, &r);
  return r;
}
void jsMsg(const char* json) { if (!ctx) return; duk_push_string(ctx, json); call("__msg", 1, nullptr, 0); }
void jsFetched(int id, int status, const char* body) {
  if (!ctx) return;
  duk_push_int(ctx, id); duk_push_int(ctx, status); duk_push_string(ctx, body ? body : "");
  call("__done", 3, nullptr, 0);
}
