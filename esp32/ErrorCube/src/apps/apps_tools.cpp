// apps_tools.cpp: CLOCK, CALC, PAINT, LEVEL, SNAKE, SOUND, FILES
#include "../ui/ui.h"
#include "../core/img.h"
#include <strings.h>
#include <string>
#include <algorithm>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef ARDUINO
#include <esp_heap_caps.h>
static void* bigAlloc(size_t n) { void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); return p ? p : malloc(n); }
#else
static void* bigAlloc(size_t n) { return malloc(n); }
#endif

static void tabs(const char* const* names, int n, int cur, std::function<void(int)> pick) {
  int w = (450 - (n - 1) * 6) / n;
  for (int i = 0; i < n; i++) {
    int x = 15 + i * (w + 6);
    if (i == cur) invBar(x, 56, w, 38); else rect(x, 56, w, 38, C_DIM);
    drawTextC(FT, x + w / 2, 59, names[i], i == cur ? C_BG : C_DIM, TX_FLAT);
    hit(x, 50, w, 48, [pick, i] { sndKey(); pick(i); }, nullptr, false);
  }
}

// ================================================================== CLOCK
Screen* makeAlarmSetup();
uint32_t tmrEnd = 0, tmrLeft = 0;
bool tmrRun = false;
static uint32_t swStart = 0, swAcc = 0, swLaps[3];
static bool swRun = false;
static int swLapN = 0;
static int tmrSet = 300;            // seconds
static int lastAlarmMin = -1;

void clockTick() {
  uint32_t now = msNow();
  if (tmrRun && (int32_t)(now - tmrEnd) >= 0) { tmrRun = false; tmrLeft = 0; alarmRing("TIMER"); }
  if (cfg.alarmOn) {
    struct tm t;
    if (sysLocalTime(t)) {
      int day = (t.tm_wday + 6) % 7, m = t.tm_hour * 60 + t.tm_min;
      if (t.tm_hour == cfg.alarmH && t.tm_min == cfg.alarmM && (cfg.alarmDays >> day & 1) && lastAlarmMin != m) {
        lastAlarmMin = m; alarmRing("ALARM");
      }
    }
  }
}

struct Clock : Screen {
  int tab = 0;
  const char* name() override { return "CLOCK.EXE"; }
  bool keepAwake() override { return tab == 3 && swRun; }
  void draw() override {
    static const char* T[4] = {"TIME", "ALARM", "TIMER", "WATCH"};
    tabs(T, 4, tab, [this](int i) { tab = i; uiDirty(); });
    if (tab == 0) drawTime(); else if (tab == 1) drawAlarm(); else if (tab == 2) drawTimer(); else drawWatch();
    fkeysBack();
  }
  void drawTime() {
    struct tm t;
    if (!sysLocalTime(t)) {
      drawTextC(FH, 240, 120, "--:--", C_FG);
      drawTextC(FT, 240, 260, "THE CLOCK IS NOT SET", C_DIM, TX_FLAT);
      drawTextC(FT, 240, 296, S.net == NET_UP ? "ASKING THE INTERNET..." : "CONNECT WIFI OR SETUP>TIME", C_DIM, TX_FLAT);
      redrawIn(1000);
      return;
    }
    char c[12]; fmtTime(c, t.tm_hour, t.tm_min, 0, false);
    drawTextC(FH, 240, 112, c, C_FG);
    if (!cfg.h24) drawText(FT, 402, 182, t.tm_hour < 12 ? "AM" : "PM", C_FG);
    // the seconds as a row of 60 dots (lit up to now), with the number under it
    if (cfg.secs) {
      for (int i = 0; i < 60; i++) {
        int x = 36 + i * 7, y = 224;
        if (i <= t.tm_sec) fillRect(x, y, 5, (i % 5 == 0) ? 12 : 8, C_FG);
        else fillRect(x, y + 2, 5, 4, mix(C_BG, C_DIM, 50));
      }
      char s[8]; snprintf(s, sizeof(s), ":%02d", t.tm_sec);
      drawTextC(FB, 240, 244, s, C_FG);
    }
    static const char* DAYS[7] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
    static const char* MON[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    char d[48]; snprintf(d, sizeof(d), "%s %d %s %d", DAYS[t.tm_wday], t.tm_mday, MON[t.tm_mon], t.tm_year + 1900);
    drawTextC(FT, 240, 300, d, C_FG);
    char z[48]; snprintf(z, sizeof(z), "%s  %s", TIMEZONES[cfg.tzIdx % N_TZ].name, S.timeOk ? "NTP OK" : "RTC");
    drawTextC(FT, 240, 336, z, C_DIM, TX_FLAT);
    if (cfg.alarmOn) { char a[24]; snprintf(a, sizeof(a), "ALARM %02d:%02d", cfg.alarmH, cfg.alarmM); drawTextC(FT, 240, 372, a, C_DIM, TX_FLAT); }
    redrawIn(1000 - msNow() % 1000 + 5);
  }
  void drawAlarm() {
    char c[8]; snprintf(c, sizeof(c), "%02d:%02d", cfg.alarmH, cfg.alarmM);
    drawTextC(FH, 240, 104, c, cfg.alarmOn ? C_FG : C_DIM, cfg.alarmOn ? TX_GLOW : TX_FLAT);
    // + / - under hours and minutes
    dosButton(90, 206, 60, 44, "+", [] { cfg.alarmH = (cfg.alarmH + 1) % 24; cfgSave(); });
    dosButton(160, 206, 60, 44, "-", [] { cfg.alarmH = (cfg.alarmH + 23) % 24; cfgSave(); });
    dosButton(260, 206, 60, 44, "+", [] { cfg.alarmM = (cfg.alarmM + 5) % 60; cfgSave(); });
    dosButton(330, 206, 60, 44, "-", [] { cfg.alarmM = (cfg.alarmM + 59) % 60; cfgSave(); });
    static const char* D[7] = {"M", "T", "W", "T", "F", "S", "S"};
    for (int i = 0; i < 7; i++) {
      bool on = cfg.alarmDays >> i & 1;
      int bit = i;
      dosButton(27 + i * 62, 266, 54, 46, D[i], [bit] { cfg.alarmDays ^= 1 << bit; cfgSave(); }, on);
    }
    dosButton(60, 322, 250, 56, cfg.alarmOn ? "ALARM IS ON" : "ALARM IS OFF", [] { cfg.alarmOn = !cfg.alarmOn; cfgSave(); }, cfg.alarmOn);
    dosButton(320, 322, 100, 56, "SOUND", [] { uiOpen(makeAlarmSetup()); });
    const char* n = strrchr(cfg.alarmSong, '/');
    char s[48]; snprintf(s, sizeof(s), "%s%s", cfg.alarmSong[0] ? "\xE2\x99\xAA " : "BEEPS", cfg.alarmSong[0] ? (n ? n + 1 : cfg.alarmSong) : ""); utf8Cut(s, 30);
    drawTextC(FT, 240, 392, s, C_DIM, TX_FLAT);
  }
  void drawTimer() {
    uint32_t left = tmrRun ? (uint32_t)((int32_t)(tmrEnd - msNow()) > 0 ? tmrEnd - msNow() : 0) : (tmrLeft ? tmrLeft : tmrSet * 1000u);
    int s = (left + 999) / 1000;
    char c[12];
    if (s >= 3600) snprintf(c, sizeof(c), "%d:%02d", s / 3600, s / 60 % 60); else snprintf(c, sizeof(c), "%02d:%02d", s / 60, s % 60);
    drawTextC(FH, 240, 104, c, tmrRun || tmrLeft ? C_FG : C_DIM, TX_GLOW);
    if (!tmrRun && !tmrLeft) {
      dosButton(27, 214, 100, 46, "+1 M", [] { tmrSet += 60; if (tmrSet > 99 * 60) tmrSet = 99 * 60; });
      dosButton(135, 214, 100, 46, "+5 M", [] { tmrSet += 300; if (tmrSet > 99 * 60) tmrSet = 99 * 60; });
      dosButton(245, 214, 100, 46, "-1 M", [] { tmrSet -= 60; if (tmrSet < 60) tmrSet = 60; });
      dosButton(353, 214, 100, 46, "+10S", [] { tmrSet += 10; });
    }
    if (tmrRun) dosButton(60, 290, 170, 60, "PAUSE", [] { tmrLeft = tmrEnd - msNow(); tmrRun = false; }, true);
    else dosButton(60, 290, 170, 60, "START", [] { tmrEnd = msNow() + (tmrLeft ? tmrLeft : tmrSet * 1000u); tmrRun = true; tmrLeft = 0; }, true);
    dosButton(250, 290, 170, 60, "RESET", [] { tmrRun = false; tmrLeft = 0; });
    if (tmrRun) redrawIn(250);
  }
  void drawWatch() {
    uint32_t ms = swAcc + (swRun ? msNow() - swStart : 0);
    char c[16]; snprintf(c, sizeof(c), "%02u:%02u", (unsigned)(ms / 60000 % 100), (unsigned)(ms / 1000 % 60));
    drawTextC(FH, 220, 100, c, C_FG);
    char cc[8]; snprintf(cc, sizeof(cc), ".%02u", (unsigned)(ms / 10 % 100));
    drawText(FB, 380, 152, cc, C_FG);
    for (int i = 0; i < swLapN && i < 3; i++) {
      uint32_t l = swLaps[i]; char b[32];
      snprintf(b, sizeof(b), "LAP %d  %02u:%02u.%02u", swLapN - i, (unsigned)(l / 60000), (unsigned)(l / 1000 % 60), (unsigned)(l / 10 % 100));
      drawTextC(FT, 240, 206 + i * 30, b, i ? C_DIM : C_FG, i ? TX_FLAT : TX_GLOW);
    }
    if (swRun) dosButton(60, 314, 170, 60, "STOP", [] { swAcc += msNow() - swStart; swRun = false; }, true);
    else dosButton(60, 314, 170, 60, "START", [] { swStart = msNow(); swRun = true; }, true);
    if (swRun) dosButton(250, 314, 170, 60, "LAP", [] {
      uint32_t ms = swAcc + msNow() - swStart;
      swLaps[2] = swLaps[1]; swLaps[1] = swLaps[0]; swLaps[0] = ms; swLapN++;
    });
    else dosButton(250, 314, 170, 60, "RESET", [] { swAcc = 0; swLapN = 0; });
    if (swRun) redrawIn(40);
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_SWIPE && e.dir == SW_LEFT) { tab = (tab + 1) % 4; uiDirty(); return true; }
    if (e.t == EV_SWIPE && e.dir == SW_RIGHT && tab > 0) { tab--; uiDirty(); return true; }
    return false;
  }
};
Screen* makeClock() { return new Clock(); }

