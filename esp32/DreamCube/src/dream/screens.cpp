// screens.cpp: DREAM-CUBE's screens. Not connected yet: WIFI first (the networks it finds: pick one, type the password);
// only when it's connected does the rest start, in the DREAM view:
//   the scene, filling the screen; its title at the top, the words it heard at the bottom, a dot for how it listens.
//   Drag: turn the camera. Tap: the menu (LISTEN / PAUSE, TYPE, SCENES, SETUP). Buttons: - older scene, + newer.
//   SCENES  the 40 last ones (tap: show it again)      LINK  the QR code for the control page at error.blue/dream
//   SETUP   how often, thinking, what's on screen, camera, listening, WiFi, sound + screen, firmware
// Uses ERROR-CUBE's interface (src/ui), board and drawing as they are, in ERROR-CUBE's colours.
#include "../ui/ui.h"
#include "../lib/cubeqr.h"
#include "../core/g3d.h"
#include "dream.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <vector>

// ------------------------------------------------------------------ the tables behind the settings (ui.cpp, board)
const uint16_t OFF_SECS[] = {15, 30, 60, 120, 300, 600, 1800, 0};
const int N_OFF = 8;
const uint16_t CHARGE_MA[] = {100, 200, 300, 400, 500, 700, 1000};
const int N_CHARGE = 7;
const uint16_t AUTO_OFF_MIN[] = {0, 10, 30, 60, 180};
const int N_AUTO_OFF = 5;
const uint8_t UPD_HOURS[] = {1, 6, 24};
const int N_UPD = 3;
const Tz TIMEZONES[] = {
  {"AMSTERDAM/BERLIN", "CET-1CEST,M3.5.0,M10.5.0/3"}, {"LONDON/DUBLIN", "GMT0IST,M3.5.0/1,M10.5.0"}, {"LISBON", "WET0WEST,M3.5.0/1,M10.5.0"},
  {"KYIV/HELSINKI", "EET-2EEST,M3.5.0/3,M10.5.0/4"}, {"ISTANBUL", "<+03>-3"}, {"MOSCOW", "MSK-3"}, {"DUBAI", "<+04>-4"}, {"INDIA", "IST-5:30"},
  {"BANGKOK", "<+07>-7"}, {"CHINA/SINGAPORE", "CST-8"}, {"TOKYO/SEOUL", "JST-9"}, {"SYDNEY", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
  {"AUCKLAND", "NZST-12NZDT,M9.5.0,M4.1.0/3"}, {"UTC", "UTC0"}, {"NEW YORK", "EST5EDT,M3.2.0,M11.1.0"}, {"CHICAGO", "CST6CDT,M3.2.0,M11.1.0"},
  {"DENVER", "MST7MDT,M3.2.0,M11.1.0"}, {"LOS ANGELES", "PST8PDT,M3.2.0,M11.1.0"}, {"SAO PAULO", "<-03>3"},
};
const int N_TZ = sizeof(TIMEZONES) / sizeof(TIMEZONES[0]);
std::vector<Prog> PROGS;

void cfgChanged() {
  gfxTheme(THEME_COLOR, lowFx() ? 0 : cfg.glow);
  sysBrightness(uiBrightNow());
  sysApplyRotation();
  sysApplyButtons();
  cfgSave();
  uiDirty();
}
static const float CAM_SPEEDS[4] = {0, 0.45f, 1, 2};
// too slow (more than ~11 frames a second lost): the place's own trees and houses go until the next scene
static bool autoLess = false;
static Col fgCol() { return col((C_FG >> 11) / 31.0f, ((C_FG >> 5) & 63) / 63.0f, (C_FG & 31) / 31.0f); }
static void applyLook() {
  sceneSoft(dcfg.soft && !autoLess); sceneSpeed(CAM_SPEEDS[dcfg.cam & 3]); sceneDecor(dcfg.detail && !autoLess);
  sceneFx(dcfg.fxOn); r3dLook(dcfg.colors % 3, fgCol());
}

// ------------------------------------------------------------------ settings rows (as in ERROR-CUBE's SETUP)
static Row heading(const char* n, uint8_t ic = 0) { Row r; r.name = n; r.kind = 4; r.icon = ic; return r; }
static Row info(const char* n, std::function<void(char*)> v) { Row r; r.name = n; r.val = v; r.kind = 3; return r; }
static Row action(const char* n, std::function<void()> f, std::function<void(char*)> v = nullptr) {
  Row r; r.name = n; r.val = v; r.kind = 0; r.act = [f](int) { f(); }; return r;
}
// a choice in ERROR-CUBE's settings (cfg) or DREAM-CUBE's own (dcfg)
static Row choice(const char* n, uint8_t* v, int count, std::function<const char*(int)> label, std::function<void()> after = nullptr, bool own = false) {
  Row r; r.name = n; r.kind = 1;
  r.val = [v, label](char* o) { snprintf(o, 40, "%s", label(*v)); };
  r.act = [v, count, after, own](int d) {
    *v = (*v + (d < 0 ? count - 1 : 1)) % count;
    if (after) after();
    if (own) { dreamCfgSave(); applyLook(); uiDirty(); } else cfgChanged();
  };
  return r;
}
static Row dchoice(const char* n, uint8_t* v, int count, std::function<const char*(int)> label, std::function<void()> after = nullptr) {
  return choice(n, v, count, label, after, true);
}
static Row dtoggle(const char* n, uint8_t* v, std::function<void()> after = nullptr) {
  return dchoice(n, v, 2, [](int i) { return i ? "ON" : "OFF"; }, after);
}
static Row slider(const char* n, uint8_t* v, int lo, std::function<void()> live = nullptr) {
  Row r; r.name = n; r.kind = 2;
  r.pct = [v]() { return (int)*v; };
  r.setPct = [v, lo, live](int p) { *v = p < lo ? lo : p; if (live) live(); };
  r.act = [v, lo, live](int) { *v = *v >= 100 ? lo : (*v + 10 > 100 ? 100 : *v + 10); if (live) live(); cfgChanged(); };
  return r;
}
static const char* offLabel(int i) {
  static char b[12]; int s = OFF_SECS[i];
  if (!s) return "NEVER";
  if (s < 60) snprintf(b, sizeof(b), "%d SEC", s); else snprintf(b, sizeof(b), "%d MIN", s / 60);
  return b;
}
static void drawQr(const char* text, int cx, int y0, int size) {
  static uint8_t qr[cubeqr_BUFFER_LEN_FOR_VERSION(8)], tmp[cubeqr_BUFFER_LEN_FOR_VERSION(8)];
  if (!cubeqr_encodeText(text, tmp, qr, cubeqr_Ecc_MEDIUM, 1, 8, cubeqr_Mask_AUTO, true)) return;
  int n = cubeqr_getSize(qr), sc = size / (n + 4), sz = sc * (n + 4), x0 = cx - sz / 2;
  fillRect(x0, y0, sz, sz, C_WHITE);
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++)
      if (cubeqr_getModule(qr, x, y)) fillRect(x0 + (x + 2) * sc, y0 + (y + 2) * sc, sc, sc, C_BLACK);
}

