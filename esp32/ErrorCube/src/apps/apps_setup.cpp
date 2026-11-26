// apps_setup.cpp: SETUP (every setting), WIFI, UPDATE, INFO, COMMAND
#include "../ui/ui.h"
#include "../core/holo.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

// ------------------------------------------------------------------ building blocks for settings pages
static Row heading(const char* n, uint8_t ic = 0) { Row r; r.name = n; r.kind = 4; r.icon = ic; return r; }
static Row info(const char* n, std::function<void(char*)> v) { Row r; r.name = n; r.val = v; r.kind = 3; return r; }
static Row action(const char* n, std::function<void()> f, std::function<void(char*)> v = nullptr) {
  Row r; r.name = n; r.val = v; r.kind = 0; r.act = [f](int) { f(); }; return r;
}
static Row toggle(const char* n, uint8_t* v, std::function<void()> after = nullptr) {
  Row r; r.name = n; r.kind = 1;
  r.val = [v](char* o) { strcpy(o, *v ? "ON" : "OFF"); };
  r.act = [v, after](int) { *v = !*v; if (after) after(); cfgChanged(); };
  return r;
}
static Row choice(const char* n, uint8_t* v, int count, std::function<const char*(int)> label, std::function<void()> after = nullptr) {
  Row r; r.name = n; r.kind = 1;
  r.val = [v, label](char* o) { snprintf(o, 40, "%s", label(*v)); };
  r.act = [v, count, after](int d) { *v = (*v + (d < 0 ? count - 1 : 1)) % count; if (after) after(); cfgChanged(); };
  return r;
}
static Row slider(const char* n, uint8_t* v, int lo, std::function<void()> live = nullptr) {
  Row r; r.name = n; r.kind = 2;
  r.pct = [v]() { return (int)*v; };
  r.setPct = [v, lo, live](int p) { *v = p < lo ? lo : p; if (live) live(); };
  r.act = [v, lo, live](int) { *v = *v >= 100 ? lo : (*v + 10 > 100 ? 100 : *v + 10); if (live) live(); cfgChanged(); };
  return r;
}

// a number with < >: lo..hi in steps, shown with a unit ("PX", "°"); 0 can read as OFF
static Row number(const char* n, uint8_t* v, int lo, int hi, int step, const char* unit, const char* zero = nullptr) {
  Row r; r.name = n; r.kind = 1;
  r.val = [v, unit, zero](char* o) { if (!*v && zero) strcpy(o, zero); else snprintf(o, 40, "%d%s", *v, unit); };
  r.act = [v, lo, hi, step](int d) {
    int x = *v + (d < 0 ? -step : step);
    if (x > hi) x = lo; if (x < lo) x = hi;           // wraps round, like the choices
    *v = x; cfgChanged();
  };
  return r;
}

static Row section(const char* n, const char* desc, uint8_t ic, Screen* (*open)());
static Screen* pageAlarm();
struct Page : Screen {
  char title[24], path[40];
  std::function<void(std::vector<Row>&)> build;
  std::vector<Row> rows;
  ListView L;
  Page(const char* t, const char* p, std::function<void(std::vector<Row>&)> b) : build(b) {
    snprintf(title, sizeof(title), "%s", t); snprintf(path, sizeof(path), "%s", p);
  }
  const char* name() override { return title; }
  void enter() override { L.setBox(15, 92, 450, 330); L.rowH = 55; rows.clear(); build(rows); L.n = rows.size(); if (L.sel < 0 || L.sel >= L.n || rows[L.sel].kind >= 3) { L.sel = 0; while (L.sel < L.n - 1 && rows[L.sel].kind >= 3) L.sel++; } }
  void draw() override {
    rows.clear(); build(rows);
    pathLine(path);
    drawRows(L, rows);
    fkeysBack();
  }
  void tick() override { L.tick(); }
  bool ev(const Ev& e) override { return rowsEv(L, rows, e); }
};
static Screen* page(const char* t, const char* p, std::function<void(std::vector<Row>&)> b) { return new Page(t, p, b); }

static const char* offLabel(int i) {
  static char b[12]; int s = OFF_SECS[i];
  if (!s) return "NEVER";
  if (s < 60) snprintf(b, sizeof(b), "%d SEC", s); else snprintf(b, sizeof(b), "%d MIN", s / 60);
  return b;
}
static void fmtUptime(char* o) { uint32_t s = uptimeSecs(); sprintf(o, "%ud %02u:%02u", (unsigned)(s / 86400), (unsigned)(s / 3600 % 24), (unsigned)(s / 60 % 60)); }