// ================================================================== CALC
struct Calc : Screen {
  double acc = 0, cur = 0; char op = 0; bool fresh = true, err = false;
  char disp[32] = "0", hist[40] = "";
  const char* name() override { return "CALC.EXE"; }
  void show(double v) {
    if (isnan(v) || isinf(v)) { err = true; snprintf(disp, sizeof(disp), "ERROR"); return; }
    snprintf(disp, sizeof(disp), "%.10g", v);
    if (strlen(disp) > 14) snprintf(disp, sizeof(disp), "%.6e", v);
  }
  double apply(double a, double b, char o) {
    switch (o) { case '+': return a + b; case '-': return a - b; case '*': return a * b; case '/': return b == 0 ? NAN : a / b; }
    return b;
  }
  void key(const char* k) {
    if (err && strcmp(k, "C")) return;
    if (!strcmp(k, "C")) { acc = cur = 0; op = 0; fresh = true; err = false; strcpy(disp, "0"); hist[0] = 0; return; }
    if (isdigit((unsigned char)k[0]) || !strcmp(k, ".")) {
      if (fresh) { strcpy(disp, k[0] == '.' ? "0." : k); fresh = false; }
      else if (strlen(disp) < 14) { if (k[0] == '.' && strchr(disp, '.')) return; if (!strcmp(disp, "0") && k[0] != '.') disp[0] = 0; strcat(disp, k); }
      cur = atof(disp); return;
    }
    if (!strcmp(k, "<")) { if (!fresh && strlen(disp) > 1) { disp[strlen(disp) - 1] = 0; } else { strcpy(disp, "0"); fresh = true; } cur = atof(disp); return; }
    if (!strcmp(k, "+-")) { cur = -atof(disp); show(cur); return; }
    if (!strcmp(k, "%")) { cur = op ? acc * atof(disp) / 100 : atof(disp) / 100; show(cur); fresh = true; return; }
    char o = k[0] == 'x' ? '*' : k[0] == ':' ? '/' : k[0];
    if (o == '=') {
      if (!op) return;
      double r = apply(acc, atof(disp), op);
      snprintf(hist, sizeof(hist), "%.8g %c %.8g =", acc, op == '*' ? 'x' : op == '/' ? ':' : op, atof(disp));
      show(r); acc = r; op = 0; fresh = true; return;
    }
    if (op && !fresh) { acc = apply(acc, atof(disp), op); show(acc); } else if (!op) acc = atof(disp);
    op = o; fresh = true;
    snprintf(hist, sizeof(hist), "%.8g %c", acc, o == '*' ? 'x' : o == '/' ? ':' : o);
  }
  void draw() override {
    dosBox(15, 54, 450, 96, C_FG);
    drawTextR(FT, 445, 62, hist, C_DIM, TX_FLAT);
    drawTextR(FB, 445, 96, disp, err ? C_RED : C_FG);
    static const char* K[5][4] = {{"C", "<", "%", ":"}, {"7", "8", "9", "x"}, {"4", "5", "6", "-"}, {"1", "2", "3", "+"}, {"+-", "0", ".", "="}};
    static const char* L[5][4] = {{"C", "\xE2\x86\x90", "%", "\xC3\xB7"}, {"7", "8", "9", "\xC3\x97"}, {"4", "5", "6", "-"}, {"1", "2", "3", "+"}, {"\xC2\xB1", "0", ".", "="}};
    for (int r = 0; r < 5; r++)
      for (int c = 0; c < 4; c++) {
        int x = 15 + c * 115, y = 160 + r * 54;
        const char* k = K[r][c];
        bool opk = c == 3 || r == 0;
        dosButton(x, y, 105, 48, L[r][c], [this, k] { key(k); uiDirty(); }, !strcmp(k, "=") || (op && opk && k[0] == (op == '*' ? 'x' : op == '/' ? ':' : op) && fresh), FB);
      }
  }
};
Screen* makeCalc() { return new Calc(); }