// ------------------------------------------------------------------ MIC TEST: what the microphones hear
struct MicTest : Screen {
  const char* name() override { return "MIC TEST"; }
  bool keepAwake() override { return true; }
  void tick() override { redrawIn(50); }
  void gain(int d) { int g = dcfg.gain + d; if (g < 0) g = 0; if (g > 14) g = 14; dcfg.gain = g; micGain(g); dreamCfgSave(); uiDirty(); }
  void draw() override {
    pathLine("C:\\DREAM\\MICTEST>");
    if (!micOk()) { drawTextC(FT, 240, 200, "NO MICROPHONES FOUND", C_RED); fkeysBack(); return; }
    const char* K[3] = {"MIX", "L", "R"};
    for (int i = 0; i < 3; i++) { int y = 104 + i * 46; drawText(FT, 15, y, K[i], C_DIM, TX_FLAT); meter(75, y + 2, 390, 30, micLevel(i), true); }
    int x0 = 75, w = 390, y = 250;
    auto px = [&](float db) { int p = (int)((db + 66) * w / 60); return x0 + (p < 0 ? 0 : p > w ? w : p); };
    rect(x0, y, w, 34, C_DIM);
    int now = px(micDb()), fl = px(micFloorDb()), gt = px(micGateDb());
    bool sp = micDb() > micGateDb();
    fillRect(x0 + 2, y + 2, now - x0 - 2 > 0 ? now - x0 - 2 : 0, 30, sp ? C_FG : C_DIM);
    fillRect(fl - 1, y - 8, 3, 50, C_WARN);
    fillRect(gt - 1, y - 8, 3, 50, C_RED);
    drawText(FT, 15, y, "NOW", C_DIM, TX_FLAT);
    char b[64];
    snprintf(b, sizeof(b), "NOISE %d dB", (int)micFloorDb()); drawText(FT, 15, y + 44, b, C_WARN, TX_FLAT);
    snprintf(b, sizeof(b), "SPEECH > %d dB", (int)micGateDb()); drawTextR(FT, 465, y + 44, b, C_RED, TX_FLAT);
    drawTextC(FT, 240, y + 84, sp ? "SPEECH" : "QUIET", sp ? C_FG : C_DIM);
    snprintf(b, sizeof(b), "GAIN %s", gainLabel(dcfg.gain)); drawTextC(FT, 240, y + 120, b, C_FG);
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {"-", "Less", [this] { gain(-1); }}, {"+", "More", [this] { gain(1); }}});
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_KEY && e.key == K_PREV) { gain(-1); return true; }
    if (e.t == EV_KEY && e.key == K_NEXT) { gain(1); return true; }
    return false;
  }
};

// ------------------------------------------------------------------ LINK: the control page at error.blue/dream
static void askNewCode() {
  ask("NEW CODE?", "The control page link with the old code stops working. Open the new QR code on your phone.", {"NEW CODE", "CANCEL"}, [](int i) {
    if (i) return;
    dreamNewCode(); dreamCfgSave(); toast("NEW CODE"); uiDirty();
  });
}
struct Link : Screen {
  const char* name() override { return "LINK"; }
  bool keepAwake() override { return true; }
  void tick() override { redrawIn(1000); }
  void draw() override {
    char url[64]; snprintf(url, sizeof(url), "https://error.blue/dream/#%s", dcfg.code);
    drawText(FT, 15, TOP_Y, "CONTROL PAGE: SCAN WITH A PHONE", C_FG);
    drawQr(url, 240, 92, 230);
    drawTextC(FT, 240, 330, "ERROR.BLUE/DREAM", C_FG);
    char b[48]; snprintf(b, sizeof(b), "CODE  %s", dcfg.code);
    drawTextC(FB, 240, 360, b, C_FG);
    const char* st = !dcfg.link ? "THE LINK IS OFF (SETUP)" : S.net != NET_UP ? "NEEDS THE INTERNET" :
                     enetLinkAt() && msNow() - enetLinkAt() < 30000 ? "LINKED" : "LINKING...";
    drawTextC(FT, 240, 402, st, dcfg.link && S.net == NET_UP ? C_DIM : C_WARN, TX_FLAT);
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {"N", "New code", [] { askNewCode(); }}});
  }
};

