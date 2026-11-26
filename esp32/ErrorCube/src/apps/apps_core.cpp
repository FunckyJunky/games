// apps_core.cpp: the program list, ERROR-BIOS start-up, the ERROR-DOS menu, the always-on clock,
// and the tables behind the settings.
#include "../ui/ui.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

const uint16_t OFF_SECS[] = {15, 30, 60, 120, 300, 600, 1800, 0};
const int N_OFF = 8;
const uint16_t CHARGE_MA[] = {100, 200, 300, 400, 500, 700, 1000};
const int N_CHARGE = 7;
const uint16_t AUTO_OFF_MIN[] = {0, 10, 30, 60, 180};
const int N_AUTO_OFF = 5;
const uint8_t UPD_HOURS[] = {1, 6, 24};
const int N_UPD = 3;
const Tz TIMEZONES[] = {
  {"AMSTERDAM/BERLIN", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"LONDON/DUBLIN", "GMT0IST,M3.5.0/1,M10.5.0"},
  {"LISBON", "WET0WEST,M3.5.0/1,M10.5.0"},
  {"KYIV/HELSINKI", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"ISTANBUL", "<+03>-3"},
  {"MOSCOW", "MSK-3"},
  {"DUBAI", "<+04>-4"},
  {"INDIA", "IST-5:30"},
  {"BANGKOK", "<+07>-7"},
  {"CHINA/SINGAPORE", "CST-8"},
  {"TOKYO/SEOUL", "JST-9"},
  {"SYDNEY", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
  {"AUCKLAND", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
  {"UTC", "UTC0"},
  {"NEW YORK", "EST5EDT,M3.2.0,M11.1.0"},
  {"CHICAGO", "CST6CDT,M3.2.0,M11.1.0"},
  {"DENVER", "MST7MDT,M3.2.0,M11.1.0"},
  {"LOS ANGELES", "PST8PDT,M3.2.0,M11.1.0"},
  {"SAO PAULO", "<-03>3"},
};
const int N_TZ = sizeof(TIMEZONES) / sizeof(TIMEZONES[0]);

std::vector<Prog> PROGS;

Screen* findProg(const char* dos) {
  for (auto& p : PROGS) if (!strcmp(p.dos, dos)) return p.make();
  return nullptr;
}
void openProg(const char* dos) {
  // OFFLINE: programs that need the internet ask first
  for (auto& p : PROGS) {
    if (strcmp(p.dos, dos) || !cfg.offline) continue;
    if (p.folder == F_ONLINE || !strcmp(dos, "UPDATE")) {
      std::string d = dos;
      ask("OFFLINE", "This needs the internet. Go online?", {"GO ONLINE", "CANCEL"}, [d](int i) {
        if (i) return;
        cfg.offline = 0; netOffline(false); cfgChanged();
        Screen* s = findProg(d.c_str()); if (s) { snd(SND_FLOPPY); uiOpen(s); }
      });
      return;
    }
  }
  Screen* s = findProg(dos);
  if (s) { snd(SND_FLOPPY); uiOpen(s); }
}

void cfgChanged() {
  static int lastView = -1;
  int view = cfg.view && !cfg.lowGfx;
  if (lastView >= 0 && lastView != view) uiReplaceRoot(homeScreen());   // DOS list <-> 3D carousel
  lastView = view;
  gfxTheme(cfg.color, lowFx() ? 0 : cfg.glow);
  sysBrightness(uiBrightNow());
  sysApplyRotation();
  sysApplyButtons();
  cfgSave();
  uiDirty();
}

// ------------------------------------------------------------------ ERROR-BIOS
struct Boot : Screen {
  struct Line { char s[40]; bool inv; };
  std::vector<Line> lines;
  int step = 0; uint32_t next = 0; bool skip = false;
  int memK = 0, memMax = 0, prog = 0, typing = 0;
  char cmd[24] = "";
  const char* name() override { return "BOOT"; }
  bool fullScreen() override { return true; }
  bool keepAwake() override { return true; }
  void enter() override {
    memMax = (int)((freePsram() + freeHeap()) / 1024);
    if (memMax < 512) memMax = 8192;
    next = msNow() + 200;
    if (cfg.boot == 2) step = 100;            // OFF: straight to the menu
    else if (cfg.boot == 1) step = 50;        // SHORT: just "Starting ERROR-DOS..."
  }
  void print(const char* s, bool inv = false) {
    Line l; snprintf(l.s, sizeof(l.s), "%s", s); l.inv = inv;
    lines.push_back(l);
    if (lines.size() > ROWS) lines.erase(lines.begin());
    uiDirty();
  }
  void wait(int ms) { next = msNow() + (skip ? 0 : ms); }
  void tick() override {
    if ((int32_t)(msNow() - next) < 0) return;
    char b[64];
    switch (step) {
      case 0: snd(SND_BEEP); print("ERROR-BIOS (C) 2026 ERROR.BLUE"); print("ERROR-CUBE " CUBE_VERSION); print(""); step++; wait(250); return;
      case 1: print("CPU     : ESP32-S3 240MHz x2"); print("Memory  : 0K"); step++; wait(60); return;
      case 2:
        memK += memMax / 14 + 1;
        if (memK >= memMax) { memK = memMax; step++; }
        snprintf(b, sizeof(b), "Memory  : %dK%s", memK, memK >= memMax ? " OK" : "");
        snprintf(lines.back().s, sizeof(lines.back().s), "%s", b); uiDirty();
        wait(30); return;
      case 3: print("Display : 480x480 AMOLED"); step++; wait(120); return;
      case 4: print(S.touch ? "Touch   : CST9220 OK" : "Touch   : not found"); step++; wait(110); return;
      case 5:
        if (!S.pmu) print("Power   : not found");
        else if (S.battery) { snprintf(b, sizeof(b), "Power   : battery %d%%", S.batPct); print(b); }
        else print("Power   : USB, no battery");
        step++; wait(110); return;
      case 6: print(S.rtc ? "Clock   : PCF85063 OK" : "Clock   : not found"); step++; wait(110); return;
      case 7: print(S.imu ? "Motion  : QMI8658 OK" : "Motion  : not found"); step++; wait(110); return;
      case 8: print(S.codec ? "Sound   : ES8311 speaker" : "Sound   : not found"); step++; wait(110); return;
      case 9: snprintf(b, sizeof(b), "Drive A : %s", S.sd ? S.sdInfo : "no TF card"); print(b); step++; wait(110); return;
      case 10: print("Drive C : flash"); step++; wait(110); return;
      case 11: snprintf(b, sizeof(b), "Network : %s", S.mac[0] ? S.mac : "WiFi"); print(b); step++; wait(350); return;
      case 12: print(""); step = 50; return;
      case 50: print("Starting ERROR-DOS..."); step++; wait(cfg.boot == 0 ? 500 : 200); return;
      case 51: print(""); snprintf(cmd, sizeof(cmd), "SCAN *.EXE /S"); typing = 0; print("C:\\>"); step++; return;
      case 52:
        if (typing <= (int)strlen(cmd)) {
          snprintf(lines.back().s, sizeof(lines.back().s), "C:\\>%.*s", typing, cmd); typing++;
          if (cfg.clicks) snd(SND_KEY);
          uiDirty(); wait(45 + rnd() % 50); return;
        }
        step++; prog = 0; wait(150); return;
      case 53:
        if (prog < (int)PROGS.size()) {
          snprintf(b, sizeof(b), "  %-12s <%s>", (std::string(PROGS[prog].dos) + ".EXE").c_str(),
                   PROGS[prog].folder == F_ONLINE ? "ONLINE" : PROGS[prog].folder == F_SYSTEM ? "SYS" : "APP");
          print(b); prog++; if (!skip) snd(SND_MOVE); wait(cfg.boot == 0 ? 90 : 40); return;
        }
        snprintf(b, sizeof(b), "  %d program(s) found", (int)PROGS.size()); print(b); print("");
        snprintf(cmd, sizeof(cmd), "CD APPS"); typing = 0; print("C:\\>"); step++; return;
      case 54:
        if (typing <= (int)strlen(cmd)) {
          snprintf(lines.back().s, sizeof(lines.back().s), "C:\\>%.*s", typing, cmd); typing++;
          if (cfg.clicks) snd(SND_KEY);
          uiDirty(); wait(45 + rnd() % 50); return;
        }
        step = 100; wait(250); return;
      case 100: step = 101; uiStart(); return;
    }
  }
  void draw() override {
    for (size_t i = 0; i < lines.size(); i++) drawText(FT, colX(0), rowY(i), lines[i].s, C_FG);
    if (lines.size() < ROWS && (msNow() / 530) % 2 == 0) {
      int x = colX(utf8Len(lines.empty() ? "" : lines.back().s));
      fillRect(x, rowY(lines.size() - 1) + 4, CW, LH - 6, C_FG);
    }
    redrawIn(265);
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_TAP || e.t == EV_KEY) { skip = true; next = 0; }
    return true;
  }
};
Screen* bootScreen() { return new Boot(); }

// ------------------------------------------------------------------ ERROR-DOS: the program menu
// Four folders: APPS (built in), WEB (web apps from error.blue/cube/apps), ONLINE, SYSTEM.
static const char* TAB_NAMES[4] = {"APPS", "WEB", "ONLINE", "SYSTEM"};
static const int TAB_F[4] = {F_APPS, F_WEB, F_ONLINE, F_SYSTEM};
struct Home : Screen {
  int tab = 0;
  ListView L;
  struct Item { std::string name, desc, id; int prog; };
  std::vector<Item> items;
  int selT[4] = {0, 0, 0, 0};
  int webState = -1;
  const char* name() override { return "ERROR-DOS"; }
  void fill() {
    items.clear();
    if (TAB_F[tab] == F_WEB) {
      static AppInfo* a = (AppInfo*)malloc(sizeof(AppInfo) * 32);
      int n = netApps(a, 32);
      webState = n;
      for (int i = 0; i < n; i++) items.push_back({a[i].name, a[i].info, a[i].id, -1});
    } else {
      for (size_t i = 0; i < PROGS.size(); i++) if (PROGS[i].folder == TAB_F[tab]) items.push_back({PROGS[i].dos, PROGS[i].desc, "", (int)i});
    }
    L.n = items.size();
  }
  void enter() override {
    netReportProg("ERROR-DOS");
    fill(); L.setBox(27, 118, 426, 260); L.rowH = 52; L.sel = selT[tab]; L.showSel();
  }
  void setTab(int t) {
    if (t < 0 || t > 3 || t == tab) return;
    selT[tab] = L.sel;
    tab = t;
    if (TAB_F[tab] == F_WEB) netAppsRefresh();
    fill(); L.sel = selT[t]; L.scroll = 0; L.showSel();
    snd(SND_MOVE); garbleUntil = msNow() + 120;
    uiDirty();
  }
  void run(int i) {
    if (i < 0 || i >= (int)items.size()) return;
    selT[tab] = L.sel;
    if (items[i].prog >= 0) openProg(PROGS[items[i].prog].dos);
    else { snd(SND_FLOPPY); uiOpen(makeWebApp(items[i].id.c_str(), items[i].name.c_str())); }
  }
  void draw() override {
    for (int t = 0; t < 4; t++) {
      int x = 15 + t * 114, w = 108;
      if (t == tab) invBar(x, 56, w, 38);
      else rect(x, 56, w, 38, C_DIM);
      drawTextC(FT, x + w / 2, 59, TAB_NAMES[t], t == tab ? C_BG : C_DIM, TX_FLAT);
      hit(x, 50, w, 48, [this, t] { setTab(t); }, nullptr, false);
    }
    dosBox(15, 102, 450, 292, C_FG);
    if (TAB_F[tab] == F_WEB && items.empty()) {
      const char* m = webState == -1 ? "LOADING THE LIST..." : S.net != NET_UP ? "NO WIFI" : "NO WEB APPS YET";
      drawTextC(FT, 240, 210, m, C_DIM, TX_FLAT);
      drawTextC(FT, 240, 250, "error.blue/cube/apps", C_DIM, TX_FLAT);
    }
    L.n = items.size();
    L.draw([&](int i, int x, int y, int w, int h, bool sel) {
      const Item& p = items[i];
      int ty = y + (h - LH) / 2;
      if (sel) invBar(x + 2, y + 3, w - 4, h - 6);
      char nm[16]; snprintf(nm, sizeof(nm), "%-8s", p.name.c_str()); utf8Cut(nm, 11);
      bool web = p.prog < 0;
      drawText(FT, x + 10, ty, nm, sel ? C_BG : C_FG, sel ? TX_FLAT : TX_GLOW);
      int dx = web ? 12 : 9;
      drawText(FT, x + 10 + CW * dx, ty, p.desc.c_str(), sel ? C_BG : C_DIM, TX_FLAT, 27 - dx);
    }, nullptr);
    char pr[40];
    snprintf(pr, sizeof(pr), "C:\\%s>%s", TAB_NAMES[tab], L.sel >= 0 && L.sel < (int)items.size() ? (items[L.sel].prog >= 0 ? items[L.sel].name.c_str() : items[L.sel].id.c_str()) : "");
    utf8Cut(pr, 29);
    drawText(FT, colX(0), rowY(12), pr, C_FG);
    if ((msNow() / 530) % 2 == 0) fillRect(colX(utf8Len(pr)), rowY(12) + 4, CW, LH - 6, C_FG);
    redrawIn(530 - msNow() % 530);
    fkeys({{"\xE2\x86\xB5", "Run", [this] { run(L.sel); }},
           {"\xE2\x86\x90\xE2\x86\x92", "Dir", [this] { setTab((tab + 1) % 4); }},
           {"\xE2\x89\xA1", "Quick", [] { quickOpen(); }}});
  }
  void tick() override {
    L.tick();
    if (TAB_F[tab] == F_WEB) {
      static AppInfo* a = (AppInfo*)malloc(sizeof(AppInfo) * 32);
      int n = netApps(a, 32);
      if (n != webState) { fill(); uiDirty(); }
    }
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_SWIPE && e.dir == SW_LEFT) { setTab(tab < 3 ? tab + 1 : 3); return true; }
    if (e.t == EV_SWIPE && e.dir == SW_RIGHT) { setTab(tab > 0 ? tab - 1 : 0); return true; }
    if (e.t == EV_KEY && e.key == K_BACK) { setTab((tab + 1) % 4); return true; }
    if (e.t == EV_KEY && e.key == K_NEXT && L.sel >= L.n - 1) { setTab((tab + 1) % 4); L.sel = 0; L.showSel(); return true; }
    if (e.t == EV_KEY && e.key == K_PREV && L.sel <= 0 && tab > 0) { setTab(tab - 1); L.sel = L.n - 1; L.showSel(); return true; }
    return L.ev(e, [this](int i) { run(i); });
  }
};
Screen* homeScreen() { return cfg.view && !cfg.lowGfx ? makeHome3D() : (Screen*)new Home(); }

