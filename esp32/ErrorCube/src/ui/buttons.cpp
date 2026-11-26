// buttons.cpp: the three buttons (-, +, PWR): press, double press, hold, repeat and two together,
// each set to any action in SETUP > BUTTONS. The board reports the raw presses (btnRaw, btnPwr).
// Plain C++ (also in the simulator).
#include "ui.h"
#include "../core/holo.h"
#include <string.h>
#include <stdio.h>

char btnLast[48] = "";
static const char* NAMES[BA_COUNT] = {
  "NOTHING", "PREV / UP", "NEXT / DOWN", "OK", "BACK", "HOME", "QUICK PANEL", "SCREEN ON/OFF", "POWER MENU",
  "VOLUME +", "VOLUME -", "LIGHT +", "LIGHT -", "HOLO ON/OFF", "HOLO CENTRE", "OFFLINE ON/OFF", "SCREENSHOT",
  "TORCH", "GLITCH", "CLOCK", "FILES", "SETUP", "PLAY / PAUSE", "NEXT SONG", "PREVIOUS SONG", "MUSIC"};
const char* btnName(int a) { return a >= 0 && a < BA_COUNT ? NAMES[a] : "?"; }
static const char* BN[3] = {"-", "+", "PWR"};

// ------------------------------------------------------------------ the actions
struct Torch : Screen {
  const char* name() override { return "TORCH"; }
  bool fullScreen() override { return true; }
  bool keepAwake() override { return true; }
  int was = -1;
  void enter() override { was = cfg.bright; sysBrightness(100); }
  void leave() override { sysBrightness(uiBrightNow()); }
  void draw() override { gfxClearTo(0xFFFF); }
  bool ev(const Ev& e) override { if (e.t == EV_TAP || e.t == EV_KEY) { uiBack(); return true; } return false; }
};

static void shot() {
  // the picture as it is on the screen now (through the glass), as a BMP on the SD card (or the cube's own drive)
  static uint16_t n = 0;
  const char* dir = S.sd ? "/sd/SHOTS" : "/c/shots";
  fsMkdir(dir);
  char p[48];
  for (int k = 0; k < 999; k++) { n++; snprintf(p, sizeof(p), "%s/SHOT%04u.BMP", dir, n); if (fsSize(p) < 0) break; }
  const int W = SCR_W, H = SCR_H, row = W * 3, sz = 54 + row * H;
  uint8_t* b = (uint8_t*)malloc(sz);
  if (!b) { toast("NO MEMORY"); return; }
  memset(b, 0, 54);
  b[0] = 'B'; b[1] = 'M'; *(uint32_t*)(b + 2) = sz; *(uint32_t*)(b + 10) = 54; *(uint32_t*)(b + 14) = 40;
  *(int32_t*)(b + 18) = W; *(int32_t*)(b + 22) = -H; *(uint16_t*)(b + 26) = 1; *(uint16_t*)(b + 28) = 24;
  *(uint32_t*)(b + 34) = row * H;
  for (int i = 0; i < W * H; i++) {
    uint16_t c = fb[i];
    b[54 + i * 3] = (c & 31) * 255 / 31; b[55 + i * 3] = ((c >> 5) & 63) * 255 / 63; b[56 + i * 3] = (c >> 11) * 255 / 31;
  }
  bool ok = fsWrite(p, b, sz);
  free(b);
  char t[48]; snprintf(t, sizeof(t), ok ? "SAVED %s" : "COULD NOT SAVE", strrchr(p, '/') + 1);
  toast(t);
  snd(ok ? SND_OK : SND_ERR);
}