// ------------------------------------------------------------------ WIFI: the networks it finds; pick one, type the password
// root = the start: when it's connected, the whole interface starts (uiStart -> the DREAM view).
// While it's open nothing else is tried by itself (netHold), apart from the saved networks at the start.
struct WifiGate : Screen {
  bool root, autoTry = false;
  WifiNet nets[16]; int nn = -1;
  ListView L;
  int mode = 0;                           // 0 pick, 1 connecting
  uint32_t joinAt = 0, scanAt = 0;
  char joining[33] = "", failMsg[72] = "";
  explicit WifiGate(bool r) : root(r) {}
  const char* name() override { return "WIFI"; }
  bool keepAwake() override { return true; }
  bool noSwipeBack() override { return root; }
  void enter() override {
    L.setBox(15, 134, 450, 290); L.rowH = 56;
    if (root && S.net == NET_JOINING) { autoTry = true; mode = 1; joinAt = msNow(); snprintf(joining, sizeof(joining), "%s", S.ssid); }
    else scan();
  }
  void leave() override { netHold = false; }
  void scan() {
    netHold = true; autoTry = false; mode = 0;
    if (cfg.offline) { cfg.offline = 0; cfgSave(); netOffline(false); }
    if (!cfg.wifiOn || S.net == NET_OFF) { cfg.wifiOn = 1; cfgSave(); netWifi(true); }
    if (S.net != NET_UP) netStopTrying();
    nn = -1; scanAt = msNow(); netScan();
    uiDirty();
  }
  void join(const char* ssid, const char* pass) {
    netHold = true; autoTry = false;
    snprintf(joining, sizeof(joining), "%s", ssid); failMsg[0] = 0;
    netJoin(ssid, pass);
    mode = 1; joinAt = msNow(); uiDirty();
  }
  void pick(int i) {
    if (i < 0 || i >= nn) return;
    std::string ss = nets[i].ssid; bool saved = nets[i].saved;
    if (nets[i].open) { join(ss.c_str(), ""); return; }
    kbOpen(saved ? "PASSWORD \xC2\xB7 EMPTY = SAVED" : "PASSWORD", "", 63, [this, ss, saved](const char* p) {
      if (!p[0] && !saved) { toast("TYPE THE PASSWORD"); return; }
      join(ss.c_str(), p[0] ? p : nullptr);
    }, false);
  }
  void failed(const char* why) {
    snprintf(failMsg, sizeof(failMsg), "%s: %s", joining, why && why[0] ? why : "COULDN'T CONNECT");
    utf8Cut(failMsg, 29);
    scan();
  }
  void tick() override {
    if (S.net == NET_UP) {
      netHold = false;
      toast("CONNECTED");
      if (root) uiStart(); else uiBack();   // (this screen is gone after these)
      return;
    }
    uint32_t now = msNow();
    if (mode == 0) {
      if (nn < 0) {
        int k = netScanResults(nets, 16);
        if (k >= 0) {
          nn = k;
          // saved ones first, then the strongest
          for (int a = 0; a < nn; a++) for (int b = a + 1; b < nn; b++)
            if ((nets[b].saved && !nets[a].saved) || (nets[b].saved == nets[a].saved && nets[b].rssi > nets[a].rssi)) { WifiNet t = nets[a]; nets[a] = nets[b]; nets[b] = t; }
          L.n = nn; L.sel = 0; uiDirty();
        } else if (now - scanAt > 12000) scan();      // a scan that never ended
      } else if (nn == 0 && now - scanAt > 6000) scan(); // nothing found: look again
    } else if (autoTry) {
      // the start: the cube tries its saved networks by itself
      if (S.net == NET_JOINING) snprintf(joining, sizeof(joining), "%s", S.ssid);
      else if (S.net == NET_FAIL || S.net == NET_NOSAVED || S.net == NET_OFF) failed(netReason());
    } else {
      bool bad = S.net == NET_FAIL || S.net == NET_NOSAVED || now - joinAt > 25000 || (netReason()[0] && now - joinAt > 7000);
      if (bad) { const char* r = netReason(); char why[40]; snprintf(why, sizeof(why), "%s", r); netStopTrying(); failed(why); }
    }
    L.tick();
    redrawIn(150);
  }
  void draw() override {
    pathLine("C:\\DREAM\\WIFI>");
    uint32_t now = msNow();
    if (mode == 1) {
      drawTextC(FT, 240, 130, "CONNECTING TO", C_DIM, TX_FLAT);
      char n[40]; snprintf(n, sizeof(n), "%s", joining); utf8Cut(n, 18);
      drawTextC(FB, 240, 170, n, C_FG);
      int k = (now / 300) % 4; char d[8] = ""; for (int i = 0; i < k; i++) strcat(d, ".");
      drawTextC(FT, 240, 230, d, C_FG, TX_FLAT);
      int pct = (int)((now - joinAt) * 100 / 20000); if (pct > 100) pct = 100;
      meter(60, 280, 360, 26, pct, true);
      if (root) drawTextC(FT, 240, 330, "THEN DREAM-CUBE STARTS", C_DIM, TX_FLAT);
      fkeys({{"ESC", autoTry ? "Pick a WiFi" : "Pick another", [this] { netStopTrying(); scan(); }}});
      return;
    }
    drawText(FT, 15, TOP_Y + 40, root ? "PICK YOUR WIFI" : "PICK A WIFI", C_FG);
    if (failMsg[0]) drawText(FT, 15, TOP_Y + 74, failMsg, C_RED, TX_FLAT);
    if (nn < 0) {
      int k = (now / 300) % 4; char b[24] = "SCANNING"; for (int i = 0; i < k; i++) strcat(b, ".");
      drawTextC(FT, 240, 250, b, C_DIM, TX_FLAT);
    } else if (nn == 0) {
      drawTextC(FT, 240, 230, "NO WIFI FOUND", C_WARN);
      drawTextC(FT, 240, 266, "LOOKING AGAIN...", C_DIM, TX_FLAT);
    } else {
      L.n = nn;
      L.draw([this](int i, int x, int y, int w, int h, bool sel) {
        const WifiNet& n = nets[i];
        if (sel) invBar(x + 2, y + 3, w - 4, h - 6);
        uint16_t c = sel ? C_BG : C_FG;
        char t[40]; snprintf(t, sizeof(t), "%s", n.ssid); utf8Cut(t, 19);
        drawText(FT, x + 10, y + 12, t, c, sel ? TX_FLAT : TX_GLOW);
        // signal: 4 bars; a closed box = it has a password; * = saved
        int bars = n.rssi > -55 ? 4 : n.rssi > -67 ? 3 : n.rssi > -78 ? 2 : 1;
        for (int b = 0; b < 4; b++) fillRect(x + w - 58 + b * 11, y + 38 - b * 6 - 8, 8, b * 6 + 8, b < bars ? c : (sel ? mix(C_BG, C_FG, 60) : C_DIM));
        if (!n.open) { rect(x + w - 86, y + 22, 16, 14, c); rect(x + w - 82, y + 15, 8, 8, c); }
        if (n.saved) drawText(FT, x + w - 112, y + 12, "*", c, TX_FLAT);
      }, [this](int i) { pick(i); });
    }
    if (root) fkeys({{"R", "Scan again", [this] { scan(); }}});
    else fkeys({{"ESC", "Back", [] { uiBack(); }}, {"R", "Scan again", [this] { scan(); }}});
  }
  bool ev(const Ev& e) override {
    if (mode == 0 && nn > 0) { L.n = nn; return L.ev(e, [this](int i) { pick(i); }); }
    return false;
  }
};