// ================================================================== PAINT
static const uint32_t EGA[16] = {0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
                                 0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF};
static uint16_t* canvas = nullptr;
struct Paint : Screen {
  int col = 15, size = 1, lx = -1, ly = -1; bool erase = false, palette = false;
  const char* name() override { return "PAINT.EXE"; }
  bool fullScreen() override { return true; }
  bool noSwipeBack() override { return true; }
  bool keepAwake() override { return true; }
  void enter() override {
    if (!canvas) { canvas = (uint16_t*)bigAlloc(SCR_W * SCR_H * 2); clearCanvas(); }
  }
  void clearCanvas() { if (canvas) for (int i = 0; i < SCR_W * SCR_H; i++) canvas[i] = 0; }
  int brush() { static const int B[3] = {4, 10, 24}; return B[size] * (erase ? 2 : 1); }
  void stroke(int x0, int y0, int x1, int y1) {
    if (!canvas) return;
    uint16_t* keep = fb; fb = canvas;
    clipSet(0, 0, SCR_W, 404);
    thickLine(x0, y0, x1, y1, brush(), erase ? 0 : rgb(EGA[col]));
    clipReset();
    fb = keep;
  }
  void save() {
    // 24-bit BMP on the TF card (or the cube's own drive)
    const char* dir = S.sd ? "/sd" : "/c";
    char path[48]; static FileEnt* ents = (FileEnt*)bigAlloc(sizeof(FileEnt) * 64); int n = ents ? fsList(dir, ents, 64) : 0, num = 1;
    for (int i = 0; i < n; i++) { int k; if (sscanf(ents[i].name, "PAINT%d.BMP", &k) == 1 && k >= num) num = k + 1; }
    snprintf(path, sizeof(path), "%s/PAINT%03d.BMP", dir, num);
    int W = SCR_W, H = 404, row = W * 3;
    size_t sz = 54 + row * H;
    uint8_t* b = (uint8_t*)bigAlloc(sz);
    if (!b) { toast("NO MEMORY"); return; }
    memset(b, 0, 54);
    b[0] = 'B'; b[1] = 'M'; *(uint32_t*)(b + 2) = sz; *(uint32_t*)(b + 10) = 54; *(uint32_t*)(b + 14) = 40;
    *(int32_t*)(b + 18) = W; *(int32_t*)(b + 22) = H; *(uint16_t*)(b + 26) = 1; *(uint16_t*)(b + 28) = 24; *(uint32_t*)(b + 34) = row * H;
    for (int y = 0; y < H; y++) {
      uint8_t* o = b + 54 + (H - 1 - y) * row;
      for (int x = 0; x < W; x++) {
        uint16_t c = canvas[y * SCR_W + x];
        o[x * 3 + 2] = (c >> 8 & 0xF8) | (c >> 13); o[x * 3 + 1] = (c >> 3 & 0xFC) | (c >> 9 & 3); o[x * 3] = (c << 3 & 0xF8) | (c >> 2 & 7);
      }
    }
    bool ok = fsWrite(path, b, sz);
    free(b);
    char m[64]; snprintf(m, sizeof(m), ok ? "SAVED %s" : "COULD NOT SAVE", path + (S.sd ? 4 : 3));
    toast(m); snd(ok ? SND_OK : SND_ERR);
  }
  void draw() override {
    if (canvas) memcpy(fb, canvas, SCR_W * 404 * 2);
    // tool bar at the bottom: colour, size, eraser, clear, save, exit
    fillRect(0, 404, SCR_W, 76, C_BG);
    hline(0, 404, SCR_W, C_FG);
    int y = 412;
    fillRect(30, y, 56, 44, rgb(EGA[col])); rect(28, y - 2, 60, 48, C_FG);
    hit(28, y - 4, 60, 52, [this] { palette = !palette; uiDirty(); });
    static const char* SZ[3] = {"S", "M", "L"};
    dosButton(96, y, 50, 44, SZ[size], [this] { size = (size + 1) % 3; });
    dosButton(154, y, 70, 44, "ERA", [this] { erase = !erase; }, erase);
    dosButton(232, y, 70, 44, "CLR", [this] { ask("PAINT", "Clear the picture?", {"CLEAR", "KEEP"}, [this](int i) { if (i == 0) clearCanvas(); }); });
    dosButton(310, y, 76, 44, "SAVE", [this] { save(); });
    dosButton(394, y, 60, 44, "ESC", [] { uiBack(); });
    if (palette) {
      fillRect(20, 250, 440, 150, C_BG); dosBox(20, 250, 440, 150, C_FG);
      for (int i = 0; i < 16; i++) {
        int x = 36 + (i % 8) * 52, yy = 266 + (i / 8) * 60;
        fillRect(x, yy, 44, 50, rgb(EGA[i]));
        if (i == col) { rect(x - 3, yy - 3, 50, 56, C_FG); rect(x - 2, yy - 2, 48, 54, C_FG); }
        int k = i;
        hit(x - 3, yy - 3, 50, 56, [this, k] { col = k; erase = false; palette = false; sndKey(); uiDirty(); });
      }
    }
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_KEY) {
      if (e.key == K_BACK) { uiBack(); return true; }
      if (e.key == K_NEXT) { col = (col + 1) % 16; erase = false; uiDirty(); return true; }
      if (e.key == K_PREV) { size = (size + 1) % 3; uiDirty(); return true; }
      return true;
    }
    if (palette) return false;
    if (e.y0 >= 400) return false;                 // the tool bar
    if (e.t == EV_DOWN) { lx = e.x; ly = e.y; stroke(lx, ly, lx, ly); uiDirty(); return true; }
    if (e.t == EV_MOVE && lx >= 0) { stroke(lx, ly, e.x, e.y); lx = e.x; ly = e.y; uiDirty(); return true; }
    if (e.t == EV_UP || e.t == EV_TAP) { lx = -1; return true; }
    return true;
  }
};
Screen* makePaint() { return new Paint(); }