void btnDo(int a) {
  char t[32];
  switch (a) {
    case BA_PREV: uiKey(K_PREV); break;
    case BA_NEXT: uiKey(K_NEXT); break;
    case BA_OK: uiKey(K_OK); break;
    case BA_BACK: uiKey(K_BACK); break;
    case BA_HOME: uiWake(); uiHome(); break;
    case BA_QUICK: uiWake(); quickOpen(); break;
    case BA_SCREEN: if (uiAwake()) uiSleep(); else uiWake(); break;
    case BA_POWER: uiWake(); powerMenu(); break;
    case BA_VOLUP: case BA_VOLDN: {
      int v = cfg.vol + (a == BA_VOLUP ? 10 : -10); cfg.vol = v < 0 ? 0 : v > 100 ? 100 : v;
      snprintf(t, sizeof(t), "VOLUME %d%%", cfg.vol); toast(t, 900); cfgChanged(); snd(SND_KEY); break;
    }
    case BA_LIGHTUP: case BA_LIGHTDN: {
      int v = cfg.bright + (a == BA_LIGHTUP ? 10 : -10); cfg.bright = v < 5 ? 5 : v > 100 ? 100 : v;
      snprintf(t, sizeof(t), "LIGHT %d%%", cfg.bright); toast(t, 900); cfgChanged(); break;
    }
    case BA_HOLO: case BA_HCENTRE: break;                     // HOLO 3D is gone (1.5)
    case BA_OFFLINE: cfg.offline = !cfg.offline; netOffline(cfg.offline); toast(cfg.offline ? "OFFLINE" : "ONLINE", 1200); cfgChanged(); break;
    case BA_SHOT: shot(); break;
    case BA_TORCH: uiWake(); uiOpen(new Torch()); break;
    case BA_GLITCH: uiGlitch(-1, 0); break;
    case BA_CLOCK: uiWake(); openProg("CLOCK"); break;
    case BA_FILES: uiWake(); openProg("FILES"); break;
    case BA_SETUP: uiWake(); openProg("SETUP"); break;
    case BA_PLAY: musicToggle(); break;
    case BA_NEXTTRK: musicNext(1); break;
    case BA_PREVTRK: musicNext(-1); break;
    case BA_MUSIC: uiWake(); uiOpen(makeNowPlaying()); break;
    default: break;
  }
}

// ------------------------------------------------------------------ the gestures
struct B { bool down = false, held = false, combo = false, second = false; uint32_t at = 0, upAt = 0, repAt = 0; int pending = 0; };
static B b[3];                       // -, +, PWR
static bool comboOn = false, comboHeld = false; static uint32_t comboAt = 0;

static void fire(int a, const char* what) {
  snprintf(btnLast, sizeof(btnLast), "%s: %s", what, btnName(a));
  uiDirty();
  if (a == BA_NONE) return;
  // the screen is off: a button only wakes it (PWR's own actions still work)
  if (!uiAwake() && a != BA_SCREEN && a != BA_POWER && a != BA_TORCH && a != BA_PLAY && a != BA_NEXTTRK && a != BA_PREVTRK) {
    if (cfg.btnWake == 0 || strstr(what, "PWR")) uiWake();
    return;
  }
  if (cfg.clicks && a != BA_SCREEN && a != BA_POWER) snd(SND_KEY);
  btnDo(a);
}
static bool repeatable(int a) { return a == BA_PREV || a == BA_NEXT || a == BA_VOLUP || a == BA_VOLDN || a == BA_LIGHTUP || a == BA_LIGHTDN; }
static uint32_t holdMs() { int h = cfg.btnHold * 50; return h < 250 ? 250 : h; }
static uint32_t dblMs() { return cfg.btnDouble * 10; }

static void release(int id) {
  char w[24];
  B& x = b[id];
  int pa = cfg.btnAct[id][0], da = cfg.btnAct[id][1];
  if (dblMs() && da != BA_NONE) {
    if (x.second) { x.second = false; x.pending = 0; snprintf(w, sizeof(w), "%s DOUBLE", BN[id]); fire(da, w); return; }
    x.pending = 1; x.upAt = msNow();       // wait: a second press makes it a double press
    return;
  }
  snprintf(w, sizeof(w), "%s PRESS", BN[id]);
  fire(pa, w);
}

