// p8run.cpp: runs one PICO-8 cart (see p8.h). Board: a task on core 0 with a 28 KB internal stack (Lua's parser
// and calls go deep; the cart's files are on flash, so the stack can't be in PSRAM); everything big is in PSRAM.
#include "p8.h"
#include "core/vm.h"
#include "core/host.h"
#include "core/PicoRam.h"
#include "core/Audio.h"
#include "core/nibblehelpers.h"
#include "../core/sys.h"
#include <string.h>
#include <stdlib.h>
#include <string>
#ifdef ARDUINO
#include <Arduino.h>
#include <esp_heap_caps.h>
static void* big(size_t n) { void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); return p ? p : malloc(n); }
#else
static void* big(size_t n) { return malloc(n); }
std::string simRealPath(const char* p);              // the simulator: /c and /sd are folders on the PC
#endif

extern uint8_t p8InDown, p8InHeld;              // p8host.cpp
static const char BIOS[] = "__FAKE08-DEFAULT.p8";

static Host* host = nullptr;
static PicoRam* mem = nullptr;
static Audio* audio = nullptr;
static Vm* vm = nullptr;
static uint16_t* frames[2] = {nullptr, nullptr};
static volatile uint32_t frameSeq = 0;
static volatile uint8_t held = 0;
static volatile bool running = false, quit = false, paused = false, taskDone = true;
static char err[96] = "";
static uint32_t stepN = 0, fpsAt = 0, fpsN = 0; static int fps = 0;
static uint8_t prevHeld = 0, pendDown = 0;
static int16_t* abuf = nullptr;
static const int P8_STACK = 32768;

// the picture: 128x128 4-bit + the screen palette -> RGB565
static void takeFrame() {
  uint8_t* fb = vm->GetPicoInteralFb();
  uint8_t* map = vm->GetScreenPaletteMap();
  Color* pal = host->GetPaletteColors();
  uint16_t c16[16];
  for (int i = 0; i < 16; i++) { Color c = pal[map[i] & 0x8f]; c16[i] = ((c.Red & 0xF8) << 8) | ((c.Green & 0xFC) << 3) | (c.Blue >> 3); }
  uint16_t* o = frames[(frameSeq + 1) & 1];
  for (int i = 0; i < 128 * 64; i++) { uint8_t b = fb[i]; o[i * 2] = c16[b & 15]; o[i * 2 + 1] = c16[b >> 4]; }
  frameSeq++;
}

// one step: buttons in, the cart's update/draw (60 a second; a 30 fps cart moves every other one), sound out
static void step() {
  // a new press waits until the cart reads the buttons (a 30 fps cart reads them every other step);
  // p8host.cpp's scanInput takes it then
  uint8_t h = held;
  p8InDown |= (h & ~prevHeld) | pendDown;
  p8InHeld = h | pendDown;               // a very quick tap still counts as held once
  prevHeld = h; pendDown = 0;
  bool okStep = vm->Step();
#ifdef P8DEBUG
  if (stepN < 5 || !okStep) printf("step %u ok %d cart %s err %s\n", (unsigned)stepN, okStep, vm->CurrentCartFilename().c_str(), vm->GetBiosError().c_str());
#endif
  (void)okStep;
  stepN++;
  // keep about 100 ms of sound ahead of the speaker (the cube mixes at 44.1 kHz, PICO-8 makes 22.05 kHz)
  int want = (4400 - sndMediaQueued()) / 2;
  if (want > 735) want = 735;
  if (want > 0 && abuf) { audio->FillAudioBuffer(abuf, 0, want); sndMediaPut(abuf, want, 22050); }
  takeFrame();
  fpsN++;
  uint32_t now = msNow();
  if (now - fpsAt >= 1000) { fps = fpsN; fpsN = 0; fpsAt = now; }
  // the cart's own menu "exit", or an error: FAKE-08 goes to its BIOS cart -> we stop
  if (vm->CurrentCartFilename() == BIOS) {
    std::string e = vm->GetBiosError();
    snprintf(err, sizeof(err), "%s", e.c_str());
    running = false;
  }
}

static void freeAll() {
  if (vm) delete vm; else { delete audio; delete mem; }   // the Vm deletes the memory, graphics, input and audio it was given
  vm = nullptr;
  mem = nullptr; audio = nullptr;
  delete host; host = nullptr;
  p8InDown = p8InHeld = 0;
}