// ================================================================== LEVEL
static float levOff[3] = {0, 0, 0};
struct Level : Screen {
  float a[3] = {0, 0, 9.8f}; bool wasLevel = false;
  const char* name() override { return "LEVEL.EXE"; }
  bool keepAwake() override { return true; }
  void tick() override {
    float g[3], r[3];
    if (imuRead(r, g)) { for (int i = 0; i < 3; i++) a[i] = a[i] * 0.8f + (r[i] - levOff[i]) * 0.2f; }
    redrawIn(60);
  }
  void draw() override {
    if (!S.imu) { drawTextC(FT, 240, 200, "NO MOTION SENSOR FOUND", C_RED); fkeysBack(); return; }
    float gx = a[0], gy = a[1], gz = a[2];
    float g = sqrtf(gx * gx + gy * gy + gz * gz); if (g < 0.1f) g = 9.8f;
    bool flat = fabsf(gz) > 0.75f * g;
    bool level;
    char b[40];
    if (flat) {
      // lying on its back: a round bubble
      float px = atan2f(gx, fabsf(gz)) * 57.3f, py = atan2f(gy, fabsf(gz)) * 57.3f;
      level = fabsf(px) < 1 && fabsf(py) < 1;
      int cx = 240, cy = 238;
      ring(cx, cy, 160, 3, C_FG); ring(cx, cy, 80, 2, C_DIM); ring(cx, cy, 24, 2, C_DIM);
      hline(cx - 160, cy, 320, C_DIM); vline(cx, cy - 160, 320, C_DIM);
      float bx = -px * 8, by = -py * 8, d = sqrtf(bx * bx + by * by);
      if (d > 140) { bx *= 140 / d; by *= 140 / d; }
      fillCircle(cx + (int)bx, cy + (int)by, 20, level ? C_FG : C_WARN);
      snprintf(b, sizeof(b), "X %+5.1f\xC2\xB0   Y %+5.1f\xC2\xB0", px, py);
    } else {
      // standing up: the angle of the bottom edge, with a line that stays level
      float ang = atan2f(gx, gy) * 57.3f;
      level = fabsf(ang) < 1;
      int cx = 240, cy = 238;
      ring(cx, cy, 160, 3, C_FG);
      float r = -ang / 57.3f;
      int dx = (int)(cosf(r) * 150), dy = (int)(sinf(r) * 150);
      thickLine(cx - dx, cy - dy, cx + dx, cy + dy, 6, level ? C_FG : C_WARN);
      for (int k = -60; k <= 60; k += 15) {
        float rr = k / 57.3f;
        line(cx + (int)(sinf(rr) * 150), cy - (int)(cosf(rr) * 150), cx + (int)(sinf(rr) * 164), cy - (int)(cosf(rr) * 164), C_DIM);
      }
      snprintf(b, sizeof(b), "%+5.1f\xC2\xB0", ang);
      drawTextC(FB, 240, 270, b, level ? C_FG : C_WARN);
      snprintf(b, sizeof(b), "EDGE");
    }
    if (level) { invBar(165, 60, 150, 36); drawTextC(FT, 240, 62, "LEVEL", C_BG, TX_FLAT); }
    if (level && !wasLevel) snd(SND_OK);
    wasLevel = level;
    drawTextC(FT, 240, 400, b, C_FG);
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {"0", "Zero", [this] {
      float g[3], r[3];
      if (imuRead(r, g)) {
        float gg = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
        bool flat = fabsf(r[2]) > 0.75f * gg;
        levOff[0] = r[0]; levOff[1] = flat ? r[1] : 0; levOff[2] = 0;
        if (!flat) levOff[0] = r[0];
        if (flat) levOff[2] = r[2] - (r[2] > 0 ? gg : -gg);
        toast("ZERO SET");
      }
    }}, {"R", "Reset", [] { levOff[0] = levOff[1] = levOff[2] = 0; toast("ZERO RESET"); }}});
  }
};
Screen* makeLevel() { return new Level(); }

