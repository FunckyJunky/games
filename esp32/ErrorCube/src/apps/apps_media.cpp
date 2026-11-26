// apps_media.cpp: web apps (JavaScript from error.blue/cube/apps) and LIVE (video + sound from the phone)
#include "../ui/ui.h"
#include "../js/jsapp.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ================================================================== a web app
struct WebApp : Screen {
  char id[24], title[28], err[160] = "";
  int state = 0;            // 0 loading, 1 running, 2 failed
  uint32_t last = 0, startAt = 0;
  WebApp(const char* i, const char* n) { snprintf(id, sizeof(id), "%s", i); snprintf(title, sizeof(title), "%s", n && n[0] ? n : i); }
  const char* name() override { return title; }
  bool fullScreen() override { return state == 1; }
  bool noSwipeBack() override { return state == 1; }
  bool keepAwake() override { return state == 1; }
  void enter() override { startAt = msNow(); netReportProg(title); }
  void leave() override { jsStop(); }
  void start() {
    size_t n = 0;
    char* src = netAppSource(id, &n);
    if (!src) { state = 2; snprintf(err, sizeof(err), "COULD NOT READ %s.JS", id); return; }
    gfxClear();
    if (jsStart(id, src, n, err, sizeof(err))) { state = 1; last = msNow(); }
    else state = 2;
    free(src);
  }
  void tick() override {
    if (state == 0) {
      int st = netAppGet(id);
      if (st == 2) start();
      else if (st < 0) { state = 2; snprintf(err, sizeof(err), "COULD NOT DOWNLOAD IT (%s)", S.net == NET_UP ? "ERROR.BLUE" : "NO WIFI"); }
      uiDirty(); redrawIn(200);
      return;
    }
    if (state != 1) return;
    if (jsWantsExit()) { uiBack(); return; }
    // messages and answers for the app
    char m[1024];
    while (netAppMsg(m, sizeof(m))) jsMsg(m);
    int fid, fst; char* body;
    while (netFetchDone(fid, fst, body)) { jsFetched(fid, fst, body); free(body); }
    uint32_t now = msNow(), every = 1000 / jsFps();
    if (now - last >= every) uiDirty();
    redrawIn(every);
  }
  void draw() override {
    if (state == 1) {
      uint32_t now = msNow(), dt = now - last; last = now;
      if (dt > 250) dt = 250;
      if (!jsFrame(dt, err, sizeof(err))) { jsStop(); state = 2; snd(SND_ERR); uiDirty(); }
      return;
    }
    char p[48]; snprintf(p, sizeof(p), "C:\\WEB>%s", id);
    pathLine(p);
    if (state == 0) {
      drawTextC(FT, 240, 190, "LOADING FROM ERROR.BLUE...", C_FG);
      meter(90, 236, 300, 30, (int)((msNow() - startAt) / 60 % 100), true);
    } else {
      dosBox(15, 110, 450, 250, C_RED);
      drawTextC(FT, 240, 126, "THE APP STOPPED", C_RED);
      char l[6][64]; int n = wrapText(err, 26, l, 6);
      for (int i = 0; i < n; i++) drawTextC(FT, 240, 170 + i * 30, l[i], C_FG, TX_FLAT);
    }
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {"R", "Again", [this] { state = 0; err[0] = 0; startAt = msNow(); }}});
  }
  bool ev(const Ev& e) override {
    if (state != 1) return false;
    switch (e.t) {
      case EV_DOWN: jsTouch("down", e.x, e.y); break;
      case EV_MOVE: jsTouch("move", e.x, e.y); break;
      case EV_UP: jsTouch("up", e.x, e.y); break;
      case EV_TAP: jsTouch("tap", e.x, e.y); break;
      case EV_SWIPE:
        if (e.dir == SW_RIGHT && e.x0 < 40) { uiBack(); return true; }     // from the left edge: always back
        jsSwipe(e.dir == SW_LEFT ? "left" : e.dir == SW_RIGHT ? "right" : e.dir == SW_UP ? "up" : "down"); break;
      case EV_KEY: {
        static const char* K[] = {"", "prev", "next", "ok", "back"};
        if (e.key <= K_BACK && jsKey(K[e.key])) return true;
        if (e.key == K_BACK) { uiBack(); return true; }
        break;
      }
    }
    uiDirty();
    return true;
  }
};
Screen* makeWebApp(const char* id, const char* name) { return new WebApp(id, name); }

