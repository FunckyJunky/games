// HOME FRONT: a screen at home that shows one web app from error.blue, controlled from a phone at
// error.blue/thuisfront. Board: ESP32-S3-LCD-1.47 (or 1.47B), the same board as pocket.DOS.
//
//  - No WiFi yet (or none of the saved ones found): it makes the open WiFi HOMEFRONT-XXXX with a setup page
//    at http://192.168.4.1 where you pick your WiFi. It remembers up to 8 networks.
//  - Connected: it says CONNECTED for 3 seconds, then loads its web app from error.blue/thuisfront/dev.
//  - It remembers the last web app and its settings, and shows them again after a restart, also offline.
//  - BOOT button: press = info (name, WiFi, IP), hold 3 s = open the setup WiFi.
//
// Arduino IDE: board "ESP32S3 Dev Module", USB CDC On Boot: Enabled,
// Partition Scheme: Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS) (two app slots: from 1.4 it updates itself over WiFi),
// PSRAM: OPI PSRAM. Libraries: ArduinoJson 7. Or put it on from the browser: error.blue/thuisfront/flash
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "esp_mac.h"
#include <sys/time.h>
#include <time.h>
#include "config.h"
#include "lcd.h"
#include "screen.h"
#include "hf.h"
#include "motion.h"

static ScreenInfo info;
static uint32_t bootUntil = 0, infoUntil = 0, lastFrame = 0, t0Text = 0, frames = 0, fpsAt = 0, setupForcedAt = 0;
static int lastBl = -1, lastLed = -1;
static String textKey;
static char bootLines[10][40];
static const char* bootPtr[10];
static int bootN = 0;

static void led(uint8_t r, uint8_t g, uint8_t b) {
  int v = r << 16 | g << 8 | b;
  if (v == lastLed) return;
  lastLed = v;
  rgbLedWrite(PIN_RGB, r, g, b);
}

static void button(uint32_t now) {
  static uint32_t downAt = 0;
  static bool held = false;
  bool down = digitalRead(PIN_BUTTON) == LOW;
  if (down && !downAt) { downAt = now; held = false; }
  if (down && downAt && !held && now - downAt > 3000) {
    held = true; G.wantSetup = true; setupForcedAt = now; infoUntil = 0;
    Serial.println("BUTTON: setup WiFi");
  }
  if (!down && downAt) {
    if (!held && now - downAt > 30) infoUntil = infoUntil > now ? 0 : now + 6000;
    downAt = 0;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  G.lock = xSemaphoreCreateMutex();
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);              // the chip's own address (WiFi isn't started yet)
  snprintf(G.id, sizeof(G.id), "HF-%02X%02X%02X", mac[3], mac[4], mac[5]);
  snprintf(G.ap, sizeof(G.ap), HF_AP_PREFIX "%02X%02X", mac[4], mac[5]);
  static const char* const WHY[] = { "UNKNOWN", "POWER ON", "RESET PIN", "SOFTWARE", "CRASH", "INT WATCHDOG", "TASK WATCHDOG",
                                     "WATCHDOG", "DEEP SLEEP", "BROWNOUT", "SDIO", "USB", "JTAG", "EFUSE", "PWR GLITCH", "CPU LOCKUP" };
  int why = (int)esp_reset_reason();
  const char* whyS = why >= 0 && why < 16 ? WHY[why] : "UNKNOWN";
  Serial.printf("\nHOME FRONT %s  %s  (start: %s, flash %u MB)\n", HF_VERSION, G.id, whyS, (unsigned)(ESP.getFlashChipSize() >> 20));

  lcdBegin(prefFlip());
  G.imu = motionBegin();
  scrBegin();
  lcdBacklight(80);
  if (!LittleFS.begin(true)) Serial.println("FS: no flash file system");
  linkLoadSaved();

  Net nets[MAX_NETS];
  int nn = netsLoad(nets);
  snprintf(bootLines[bootN++], 40, "HOME FRONT BIOS %s", HF_VERSION);
  snprintf(bootLines[bootN++], 40, "CPU   ESP32-S3 %d MHZ", (int)getCpuFrequencyMhz());
  snprintf(bootLines[bootN++], 40, "PSRAM %d KB %s", (int)(ESP.getPsramSize() / 1024), ESP.getPsramSize() ? "OK" : "MISSING!");
  snprintf(bootLines[bootN++], 40, "LCD   320x172 (BACKLIGHT %d)", lcdBacklightPin());
  snprintf(bootLines[bootN++], 40, "ID    %s", G.id);
  snprintf(bootLines[bootN++], 40, "WIFI  %d SAVED NETWORK%s", nn, nn == 1 ? "" : "S");
  snprintf(bootLines[bootN++], 40, "APP   %s", !G.haveState || !G.st.app ? "NONE YET" : G.st.app == APP_CLOCK ? "CLOCK (SAVED)" : G.st.app == APP_ETA ? "ETA (SAVED)" : "COMM (SAVED)");
  snprintf(bootLines[bootN++], 40, "MOTION %s", G.imu ? "QMI8658 OK" : "NO SENSOR");
  snprintf(bootLines[bootN++], 40, "START %s", whyS);
  snprintf(bootLines[bootN++], 40, "C:\\>HOMEFRNT");
  for (int i = 0; i < bootN; i++) bootPtr[i] = bootLines[i];
  bootUntil = millis() + 1500;

  netBegin();
}