// ------------------------------------------------------------------ FIRMWARE: update DREAM-CUBE, or go back to ERROR-CUBE
struct Firmware : Screen {
  bool sw;
  explicit Firmware(bool s) : sw(s) {}
  const char* name() override { return sw ? "SWITCH" : "UPDATE"; }
  bool keepAwake() override { return S.upd == UPD_LOADING || S.upd == UPD_CHECKING; }
  bool noSwipeBack() override { return S.upd == UPD_LOADING; }
  void enter() override { if (sw) enetSwitch(); else if (S.upd != UPD_LOADING && S.upd != UPD_NEW) netUpdateCheck(); }
  void tick() override { redrawIn(300); }
  void draw() override {
    pathLine(sw ? "C:\\>ERROR-CUBE" : "C:\\DREAM\\UPDATE>");
    char b[96];
    int y = 110;
    if (sw) {
      drawText(FT, 30, y, "DREAM-CUBE > ERROR-CUBE", C_FG); y += 40;
      drawText(FT, 30, y, "The cube gets ERROR-CUBE back.", C_DIM, TX_FLAT); y += 32;
      drawText(FT, 30, y, "Your WiFi, settings and", C_DIM, TX_FLAT); y += 32;
      drawText(FT, 30, y, "files stay.", C_DIM, TX_FLAT); y += 44;
    } else {
      snprintf(b, sizeof(b), "THIS CUBE: DREAM-CUBE %s", CUBE_VERSION); drawText(FT, 30, y, b, C_FG); y += 40;
      if (S.updVer[0] && (S.upd == UPD_NEW || S.upd == UPD_LATEST || S.upd == UPD_LOADING)) {
        snprintf(b, sizeof(b), "NEWEST:     DREAM-CUBE %s", S.updVer); drawText(FT, 30, y, b, C_FG); y += 36;
        char lines[3][64]; int n = wrapText(S.updNotes, 28, lines, 3);
        for (int i = 0; i < n; i++) { drawText(FT, 30, y, lines[i], C_DIM, TX_FLAT); y += 30; }
        y += 10;
      }
    }
    const char* st = "";
    switch (S.upd) {
      case UPD_CHECKING: st = sw ? "LOOKING ON ERROR.BLUE..." : "CHECKING ERROR.BLUE..."; break;
      case UPD_NEW: st = sw ? "" : "A NEW ONE IS READY"; break;
      case UPD_LATEST: st = sw ? "" : "YOU HAVE THE NEWEST"; break;
      case UPD_LOADING: case UPD_DONE: case UPD_FAIL: st = S.updMsg; break;
    }
    if (S.upd == UPD_FAIL) { drawText(FT, 30, y, "DIDN'T WORK:", C_RED); y += 34; }
    if (st[0]) { drawText(FT, 30, y, st, S.upd == UPD_FAIL ? C_RED : C_FG); y += 40; }
    if (S.upd == UPD_LOADING || S.upd == UPD_DONE) {
      meter(30, y, 420, 30, S.updPct, true);
      snprintf(b, sizeof(b), "%d%%  DON'T SWITCH IT OFF", S.updPct); drawText(FT, 30, y + 40, b, C_DIM, TX_FLAT);
      return;
    }
    if (sw) {
      if (S.upd == UPD_FAIL) fkeys({{"ESC", "Back", [] { uiBack(); }}, {"R", "Try again", [] { enetSwitch(); }}});
      else fkeysBack();
    } else if (S.upd == UPD_NEW) fkeys({{"ESC", "Back", [] { uiBack(); }}, {"\xE2\x86\xB5", "Install", [] { netUpdateInstall(); }}});
    else fkeys({{"ESC", "Back", [] { uiBack(); }}, {"R", "Check", [] { netUpdateCheck(); }}});
  }
  bool ev(const Ev& e) override {
    if (S.upd == UPD_LOADING) return true;
    if (e.t == EV_KEY && e.key == K_OK) { if (!sw && S.upd == UPD_NEW) netUpdateInstall(); else if (sw && S.upd == UPD_FAIL) enetSwitch(); return true; }
    return false;
  }
};
static void askSwitch() {
  ask("BACK TO ERROR-CUBE?", "The cube downloads ERROR-CUBE from error.blue and restarts in it. Your WiFi, settings and files stay. To come back: ERROR-CUBE > UPDATE > FW Other > DREAM-CUBE.",
      {"SWITCH", "CANCEL"}, [](int i) {
    if (i) return;
    if (S.net != NET_UP) { ask("NO WIFI", "Join a WiFi network first: SETUP > WIFI.", {"OK"}, nullptr); return; }
    uiOpen(new Firmware(true));
  });
}