// the cart: loaded where the stack is big enough for Lua's parser (the task on the board)
static std::string cartPath;
static bool loadIt() {
  host = new Host();
  host->setUpPaletteColors();
  host->setCartDirectory(cartPath.substr(0, cartPath.rfind('/')));
  mem = new PicoRam();
  audio = new Audio(mem);
  vm = new Vm(host, mem, nullptr, nullptr, audio);
  if (!vm->LoadCart(cartPath, false)) {
    std::string e = vm->GetBiosError();
    snprintf(err, sizeof(err), "%s", e.length() ? e.c_str() : "CAN'T LOAD THIS CART");
    return false;
  }
  vm->vm_run();
  return true;
}
// C++ trouble inside FAKE-08 (out of memory, a bad cart...) ends the cart, not the cube
static bool load() {
  try { return loadIt(); }
  catch (...) { snprintf(err, sizeof(err), "THE CART CAN'T START (NOT ENOUGH MEMORY?)"); return false; }
}
static bool stepSafe() {
  try { step(); return true; }
  catch (...) { snprintf(err, sizeof(err), "THE CART STOPPED (AN ERROR IN THE EMULATOR)"); running = false; return false; }
}
#ifdef ARDUINO
static volatile uint32_t stackLeft = 0;
static void task(void*) {
  if (!load()) running = false;
  TickType_t last = xTaskGetTickCount();
  while (!quit && running) {
    if (!paused) stepSafe(); else vTaskDelay(pdMS_TO_TICKS(20));
    if ((stepN & 63) == 0) stackLeft = uxTaskGetStackHighWaterMark(nullptr);
    // 60 a second if it can; if a step took longer, the next one comes right away
    TickType_t now = xTaskGetTickCount();
    if (now - last < pdMS_TO_TICKS(16)) vTaskDelayUntil(&last, pdMS_TO_TICKS(16)); else { last = now; vTaskDelay(1); }
  }
  freeAll();
  taskDone = true;
  vTaskDelete(nullptr);
}
#endif

bool p8Start(const char* path) {
  if (running || !taskDone) p8Stop();
  err[0] = 0; quit = false; paused = false; held = 0; prevHeld = pendDown = 0; stepN = 0; fps = 0;
  musStop(); vidStop();
  sndMediaClear();
  if (!frames[0]) { frames[0] = (uint16_t*)big(128 * 128 * 2); frames[1] = (uint16_t*)big(128 * 128 * 2); }
  if (!abuf) abuf = (int16_t*)big(735 * 4);
  if (!frames[0] || !frames[1] || !abuf) { snprintf(err, sizeof(err), "NOT ENOUGH MEMORY"); return false; }
  memset(frames[0], 0, 128 * 128 * 2); memset(frames[1], 0, 128 * 128 * 2);
#ifdef ARDUINO
  cartPath = path;
#else
  cartPath = simRealPath(path);
#endif
  running = true;
#ifdef ARDUINO
  taskDone = false;
  // priority 0 (with the idle task): a long cart start can't starve it (the watchdog would restart the cube).
  // The stack is internal RAM (the carts' files are on flash): check there's room for it and for WiFi after it
  if (heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < P8_STACK + 12000) {
    snprintf(err, sizeof(err), "NOT ENOUGH FAST RAM (%u KB FREE)", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
    running = false; taskDone = true;
    return false;
  }
  if (xTaskCreatePinnedToCore(task, "pico8", P8_STACK, nullptr, 0, nullptr, 0) != pdPASS) {
    snprintf(err, sizeof(err), "NOT ENOUGH MEMORY TO START");
    running = false; taskDone = true; freeAll();
    return false;
  }
#else
  if (!load()) { running = false; freeAll(); return false; }
#endif
  return true;
}

void p8Stop() {
  quit = true;
#ifdef ARDUINO
  for (int i = 0; i < 300 && !taskDone; i++) delay(10);
#else
  if (vm) freeAll();
  taskDone = true;
#endif
  running = false;
  sndMediaClear();
}

bool p8Running() { return running; }
const char* p8Error() { return err; }
void p8Buttons(uint8_t h) {
  pendDown |= h & ~held;            // a quick tap between two polls still counts
  held = h;
}
void p8Pause(bool on) { paused = on; sndMediaPause(on); }
void p8Pump() {
#ifndef ARDUINO
  if (running && !paused && vm) stepSafe();
#endif
}
int p8StackLeft() {
#ifdef ARDUINO
  return stackLeft;
#else
  return -1;
#endif
}
const uint16_t* p8Frame(uint32_t* seq) { if (seq) *seq = frameSeq; return frames[frameSeq & 1]; }
int p8Fps() { return fps; }