// CLOCK / COMM "on when moved": dark until the board is moved (or the button is pressed, or the phone changes something,
// or video is coming in: busy), then on for mtime seconds. Without a motion sensor it stays on.
static uint32_t movedAt = 0, movedRev = 0;
static float moveNow = 0;
static bool moveAsleep(uint32_t rev, int msens, int mtime, uint32_t now, bool busy) {
  if (moveNow < 0) return false;
  if (rev != movedRev) { movedRev = rev; movedAt = now; }
  if (busy || moveNow >= motionThreshold(msens) || infoUntil > now || !movedAt) movedAt = now;
  return now - movedAt >= (uint32_t)mtime * 1000UL;
}

void loop() {
  uint32_t now = millis();
  button(now);
  webLoop();
  { float m = motionPoll(); if (m < 0) moveNow = -1; else if (m > moveNow) moveNow = m; }   // the biggest since the last frame
  if (G.flipChanged) { G.flipChanged = false; lcdFlip(prefFlip()); }
  if (now - lastFrame < 33) { delay(1); return; }   // about 30 frames a second
  lastFrame = now;

  strlcpy(info.id, G.id, sizeof(info.id));
  static uint32_t nameAt = 0;
  if (!nameAt || now - nameAt > 1000) { nameAt = now; String nm = prefGet("name", G.id); strlcpy(info.name, nm.c_str(), sizeof(info.name)); }
  strlcpy(info.wifi, G.staUp ? G.wifi : "", sizeof(info.wifi));
  strlcpy(info.ip, G.staUp ? G.ip : "NO WIFI", sizeof(info.ip));
  strlcpy(info.ap, G.ap, sizeof(info.ap));
  info.online = G.wsUp;

  bool scan = false;
  const uint16_t* plane = nullptr;   // the colour picture behind the screen (photo / colour video)
  int bl = 80;
  uint8_t lr = 0, lg = 0, lb = 0;
  float tNow = now / 1000.0f;
  xSemaphoreTake(G.lock, portMAX_DELAY);
  const HfState& st = G.st;
  bool appOn = G.haveState && st.app;
  scrEffects = !G.haveState || st.fx;
  bool setupScreen = G.apOn && (!G.everStaUp ? (!appOn || G.apClients > 0 || now - G.apSince < 60000)
                                             : (G.apClients > 0 || (setupForcedAt && now - setupForcedAt < 300000)));
  if (G.staUp && setupForcedAt && !G.apOn) setupForcedAt = 0;
  if (G.apOn) { float p = 0.5f + 0.5f * sinf(tNow * 2.5f); lb = 8 + p * 40; }

  bool otaShow = G.otaPct >= 0 || (G.otaPct == -2 && now - G.otaAt < 6000);
  if (otaShow) { scrUpdate(tNow, G.otaPct, G.otaMsg); bl = 80; }
  else if ((int32_t)(bootUntil - now) > 0) scrBoot((now - (bootUntil - 1500)) / 1000.0f, bootPtr, bootN);
  else if (G.connectedAt && now - G.connectedAt < 3000) scrConnected((now - G.connectedAt) / 1000.0f, info);
  else if (setupScreen) scrSetup(tNow, info, G.setupMsg);
  else if (!G.everStaUp && G.joining && !appOn) scrJoining(tNow, info, G.joinSsid, G.joinLow);
  else if (appOn && !st.screen) { memset(FB, IX_BLACK, LCD_W * LCD_H); bl = 0; }
  else if (appOn && st.app == APP_CLOCK && st.clock.motion && moveAsleep(st.rev, st.clock.msens, st.clock.mtime, now, false)) { memset(FB, IX_BLACK, LCD_W * LCD_H); bl = 0; }
  else if (appOn && st.app == APP_CLOCK) {
    struct timeval tv; gettimeofday(&tv, nullptr);
    struct tm tmv;
    bool known = tv.tv_sec > 1600000000;           // the time comes from the internet (NTP)
    if (known) localtime_r(&tv.tv_sec, &tmv);
    scrClock(st.clock, known ? &tmv : nullptr, tv.tv_usec / 1000);
    scan = st.scan && st.fx;
    bl = st.bright;
    if (!G.wsUp && G.everStaUp) scrOfflineDot(now);
  } else if (appOn && st.app == APP_ETA) {
    struct timeval tv; gettimeofday(&tv, nullptr);
    bool known = tv.tv_sec > 1600000000;
    struct tm toTm, fromTm;
    time_t tt = st.eta.to, tf = st.eta.from;
    if (known && tt) localtime_r(&tt, &toTm);
    if (known && tf) localtime_r(&tf, &fromTm);
    scrEta(st.eta, (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000, known, known && tt ? &toTm : nullptr, known && tf ? &fromTm : nullptr);
    scan = st.scan && st.fx;
    bl = st.bright;
    if (!G.wsUp && G.everStaUp) scrOfflineDot(now);
  } else if (appOn && st.app == APP_FOX) {
    // FOX: the sensor in screen directions (as SET UP MOTION found), the fox feels it, and is drawn
    static FoxSim fox;
    static uint32_t foxAt = 0;
    float a[3] = { 0, 0, 0 }, w[3] = { 0, 0, 0 }, f[3], g[3];
    bool imu = motionRead(a, w);
    if (imu) for (int i = 0; i < 3; i++) { f[i] = st.fox.sg[i] * a[st.fox.ax[i]]; g[i] = st.fox.sg[i] * w[st.fox.ax[i]]; }
    float dt = foxAt ? (now - foxAt) / 1000.0f : 0;
    foxAt = now;
    // its own position, smoothed, before CENTRE: the phone's CENTRE button takes this as the new 0
    static float lp[6] = { 0, 1, 0, 0, 0, 0 };
    if (imu) { float k = 1 - expf(-3 * dt); for (int i = 0; i < 3; i++) { lp[i] += (f[i] - lp[i]) * k; lp[3 + i] += (g[i] - lp[3 + i]) * k; } }
    memcpy(G.foxU, lp, sizeof(lp));
    // CENTRE: worked out again when it changes; then the fox doesn't take the change for a jolt
    static FoxZero Z;
    static float zKey[7] = { -9 };
    float key[7] = { st.fox.hasZero ? 1.0f : 0.0f };
    memcpy(key + 1, st.fox.zero, sizeof(st.fox.zero));
    bool newZero = memcmp(key, zKey, sizeof(key)) != 0;
    if (newZero) { memcpy(zKey, key, sizeof(key)); if (st.fox.hasZero) foxZeroOf(st.fox.zero, Z); else Z = FoxZero(); }
    if (imu) { foxZeroApply(Z, f, g); if (newZero) memcpy(fox.gl, f, sizeof(fox.gl)); }
    foxStep(fox, imu ? f : nullptr, imu ? g : nullptr, dt);
    FoxPose fp;
    foxPoseOf(fox, fp);
    scrFox(fp, tNow, st.fox.pal);
    G.foxPose = fp; memcpy(G.foxRaw, a, sizeof(G.foxRaw)); G.foxRawOk = imu; G.foxHave = true;
    scan = st.scan && st.fx;
    bl = st.bright;
    if (!G.wsUp && G.everStaUp) scrOfflineDot(now);
  } else if (appOn && st.app == APP_COMM && st.comm.motion &&
             moveAsleep(st.rev, st.comm.msens, st.comm.mtime, now, (st.comm.mode == M_VIDEO || st.comm.mode == M_BOTH) &&
                        ((G.frameAt && now - G.frameAt < 1500) || (G.colorAt && now - G.colorAt < 1500)))) {
    memset(FB, IX_BLACK, LCD_W * LCD_H); bl = 0;
  } else if (appOn) {
    String key = String(st.tb) + "|" + st.comm.anim + "|" + st.comm.speed;
    if (key != textKey) { textKey = key; t0Text = now; }
    if (G.textAt && now - G.textAt < 300) lg = 60;   // new text: a green blink
    const uint16_t* pic = G.photo;
    if (G.animN > 1 && G.animTotal) {             // an animated photo: the frame for now (it plays also with effects off)
      uint32_t t = (now - G.animT0) % G.animTotal;
      int i = 0;
      while (i < G.animN - 1 && t >= G.animDelay[i]) { t -= G.animDelay[i]; i++; }
      pic = G.anim[i];
    }
    Media md = { G.frame, G.fw, G.fh, G.frameAt, G.color, G.colorAt, pic };
    plane = scrComm(st.comm, G.text, md, (now - t0Text) / 1000.0f, now);
    scan = st.scan && st.fx;
    bl = st.bright;
    if (!G.wsUp && G.everStaUp) scrOfflineDot(now);
  } else {
    if (G.haveState && !st.screen) { memset(FB, IX_BLACK, LCD_W * LCD_H); bl = 0; }
    else scrIdle(scrEffects ? tNow : STILL_T, info);
    if (G.haveState) bl = st.bright;
  }
  G.pushing = plane;
  xSemaphoreGive(G.lock);
  if (G.apOn && !setupScreen && (now / 1000) % 20 < 4) {
    char b[60]; snprintf(b, sizeof(b), "NO WIFI. SETUP: JOIN %s", G.ap); scrBanner(b);
  }
  if (infoUntil > now) { scrInfoBox(info, "HOLD BUTTON 3 S = WIFI SETUP"); if (bl == 0) bl = 60; }
  scrFlush(scan, plane);
  G.pushing = nullptr;
  if (bl != lastBl) { lcdBacklight(bl); lastBl = bl; }
  if (moveNow > 0) moveNow = 0;
  led(lr, lg, lb);

  frames++;
  if (now - fpsAt >= 2000) { G.renderFps = frames * 1000 / (now - fpsAt); frames = 0; fpsAt = now; }
}