// ================================================================== SNAKE
static int snakeBest = 0;
struct Snake : Screen {
  static const int GW = 20, GH = 15, CS = 22;
  int sx[GW * GH], sy[GW * GH], len, dir, nextDir, fx, fy, score;
  bool dead = true, started = false;
  uint32_t lastStep = 0;
  const char* name() override { return "SNAKE.EXE"; }
  bool noSwipeBack() override { return true; }
  bool keepAwake() override { return !dead; }
  void reset() {
    len = 4; dir = nextDir = 1; score = 0; dead = false; started = true;
    for (int i = 0; i < len; i++) { sx[i] = 8 - i; sy[i] = 7; }
    food(); lastStep = msNow();
  }
  void food() {
    for (;;) {
      fx = rnd() % GW; fy = rnd() % GH;
      bool on = false; for (int i = 0; i < len; i++) if (sx[i] == fx && sy[i] == fy) on = true;
      if (!on) return;
    }
  }
  void tick() override {
    if (dead) return;
    int speed = 170 - (score > 20 ? 60 : score * 3);
    if (msNow() - lastStep < (uint32_t)speed) return;
    lastStep = msNow();
    if ((nextDir + 2) % 4 != dir) dir = nextDir;
    static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
    int nx = sx[0] + DX[dir], ny = sy[0] + DY[dir];
    bool hitSelf = false;
    for (int i = 0; i < len - 1; i++) if (sx[i] == nx && sy[i] == ny) hitSelf = true;
    if (nx < 0 || ny < 0 || nx >= GW || ny >= GH || hitSelf) {
      dead = true; if (score > snakeBest) snakeBest = score; snd(SND_ERR); uiDirty(); return;
    }
    bool eat = nx == fx && ny == fy;
    if (eat && len < GW * GH) len++;
    for (int i = len - 1; i > 0; i--) { sx[i] = sx[i - 1]; sy[i] = sy[i - 1]; }
    sx[0] = nx; sy[0] = ny;
    if (eat) { score++; snd(SND_MOVE); food(); }
    uiDirty();
  }
  void draw() override {
    int ox = 20, oy = 60;
    rect(ox - 4, oy - 4, GW * CS + 8, GH * CS + 8, C_FG); rect(ox - 3, oy - 3, GW * CS + 6, GH * CS + 6, C_FG);
    for (int i = 0; i < len && started; i++) fillRect(ox + sx[i] * CS + 1, oy + sy[i] * CS + 1, CS - 2, CS - 2, i ? C_FG : C_WARN);
    if (started) fillRect(ox + fx * CS + 5, oy + fy * CS + 5, CS - 10, CS - 10, C_RED);
    char b[40]; snprintf(b, sizeof(b), "SCORE %d   BEST %d", started ? score : 0, snakeBest);
    drawText(FT, colX(0), rowY(12) + 2, b, C_FG);
    if (dead) {
      invBar(90, 190, 300, 80);
      drawTextC(FT, 240, 196, started ? "GAME OVER" : "SNAKE.EXE", C_BG, TX_FLAT);
      drawTextC(FT, 240, 230, "TAP TO PLAY", C_BG, TX_FLAT);
    }
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {"-+", "Turn", nullptr}});
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_KEY && e.key == K_BACK) { uiBack(); return true; }
    if (dead) { if (e.t == EV_TAP && e.y < 420) { reset(); return true; } if (e.t == EV_KEY && e.key == K_OK) { reset(); return true; } return false; }
    if (e.t == EV_SWIPE) { static const int M[5] = {0, 3, 1, 0, 2}; nextDir = M[e.dir]; return true; }
    if (e.t == EV_KEY && e.key == K_NEXT) { nextDir = (dir + 1) % 4; return true; }
    if (e.t == EV_KEY && e.key == K_PREV) { nextDir = (dir + 3) % 4; return true; }
    if (e.t == EV_TAP) {   // tap left/right of the head's direction to turn
      int hx = 20 + sx[0] * CS, hy = 60 + sy[0] * CS;
      if (dir == 0 || dir == 2) nextDir = e.x > hx ? 1 : 3; else nextDir = e.y > hy ? 2 : 0;
      return true;
    }
    return true;
  }
};
Screen* makeSnake() { return new Snake(); }

// ================================================================== SOUND
struct Sound : Screen {
  int lastKey = -1; uint32_t keyAt = 0;
  const char* name() override { return "SOUND.EXE"; }
  void draw() override {
    drawText(FT, 27, 60, "VOLUME", C_FG);
    meter(150, 58, 290, 34, cfg.vol, true);
    // the piano: two rows of 8 notes
    static const int HZ[16] = {262, 294, 330, 349, 392, 440, 494, 523, 587, 659, 698, 784, 880, 988, 1047, 1175};
    static const char* N[8] = {"C", "D", "E", "F", "G", "A", "B", "C"};
    for (int r = 0; r < 2; r++)
      for (int i = 0; i < 8; i++) {
        int k = r * 8 + i, x = 17 + i * 56, y = 112 + r * 120;
        bool on = k == lastKey && msNow() - keyAt < 200;
        if (on) invBar(x, y, 50, 110, false); else { rect(x, y, 50, 110, C_FG); rect(x + 1, y + 1, 48, 108, C_FG); }
        drawTextC(FT, x + 25, y + 70, N[i], on ? C_BG : C_FG, on ? TX_FLAT : TX_GLOW);
        int hz = HZ[k];
        hit(x, y, 50, 110, [this, k, hz] { lastKey = k; keyAt = msNow(); tone(hz, 220); uiDirty(); redrawIn(210); }, nullptr, false);
      }
    dosButton(17, 360, 104, 52, "BEEP", [] { snd(SND_BEEP); });
    dosButton(129, 360, 104, 52, "DISK", [] { snd(SND_FLOPPY); });
    dosButton(241, 360, 104, 52, "ALARM", [] { snd(SND_ALARM); });
    dosButton(353, 360, 104, 52, "ERROR", [] { snd(SND_CRACKLE); });
    if (!S.codec) drawTextC(FT, 240, 330, "SPEAKER CHIP NOT FOUND", C_RED, TX_FLAT);
    fkeysBack();
  }
  bool ev(const Ev& e) override {
    if ((e.t == EV_DOWN || e.t == EV_MOVE) && e.y0 > 40 && e.y0 < 100 && e.x > 140) {
      int p = (e.x - 150) * 100 / 290; cfg.vol = p < 0 ? 0 : p > 100 ? 100 : p; uiDirty(); return true;
    }
    if (e.t == EV_UP && e.y0 > 40 && e.y0 < 100) { cfgSave(); tone(880, 80); return true; }
    if (e.t == EV_KEY && (e.key == K_NEXT || e.key == K_PREV)) {
      int v = cfg.vol + (e.key == K_NEXT ? 10 : -10); cfg.vol = v < 0 ? 0 : v > 100 ? 100 : v; cfgSave(); tone(880, 80); return true;
    }
    return false;
  }
};
Screen* makeSound() { return new Sound(); }

// ================================================================== FILES
struct TextView : Screen {
  char title[40]; std::vector<std::string> lines; float scroll = 0; int dragY = 0; float s0 = 0;
  const char* name() override { return "TYPE"; }
  TextView(const char* t, const char* text) {
    snprintf(title, sizeof(title), "%s", t);
    const char* p = text;
    while (*p) {
      const char* e = p; int k = 0;
      while (*e && *e != '\n' && k < 29) { utf8Next(e); k++; }
      lines.push_back(std::string(p, e - p));
      p = e; if (*p == '\n') p++;
      if (lines.size() > 4000) break;
    }
  }
  void draw() override {
    pathLine(title);
    clipSet(15, 92, 450, 330);
    int first = (int)(scroll / LH);
    for (int i = first; i < (int)lines.size() && i < first + 12; i++) {
      std::string l = lines[i];
      for (auto& ch : l) if (ch == '\r' || ch == '\t') ch = ' ';
      drawText(FT, 15, 92 + i * LH - (int)scroll, l.c_str(), C_FG);
    }
    clipReset();
    char b[24]; snprintf(b, sizeof(b), "%d LINES", (int)lines.size());
    drawTextR(FT, 465, rowY(12) + 6, b, C_DIM, TX_FLAT);
    fkeys({{"ESC", "Back", [] { uiBack(); }}});
  }
  void clamp() { float mx = lines.size() * LH - 320.0f; if (mx < 0) mx = 0; if (scroll > mx) scroll = mx; if (scroll < 0) scroll = 0; }
  bool ev(const Ev& e) override {
    if (e.t == EV_DOWN) { dragY = e.y; s0 = scroll; return true; }
    if (e.t == EV_MOVE) { scroll = s0 + dragY - e.y; clamp(); uiDirty(); return true; }
    if (e.t == EV_KEY && e.key == K_NEXT) { scroll += LH * 8; clamp(); uiDirty(); return true; }
    if (e.t == EV_KEY && e.key == K_PREV) { scroll -= LH * 8; clamp(); uiDirty(); return true; }
    return false;
  }
};

