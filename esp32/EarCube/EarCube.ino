// EAR-CUBE: speech to text on the Waveshare ESP32-S3-Touch-AMOLED-2.16 (the ERROR-CUBE).
// Its two microphones listen to the talk around it; error.blue writes it down (Whisper).
//   LIVE    the text on the screen (and the page) a few seconds after it's said
//   RECORD  a long session into a text file with times, on the TF card; works without internet (catches up later)
//   its own WiFi (EAR-CUBE-XXXX, always on): a phone opens http://192.168.4.1 for the live text and the transcripts,
//   and sets the home WiFi there (the cube needs the internet for the writing)
// The third firmware for the cube: ERROR-CUBE installs it (UPDATE > FW > EAR-CUBE) and it installs ERROR-CUBE back
// (SETUP > BACK TO ERROR-CUBE). Same settings, WiFi and files.
// How it's built: ERROR-CUBE's own parts, shared (symlinks to ../ErrorCube/src):
//   src/board  the hardware      src/core  drawing, settings      src/ui  screens, lists, dialogs, keyboard
//   src/net    WiFi + the cube's web server (net.cpp, shared), error.blue for EAR-CUBE (enet.cpp)
//   src/ear    EAR-CUBE itself: ear_config.h, mic.cpp (ES7210), scribe.cpp, screens.cpp, web.cpp + web_page.h
// Build + publish: tools/ear-fwpub.sh (see README.md).
#include "src/core/sys.h"
#include "src/ui/ui.h"
#include "src/board/board.h"
#include "src/net/net.h"
#include "src/ear/ear.h"
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
  heap_caps_malloc_extmem_enable(512);           // big allocations to PSRAM: internal RAM for WiFi and TLS
  Serial.begin(115200);
  earCfgLoad();
  boardBegin();                                  // also the speaker codec and the I2S bus (sound.cpp)
  appsRegister();
  uiInit();
  uiOpen(bootScreen());
  uiFrame();
  boardShow();
  sysBrightness(uiBrightNow());
  if (cfg.crt && !cfg.lowGfx) boardCrt(1);
  boardDrives();
  bool mic = micBegin();                         // the ES7210 on the same I2S bus
  Serial.printf("EAR-CUBE %s: microphones %s\n", CUBE_VERSION, mic ? "OK" : "NOT FOUND");
  scribeBegin();
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
  // the screen off: a slower CPU and WiFi saving power, but not while it listens, sends or keeps the hotspot
  bool busy = earMode() != EAR_IDLE || earWaiting() > 0;
  bool low = !uiAwake() && millis() - asleepAt > 3000 && !busy;
  if (uiAwake()) asleepAt = millis();
  if (low != slow) {
    slow = low;
    boardSlow(low || cfg.saver);
    netSleepMode(low && !ecfg.hotspot ? cfg.aodWifi : 0);
  }
  delay(uiAwake() ? 2 : (busy ? 10 : 30));
}