// ------------------------------------------------------------------ SETUP
struct Setup : Screen {
  std::vector<Row> rows; ListView L;
  const char* name() override { return "SETUP"; }
  void enter() override { L.setBox(15, 92, 450, 330); L.rowH = 55; }
  void build() {
    rows.clear();
    rows.push_back(heading("DREAMING", IC_LOOK));
    rows.push_back(dchoice("NEW SCENE", &dcfg.often, 3, [](int i) { static const char* O[3] = {"OFTEN", "NORMAL", "CALM"}; return O[i]; }));
    rows.push_back(dchoice("THINKING", &dcfg.mind, 2, [](int i) { return i ? "SMART (SLOWER)" : "QUICK"; }));
    rows.push_back(dtoggle("WORDS ON SCREEN", &dcfg.words));
    rows.push_back(dtoggle("TITLE ON SCREEN", &dcfg.title));
    rows.push_back(dchoice("CAMERA", &dcfg.cam, 4, [](int i) { static const char* C4[4] = {"STILL", "SLOW", "NORMAL", "FAST"}; return C4[i]; }));
    rows.push_back(dchoice("COLOURS", &dcfg.colors, 3, [](int i) { static const char* C3[3] = {"NATURAL", "VIVID", "ERROR-DOS"}; return C3[i]; }));
    rows.push_back(dtoggle("EFFECTS", &dcfg.fxOn));
    rows.push_back(dchoice("PIXELS", &dcfg.soft, 2, [](int i) { return i ? "SMOOTH" : "SHARP"; }));
    rows.push_back(dchoice("DETAIL", &dcfg.detail, 2, [](int i) { return i ? "FULL" : "LESS (FASTER)"; }));
    rows.push_back(dchoice("NEW SCENE WAKES SCREEN", &dcfg.wake, 2, [](int i) { return i ? "YES" : "NO"; }));
    rows.push_back(dtoggle("FRAME RATE", &dcfg.fps));
    rows.push_back(heading("LISTENING", IC_SOUND));
    rows.push_back(action("LISTENING", [] { dreamListen(!dcfg.listen); }, [](char* o) { strcpy(o, dcfg.listen ? "ON" : "PAUSED"); }));
    rows.push_back(dchoice("LANGUAGE", &dcfg.lang, N_LANGS, [](int i) { return LANGS[i].name; }));
    rows.push_back(dchoice("SENSITIVITY", &dcfg.sens, 4, [](int i) { static const char* S4[4] = {"LOW", "NORMAL", "HIGH", "EVERYTHING"}; return S4[i]; }));
    rows.push_back(dchoice("MIC GAIN", &dcfg.gain, 15, [](int i) { return gainLabel(i); }, [] { micGain(dcfg.gain); }));
    rows.push_back(dchoice("MICROPHONES", &dcfg.mics, 3, [](int i) { static const char* M[3] = {"BOTH", "LEFT", "RIGHT"}; return M[i]; }));
    rows.push_back(dchoice("PAUSE LENGTH", &dcfg.pause, 3, [](int i) { static const char* P[3] = {"SHORT", "NORMAL", "LONG"}; return P[i]; }));
    rows.push_back(action("MIC TEST", [] { uiOpen(new MicTest()); }, [](char* o) { strcpy(o, ">"); }));
    rows.push_back(heading("CONTROL PAGE", IC_CLOUD));
    rows.push_back(action("ERROR.BLUE/DREAM", [] { uiOpen(new Link()); }, [](char* o) { snprintf(o, 40, "%s >", dcfg.code); }));
    rows.push_back(dtoggle("LINK", &dcfg.link));
    rows.push_back(info("THERE", [](char* o) {
      strcpy(o, !dcfg.link ? "OFF" : S.net != NET_UP ? "NO INTERNET" : enetLinkAt() && msNow() - enetLinkAt() < 30000 ? "LINKED" : "LINKING...");
    }));
    rows.push_back(action("NEW CODE", [] { askNewCode(); }, [](char* o) { strcpy(o, ">"); }));
    rows.push_back(heading("INTERNET", IC_CLOUD));
    rows.push_back(action("WIFI", [] { uiOpen(new WifiGate(false)); }, [](char* o) {
      if (S.net == NET_UP) { snprintf(o, 40, "%s", S.ssid); utf8Cut(o, 12); } else strcpy(o, cfg.wifiOn ? "NOT CONNECTED" : "OFF");
    }));
    rows.push_back(info("LAST SCENE", [](char* o) {
      if (dreamErr()[0]) { snprintf(o, 40, "%s", dreamErr()); utf8Cut(o, 14); }
      else if (enetDreamMs()) snprintf(o, 40, "OK, %u.%u S", (unsigned)(enetDreamMs() / 1000), (unsigned)(enetDreamMs() / 100 % 10));
      else strcpy(o, "-");
    }));
    rows.push_back(heading("SOUND + SCREEN", IC_SCREEN));
    rows.push_back(slider("VOLUME", &cfg.vol, 0, [] { tone(880, 40); }));
    rows.push_back(slider("BRIGHTNESS", &cfg.bright, 5, [] { sysBrightness(uiBrightNow()); }));
    rows.push_back(choice("ROTATION", &cfg.rot, 4, [](int i) { static const char* R[4] = {"0", "90", "180", "270"}; return R[i]; }));
    rows.push_back(choice("SCREEN OFF AFTER", &cfg.offIdx, N_OFF, [](int i) { return offLabel(i); }));
    rows.push_back(choice("COLOURS", &cfg.color, N_SCHEMES, [](int i) { return SCHEMES[i].name; }));
    rows.push_back(heading("FIRMWARE", IC_UPDATE));
    rows.push_back(action("UPDATE DREAM-CUBE", [] { uiOpen(new Firmware(false)); }, [](char* o) {
      if (S.upd == UPD_NEW && !enetSwitching) snprintf(o, 40, "%s IS OUT", S.updVer); else snprintf(o, 40, "%s", CUBE_VERSION);
    }));
    rows.push_back(choice("AUTO UPDATE", &cfg.autoUpd, 2, [](int i) { return i ? "ON" : "OFF"; }));
    rows.push_back(action("BACK TO ERROR-CUBE", [] { askSwitch(); }, [](char* o) { strcpy(o, ">"); }));
    rows.push_back(heading("THIS CUBE", IC_INFO));
    rows.push_back(info("SCENES MADE", [](char* o) { snprintf(o, 40, "%u", (unsigned)dreamScenes()); }));
    rows.push_back(info("FREE MEMORY", [](char* o) { snprintf(o, 40, "%uK + %uK", (unsigned)(freeHeap() / 1024), (unsigned)(freePsram() / 1024)); }));
    rows.push_back(info("BATTERY", [](char* o) { if (S.battery) snprintf(o, 40, "%d%%%s", S.batPct, S.charging ? " CHARGING" : ""); else strcpy(o, S.usb ? "USB" : "-"); }));
    rows.push_back(info("TF CARD", [](char* o) { snprintf(o, 40, "%s", S.sd ? S.sdInfo : "NONE"); utf8Cut(o, 14); }));
  }
  void tick() override { L.tick(); redrawIn(1000); }
  void draw() override { build(); pathLine("C:\\DREAM\\SETUP>"); drawRows(L, rows); fkeysBack(); }
  bool ev(const Ev& e) override { build(); return rowsEv(L, rows, e); }
};