// ================================================================== LIVE: what the phone sends
struct Live : Screen {
  bool info = true; uint32_t shownSeq = 0;
  const char* name() override { return "LIVE"; }
  bool fullScreen() override { return true; }
  bool keepAwake() override { return msNow() - S.vidAt < 5000 || msNow() - S.audAt < 5000; }
  void enter() override { netReportProg("LIVE"); }
  void tick() override {
    if (S.vidSeq != shownSeq) { shownSeq = S.vidSeq; uiDirty(); }
    // nothing came for a while: close by itself
    uint32_t last = S.vidAt > S.audAt ? S.vidAt : S.audAt;
    if (last && msNow() - last > 20000) { uiBack(); return; }
    redrawIn(500);
  }
  void draw() override {
    bool vid = vidBuf && msNow() - S.vidAt < 4000;
    if (vid) memcpy(fb, vidBuf, SCR_W * SCR_H * 2);
    else {
      gfxClear();
      drawTextC(FB, 240, 150, msNow() - S.audAt < 3000 ? "SOUND" : "LIVE", C_FG);
      drawTextC(FT, 240, 210, msNow() - S.audAt < 3000 ? "from your phone" : "waiting for the phone...", C_DIM, TX_FLAT);
      // a sound meter
      int lv = S.audLevel;
      for (int i = 0; i < 20; i++) {
        int h = 10 + i * 6;
        fillRect(40 + i * 20, 400 - h, 14, h, i * 5 < lv ? C_FG : mix(C_BG, C_DIM, 40));
      }
    }
    if (info) {
      bool on = (msNow() / 500) % 2;
      fillRect(20, 20, 96, 32, C_BLACK);
      fillCircle(36, 36, 7, on ? C_RED : mix(C_BLACK, C_RED, 40));
      drawText(FT, 50, 20, "LIVE", C_WHITE, TX_FLAT);
      if (msNow() - S.audAt < 2000) { fillRect(404, 20, 56, 32, C_BLACK); drawText(FT, 410, 20, "\xE2\x99\xAA", C_WHITE, TX_FLAT); fillRect(430, 44 - S.audLevel / 5, 6, S.audLevel / 5 + 2, C_WHITE); }
      redrawIn(500);
    }
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_TAP) { info = !info; uiDirty(); return true; }
    if (e.t == EV_KEY && (e.key == K_BACK || e.key == K_OK)) { uiBack(); return true; }
    return false;
  }
};
Screen* makeLive() { return new Live(); }

// ================================================================== what the control page asks for
void appsLoop() {
  musicTick();
  static uint32_t seenApp = 0, seenVid = 0, seenAud = 0, seenHome = 0, seenFile = 0;
  // a new firmware is ready: say so (once per version), UPDATE NOW or LATER
  static char askedVer[16] = "";
  if (S.upd == UPD_NEW && cfg.updAsk && S.updVer[0] && strcmp(askedVer, S.updVer) && uiAwake() && !kbIsOpen()) {
    snprintf(askedVer, sizeof(askedVer), "%s", S.updVer);
    char t[200];
    snprintf(t, sizeof(t), "ERROR-CUBE %s is ready.\n%s%s", S.updVer, S.updNotes[0] ? S.updNotes : "",
             cfg.autoUpd ? "\n(or it installs when the screen is off)" : "");
    ask("NEW FIRMWARE", t, {"UPDATE NOW", "LATER"}, [](int i) { if (i == 0) { netUpdateInstall(); openProg("UPDATE"); } });
    snd(SND_MSG);
  }
  // the last start was a crash: say so once (the details go to INFO and the phone's STATUS)
  if (S.crashAt == 1 && uiAwake() && !kbIsOpen()) {
    S.crashAt = 2;
    char t[300]; snprintf(t, sizeof(t), "The cube restarted after a problem:\n%s", S.crash);
    ask("LAST TIME", t, {"OK"}, nullptr);
  }
  if (S.cfgRestored == 1 && uiAwake()) { S.cfgRestored = 2; cfgChanged(); toast("SETTINGS + WIFI RESTORED FROM FILE", 2500); }
  static uint32_t seenOpen = 0;
  if (S.openAt && S.openAt != seenOpen) {             // the phone: "open this on the cube"
    seenOpen = S.openAt;
    uiWake();
    char p[200]; snprintf(p, sizeof(p), "%s", S.openPath);
    if (!openFile(p)) toast("NOTHING ON THE CUBE OPENS THAT", 2200);
  }
  if (S.fileAt && S.fileAt != seenFile) {             // a file arrived from the phone
    seenFile = S.fileAt;
    char t[64]; snprintf(t, sizeof(t), "RECEIVED %s", S.fileName); utf8Cut(t, 28);
    if (uiAwake()) toast(t, 2200);
    snd(SND_FLOPPY);
  }
  if (S.wantAppAt && S.wantAppAt != seenApp) {
    seenApp = S.wantAppAt;
    uiWake();
    static AppInfo* a = (AppInfo*)malloc(sizeof(AppInfo) * 24); int n = a ? netApps(a, 24) : 0;
    const char* nm = S.wantApp;
    for (int i = 0; i < n; i++) if (!strcmp(a[i].id, S.wantApp)) nm = a[i].name;
    uiHome();
    uiOpen(makeWebApp(S.wantApp, nm));
  }
  bool liveOpen = uiCurrent() && !strcmp(uiCurrent()->name(), "LIVE");
  if (S.vidAt && S.vidAt != seenVid) {
    if (!liveOpen && S.vidAt - seenVid > 5000) { uiWake(); uiOpen(makeLive()); }   // a new stream (not every frame)
    seenVid = S.vidAt;
  }
  if (S.audAt && S.audAt != seenAud) {
    if (!liveOpen && msNow() - seenAud > 10000 && msNow() - S.vidAt > 3000) { uiWake(); uiOpen(makeLive()); }
    seenAud = S.audAt;
  }
  if (S.wantHomeAt && S.wantHomeAt != seenHome) { seenHome = S.wantHomeAt; uiWake(); uiHome(); }
  static uint32_t seenCfg = 0, seenWake = 0, seenSleep = 0, seenUpd = 0;
  if (S.cfgAt != seenCfg) { seenCfg = S.cfgAt; cfgChanged(); }
  if (S.wakeAt != seenWake) { seenWake = S.wakeAt; uiWake(); }
  if (S.sleepAt != seenSleep) { seenSleep = S.sleepAt; uiSleep(); }
  // a firmware update is going in: show it
  if (S.upd == UPD_LOADING && seenUpd != 1) {
    seenUpd = 1; uiWake();
    Screen* c = uiCurrent();
    if (!c || strcmp(c->name(), "UPDATE.EXE")) openProg("UPDATE");
  } else if (S.upd != UPD_LOADING) seenUpd = 0;
}