// ------------------------------------------------------------------ the pages
static void idleText(char* o) {
  // what happens when you stop using it, in one line
  int sec = OFF_SECS[cfg.offIdx % N_OFF];
  char off[12];
  if (!sec) { strcpy(o, "STAYS ON"); return; }
  if (sec < 60) snprintf(off, sizeof(off), "%dS", sec); else snprintf(off, sizeof(off), "%dMIN", sec / 60);
  if (cfg.dim && sec > 15) snprintf(o, 40, "DIM, OFF AT %s", off); else snprintf(o, 40, "OFF AT %s", off);
}
Screen* makeFilePicker();
static Screen* pageDisplay() {
  return page("SCREEN", "C:\\SETUP\\SCREEN>", [](std::vector<Row>& r) {
    r.push_back(heading("LIGHT", IC_LIGHT));
    r.push_back(slider("BRIGHTNESS", &cfg.bright, 5, [] { sysBrightness(uiBrightNow()); }));
    r.push_back(choice("ROTATION", &cfg.rot, 5, [](int i) { static const char* R[5] = {"0", "90", "180", "270", "AUTO"}; return R[i]; }));
    r.push_back(heading("WALLPAPER", IC_WALL));
    r.push_back(choice("WALLPAPER", &cfg.holoBack, 6, [](int i) { static const char* B[6] = {"GLASS", "DOTS", "GRID", "STARS", "ROOM", "PICTURE"}; return B[i]; }));
    r.push_back(action("PICTURE", [] { uiOpen(makeFilePicker()); }, [](char* o) {
      const char* n = strrchr(cfg.wallPic, '/');
      if (!cfg.wallPic[0]) strcpy(o, "CHOOSE...");
      else { snprintf(o, 40, "%s", n ? n + 1 : cfg.wallPic); utf8Cut(o, 14); }
    }));
    r.push_back(slider("PICTURE LIGHT", &cfg.wallLight, 5));
    r.push_back(heading("WHEN YOU STOP USING IT", IC_MOON));
    r.push_back(info("IDLE", [](char* o) { idleText(o); }));
    r.push_back(info("THEN", [](char* o) { strcpy(o, !OFF_SECS[cfg.offIdx % N_OFF] ? "-" : cfg.aod ? "LOW POWER CLOCK" : "NOTHING (BLACK)"); }));
    r.push_back(choice("TURN OFF AFTER", &cfg.offIdx, N_OFF, [](int i) { return offLabel(i); }));
    r.push_back(toggle("DIM 10 S BEFORE", &cfg.dim));
    r.push_back(choice("WHEN OFF SHOW", &cfg.aod, 2, [](int i) { return i ? "LOW POWER CLOCK" : "BLACK"; }));
    r.push_back(choice("CLOCK STYLE", &cfg.aodStyle, 4, [](int i) { static const char* A[4] = {"BIG", "SMALL", "ANALOG", "DOS"}; return A[i]; }));
    r.push_back(number("CLOCK LIGHT", &cfg.aodLight, 1, 30, 1, "%"));
    r.push_back(toggle("CLOCK: DATE", &cfg.aodDate));
    r.push_back(toggle("CLOCK: BATTERY", &cfg.aodBatt));
    r.push_back(toggle("CLOCK: MESSAGES", &cfg.aodNote));
    r.push_back(choice("WIFI WHEN OFF", &cfg.aodWifi, 3, [](int i) { static const char* W[3] = {"AS NORMAL", "SAVE POWER", "OFF"}; return W[i]; }));
    r.push_back(action("TURN OFF NOW", [] { uiSleep(); }));
    r.push_back(heading("WAKE UP", IC_HAND));
    r.push_back(toggle("TAP TO WAKE", &cfg.tapWake));
    r.push_back(toggle("LIFT TO WAKE", &cfg.moveWake));
    r.push_back(choice("BUTTONS WAKE", &cfg.btnWake, 2, [](int i) { return i ? "PWR ONLY" : "ANY BUTTON"; }));
  });
}
static Screen* pageLook() {
  return page("LOOK", "C:\\SETUP\\LOOK>", [](std::vector<Row>& r) {
    r.push_back(toggle("LOW GRAPHICS", &cfg.lowGfx));
    if (cfg.lowGfx) r.push_back(info("ON =", [](char* o) { strcpy(o, "NO 3D, GLASS, GLOW, GLITCH"); }));
    r.push_back(choice("MENU", &cfg.view, 2, [](int i) { return i ? "3D" : "DOS"; }));
    r.push_back(toggle("HOLO 3D", &cfg.holo, [] { holoRecenter(); }));
    r.push_back(choice("GLASS", &cfg.lens, 3, [](int i) { static const char* L[3] = {"FLAT", "CURVED", "FISHEYE"}; return L[i]; }));
    r.push_back(toggle("COLOUR FRINGE", &cfg.fringe));
    r.push_back(choice("ANIMATION", &cfg.anim, 3, [](int i) { static const char* A[3] = {"OFF", "SIMPLE", "3D"}; return A[i]; }));
    r.push_back(toggle("MOVES WITH YOU", &cfg.parallax));
    r.push_back(choice("COLOUR", &cfg.color, N_SCHEMES, [](int i) { return SCHEMES[i].name; }, [] { garbleUntil = msNow() + 160; }));
    r.push_back(choice("GLOW", &cfg.glow, 3, [](int i) { static const char* G[3] = {"OFF", "LOW", "HIGH"}; return G[i]; }));
    r.push_back(toggle("SCANLINES", &cfg.scan));
    r.push_back(choice("GLITCH", &cfg.glitch, 4, [](int i) { static const char* G[4] = {"OFF", "LOW", "MEDIUM", "HIGH"}; return G[i]; }));
    r.push_back(toggle("CRT ON/OFF", &cfg.crt));
    r.push_back(choice("BOOT", &cfg.boot, 3, [](int i) { static const char* B[3] = {"FULL", "SHORT", "OFF"}; return B[i]; }));
    r.push_back(toggle("3D INTRO", &cfg.intro));
  });
}
static Screen* pageSound() {
  return page("SOUND", "C:\\SETUP\\SOUND>", [](std::vector<Row>& r) {
    r.push_back(slider("VOLUME", &cfg.vol, 0));
    r.push_back(toggle("KEY CLICKS", &cfg.clicks));
    r.push_back(toggle("SOUNDS", &cfg.sounds));
    r.push_back(toggle("CHAT BEEP", &cfg.chatBeep));
    r.push_back(action("TEST", [] { snd(SND_BEEP); }));
  });
}
static Screen* pageTime() {
  return page("TIME", "C:\\SETUP\\TIME>", [](std::vector<Row>& r) {
    r.push_back(info("NOW", [](char* o) {
      struct tm t; if (sysLocalTime(t)) sprintf(o, "%02d-%02d-%04d %02d:%02d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900, t.tm_hour, t.tm_min); else strcpy(o, "NOT SET");
    }));
    r.push_back(toggle("FROM INTERNET", &cfg.ntp));
    r.push_back(choice("TIME ZONE", &cfg.tzIdx, N_TZ, [](int i) { return TIMEZONES[i].name; }));
    r.push_back(toggle("24 HOURS", &cfg.h24));
    r.push_back(toggle("SECONDS", &cfg.secs));
    r.push_back(section("ALARM SOUND", "song, snooze", IC_SOUND, pageAlarm));
    r.push_back(action("SET TIME", [] {
      struct tm t; char init[8] = "12:00"; if (sysLocalTime(t)) sprintf(init, "%02d:%02d", t.tm_hour, t.tm_min);
      kbOpen("TIME HH:MM", init, 5, [](const char* s) {
        int h, m; struct tm t;
        if (sscanf(s, "%d:%d", &h, &m) == 2 && h >= 0 && h < 24 && m >= 0 && m < 60) {
          if (!sysLocalTime(t)) { t.tm_year = 126; t.tm_mon = 0; t.tm_mday = 1; }
          sysSetTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, h, m, 0); cfg.ntp = 0; cfgSave(); toast("TIME SET");
        } else toast("LIKE 07:30");
      });
    }));
    r.push_back(action("SET DATE", [] {
      struct tm t; char init[12] = "01-01-2026"; if (sysLocalTime(t)) sprintf(init, "%02d-%02d-%04d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
      kbOpen("DATE DD-MM-YYYY", init, 10, [](const char* s) {
        int d, mo, y; struct tm t;
        if (sscanf(s, "%d-%d-%d", &d, &mo, &y) == 3 && d >= 1 && d <= 31 && mo >= 1 && mo <= 12 && y >= 2024) {
          int h = 12, m = 0; if (sysLocalTime(t)) { h = t.tm_hour; m = t.tm_min; }
          sysSetTime(y, mo, d, h, m, 0); cfg.ntp = 0; cfgSave(); toast("DATE SET");
        } else toast("LIKE 25-11-2026");
      });
    }));
  });
}
static Screen* pagePower() {
  return page("POWER", "C:\\SETUP\\POWER>", [](std::vector<Row>& r) {
    r.push_back(info("BATTERY", [](char* o) {
      if (!S.battery) strcpy(o, "NONE"); else sprintf(o, "%d%%  %.2fV", S.batPct, S.batMv / 1000.0f);
    }));
    r.push_back(info("CHARGING", [](char* o) { strcpy(o, S.charging ? "YES" : S.usb ? "FULL / NO" : "NO USB"); }));
    r.push_back(info("USB", [](char* o) { if (S.usb) sprintf(o, "%.2fV", S.vbusMv / 1000.0f); else strcpy(o, "UNPLUGGED"); }));
    r.push_back(choice("CHARGE CURRENT", &cfg.chgIdx, N_CHARGE, [](int i) { static char b[12]; sprintf(b, "%d MA", CHARGE_MA[i]); return (const char*)b; },
                       [] { sysChargeCurrent(CHARGE_MA[cfg.chgIdx]); }));
    r.push_back(choice("OFF WHEN ASLEEP", &cfg.autoOffIdx, N_AUTO_OFF, [](int i) {
      static char b[12]; int m = AUTO_OFF_MIN[i]; if (!m) return "NEVER"; if (m < 60) sprintf(b, "%d MIN", m); else sprintf(b, "%d H", m / 60); return (const char*)b; }));
    r.push_back(toggle("BATTERY SAVER", &cfg.saver));
    r.push_back(action("RESTART", [] { ask("RESTART", "Restart the cube now?", {"RESTART", "CANCEL"}, [](int i) { if (!i) sysRestart(); }); }));
    r.push_back(action("POWER OFF", [] { ask("POWER OFF", "Switch off? Press PWR to start it again.", {"OFF", "CANCEL"}, [](int i) { if (!i) sysPowerOff(); }); }));
  });
}
static Screen* pageCloud() {
  return page("ERROR.BLUE", "C:\\SETUP\\ERROR.BLUE>", [](std::vector<Row>& r) {
    r.push_back(action("YOUR NAME", [] { kbOpen("YOUR NAME", cfg.name, 16, [](const char* t) { snprintf(cfg.name, sizeof(cfg.name), "%s", t); cfgSave(); netChatReset(); }); },
                       [](char* o) { strcpy(o, cfg.name); }));
    r.push_back(action("CHAT CHANNEL", [] { openProg("CHAT"); }, [](char* o) { sprintf(o, "#%s", cfg.chan); }));
    r.push_back(toggle("SHOW ME ONLINE", &cfg.online));
    r.push_back(toggle("PHONE LINK", &cfg.link, [] { netLinkState(); }));
    r.push_back(action("LINK CODE", [] { openProg("LINK"); }, [](char* o) { strcpy(o, cfg.code); }));
    r.push_back(info("LINK", [](char* o) { strcpy(o, !cfg.link ? "OFF" : S.link ? "CONNECTED" : "WAITING"); }));
    r.push_back(choice("CERTIFICATES", &cfg.tls, 2, [](int i) { return i ? "CHECK" : "SKIP"; }));
  });
}
static Screen* pageSystem() {
  return page("SYSTEM", "C:\\SETUP\\SYSTEM>", [](std::vector<Row>& r) {
    r.push_back(info("FIRMWARE", [](char* o) { strcpy(o, "ERROR-CUBE " CUBE_VERSION); }));
    r.push_back(info("NAME", [](char* o) { strcpy(o, S.host); }));
    r.push_back(info("UP", [](char* o) { fmtUptime(o); }));
    r.push_back(choice("KEYBOARD", &cfg.kbd, 2, [](int i) { return i ? "UKRAINIAN" : "ENGLISH"; }));
    r.push_back(toggle("OFFLINE (AIRPLANE)", &cfg.offline, [] { netOffline(cfg.offline); }));
    r.push_back(action("ALL INFO", [] { openProg("INFO"); }));
    r.push_back(action("FACTORY RESET", [] {
      ask("FACTORY RESET", "Every setting back to the start, WiFi forgotten. Sure?", {"RESET", "CANCEL"}, [](int i) {
        if (!i) { cfgReset(); toast("RESET. RESTARTING..."); sysRestart(); }
      });
    }));
  });
}

// ------------------------------------------------------------------ HOLO 3D: the tilt effect
// HOLO TEST: a tunnel of frames from far behind to in front of the glass, a level bubble and the numbers
struct HoloTest : Screen {
  const char* name() override { return "HOLO TEST"; }
  void draw() override {
    pathLine("C:\\SETUP\\HOLO\\TEST>");
    // the tunnel: frames on every depth, far ones dim
    static const float HS[] = {-3.0f, -2.2f, -1.5f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 1.6f};
    for (int i = 0; i < 9; i++) {
      HoloZ z(HS[i]);
      int m = 40 + i * 12;
      uint16_t c = mix(C_BG, C_FG, 20 + i * 10);
      rect(m - 25, 96 + m / 3 - 10, 480 - 2 * (m - 25), 300 - 2 * (m / 3) + 20, c);
    }
    {
      HoloZ z(1.2f);
      dosButton(150, 190, 180, 60, "HOLO", [] { holoRecenter(); toast("CENTRED"); });
    }
    // the bubble: where the tilt is
    {
      HoloZ z(H_TEXT);
      int bx = 240 + (int)(holoTX() * 120), by = 330 + (int)(holoTY() * 40);
      hline(120, 330, 240, C_DIM); vline(240, 290, 80, C_DIM);
      fillCircle(bx, by, 9, C_FG);
      char t[64];
      snprintf(t, sizeof(t), "X %+4d  Y %+4d  %d FPS", (int)lroundf(holoTX() * 100), (int)lroundf(holoTY() * 100), holoFps());
      drawTextC(FT, 240, 376, t, C_FG);
      if (!S.imu) drawTextC(FT, 240, 260, "NO MOTION SENSOR", C_RED);
      if (!cfg.holo) drawTextC(FT, 240, 260, "HOLO 3D IS OFF", C_WARN);
    }
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {"C", "Centre", [] { holoRecenter(); toast("CENTRED"); }}});
    redrawIn(33);
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_KEY && e.key == K_OK) { holoRecenter(); toast("CENTRED"); return true; }
    return false;
  }
};