// ------------------------------------------------------------------ always-on clock (screen "off")
struct Aod : Screen {
  // the low power clock (screen "off"): black, dim, moves a little every minute so the AMOLED doesn't burn in
  const char* name() override { return "AOD"; }
  bool fullScreen() override { return true; }
  void draw() override {
    struct tm t;
    bool ok = sysLocalTime(t);
    char c[12] = "--:--", d[32] = "";
    if (ok) {
      fmtTime(c, t.tm_hour, t.tm_min, 0, false);
      static const char* DAYS[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
      snprintf(d, sizeof(d), "%s %02d-%02d", DAYS[t.tm_wday], t.tm_mday, t.tm_mon + 1);
    }
    uint32_t m = msNow() / 60000;
    int ox = (int)(m * 37 % 61) - 30, oy = (int)(m * 53 % 81) - 40;
    uint16_t dim = mix(C_BLACK, C_FG, 55), dim2 = mix(C_BLACK, C_FG, 38);
    int y = 160;
    switch (cfg.aodStyle) {
      case 1:   // SMALL
        drawTextC(FB, 240 + ox, 200 + oy, c, dim, TX_FLAT); y = 252; break;
      case 2: { // ANALOG: a ring, two hands, twelve dots
        int cx = 240 + ox, cy = 190 + oy, R = 96;
        ring(cx, cy, R, 2, dim2);
        for (int i = 0; i < 12; i++) { float a = i * 0.5236f; fillCircle(cx + (int)(sinf(a) * (R - 12)), cy - (int)(cosf(a) * (R - 12)), i % 3 ? 1 : 3, dim2); }
        if (ok) {
          float ha = ((t.tm_hour % 12) + t.tm_min / 60.0f) * 0.5236f, ma = t.tm_min * 0.10472f;
          thickLine(cx, cy, cx + (int)(sinf(ha) * 52), cy - (int)(cosf(ha) * 52), 6, dim);
          thickLine(cx, cy, cx + (int)(sinf(ma) * 80), cy - (int)(cosf(ma) * 80), 3, dim);
        }
        fillCircle(cx, cy, 5, dim);
        y = 304; break;
      }
      case 3: { // DOS: a prompt with the time
        char l[40]; snprintf(l, sizeof(l), "C:\\>TIME %s", c);
        drawText(FT, 60 + ox, 200 + oy, l, dim, TX_FLAT);
        if (((msNow() / 60000) & 1) == 0) fillRect(60 + ox + textW(FT, l), 204 + oy, CW, LH - 8, dim2);
        y = 250; break;
      }
      default:  // BIG
        drawTextC(FH, 240 + ox, 160 + oy, c, dim, TX_FLAT); y = 270; break;
    }
    if (cfg.aodDate && d[0]) { drawTextC(FT, 240 + ox, y + oy, d, dim2, TX_FLAT); y += 36; }
    if (cfg.aodBatt && S.battery) { char b[16]; snprintf(b, sizeof(b), "%d%%%s", S.batPct, S.charging ? "+" : ""); drawTextC(FT, 240 + ox, y + oy, b, mix(C_BLACK, C_FG, 30), TX_FLAT); y += 36; }
    if (cfg.aodNote && S.chatUnread) drawTextC(FT, 240 + ox, y + oy, "NEW MESSAGE", dim2, TX_FLAT);
    if (cfg.offline) drawTextC(FT, 240 + ox, 420, "OFFLINE", mix(C_BLACK, C_FG, 22), TX_FLAT);
  }
};
Screen* aodScreen() { return new Aod(); }

// ------------------------------------------------------------------ the programs
Screen* makeClock(); Screen* makeCalc(); Screen* makePaint(); Screen* makeLevel(); Screen* makeFiles(); Screen* makeSnake();
Screen* makeWho(); Screen* makeChat(); Screen* makeLink();
Screen* makeMusic(); Screen* makeVideo(); Screen* makePico8();
Screen* makeSetup(); Screen* makeWifi(); Screen* makeUpdate(); Screen* makeInfo(); Screen* makeCommand(); Screen* makeSound();
void appsRegister() {
  PROGS = {
    {"CLOCK", "time alarm timer", F_APPS, makeClock},
    {"MUSIC", "mp3 / wav player", F_APPS, makeMusic},
    {"VIDEO", "video player", F_APPS, makeVideo},
    {"PICO8", "fantasy console", F_APPS, makePico8},
    {"CALC", "calculator", F_APPS, makeCalc},
    {"PAINT", "draw with a finger", F_APPS, makePaint},
    {"LEVEL", "spirit level", F_APPS, makeLevel},
    {"SNAKE", "the game", F_APPS, makeSnake},
    {"SOUND", "speaker + tones", F_APPS, makeSound},
    {"FILES", "TF card + flash", F_APPS, makeFiles},
    {"WHO", "who's online", F_ONLINE, makeWho},
    {"CHAT", "ERROR-CHAT", F_ONLINE, makeChat},
    {"LINK", "phone control", F_ONLINE, makeLink},
    {"SETUP", "all settings", F_SYSTEM, makeSetup},
    {"WIFI", "networks", F_SYSTEM, makeWifi},
    {"UPDATE", "new firmware", F_SYSTEM, makeUpdate},
    {"INFO", "system + sensors", F_SYSTEM, makeInfo},
    {"COMMAND", "C:\\> prompt", F_SYSTEM, makeCommand},
  };
}