// SHOW: pictures (JPEG / BMP), swipe or - / + for the next one in the folder
struct ImageView : Screen {
  std::vector<std::string> pics; int idx = 0;
  uint16_t* img = nullptr; char err[48] = ""; int iw = 0, ih = 0; bool info = true; uint32_t infoAt = 0;
  const char* name() override { return "SHOW"; }
  bool fullScreen() override { return true; }
  bool noSwipeBack() override { return true; }
  ImageView(std::vector<std::string> p, int i) : pics(p), idx(i) { load(); }
  ~ImageView() { free(img); }
  void load() {
    if (!img) img = (uint16_t*)bigAlloc(SCR_W * SCR_H * 2);
    err[0] = 0;
    if (!img) { strcpy(err, "NO MEMORY"); return; }
    if (!imgLoad(pics[idx].c_str(), img, SCR_W, SCR_H, IMG_FIT, err, &iw, &ih)) { if (!err[0]) strcpy(err, "CAN'T SHOW IT"); }
    info = true; infoAt = msNow(); uiDirty(); redrawIn(2600);
  }
  void go(int d) { if (pics.size() < 2) return; idx = (idx + d + pics.size()) % pics.size(); snd(SND_MOVE); load(); }
  void draw() override {
    if (img && !err[0]) memcpy(fb, img, SCR_W * SCR_H * 2); else gfxClearTo(0);
    if (err[0]) drawTextC(FT, 240, 220, err, C_RED);
    if (info && msNow() - infoAt < 2500) {
      const char* nm = strrchr(pics[idx].c_str(), '/'); nm = nm ? nm + 1 : pics[idx].c_str();
      char t[64]; snprintf(t, sizeof(t), "%s", nm); utf8Cut(t, 26);
      blendRect(0, 430, 480, 50, 0, 22);
      drawText(FT, 14, 440, t, C_FG, TX_FLAT);
      char k[32]; snprintf(k, sizeof(k), "%d/%d", idx + 1, (int)pics.size());
      drawTextR(FT, 466, 440, k, C_DIM, TX_FLAT);
      if (iw) { char r[24]; snprintf(r, sizeof(r), "%dx%d", iw, ih); blendRect(0, 0, 480, 44, 0, 22); drawTextR(FT, 466, 6, r, C_DIM, TX_FLAT); }
    }
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_SWIPE && e.dir == SW_LEFT) { go(1); return true; }
    if (e.t == EV_SWIPE && e.dir == SW_RIGHT) { go(-1); return true; }
    if (e.t == EV_SWIPE && (e.dir == SW_DOWN || e.dir == SW_UP)) { uiBack(); return true; }
    if (e.t == EV_LONG) {
      std::string p = pics[idx];
      ask("PICTURE", "Use it as the wallpaper?", {"SET AS WALLPAPER", "CANCEL"}, [p](int k) {
        if (k) return;
        snprintf(cfg.wallPic, sizeof(cfg.wallPic), "%s", p.c_str()); cfg.holoBack = 5; cfgChanged(); toast("WALLPAPER SET");
      });
      return true;
    }
    if (e.t == EV_TAP) { if (info && msNow() - infoAt < 2500) uiBack(); else { info = true; infoAt = msNow(); uiDirty(); redrawIn(2600); } return true; }
    if (e.t == EV_KEY) {
      if (e.key == K_NEXT) go(1); else if (e.key == K_PREV) go(-1); else uiBack();
      return true;
    }
    return false;
  }
};

static const char* extOf(const char* n) { const char* e = strrchr(n, '.'); return e ? e : ""; }
static bool isText(const char* n) {
  static const char* T[] = {".txt", ".md", ".json", ".js", ".csv", ".log", ".ini", ".cfg", ".p8", ".html", ".htm", ".css", ".xml", ".lua", ".py"};
  for (const char* t : T) if (!strcasecmp(extOf(n), t)) return true;
  return false;
}

// ------------------------------------------------------------------ a file -> the program made for it
static void listPics(const char* path, std::vector<std::string>& pics, int& at) {
  std::string p = path, dir = p.substr(0, p.rfind('/'));
  static FileEnt* e = nullptr;
  if (!e) e = (FileEnt*)bigAlloc(sizeof(FileEnt) * 200);
  int n = e ? fsList(dir.c_str(), e, 200) : 0;
  std::vector<std::string> v;
  for (int i = 0; i < n; i++) if (!e[i].dir && imgIsPicture(e[i].name)) v.push_back(dir + "/" + e[i].name);
  std::sort(v.begin(), v.end(), [](const std::string& a, const std::string& b) { return strcasecmp(a.c_str(), b.c_str()) < 0; });
  at = 0; for (int i = 0; i < (int)v.size(); i++) if (v[i] == p) at = i;
  if (v.empty()) v.push_back(p);
  pics = v;
}
bool openFile(const char* path) {
  const char* nm = strrchr(path, '/'); nm = nm ? nm + 1 : path;
  const char* ex = extOf(nm);
  std::string p = path, dir = p.substr(0, p.rfind('/'));
  if (!strcasecmp(ex, ".mp3") || !strcasecmp(ex, ".wav")) { musicPlayFolder(dir.c_str(), nm); uiOpen(makeNowPlaying()); return true; }
  if (!strcasecmp(ex, ".avi") || isPhoneVideo(nm)) { videoOrConvert(path); return true; }
  { size_t l = strlen(nm); if ((l > 3 && !strcasecmp(nm + l - 3, ".p8")) || (l > 7 && !strcasecmp(nm + l - 7, ".p8.png"))) { p8Open(path); return true; } }
  if (imgIsPicture(nm)) { std::vector<std::string> pics; int at; listPics(path, pics, at); uiOpen(new ImageView(pics, at)); return true; }
  if (isText(nm)) {
    size_t max = 96000, got = 0;
    uint8_t* buf = (uint8_t*)bigAlloc(max + 1);
    if (!buf || !fsRead(path, buf, max, got)) { free(buf); toast("CAN'T READ IT"); return true; }
    buf[got] = 0;
    uiOpen(new TextView(nm, (const char*)buf));
    free(buf);
    return true;
  }
  if (!strcasecmp(ex, ".bin")) {
    ask("INSTALL", "Put this firmware on the cube and restart?", {"INSTALL", "CANCEL"}, [p](int j) {
      if (j == 0 && !netInstallFile(p.c_str())) toast("NOT A GOOD FIRMWARE FILE");
    });
    return true;
  }
  return false;
}