static void holoPreset(int p) {
  // SUBTLE, NORMAL, DEEP, EXTREME
  static const uint8_t D[4] = {4, 7, 11, 16}, POP[4] = {30, 50, 70, 100}, L[4] = {12, 26, 40, 60}, PE[4] = {15, 35, 55, 85};
  static const uint8_t W[4] = {1, 3, 4, 5}, R[4] = {35, 25, 20, 15}, RB[4] = {1, 2, 3, 4};
  cfg.holoDepth = D[p]; cfg.holoPop = POP[p]; cfg.holoLens = L[p]; cfg.holoPersp = PE[p];
  cfg.holoWalls = W[p]; cfg.holoRange = R[p]; cfg.holoRainbow = RB[p];
  cfg.holoShadow = p >= 2 ? 2 : 1; cfg.holoGlare = p >= 2 ? 2 : 1;
  cfg.holo = 1; cfg.holoLayers = 1;
}
static void holoDefaults() {
  Cfg d;
  cfg.holoLayers = d.holoLayers; cfg.holoDepth = d.holoDepth; cfg.holoFocus = d.holoFocus; cfg.holoPop = d.holoPop;
  cfg.holoShadow = d.holoShadow; cfg.holoWalls = d.holoWalls; cfg.holoBack = d.holoBack; cfg.holoLens = d.holoLens;
  cfg.holoPersp = d.holoPersp; cfg.holoGlare = d.holoGlare; cfg.holoRainbow = d.holoRainbow; cfg.holoRange = d.holoRange;
  cfg.holoDead = d.holoDead; cfg.holoSmooth = d.holoSmooth; cfg.holoCentre = d.holoCentre;
  cfg.holoInvX = d.holoInvX; cfg.holoInvY = d.holoInvY; cfg.holoSensor = d.holoSensor;
}
static uint8_t presetIdx = 1;
static Screen* pageHolo() {
  return page("HOLO 3D", "C:\\SETUP\\HOLO>", [](std::vector<Row>& r) {
    r.push_back(toggle("HOLO 3D", &cfg.holo, [] { holoRecenter(); }));
    { Row p; p.name = "PRESET"; p.kind = 1;
      p.val = [](char* o) { static const char* N[4] = {"SUBTLE", "NORMAL", "DEEP", "EXTREME"}; strcpy(o, N[presetIdx & 3]); };
      p.act = [](int d) { presetIdx = (presetIdx + (d < 0 ? 3 : 1)) & 3; holoPreset(presetIdx); cfgChanged(); };
      r.push_back(p); }
    r.push_back(action("CENTRE NOW", [] { holoRecenter(); toast("CENTRED"); }));
    r.push_back(action("TEST", [] { uiOpen(new HoloTest()); }));
    r.push_back(heading("DEPTH", IC_DEPTH));
    r.push_back(toggle("LAYERS", &cfg.holoLayers));
    r.push_back(number("DEPTH", &cfg.holoDepth, 0, 20, 1, " PX", "OFF"));
    r.push_back(choice("STAYS STILL", &cfg.holoFocus, 5, [](int i) { static const char* F[5] = {"BACK", "PANELS", "TEXT", "BARS", "FRONT"}; return F[i]; }));
    r.push_back(slider("POP-OUT", &cfg.holoPop, 0));
    r.push_back(choice("SHADOWS", &cfg.holoShadow, 3, [](int i) { static const char* S3[3] = {"OFF", "SOFT", "HARD"}; return S3[i]; }));
    r.push_back(number("SIDES", &cfg.holoWalls, 0, 5, 1, "", "OFF"));
    r.push_back(choice("WALLPAPER", &cfg.holoBack, 6, [](int i) { static const char* B[6] = {"GLASS", "DOTS", "GRID", "STARS", "ROOM", "PICTURE"}; return B[i]; }));
    r.push_back(heading("GLASS", IC_GLASS));
    r.push_back(choice("GLASS", &cfg.lens, 3, [](int i) { static const char* L[3] = {"FLAT", "CURVED", "FISHEYE"}; return L[i]; }));
    r.push_back(number("GLASS MOVES", &cfg.holoLens, 0, 60, 4, " PX", "OFF"));
    r.push_back(slider("PERSPECTIVE", &cfg.holoPersp, 0));
    r.push_back(choice("REFLECTION", &cfg.holoGlare, 3, [](int i) { static const char* G[3] = {"OFF", "SOFT", "BRIGHT"}; return G[i]; }));
    r.push_back(number("RAINBOW", &cfg.holoRainbow, 0, 4, 1, " PX", "OFF"));
    r.push_back(heading("MOTION", IC_MOTION));
    r.push_back(number("FULL TILT AT", &cfg.holoRange, 10, 60, 5, "\xC2\xB0"));
    r.push_back(number("DEAD ZONE", &cfg.holoDead, 0, 10, 1, "\xC2\xB0", "OFF"));
    r.push_back(slider("SMOOTHING", &cfg.holoSmooth, 0));
    r.push_back(choice("AUTO CENTRE", &cfg.holoCentre, 4, [](int i) { static const char* C4[4] = {"OFF", "SLOW", "MEDIUM", "FAST"}; return C4[i]; }));
    r.push_back(choice("SENSOR", &cfg.holoSensor, 2, [](int i) { return i ? "GRAVITY" : "GYRO+GRAVITY"; }));
    r.push_back(toggle("INVERT LEFT/RIGHT", &cfg.holoInvX));
    r.push_back(toggle("INVERT UP/DOWN", &cfg.holoInvY));
    r.push_back(action("RESET HOLO", [] {
      ask("RESET HOLO", "All HOLO 3D settings back to normal?", {"RESET", "CANCEL"}, [](int i) { if (i == 0) { holoDefaults(); presetIdx = 1; cfgChanged(); toast("HOLO RESET"); } });
    }));
  });
}