// ------------------------------------------------------------------ SCENES: the last 40, tap to show again
struct Scenes : Screen {
  ListView L;
  const char* name() override { return "SCENES"; }
  void enter() override { L.setBox(15, 92, 450, 330); L.rowH = 55; L.sel = dreamPos(); L.showSel(); }
  void tick() override { L.tick(); redrawIn(500); }
  void draw() override {
    pathLine("C:\\DREAM\\SCENES>");
    L.n = dreamCount();
    if (!L.n) { drawTextC(FT, 240, 200, "NO SCENES YET", C_DIM, TX_FLAT); drawTextC(FT, 240, 240, "TALK NEAR THE CUBE", C_DIM, TX_FLAT); fkeysBack(); return; }
    L.draw([](int i, int x, int y, int w, int h, bool sel) {
      char t[48], at[8];
      if (sel) invBar(x + 2, y + 3, w - 4, h - 6);
      dreamTitleAt(i, t, sizeof(t), at, sizeof(at));
      utf8Cut(t, 22);
      uint16_t c = sel ? C_BG : i == dreamPos() ? C_FG : C_FG;
      drawText(FT, x + 8, y + 12, at[0] ? at : "--:--", sel ? C_BG : C_DIM, TX_FLAT);
      drawText(FT, x + 100, y + 12, t, c, sel ? TX_FLAT : TX_GLOW);
      if (i == dreamPos()) drawTextR(FT, x + w - 10, y + 12, "\xE2\x97\x8F", c, TX_FLAT);
    }, [](int i) { dreamShowAt(i); uiBack(); });
    fkeysBack();
  }
  bool ev(const Ev& e) override { L.n = dreamCount(); return L.ev(e, [](int i) { dreamShowAt(i); uiBack(); }); }
};