struct Files : Screen {
  char dir[96] = "";                 // "" = the drives
  FileEnt ents[160]; int n = 0;
  ListView L;
  bool pickWall = false;             // choosing a wallpaper picture (SETUP > DISPLAY)
  bool pickSong = false;             // choosing the alarm's song (CLOCK > ALARM)
  const char* name() override { return pickWall ? "PICK PICTURE" : pickSong ? "PICK SONG" : "FILES.EXE"; }
  void load() {
    n = 0;
    if (!dir[0]) {
      strcpy(ents[n].name, "A:  SD CARD"); ents[n].dir = true; ents[n].size = 0; n++;
      strcpy(ents[n].name, "C:  CUBE FLASH"); ents[n].dir = true; ents[n].size = 0; n++;
    } else {
      strcpy(ents[0].name, ".."); ents[0].dir = true; ents[0].size = 0;
      int k = fsList(dir, ents + 1, 159);
      n = k < 0 ? 1 : k + 1;
      // folders first, then by name
      for (int a = 1; a < n; a++) for (int b = a + 1; b < n; b++) {
        bool sw = ents[a].dir != ents[b].dir ? ents[b].dir : strcasecmp(ents[a].name, ents[b].name) > 0;
        if (sw) { FileEnt t = ents[a]; ents[a] = ents[b]; ents[b] = t; }
      }
    }
    L.n = n; if (L.sel >= n) L.sel = n - 1; if (L.sel < 0) L.sel = 0; L.clampScroll();
    uiReveal();
  }
  void enter() override { L.setBox(15, 92, 450, 312); L.rowH = 52; if (!n) load(); }
  void openDrive(bool sd) {
    if (sd) {
      if (!sdMount()) {
        ask("NO SD CARD", "Put a card in (FAT32 or exFAT) and try again.", {"TRY AGAIN", "OK"}, [this](int i) { if (i == 0) openDrive(true); });
        return;
      }
      toast(S.sdInfo, 1400);
    }
    strcpy(dir, sd ? "/sd" : "/c"); L.sel = 0; L.scroll = 0; load();
  }
  void up() {
    char* s = strrchr(dir, '/');
    if (s && s != dir && strchr(dir + 1, '/')) *s = 0; else dir[0] = 0;
    L.sel = 0; L.scroll = 0; load(); uiDirty();
  }
  std::string pathOf(int i) { char p[200]; snprintf(p, sizeof(p), "%s/%s", dir, ents[i].name); return p; }
  void showPics(const std::string& p) {
    std::vector<std::string> pics; int at = 0;
    for (int i = 1; i < n; i++) if (!ents[i].dir && imgIsPicture(ents[i].name)) { if (pathOf(i) == p) at = pics.size(); pics.push_back(pathOf(i)); }
    if (pics.empty()) pics.push_back(p);
    uiOpen(new ImageView(pics, at));
  }
  void setWall(const std::string& p) {
    snprintf(cfg.wallPic, sizeof(cfg.wallPic), "%s", p.c_str());
    cfg.holoBack = 5;
    cfgChanged();
    toast("WALLPAPER SET");
  }
  void more(int i) {
    // options for a file or folder: copy to the other drive, rename, delete
    FileEnt f = ents[i];
    std::string p = pathOf(i);
    bool onSd = !strncmp(dir, "/sd", 3);
    std::vector<const char*> b;
    bool pic = !f.dir && imgIsPicture(f.name);
    const char* ex = strrchr(f.name, '.');
    bool song = !f.dir && ex && (!strcasecmp(ex, ".mp3") || !strcasecmp(ex, ".wav"));
    if (pic) b.push_back("SET AS WALLPAPER");
    if (song) b.push_back("ALARM SONG");
    if (!f.dir) b.push_back(onSd ? "COPY TO FLASH" : "COPY TO SD");
    b.push_back("RENAME"); b.push_back("DELETE"); b.push_back("CANCEL");
    char info[120], sz[16];
    if (f.size >= 1048576) snprintf(sz, sizeof(sz), "%.1f MB", f.size / 1048576.0); else snprintf(sz, sizeof(sz), "%u bytes", (unsigned)f.size);
    if (f.dir) snprintf(info, sizeof(info), "%s", f.name); else snprintf(info, sizeof(info), "%s\n%s", f.name, sz);
    ask(f.dir ? "FOLDER" : "FILE", info, b, [this, f, p, onSd, pic, song](int k) {
      if (pic) { if (k == 0) { setWall(p); return; } k--; }
      if (song) { if (k == 0) { snprintf(cfg.alarmSong, sizeof(cfg.alarmSong), "%s", p.c_str()); cfgChanged(); toast("ALARM SONG SET"); return; } k--; }
      if (!f.dir) k--;                          // the first button was COPY
      if (k == -1) {
        char to[200]; snprintf(to, sizeof(to), "%s/%s", onSd ? "/c" : "/sd", f.name);
        if (!onSd && !sdMount()) { toast("NO SD CARD"); return; }
        toast("COPYING...", 600); uiFrame();
        bool ok = fsCopy(p.c_str(), to);
        toast(ok ? "COPIED" : "COULD NOT COPY (FULL?)");
      } else if (k == 0) {
        std::string pp = p;
        kbOpen("NEW NAME", f.name, 60, [this, pp](const char* nm) {
          if (!nm[0] || strchr(nm, '/')) return;
          std::string to = pp.substr(0, pp.rfind('/') + 1) + nm;
          toast(fsRename(pp.c_str(), to.c_str()) ? "RENAMED" : "COULD NOT RENAME"); load();
        });
      } else if (k == 1) {
        std::string pp = p; bool d = f.dir;
        ask("DELETE", d ? "Delete this folder? (it has to be empty)" : "Delete this file?", {"DELETE", "KEEP"}, [this, pp, d](int j) {
          if (j) return;
          bool ok = d ? fsRmdir(pp.c_str()) : fsRemove(pp.c_str());
          toast(ok ? "DELETED" : d ? "THE FOLDER ISN'T EMPTY" : "COULD NOT DELETE"); load();
        });
      }
    });
  }
  void open(int i) {
    FileEnt& f = ents[i];
    if (!dir[0]) { openDrive(f.name[0] == 'A'); return; }
    if (!strcmp(f.name, "..")) { up(); return; }
    std::string p = pathOf(i);
    if (f.dir) { snprintf(dir, sizeof(dir), "%s", p.c_str()); L.sel = 0; L.scroll = 0; load(); return; }
    if (pickWall) { if (imgIsPicture(f.name)) { setWall(p); uiBack(); } else toast("PICK A JPEG OR BMP"); return; }
    if (pickSong) {
      const char* e = strrchr(f.name, '.');
      if (e && (!strcasecmp(e, ".mp3") || !strcasecmp(e, ".wav"))) { snprintf(cfg.alarmSong, sizeof(cfg.alarmSong), "%s", p.c_str()); cfgChanged(); toast("ALARM SONG SET"); uiBack(); }
      else toast("PICK AN MP3 OR WAV");
      return;
    }
    if (openFile(p.c_str())) return;              // music, video, pictures, text, firmware: straight into its program
    more(i);                                       // nothing plays it: what to do with it
  }
  void draw() override {
    char path[110];
    if (!dir[0]) strcpy(path, "DRIVES"); else snprintf(path, sizeof(path), "%c:%s>", dir[1] == 's' ? 'A' : 'C', strchr(dir + 1, '/') ? strchr(dir + 1, '/') : "\\");
    for (char* p = path; *p; p++) if (*p == '/') *p = '\\';
    char pp[32]; snprintf(pp, sizeof(pp), "%s", path); utf8Cut(pp, 28);
    pathLine(pp);
    L.n = n;
    L.draw([&](int i, int x, int y, int w, int h, bool sel) {
      FileEnt& f = ents[i];
      int ty = y + (h - LH) / 2;
      if (sel) invBar(x + 2, y + 3, w - 4, h - 6);
      uint16_t c = sel ? C_BG : C_FG; int m = sel ? TX_FLAT : TX_GLOW;
      char nm[64]; snprintf(nm, sizeof(nm), "%s", f.name); utf8Cut(nm, dir[0] ? 19 : 16);
      drawText(FT, x + 10, ty, nm, c, m);
      char sz[24];
      if (!dir[0]) {                            // the drives: what's in them
        uint64_t t, u;
        if (i == 0) snprintf(sz, sizeof(sz), "%s", S.sd ? S.sdInfo : "PUT A CARD IN");
        else if (fsInfo("/c", t, u)) snprintf(sz, sizeof(sz), "%u MB FREE", (unsigned)((t - u) >> 20)); else sz[0] = 0;
      }
      else if (f.dir) strcpy(sz, "<DIR>");
      else if (f.size >= 1048576) snprintf(sz, sizeof(sz), "%uM", (unsigned)(f.size >> 20));
      else if (f.size >= 10240) snprintf(sz, sizeof(sz), "%uK", (unsigned)(f.size >> 10));
      else snprintf(sz, sizeof(sz), "%u", (unsigned)f.size);
      drawTextR(FT, x + w - 10, ty, sz, sel ? C_BG : C_DIM, TX_FLAT);
    }, nullptr);
    if (dir[0]) {
      uint64_t tot, used; char b[48] = "";
      if (fsInfo(dir, tot, used)) snprintf(b, sizeof(b), "%d item(s)  %u MB free", n - 1, (unsigned)((tot - used) >> 20));
      drawText(FT, colX(0), rowY(12) + 6, b, C_DIM, TX_FLAT);
      bool sdRoot = !strcmp(dir, "/sd");
      if (sdRoot) fkeys({{"ESC", "Up", [this] { up(); }}, {"N", "New", [this] { newFolder(); }}, {"O", "Opt", [this] { if (L.sel > 0) more(L.sel); }},
                         {"E", "Eject", [this] { sdEject(); dir[0] = 0; load(); toast("SAFE TO TAKE OUT"); }}});
      else fkeys({{"ESC", "Up", [this] { up(); }}, {"N", "New", [this] { newFolder(); }}, {"O", "Opt", [this] { if (L.sel > 0) more(L.sel); }}, {"R", "Ref", [this] { load(); }}});
    } else fkeys({{"ESC", "Back", [] { uiBack(); }}, {"R", "Refresh", [this] { sdMount(); load(); }}});
  }
  void newFolder() {
    kbOpen("NEW FOLDER", "", 40, [this](const char* nm) {
      if (!nm[0] || strchr(nm, '/')) return;
      char p[200]; snprintf(p, sizeof(p), "%s/%s", dir, nm);
      toast(fsMkdir(p) ? "FOLDER MADE" : "COULD NOT MAKE IT"); load();
    });
  }
  void tick() override {
    L.tick();
    static uint32_t seen = 0;
    if (S.fileAt != seen) { seen = S.fileAt; if (dir[0]) load(); }      // a file came in from the phone
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_KEY && e.key == K_BACK) { if (dir[0]) up(); else uiBack(); return true; }
    if (e.t == EV_SWIPE && e.dir == SW_RIGHT && dir[0]) { up(); return true; }
    if (e.t == EV_LONG && dir[0] && e.x0 >= L.x && e.x0 < L.x + L.w && e.y0 >= L.y && e.y0 < L.y + L.h) {
      int i = (int)((e.y0 - L.y + L.scroll) / L.rowH);           // hold a row: what to do with it
      if (i > 0 && i < n) { L.sel = i; more(i); }
      return true;
    }
    return L.ev(e, [this](int i) { open(i); uiDirty(); });
  }
};
Screen* makeFilePicker() { Files* f = new Files(); f->pickWall = true; return f; }
Screen* makeSongPicker() {
  Files* f = new Files(); f->pickSong = true;
  sdMount();
  const char* d = S.sd && fsSize("/sd/MUSIC") >= 0 ? "/sd/MUSIC" : S.sd ? "/sd" : "/c";
  snprintf(f->dir, sizeof(f->dir), "%s", d);
  return f;
}
Screen* makeFiles() { return new Files(); }