// ------------------------------------------------------------------ BUTTONS: what - + PWR do
struct BtnTest : Screen {
  const char* name() override { return "BUTTON TEST"; }
  void draw() override {
    pathLine("C:\\SETUP\\BUTTONS\\TEST>");
    drawTextC(FT, 240, 110, "Press, double press, hold", C_DIM, TX_FLAT);
    drawTextC(FT, 240, 142, "or press two together.", C_DIM, TX_FLAT);
    dosBox(30, 190, 420, 110, C_FG);
    char a[48]; snprintf(a, sizeof(a), "%s", btnLast[0] ? btnLast : "...");
    char* c = strchr(a, ':');
    if (c) { *c = 0; drawTextC(FB, 240, 206, a, C_FG); drawTextC(FT, 240, 252, c + 2, C_WARN); }
    else drawTextC(FB, 240, 222, a, C_FG);
    drawTextC(FT, 240, 330, "(the actions still happen)", C_DIM, TX_FLAT);
    fkeysBack();
  }
};
static const char* actLabel(int i) { return btnName(i); }
static Row actRow(const char* n, uint8_t* v) { return choice(n, v, BA_COUNT, [](int i) { return actLabel(i); }); }
static Screen* pageButtons() {
  return page("BUTTONS", "C:\\SETUP\\BUTTONS>", [](std::vector<Row>& r) {
    r.push_back(action("TEST", [] { btnLast[0] = 0; uiOpen(new BtnTest()); }));
    static const char* BN[3] = {"- BUTTON (LEFT)", "+ BUTTON (RIGHT)", "PWR BUTTON"};
    for (int b = 0; b < 3; b++) {
      r.push_back(heading(BN[b], IC_KEY));
      r.push_back(actRow("PRESS", &cfg.btnAct[b][0]));
      r.push_back(actRow("DOUBLE PRESS", &cfg.btnAct[b][1]));
      r.push_back(actRow("HOLD", &cfg.btnAct[b][2]));
    }
    r.push_back(heading("TOGETHER", IC_HAND));
    r.push_back(actRow("- AND +", &cfg.btnCombo[0]));
    r.push_back(actRow("HOLD - AND +", &cfg.btnCombo[1]));
    r.push_back(actRow("HOLD -, PWR", &cfg.btnCombo[2]));
    r.push_back(actRow("HOLD +, PWR", &cfg.btnCombo[3]));
    r.push_back(heading("TIMING", IC_TIME));
    { Row t; t.name = "HOLD AFTER"; t.kind = 1;
      t.val = [](char* o) { sprintf(o, "%d MS", cfg.btnHold * 50); };
      t.act = [](int d) { int v = cfg.btnHold + (d < 0 ? -2 : 2); if (v > 40) v = 6; if (v < 6) v = 40; cfg.btnHold = v; cfgChanged(); };
      r.push_back(t); }
    { Row t; t.name = "DOUBLE PRESS GAP"; t.kind = 1;
      t.val = [](char* o) { if (!cfg.btnDouble) strcpy(o, "OFF"); else sprintf(o, "%d MS", cfg.btnDouble * 10); };
      t.act = [](int d) { static const uint8_t G[7] = {0, 15, 20, 25, 30, 40, 50}; int k = 0; for (int i = 0; i < 7; i++) if (G[i] == cfg.btnDouble) k = i;
                          k = (k + (d < 0 ? 6 : 1)) % 7; cfg.btnDouble = G[k]; cfgChanged(); };
      r.push_back(t); }
    { Row t; t.name = "REPEAT WHEN HELD"; t.kind = 1;
      t.val = [](char* o) { if (!cfg.btnRepeat) strcpy(o, "OFF"); else sprintf(o, "%d MS", cfg.btnRepeat * 10); };
      t.act = [](int d) { static const uint8_t G[6] = {0, 8, 12, 18, 25, 40}; int k = 0; for (int i = 0; i < 6; i++) if (G[i] == cfg.btnRepeat) k = i;
                          k = (k + (d < 0 ? 5 : 1)) % 6; cfg.btnRepeat = G[k]; cfgChanged(); };
      r.push_back(t); }
    r.push_back(choice("PWR HOLD AFTER", &cfg.pwrHold, 4, [](int i) { static const char* H[4] = {"1 S", "1.5 S", "2 S", "2.5 S"}; return H[i]; }));
    r.push_back(choice("PWR FORCE OFF", &cfg.pwrOff, 4, [](int i) { static const char* H[4] = {"4 S", "6 S", "8 S", "10 S"}; return H[i]; }));
    r.push_back(heading("OTHER", IC_INFO));
    r.push_back(toggle("SWAP - AND +", &cfg.btnSwap));
    r.push_back(choice("WAKE WITH", &cfg.btnWake, 2, [](int i) { return i ? "PWR ONLY" : "ANY BUTTON"; }));
    r.push_back(action("RESET BUTTONS", [] {
      ask("RESET BUTTONS", "Buttons back to how they were?", {"RESET", "CANCEL"}, [](int i) {
        if (i) return;
        Cfg d;
        memcpy(cfg.btnAct, d.btnAct, sizeof(cfg.btnAct)); memcpy(cfg.btnCombo, d.btnCombo, sizeof(cfg.btnCombo));
        cfg.btnHold = d.btnHold; cfg.btnDouble = d.btnDouble; cfg.btnRepeat = d.btnRepeat; cfg.pwrHold = d.pwrHold;
        cfg.pwrOff = d.pwrOff; cfg.btnSwap = d.btnSwap; cfg.btnWake = d.btnWake;
        cfgChanged(); toast("BUTTONS RESET");
      });
    }));
  });
}

