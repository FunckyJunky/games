// ERROR-CUBE: ERROR-DOS for the Waveshare ESP32-S3-Touch-AMOLED-2.16, linked to error.blue.
// How it's built (each part only talks to the ones under it):
//   src/apps   the programs: menu, CLOCK, CALC, PAINT, SETUP, WHO, CHAT, LINK, web apps, LIVE video...
//   src/ui     screens, touch + buttons, lists, keyboard, dialogs, sleep / wake, glitches
//   src/js     the web app engine (Duktape): JavaScript from error.blue/cube/apps
//   src/core   drawing (gfx), the ERROR-DOS letters (fonts.h), settings + what the board reports (sys.h)
//   src/board  the hardware: screen, touch, power chip, clock, motion, TF card, speaker
//   src/net    WiFi + the cube's web page (net.cpp), error.blue: link, chat, who, updates (cloud.cpp)
// Build + publish: tools/cube-fwpub.sh (see README.md). Screens on a PC: sim/build.sh.
#include "src/core/sys.h"
#include "src/ui/ui.h"
#include "src/board/board.h"
#include "src/net/net.h"

// new firmware has to prove it works: if it crashes before cloudMarkGood(), the cube goes back to the old one
bool verifyRollbackLater() { return true; }

// the interface runs in loop(): web apps (JavaScript) need room
SET_LOOP_TASK_STACK_SIZE(24 * 1024);

static void netTask(void*) {
  for (;;) {
    netStep();
    cloudStep();
    vTaskDelay(pdMS_TO_TICKS(4));
  }
}

void setup() {
  Serial.begin(115200);
  boardBegin();
  appsRegister();
  uiInit();
  uiOpen(cfg.boot == 0 && cfg.intro ? introScreen() : bootScreen());
  uiFrame();
  boardShow();
  sysBrightness(uiBrightNow());
  if (cfg.crt) boardCrt(1);
  boardDrives();
  netBegin();
  cloudBegin();
  xTaskCreatePinnedToCore(netTask, "net", 16384, nullptr, 1, nullptr, 0);
}

void loop() {
  static int lastBright = -1;
  static bool good = false, slow = false;
  static uint32_t asleepAt = 0;
  boardPoll();
  uiLoop();
  if (crtWant < 0) { crtWant = 0; boardCrt(-1); }           // going to sleep: fold the picture away first
  int b = uiBrightNow();
  if (b != lastBright) { sysBrightness(b); lastBright = b; }
  if (uiFrame()) {
    if (crtWant > 0) { crtWant = 0; boardCrt(1); }
    boardShow();
  }
  // the new firmware ran for 30 s: keep it
  if (!good && millis() > 30000) { good = true; cloudMarkGood(); }
  // asleep for a while: a slower CPU (WiFi still works)
  bool want = !uiAwake() && !uiAod() ? millis() - asleepAt > 10000 : cfg.saver;
  if (uiAwake()) asleepAt = millis();
  if (want != slow) { slow = want; boardSlow(slow); }
  delay(uiAwake() ? 2 : 20);
}
