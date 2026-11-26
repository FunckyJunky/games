// PICO-CUBE: a PICO-8 player for the Waveshare ESP32-S3-Touch-AMOLED-2.16 (the ERROR-CUBE).
// It starts straight into the carts: on the cube (C:\pico8), the TF card (A:\PICO8) and error.blue/pico8.
// The second firmware for the cube: ERROR-CUBE installs it (UPDATE > PICO-CUBE) and it installs ERROR-CUBE back
// (SETUP > BACK TO ERROR-CUBE). Both keep the same settings, WiFi and files.
// How it's built: ERROR-CUBE's own parts, shared (symlinks to ../ErrorCube/src):
//   src/board  the hardware      src/core  drawing, settings      src/ui  screens, lists, dialogs, keyboard
//   src/p8     the PICO-8 emulator (FAKE-08 + z8lua, MIT)
//   src/net    WiFi + the cube's web page (net.cpp, shared), error.blue for PICO-CUBE (pnet.cpp)
//   src/pico   PICO-CUBE itself: pico_config.h (name, version, colours), pico.cpp (the screens)
// Build + publish: tools/pico-fwpub.sh (see README.md).
#include "src/core/sys.h"
#include "src/ui/ui.h"
#include "src/board/board.h"
#include "src/net/net.h"
#include <esp_heap_caps.h>

// new firmware has to prove it works: if it crashes before cloudMarkGood(), the cube goes back to the old one
bool verifyRollbackLater() { return true; }

SET_LOOP_TASK_STACK_SIZE(16 * 1024);

static void netTask(void*) {
  for (;;) {
    netStep();
    cloudStep();
    vTaskDelay(pdMS_TO_TICKS(4));
  }
}

void setup() {
  heap_caps_malloc_extmem_enable(512);           // big allocations to PSRAM: internal RAM for WiFi, TLS, the cart task
  Serial.begin(115200);
  boardBegin();
  appsRegister();
  uiInit();
  uiOpen(bootScreen());
  uiFrame();
  boardShow();
  sysBrightness(uiBrightNow());
  if (cfg.crt && !cfg.lowGfx) boardCrt(1);
  boardDrives();
  netBegin();
  cloudBegin();
  xTaskCreatePinnedToCore(netTask, "net", 12288, nullptr, 1, nullptr, 0);
}

void loop() {
  static int lastBright = -1;
  static bool good = false, slow = false;
  static uint32_t asleepAt = 0;
  boardPoll();
  uiLoop();
  if (crtWant < 0) { crtWant = 0; boardCrt(-1); }
  int b = uiBrightNow();
  if (b != lastBright) { sysBrightness(b); lastBright = b; }
  if (uiFrame()) {
    if (crtWant > 0) { crtWant = 0; boardCrt(1); }
    boardShow();
  }
  if (!good && millis() > 30000) { good = true; cloudMarkGood(); }
  bool low = !uiAwake() && millis() - asleepAt > 3000;
  if (uiAwake()) asleepAt = millis();
  if (low != slow) { slow = low; sysLowPower(low); }
  delay(uiAwake() ? 2 : 30);
}