// ------------------------------------------------------------------ SETUP.EXE: the sections
static Screen* pageUpdates() {
  return page("UPDATES", "C:\\SETUP\\UPDATES>", [](std::vector<Row>& r) {
    r.push_back(heading("FIRMWARE", IC_UPDATE));
    r.push_back(info("THIS CUBE", [](char* o) { strcpy(o, CUBE_VERSION); }));
    r.push_back(info("ON ERROR.BLUE", [](char* o) { strcpy(o, S.updVer[0] ? S.updVer : "?"); }));
    r.push_back(info("STATUS", [](char* o) {
      static const char* W[] = {"NOT CHECKED", "CHECKING...", "NEW ONE READY", "INSTALLING...", "DONE", "FAILED", "UP TO DATE"};
      strcpy(o, cfg.offline ? "OFFLINE" : W[S.upd % 7]);
    }));
    if (S.upd == UPD_NEW) r.push_back(action("INSTALL NOW", [] { netUpdateInstall(); openProg("UPDATE"); }));
    r.push_back(action("CHECK NOW", [] { if (cfg.offline) toast("OFFLINE"); else { netUpdateCheck(); toast("CHECKING...", 1200); } }));
    r.push_back(heading("WHEN THERE'S A NEW ONE", IC_INFO));
    r.push_back(choice("INSTALL", &cfg.autoUpd, 2, [](int i) { return i ? "BY ITSELF" : "ASK ME"; }));
    r.push_back(toggle("MESSAGE WHEN NEW", &cfg.updAsk));
    r.push_back(choice("CHECK EVERY", &cfg.updIdx, N_UPD, [](int i) { static char b[8]; snprintf(b, sizeof(b), "%d H", UPD_HOURS[i]); return (const char*)b; }));
    r.push_back(info("BY ITSELF =", [](char* o) { strcpy(o, "WHEN THE SCREEN IS OFF"); }));
    r.push_back(action("OPEN UPDATE.EXE", [] { openProg("UPDATE"); }));
  });
}
static Screen* pageP8() {
  return page("PICO-8", "C:\\SETUP\\PICO-8>", [](std::vector<Row>& r) {
    r.push_back(heading("TILT = THE ARROWS", IC_HOLO));
    r.push_back(action("CALIBRATE", [] { ask("CALIBRATE", "Hold the cube the way you want to play, then OK.", {"OK", "CANCEL"}, [](int i) { if (!i) p8Calibrate(); }); },
                       [](char* o) { strcpy(o, cfg.p8CalOk ? "DONE" : "NOT YET"); }));
    r.push_back(number("TILT BEFORE IT MOVES", &cfg.p8Dead, 4, 30, 2, "\xC2\xB0"));
    r.push_back(toggle("INVERT LEFT/RIGHT", &cfg.p8InvX));
    r.push_back(toggle("INVERT UP/DOWN", &cfg.p8InvY));
    r.push_back(info("NOT CALIBRATED =", [](char* o) { strcpy(o, "HOW YOU HOLD IT AT START"); }));
    r.push_back(heading("BUTTONS", IC_KEY));
    r.push_back(choice("- AND +", &cfg.p8Swap, 2, [](int i) { return i ? "X AND O" : "O AND X"; }));
    r.push_back(info("PWR", [](char* o) { strcpy(o, "MENU / QUIT"); }));
    r.push_back(action("TEST IT", [] { p8Open("/c/pico8/tilttest.p8"); }));
  });
}
Screen* makeP8Setup() { return pageP8(); }
Screen* makeSongPicker();
static Screen* pageAlarm() {
  return page("ALARM", "C:\\CLOCK\\ALARM>", [](std::vector<Row>& r) {
    r.push_back(heading("WHAT IT PLAYS", IC_SOUND));
    r.push_back(action("SONG", [] { uiOpen(makeSongPicker()); }, [](char* o) {
      const char* n = strrchr(cfg.alarmSong, '/');
      snprintf(o, 40, "%s", cfg.alarmSong[0] ? (n ? n + 1 : cfg.alarmSong) : "BEEPS (PICK ONE)");
    }));
    if (cfg.alarmSong[0]) r.push_back(action("BEEPS INSTEAD", [] { cfg.alarmSong[0] = 0; cfgChanged(); }));
    r.push_back(toggle("FULL VOLUME", &cfg.alarmLoud));
    r.push_back(heading("SNOOZE", IC_TIME));
    r.push_back(toggle("MOVE THE CUBE = SNOOZE", &cfg.alarmMove));
    r.push_back(number("SNOOZE FOR", &cfg.alarmSnooze, 1, 30, 1, " MIN"));
    r.push_back(action("TEST IT", [] { alarmRing("ALARM"); }));
  });
}
Screen* makeAlarmSetup() { return pageAlarm(); }
static Row section(const char* n, const char* desc, uint8_t ic, Screen* (*open)()) {
  Row r; r.name = n; r.kind = 0; r.icon = ic;
  r.val = [desc](char* o) { strcpy(o, desc); };
  r.act = [open](int) { Screen* s = open(); if (s) uiOpen(s); };
  return r;
}
Screen* makeSetup() {
  Page* p = (Page*)page("SETUP.EXE", "C:\\SETUP>", [](std::vector<Row>& r) {
    r.push_back(heading("SCREEN", IC_SCREEN));
    { Row o = toggle("LOW GRAPHICS", &cfg.lowGfx); o.icon = IC_LOOK; r.push_back(o); }
    r.push_back(section("SCREEN", "light, wallpaper", IC_LIGHT, pageDisplay));
    r.push_back(section("LOOK", "colour, glitch", IC_LOOK, pageLook));
    r.push_back(section("HOLO 3D", "tilt, depth", IC_HOLO, pageHolo));
    r.push_back(heading("CONTROLS", IC_KEY));
    r.push_back(section("SOUND", "volume, clicks", IC_SOUND, pageSound));
    r.push_back(section("BUTTONS", "hold, combos", IC_KEY, pageButtons));
    r.push_back(section("PICO-8", "tilt, O / X", IC_KEY, pageP8));
    r.push_back(heading("CONNECT", IC_WIFI));
    r.push_back(section("WIFI", "networks", IC_WIFI, [] { return findProg("WIFI"); }));
    { Row o = toggle("OFFLINE", &cfg.offline, [] { netOffline(cfg.offline); }); o.icon = IC_OFFLINE; r.push_back(o); }
    r.push_back(section("ERROR.BLUE", "name, link", IC_CLOUD, pageCloud));
    r.push_back(heading("DEVICE", IC_SYSTEM));
    r.push_back(section("TIME", "zone, alarm", IC_TIME, pageTime));
    r.push_back(section("POWER", "battery, saver", IC_POWER, pagePower));
    r.push_back(section("UPDATES", "firmware", IC_UPDATE, pageUpdates));
    r.push_back(section("SYSTEM", "info, reset", IC_SYSTEM, pageSystem));
  });
  return p;
}