// ------------------------------------------------------------------ DREAM: the scene, full screen
static void shade(int y0, int h, bool down) {     // a soft dark band (text stays readable on bright skies)
  for (int i = 0; i < h; i++) {
    int a = (down ? h - i : i) * 17 / h;
    blendRect(0, y0 + i, SCR_W, 1, C_BLACK, a);
  }
}
static void outlined(int f, int cx, int y, const char* s, uint16_t c) {
  for (int dy = -2; dy <= 2; dy += 2) for (int dx = -2; dx <= 2; dx += 2) if (dx || dy) drawTextC(f, cx + dx, y + dy, s, C_BLACK, TX_FLAT);
  drawTextC(f, cx, y, s, c, TX_FLAT);
}
static uint16_t stColor(int st) {
  switch (st) {
    case ST_HEAR: return C_WHITE;
    case ST_SEND: case ST_THINK: return C_WARN;
    case ST_OFFLINE: case ST_LIMIT: case ST_ERROR: case ST_NOMIC: return C_RED;
    case ST_PAUSED: return C_DIM;
  }
  return C_FG;
}
struct Dream : Screen {
  uint32_t last = 0, menuUntil = 0, glitchUntil = 0, fpsAt = 0; int frames = 0, fps = 0;
  bool dragging = false; int lx = 0, ly = 0;
  const char* name() override { return "DREAM"; }
  bool fullScreen() override { return true; }
  bool plain() override { return true; }
  bool noSwipeBack() override { return true; }
  void enter() override {
    last = msNow(); applyLook();
    static bool hinted = false;
    if (!hinted && dcfg.link) { hinted = true; toast("CONTROL PAGE: ERROR.BLUE/DREAM", 6000); }
  }
  uint32_t lostAt = 0;
  void tick() override {
    // the WiFi gone for a minute: back to the WiFi screen (DREAM-CUBE starts again when it's connected)
    if (S.net == NET_UP) lostAt = 0;
    else if (!lostAt) lostAt = msNow() | 1;
    else if (msNow() - lostAt > 60000 && uiCurrent() == this) { uiStart(); return; }
    redrawIn(0);
  }
  void menu() { menuUntil = msNow() + 6000; uiDirty(); }
  void draw() override {
    uint32_t now = msNow();
    float dt = (now - last) / 1000.0f; last = now;
    if (dt > 0.2f) dt = 0.2f;
    static float avg = 0.05f; static uint32_t slowSince = 0;
    avg = avg * 0.95f + dt * 0.05f;
    if (sceneTransition() >= 1 && avg > 0.09f) { if (!slowSince) slowSince = now; else if (now - slowSince > 3000) autoLess = true; } else slowSince = 0;
    applyLook();
    sceneDraw(fb, dt);
    // a short glitch when a new scene comes (ERROR-DOS)
    if (sceneTransition() < 0.08f) { fxTear(3, now); fxStatic(6, now * 7); }
    const SceneDef& d = sceneNow();
    // the title
    if (dcfg.title && d.title[0]) {
      shade(0, 70, true);
      char t[48]; snprintf(t, sizeof(t), "%s", d.title);
      for (char* p = t; *p; p++) if ((unsigned char)*p < 128) *p = toupper((unsigned char)*p);
      utf8Cut(t, 27);
      float tr = sceneTransition();
      int n = tr < 1 ? (int)(utf8Len(t) * fminf(1, tr * 2.5f)) : 99;   // types itself in
      char shown[48]; snprintf(shown, sizeof(shown), "%s", t); utf8Cut(shown, n);
      outlined(FT, 240, 16, shown, C_FG);
    }
    // the dot: how it listens (top left), older scenes (top right)
    int st = dreamStatus();
    uint16_t c = stColor(st);
    bool blink = (st == ST_HEAR || st == ST_SEND || st == ST_THINK) && (now / 250) % 2;
    if (st == ST_PAUSED) { fillRect(16, 18, 5, 16, c); fillRect(25, 18, 5, 16, c); }
    else { fillCircle(23, 26, 7, blink ? C_BLACK : c); ring(23, 26, 8, 2, c); }
    if (dreamPos() > 0) { char b[16]; snprintf(b, sizeof(b), "-%d", dreamPos()); drawTextR(FT, 466, 16, b, C_WARN, TX_FLAT); }
    // what it heard (fades after a while), or why it can't
    bool menuOn = now < menuUntil;
    int by = menuOn ? 374 : 420;
    const char* why = nullptr;
    char wb[64];
    if (st == ST_NOMIC) why = "NO MICROPHONES FOUND";
    else if (S.net != NET_UP) why = "WIFI LOST: RECONNECTING...";
    else if (st == ST_LIMIT || st == ST_ERROR) { snprintf(wb, sizeof(wb), "%s", dreamErr()); utf8Cut(wb, 28); why = wb; }
    if (why) { shade(by - 30, 90, false); outlined(FT, 240, by, why, st == ST_PAUSED ? C_DIM : C_WARN); }
    else if (st == ST_THINK) { shade(by - 30, 90, false); char b[24] = "DREAMING"; int k = (now / 300) % 4; for (int i = 0; i < k; i++) strcat(b, "."); outlined(FT, 240, by, b, C_WARN); }
    else if (dcfg.words && dreamHeard()[0] && now - dreamHeardAt() < 9000) {
      char lines[6][64];
      int all = wrapText(dreamHeard(), 29, lines, 6), n = all > 2 ? 2 : all;   // the newest two lines
      shade(by - 30 - (n - 1) * 30, 90 + (n - 1) * 30, false);
      for (int i = 0; i < n; i++) outlined(FT, 240, by - (n - 1 - i) * 30, lines[all - n + i], C_WHITE);
    }
    if (dcfg.fps) {
      frames++;
      if (now - fpsAt >= 1000) { fps = frames * 1000 / (now - fpsAt ? now - fpsAt : 1); frames = 0; fpsAt = now; }
      char b[24]; snprintf(b, sizeof(b), "%d FPS %d", fps, sceneTris()); drawText(FT, 40, 52, b, C_WARN, TX_FLAT);
    }
    if (menuOn) {
      blendRect(0, BOT_Y - 6, SCR_W, SCR_H - BOT_Y + 6, C_BLACK, 22);
      fkeys({{dcfg.listen ? "II" : "\xE2\x96\xB6", dcfg.listen ? "Pause" : "Listen", [this] { dreamListen(!dcfg.listen); toast(dcfg.listen ? "LISTENING" : "PAUSED"); menu(); }},
             {"T", "Type", [] { kbOpen("DREAM OF...", "", 120, [](const char* t) { if (t[0]) { dreamSay(t); toast(S.net == NET_UP ? "DREAMING..." : "NO INTERNET"); } }); }},
             {"L", "Scenes", [] { uiOpen(new Scenes()); }},
             {"S", "Setup", [] { uiOpen(new Setup()); }}});
    }
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_DOWN) { dragging = false; lx = e.x; ly = e.y; return true; }
    if (e.t == EV_MOVE) {
      if (!dragging && (abs(e.x - e.x0) > 10 || abs(e.y - e.y0) > 10)) dragging = true;
      if (dragging) { sceneDrag(e.x - lx, e.y - ly); lx = e.x; ly = e.y; }
      return true;
    }
    if (e.t == EV_UP) return true;
    if (e.t == EV_SWIPE) return true;
    if (e.t == EV_TAP) { if (msNow() < menuUntil) menuUntil = 0; else menu(); uiDirty(); return true; }
    if (e.t == EV_KEY) {
      if (e.key == K_PREV) { if (dreamPos() + 1 < dreamCount()) dreamShowAt(dreamPos() + 1); else toast("THE OLDEST"); return true; }
      if (e.key == K_NEXT) { if (dreamPos() > 0) dreamShowAt(dreamPos() - 1); else menu(); return true; }
      if (e.key == K_OK) { menu(); return true; }
      if (e.key == K_BACK) { menuUntil = 0; return true; }
    }
    return false;
  }
};
Screen* homeScreen() { if (S.net == NET_UP) return new Dream(); return new WifiGate(true); }

