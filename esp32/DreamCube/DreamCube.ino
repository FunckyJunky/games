// DREAM-CUBE: speech to little 3D worlds on the Waveshare ESP32-S3-Touch-AMOLED-2.16 (the ERROR-CUBE).
// Its two microphones listen; error.blue writes down what's said (Whisper) and turns it into a scene (an AI picks
// the place, the time, the weather, the mood and the things from a library of ~120 low-poly models); the cube draws
// it in 3D, animated: things walk, fly, dance, sleep; rain falls, hearts float up, the camera circles slowly.
//   the DREAM view    the scene, its title, the words it heard. Drag: turn the camera. Tap: menu. - / +: older / newer
//   error.blue/dream  the control page: the scenes, start / pause, the screen, settings, type words to dream
//   WiFi              no WiFi yet: the cube shows the networks it finds; pick one, type the password. Only when
//                     it's connected does DREAM-CUBE start.
// The fourth firmware for the cube: ERROR-CUBE installs it (UPDATE > FW Other > DREAM-CUBE) and it installs ERROR-CUBE
// back (SETUP > BACK TO ERROR-CUBE). Same settings, WiFi and files. Or with USB: flash.bat.
// How it's built: ERROR-CUBE's own parts, shared (symlinks to ../ErrorCube/src):
//   src/board  the hardware      src/core  drawing, settings      src/ui  screens, lists, dialogs, keyboard
//   src/net    WiFi + the cube's web server (net.cpp, shared), error.blue for DREAM-CUBE (enet.cpp)
//   src/dream  DREAM-CUBE itself: the 3D (r3d, models, scene), listening (listen.cpp + EAR-CUBE's mic.cpp), screens
// Build + publish: tools/dream-fwpub.sh (see README.md).
#include "src/core/sys.h"
#include "src/ui/ui.h"
#include "src/board/board.h"
#include "src/net/net.h"
#include "src/dream/dream.h"
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
  dreamCfgLoad();
  boardBegin();                                  // also the speaker codec and the I2S bus (sound.cpp)
  appsRegister();
  uiInit();
  uiOpen(bootScreen());
  uiFrame();
  boardShow();
  sysBrightness(uiBrightNow());
  boardDrives();
  bool mic = micBegin();                         // the ES7210 on the same I2S bus
  Serial.printf("DREAM-CUBE %s: microphones %s\n", CUBE_VERSION, mic ? "OK" : "NOT FOUND");
  dreamBegin();
  netBegin();
  if (S.portal) netPortal(false);                // no phone-setup hotspot: WiFi is picked on the cube itself
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
  // the screen off: a slower CPU; WiFi only dozes while it listens (the words have to get to error.blue)
  bool low = !uiAwake() && millis() - asleepAt > 3000;
  if (uiAwake()) asleepAt = millis();
  if (low != slow) {
    slow = low;
    boardSlow(low && !dreamListening() ? true : cfg.saver);
    netSleepMode(!low ? 0 : dreamListening() || dcfg.link ? 1 : cfg.aodWifi);
  }
  delay(uiAwake() ? 1 : 20);
}