// ------------------------------------------------------------------ WIFI.EXE
struct Wifi : Screen {
  WifiNet nets[16]; int nn = -1; char saved[6][33]; int ns = 0;
  std::vector<Row> rows; ListView L;
  const char* name() override { return "WIFI.EXE"; }
  void enter() override { L.setBox(15, 92, 450, 330); L.rowH = 55; if (cfg.wifiOn) netScan(); }
  void join(const char* ssid, bool open, bool known) {
    std::string s = ssid;
    if (open || known) { netJoin(ssid, nullptr); toast("JOINING..."); return; }
    kbOpen("PASSWORD", "", 63, [s](const char* p) { netJoin(s.c_str(), p); toast("JOINING..."); }, true);
  }
  void build() {
    rows.clear();
    rows.push_back(toggle("WIFI", &cfg.wifiOn, [] { netWifi(cfg.wifiOn); }));
    rows.push_back(info("STATUS", [](char* o) {
      static const char* N[5] = {"OFF", "JOINING", "CONNECTED", "FAILED", "NONE SAVED"};
      snprintf(o, 40, "%s", N[S.net % 5]);
    }));
    if (S.net == NET_UP) {
      rows.push_back(info("NETWORK", [](char* o) { snprintf(o, 40, "%s", S.ssid); utf8Cut(o, 16); }));
      rows.push_back(info("ADDRESS", [](char* o) { snprintf(o, 40, "%s", S.ip); }));
      rows.push_back(info("SIGNAL", [](char* o) { sprintf(o, "%d dBm", S.rssi); }));
    }
    if (!cfg.wifiOn) return;
    rows.push_back(heading("NETWORKS"));
    if (nn < 0) rows.push_back(info("SCANNING...", nullptr));
    for (int i = 0; i < nn; i++) {
      WifiNet* w = &nets[i];
      Row r; r.name = w->ssid; r.kind = 0;
      r.val = [w](char* o) { sprintf(o, "%s%s %s", w->saved ? "*" : "", w->open ? "" : "\xE2\x96\xA0", w->rssi > -60 ? "####" : w->rssi > -70 ? "###" : w->rssi > -80 ? "##" : "#"); };
      r.act = [this, w](int) { join(w->ssid, w->open, w->saved); };
      rows.push_back(r);
    }
    rows.push_back(action("SCAN AGAIN", [this] { nn = -1; netScan(); }));
    if (ns) rows.push_back(heading("SAVED"));
    for (int i = 0; i < ns; i++) {
      char* s = saved[i];
      Row r; r.name = s; r.kind = 0; r.val = [](char* o) { strcpy(o, "..."); };
      r.act = [s](int) {
        std::string ss = s;
        ask("SAVED", s, {"JOIN", "FORGET", "CANCEL"}, [ss](int k) { if (k == 0) netJoin(ss.c_str(), nullptr); else if (k == 1) { netForget(ss.c_str()); toast("FORGOTTEN"); } });
      };
      rows.push_back(r);
    }
    rows.push_back(heading("FROM A PHONE"));
    rows.push_back(toggle("PHONE SETUP", (uint8_t*)&S.portal, [] { netPortal(S.portal); }));
    if (S.portal) {
      rows.push_back(info("1. JOIN WIFI", [](char* o) { strcpy(o, S.portalName); }));
      rows.push_back(info("2. OPEN", [](char* o) { strcpy(o, "192.168.4.1"); }));
    } else if (S.net == NET_UP) rows.push_back(info("OR OPEN", [](char* o) { sprintf(o, "%s", S.ip); }));
  }
  void tick() override {
    if (nn < 0) { int k = netScanResults(nets, 16); if (k >= 0) { nn = k; uiDirty(); } }
    ns = netSaved(saved, 6);
    L.tick();
    redrawIn(1000);
  }
  void draw() override {
    build();
    pathLine("C:\\SYSTEM\\WIFI>");
    drawRows(L, rows);
    fkeysBack();
  }
  bool ev(const Ev& e) override { build(); return rowsEv(L, rows, e); }
};
Screen* makeWifi() { return new Wifi(); }