bool (*btnHook)(int id, bool down) = nullptr;
void btnRaw(int id, bool down) {
  if (btnHook && btnHook(id, down)) return;   // a game (PICO-8) takes the buttons as they are
  if (cfg.btnSwap) id ^= 1;
  if (id < 0 || id > 1) return;
  uint32_t now = msNow();
  B& x = b[id]; B& o = b[id ^ 1];
  if (down && !x.down) {
    x.down = true; x.held = false; x.at = now; x.repAt = 0;
    if (x.pending && now - x.upAt <= dblMs()) { x.second = true; x.pending = 0; }
    if (o.down && !o.held) { comboOn = true; comboHeld = false; comboAt = now; x.combo = o.combo = true; x.pending = o.pending = 0; }
    return;
  }
  if (!down && x.down) {
    x.down = false;
    if (x.combo) {
      x.combo = false;
      if (!o.down && comboOn) { if (!comboHeld) fire(cfg.btnCombo[0], "- AND +"); comboOn = false; }
      x.held = true;                       // nothing more from this press
      return;
    }
    if (!x.held && now - x.at > 25) release(id);
    x.second = x.second && !x.held ? x.second : false;
  }
}

void btnPwr(bool held) {
  char w[24];
  if (!held && btnHook && btnHook(2, true)) return;
  // with - or + held down: a combination
  if (!held && (b[0].down || b[1].down)) {
    int k = b[0].down ? 0 : 1;
    if (cfg.btnSwap) k ^= 1;
    b[0].held = b[0].down ? true : b[0].held; b[1].held = b[1].down ? true : b[1].held;
    fire(cfg.btnCombo[2 + k], k ? "HOLD + AND PWR" : "HOLD - AND PWR");
    return;
  }
  if (held) { fire(cfg.btnAct[2][2], "PWR HOLD"); b[2].pending = 0; return; }
  B& x = b[2];
  uint32_t now = msNow();
  if (dblMs() && cfg.btnAct[2][1] != BA_NONE) {
    if (x.pending && now - x.upAt <= dblMs() + 150) { x.pending = 0; fire(cfg.btnAct[2][1], "PWR DOUBLE"); return; }
    x.pending = 1; x.upAt = now;           // the power chip reports a press when it's let go: wait for a second one
    return;
  }
  snprintf(w, sizeof(w), "PWR PRESS");
  fire(cfg.btnAct[2][0], w);
}

void btnPoll() {
  uint32_t now = msNow();
  char w[24];
  // two held together
  if (comboOn && b[0].down && b[1].down && !comboHeld && now - comboAt >= holdMs()) {
    comboHeld = true; fire(cfg.btnCombo[1], "HOLD - AND +");
  }
  for (int id = 0; id < 3; id++) {
    B& x = b[id];
    if (id < 2 && x.down && !x.combo && !x.held && now - x.at >= holdMs()) {
      int ha = cfg.btnAct[id][2], pa = cfg.btnAct[id][0];
      x.held = true;
      if (ha == BA_NONE && cfg.btnRepeat && repeatable(pa)) { x.repAt = now; snprintf(w, sizeof(w), "%s REPEAT", BN[id]); fire(pa, w); }
      else { snprintf(w, sizeof(w), "%s HOLD", BN[id]); fire(ha, w); }
    }
    if (id < 2 && x.down && x.repAt && cfg.btnRepeat && now - x.repAt >= (uint32_t)cfg.btnRepeat * 10) {
      x.repAt = now; snprintf(w, sizeof(w), "%s REPEAT", BN[id]); fire(cfg.btnAct[id][0], w);
    }
    if (x.pending && !x.down && now - x.upAt > dblMs() + (id == 2 ? 150 : 0)) {
      x.pending = 0; x.second = false;
      snprintf(w, sizeof(w), "%s PRESS", BN[id]); fire(cfg.btnAct[id][0], w);
    }
  }
}