// ------------------------------------------------------------------ start-up: a turning cube, the name
struct Boot : Screen {
  uint32_t t0 = 0;
  const char* name() override { return "BOOT"; }
  bool fullScreen() override { return true; }
  bool keepAwake() override { return true; }
  void enter() override { t0 = msNow(); snd(SND_ON); }
  void tick() override {
    if (msNow() - t0 > (cfg.boot == 2 ? 400u : 2400u)) { uiStart(); return; }
    redrawIn(30);
  }
  void draw() override {
    uint32_t t = msNow() - t0;
    gfxClearTo(C_BG);
    float a = t / 1000.0f;
    g3dCam(0, 0, -6, 0, 0, 0, 420, 240, 150);
    g3dObj(v3(0, 0, 0), a * 0.9f, a * 0.55f + 0.4f, 0, t < 600 ? t / 600.0f : 1.0f);
    box3(v3(0, 0, 0), v3(1, 1, 1), C_FG, 32);
    box3(v3(0, 0, 0), v3(0.45f, 0.45f, 0.45f), C_DIM, 24);
    int k = t < 900 ? (int)(10 * t / 900) : 10;
    char name[16]; snprintf(name, sizeof(name), "%.*s", k, "DREAM-CUBE");
    drawTextC(FB, 240, 300, name, C_FG);
    if (t > 1000) drawTextC(FT, 240, 356, "VERSION " CUBE_VERSION, C_DIM, TX_FLAT);
    if (t > 1300) drawTextC(FT, 240, 392, "SPEECH TO 3D \xC2\xB7 ERROR.BLUE", C_DIM, TX_FLAT);
  }
  bool ev(const Ev& e) override { if (e.t == EV_TAP || e.t == EV_KEY) t0 = msNow() - 5000; return true; }
};
Screen* bootScreen() { return new Boot(); }
Screen* introScreen() { return new Boot(); }

// ------------------------------------------------------------------ the dim clock while the screen is "off"
struct Aod : Screen {
  const char* name() override { return "AOD"; }
  bool fullScreen() override { return true; }
  void draw() override {
    struct tm t;
    char c[12] = "--:--";
    if (sysLocalTime(t)) fmtTime(c, t.tm_hour, t.tm_min, 0, false);
    uint32_t m = msNow() / 60000;
    int ox = (int)(m * 37 % 61) - 30, oy = (int)(m * 53 % 81) - 40;
    drawTextC(FH, 240 + ox, 160 + oy, c, mix(C_BLACK, C_FG, 50), TX_FLAT);
    const char* s = dreamListening() ? (dreamThinking() ? "DREAMING" : "LISTENING") : "PAUSED";
    drawTextC(FT, 240 + ox, 280 + oy, s, mix(C_BLACK, C_FG, 40), TX_FLAT);
    redrawIn(5000);
  }
};
Screen* aodScreen() { return new Aod(); }

// ------------------------------------------------------------------ what ui.cpp and the board ask of the programs
void appsRegister() {}
void appsLoop() {
  // the page or the control page: screen on / off
  static uint32_t seenWake = 0, seenSleep = 0;
  if (S.wakeAt != seenWake) { seenWake = S.wakeAt; uiWake(); }
  if (S.sleepAt != seenSleep) { seenSleep = S.sleepAt; uiSleep(); }
  dreamLoop();
  // a scene to show: new from error.blue, from the control page, or an older one
  static bool first = true;
  if (first) { first = false; SceneDef d; sceneDefault(d); sceneShow(d, true); applyLook(); }
  SceneDef d; bool fresh = false;
  if (dreamPoll(d, fresh)) {
    autoLess = false; applyLook();
    sceneShow(d, false);
    if (fresh && dcfg.wake && !uiAwake()) uiWake();
    uiDirty();
  }
}
void clockTick() {}
uint32_t tmrEnd = 0, tmrLeft = 0; bool tmrRun = false;
Screen* findProg(const char* dos) {
  if (!strcmp(dos, "WIFI")) return new WifiGate(false);
  if (!strcmp(dos, "UPDATE")) return new Firmware(false);
  if (!strcmp(dos, "MICTEST")) return new MicTest();
  if (!strcmp(dos, "SETUP")) return new Setup();
  if (!strcmp(dos, "SCENES")) return new Scenes();
  if (!strcmp(dos, "LINK")) return new Link();
  return nullptr;
}
void openProg(const char* dos) { Screen* s = findProg(dos); if (s) uiOpen(s); }
bool openFile(const char* path) {
  if (netInstallFile(path)) { uiOpen(new Firmware(false)); return true; }
  return false;
}
void musicNext(int) {}
void musicToggle() {}
void musicTick() {}
void musicPlayFolder(const char*, const char*) {}
Screen* makeNowPlaying() { return nullptr; }
const char* bootLine(int) { return ""; }