// ------------------------------------------------------------------ UPDATE.EXE
struct Update : Screen {
  const char* name() override { return "UPDATE.EXE"; }
  void enter() override { if (S.upd == UPD_IDLE || S.upd == UPD_LATEST || S.upd == UPD_FAIL) netUpdateCheck(); }
  bool keepAwake() override { return S.upd == UPD_LOADING; }
  void draw() override {
    pathLine("C:\\SYSTEM\\UPDATE>");
    dosBox(15, 96, 450, 196, C_FG);
    drawText(FT, 36, 112, "THIS CUBE", C_DIM, TX_FLAT); drawTextR(FT, 444, 112, CUBE_VERSION, C_FG);
    drawText(FT, 36, 148, "ON ERROR.BLUE", C_DIM, TX_FLAT); drawTextR(FT, 444, 148, S.updVer[0] ? S.updVer : "?", C_FG);
    const char* st = "";
    switch (S.upd) {
      case UPD_IDLE: st = "NOT CHECKED YET"; break;
      case UPD_CHECKING: st = "CHECKING..."; break;
      case UPD_NEW: st = "NEW FIRMWARE!"; break;
      case UPD_LOADING: st = "INSTALLING..."; break;
      case UPD_DONE: st = "DONE. RESTARTING"; break;
      case UPD_FAIL: st = S.updMsg[0] ? S.updMsg : "FAILED"; break;
      case UPD_LATEST: st = "UP TO DATE"; break;
    }
    drawTextC(FT, 240, 196, st, S.upd == UPD_FAIL ? C_RED : S.upd == UPD_NEW ? C_WARN : C_FG);
    if (S.upd == UPD_LOADING) { meter(40, 236, 400, 36, S.updPct, true); redrawIn(200); }
    else if (S.updNotes[0]) { char l[2][64]; int n = wrapText(S.updNotes, 26, l, 2); for (int i = 0; i < n; i++) drawTextC(FT, 240, 228 + i * 28, l[i], C_DIM, TX_FLAT); }
    if (S.upd == UPD_NEW) dosButton(30, 304, 205, 56, "INSTALL", [] { netUpdateInstall(); }, true);
    else dosButton(30, 304, 205, 56, "CHECK", [] { netUpdateCheck(); });
    dosButton(245, 304, 205, 56, cfg.autoUpd ? "BY ITSELF" : "ASK ME", [] {
      cfg.autoUpd = !cfg.autoUpd; cfgSave();
      toast(cfg.autoUpd ? "INSTALLS BY ITSELF (SCREEN OFF)" : "ASKS YOU FIRST", 2200);
    }, cfg.autoUpd);
    char up[48];
    if (S.net == NET_UP) snprintf(up, sizeof(up), "WEB PAGE: %s", S.ip); else strcpy(up, "OR A .BIN ON THE TF CARD");
    drawTextC(FT, 240, 372, up, C_DIM, TX_FLAT);
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {"A:", "From card", [] { openProg("FILES"); }}});
    if (S.upd == UPD_CHECKING) redrawIn(300);
  }
};
Screen* makeUpdate() { return new Update(); }

// ------------------------------------------------------------------ INFO.EXE
struct Info : Screen {
  int tab = 0; int tx = -1, ty = -1;
  const char* name() override { return "INFO.EXE"; }
  bool keepAwake() override { return tab == 2; }
  void line(int i, const char* k, const char* v) { drawText(FT, 27, 104 + i * 34, k, C_DIM, TX_FLAT); drawTextR(FT, 453, 104 + i * 34, v, C_FG); }
  void draw() override {
    static const char* T[4] = {"SYSTEM", "POWER", "SENSORS", "NET"};
    int w = 108;
    for (int i = 0; i < 4; i++) {
      int x = 15 + i * (w + 6);
      if (i == tab) invBar(x, 56, w, 38); else rect(x, 56, w, 38, C_DIM);
      drawTextC(FT, x + w / 2, 59, T[i], i == tab ? C_BG : C_DIM, TX_FLAT);
      hit(x, 50, w, 48, [this, i] { tab = i; sndKey(); uiDirty(); }, nullptr, false);
    }
    char b[48];
    if (tab == 0) {
      line(0, "FIRMWARE", "ERROR-CUBE " CUBE_VERSION);
      line(1, "CHIP", "ESP32-S3 240MHz x2");
      sprintf(b, "%uK FREE", (unsigned)(freeHeap() / 1024)); line(2, "RAM", b);
      sprintf(b, "%uK FREE", (unsigned)(freePsram() / 1024)); line(3, "PSRAM", b);
      line(4, "FLASH", "16 MB");
      fmtUptime(b); line(5, "UP", b);
      sprintf(b, "%.1f C", S.chipTemp); line(6, "CHIP TEMP", b);
      line(7, "LAST START", resetReason());
      if (S.crash[0]) {          // what crashed last (for the person fixing it: task, PC, backtrace)
        char c[48]; snprintf(c, sizeof(c), "%s", S.crash); utf8Cut(c, 38);
        drawText(FT, 27, 104 + 8 * 34, c, C_WARN, TX_FLAT);
      } else line(8, "BUILT", __DATE__);
    } else if (tab == 1) {
      line(0, "POWER CHIP", S.pmu ? "AXP2101" : "NOT FOUND");
      if (S.battery) sprintf(b, "%d%%", S.batPct); else strcpy(b, "NONE"); line(1, "BATTERY", b);
      sprintf(b, "%d mV", S.batMv); line(2, "VOLTS", b);
      line(3, "CHARGING", S.charging ? "YES" : "NO");
      if (S.usb) sprintf(b, "%d mV", S.vbusMv); else strcpy(b, "NO"); line(4, "USB", b);
      sprintf(b, "%.1f C", S.pmuTemp); line(5, "CHIP TEMP", b);
      sprintf(b, "%d MA", CHARGE_MA[cfg.chgIdx]); line(6, "CHARGE AT", b);
    } else if (tab == 2) {
      float a[3] = {0, 0, 0}, g[3] = {0, 0, 0};
      bool ok = imuRead(a, g);
      sprintf(b, "%+.1f %+.1f %+.1f", a[0], a[1], a[2]); line(0, "ACCEL m/s2", ok ? b : "-");
      sprintf(b, "%+.0f %+.0f %+.0f", g[0], g[1], g[2]); line(1, "GYRO deg/s", ok ? b : "-");
      struct tm t; if (sysLocalTime(t)) sprintf(b, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec); else strcpy(b, "-");
      line(2, S.rtc ? "RTC PCF85063" : "CLOCK", b);
      line(3, "TOUCH", S.touch ? "CST9220 OK" : "NOT FOUND");
      line(4, "TF CARD", S.sd ? S.sdInfo : "NONE");
      line(5, "SOUND", S.codec ? "ES8311 OK" : "NOT FOUND");
      drawTextC(FT, 240, 104 + 6 * 34 + 4, "TOUCH TEST: DRAW BELOW", C_DIM, TX_FLAT);
      rect(27, 350, 426, 70, C_DIM);
      if (tx >= 0) { vline(tx, 350, 70, C_FG); fillCircle(tx, ty, 8, C_FG); }
      redrawIn(150);
    } else {
      static const char* N[5] = {"OFF", "JOINING", "CONNECTED", "FAILED", "NONE SAVED"};
      line(0, "WIFI", N[S.net % 5]);
      line(1, "NETWORK", S.ssid[0] ? S.ssid : "-");
      line(2, "ADDRESS", S.ip[0] ? S.ip : "-");
      sprintf(b, "%d dBm", S.rssi); line(3, "SIGNAL", S.net == NET_UP ? b : "-");
      line(4, "MAC", S.mac);
      line(5, "NAME", S.host);
      line(6, "TIME", S.timeOk ? "NTP OK" : "NOT FROM NET");
      sprintf(b, "%u KB FREE", (unsigned)(freeHeap() / 1024)); line(7, "FAST RAM", b);
      line(8, "PHONE LINK", S.link ? "LINKED" : cfg.link ? "NOT LINKED" : "OFF");
      // the last error, on its own line (long)
      const char* er = !S.link && S.linkErr[0] ? S.linkErr : S.netErr;
      if (er[0]) { char e[48]; snprintf(e, sizeof(e), "%s", er); utf8Cut(e, 38); drawText(FT, 27, 104 + 9 * 34 - 4, e, C_WARN, TX_FLAT); }
    }
    fkeysBack();
    redrawIn(1000);
  }
  bool ev(const Ev& e) override {
    if (tab == 2 && (e.t == EV_DOWN || e.t == EV_MOVE) && e.y > 340) { tx = e.x; ty = e.y < 350 ? 350 : e.y > 420 ? 420 : e.y; uiDirty(); return true; }
    if (e.t == EV_SWIPE && e.dir == SW_LEFT) { tab = (tab + 1) % 4; uiDirty(); return true; }
    if (e.t == EV_SWIPE && e.dir == SW_RIGHT && tab) { tab--; uiDirty(); return true; }
    if (e.t == EV_KEY && e.key == K_NEXT) { tab = (tab + 1) % 4; uiDirty(); return true; }
    if (e.t == EV_KEY && e.key == K_PREV) { tab = (tab + 3) % 4; uiDirty(); return true; }
    return false;
  }
};
Screen* makeInfo() { return new Info(); }

// ------------------------------------------------------------------ COMMAND.COM: the C:\> prompt
struct Command : Screen {
  std::vector<std::string> out;
  const char* name() override { return "COMMAND.COM"; }
  void enter() override { if (out.empty()) { print("ERROR-DOS " CUBE_VERSION " on ERROR-CUBE"); print("Type HELP for the commands."); print(""); } }
  void print(const char* s) {
    char l[12][64]; int n = wrapText(s, 30, l, 12);
    if (!n) out.push_back("");
    for (int i = 0; i < n; i++) out.push_back(l[i]);
    while (out.size() > 60) out.erase(out.begin());
  }
  void run(const char* line) {
    char b[160]; snprintf(b, sizeof(b), "C:\\>%s", line); print(b);
    char cmd[24] = "", arg[128] = "";
    sscanf(line, "%23s %127[^\n]", cmd, arg);
    for (char* p = cmd; *p; p++) *p = toupper((unsigned char)*p);
    if (!cmd[0]) return;
    if (!strcmp(cmd, "HELP") || !strcmp(cmd, "?")) {
      print("VER MEM IP TIME CLS BEEP");
      print("BRIGHT n  VOL n  COLOR n");
      print("WIFI ON|OFF  SAY text");
      print("UPDATE  REBOOT  OFF  EXIT");
      print("or a program: CLOCK, WHO...");
    } else if (!strcmp(cmd, "VER")) print("ERROR-CUBE " CUBE_VERSION " (" __DATE__ ")");
    else if (!strcmp(cmd, "MEM")) { sprintf(b, "%uK RAM + %uK PSRAM free", (unsigned)(freeHeap() / 1024), (unsigned)(freePsram() / 1024)); print(b); }
    else if (!strcmp(cmd, "IP")) { sprintf(b, "%s %s %ddBm", S.ssid, S.ip, S.rssi); print(S.net == NET_UP ? b : "No network"); }
    else if (!strcmp(cmd, "TIME") || !strcmp(cmd, "DATE")) {
      struct tm t; if (sysLocalTime(t)) { sprintf(b, "%02d-%02d-%04d %02d:%02d:%02d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900, t.tm_hour, t.tm_min, t.tm_sec); print(b); } else print("The clock is not set");
    }
    else if (!strcmp(cmd, "CLS")) out.clear();
    else if (!strcmp(cmd, "BEEP")) snd(SND_BEEP);
    else if (!strcmp(cmd, "BRIGHT")) { int v = atoi(arg); if (v >= 5 && v <= 100) { cfg.bright = v; cfgChanged(); } sprintf(b, "Brightness %d%%", cfg.bright); print(b); }
    else if (!strcmp(cmd, "VOL")) { int v = atoi(arg); if (v >= 0 && v <= 100 && arg[0]) { cfg.vol = v; cfgSave(); } sprintf(b, "Volume %d%%", cfg.vol); print(b); }
    else if (!strcmp(cmd, "COLOR") || !strcmp(cmd, "COLOUR")) {
      int v = atoi(arg); if (arg[0] && v >= 0 && v < N_SCHEMES) { cfg.color = v; cfgChanged(); }
      sprintf(b, "Colour %d = %s (0-%d)", cfg.color, SCHEMES[cfg.color].name, N_SCHEMES - 1); print(b);
    }
    else if (!strcmp(cmd, "WIFI")) { if (!strcasecmp(arg, "ON")) { cfg.wifiOn = 1; netWifi(true); } else if (!strcasecmp(arg, "OFF")) { cfg.wifiOn = 0; netWifi(false); } cfgSave(); print(cfg.wifiOn ? "WiFi on" : "WiFi off"); }
    else if (!strcmp(cmd, "SAY")) { if (arg[0]) { netChatSay(arg); sprintf(b, "Said in #%s", cfg.chan); print(b); } }
    else if (!strcmp(cmd, "UPDATE")) { netUpdateCheck(); print("Checking error.blue..."); }
    else if (!strcmp(cmd, "REBOOT")) { print("Restarting..."); sysRestart(); }
    else if (!strcmp(cmd, "OFF")) sysPowerOff();
    else if (!strcmp(cmd, "EXIT")) uiBack();
    else {
      char* dot = strchr(cmd, '.'); if (dot) *dot = 0;
      if (findProg(cmd)) { Screen* s = findProg(cmd); delete s; openProg(cmd); }
      else print("Bad command or file name");
    }
  }
  void draw() override {
    int rows = 11, n = out.size(), start = n > rows ? n - rows : 0;
    for (int i = start; i < n; i++) drawText(FT, colX(0), TOP_Y + (i - start) * LH, out[i].c_str(), C_FG);
    int y = TOP_Y + (n - start) * LH;
    drawText(FT, colX(0), y, "C:\\>", C_FG);
    if ((msNow() / 530) % 2 == 0) fillRect(colX(4), y + 4, CW, LH - 6, C_FG);
    redrawIn(530 - msNow() % 530);
    hit(15, 56, 450, 370, [this] { kbOpen("C:\\>", "", 120, [this](const char* t) { run(t); }); }, nullptr, false);
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {"\xE2\x86\xB5", "Type", [this] { kbOpen("C:\\>", "", 120, [this](const char* t) { run(t); }); }}});
  }
};
Screen* makeCommand() { return new Command(); }
