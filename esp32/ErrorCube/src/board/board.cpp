// board.cpp: see board.h. Pins in core/config.h.
#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <SD.h>
#include <SPI.h>
#include <FFat.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_mac.h>
#include "Arduino_GFX_Library.h"
#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"
#include "TouchDrvCSTXXX.hpp"
#include "SensorQMI8658.hpp"
#include "SensorPCF85063.hpp"
#include "../ui/ui.h"
#include "board.h"
#include "sound.h"

Cfg cfg;
SysState S;
uint32_t msNow() { return millis(); }

static Arduino_DataBus* bus = nullptr;
static Arduino_CO5300* gfx = nullptr;
static XPowersPMU pmu;
static TouchDrvCST92xx touch;
static SensorQMI8658 qmi;
static SensorPCF85063 rtc;
static uint16_t* shadow = nullptr;     // what the panel shows now (after turning)
static uint16_t* phys = nullptr;       // fb turned for cfg.rot
static bool forceFull = true;
static int rotNow = 0, autoRot = 0;
static bool sdSpi = false;

// ------------------------------------------------------------------ settings in flash
static Preferences prefs;
static uint32_t cfgDirtyAt = 0;
static void cfgLoad() {
  prefs.begin("cube", false);
  Cfg c;
  if (prefs.getBytesLength("cfg") == sizeof(Cfg)) {
    prefs.getBytes("cfg", &c, sizeof(c));
    if (c.magic == Cfg().magic) cfg = c;
  }
}
void cfgSave() { cfgDirtyAt = millis() | 1; }
static void cfgFlush(bool now = false) {
  if (cfgDirtyAt && (now || millis() - cfgDirtyAt > 1500)) { prefs.putBytes("cfg", &cfg, sizeof(cfg)); cfgDirtyAt = 0; }
}
void netForgetAll();
void cfgReset() { prefs.clear(); netForgetAll(); cfg = Cfg(); }

// ------------------------------------------------------------------ time
static void applyTz() {
  static int last = -1;
  if (last == cfg.tzIdx) return;
  last = cfg.tzIdx;
  setenv("TZ", TIMEZONES[cfg.tzIdx % N_TZ].posix, 1);
  tzset();
}
bool sysLocalTime(struct tm& t) {
  applyTz();
  time_t now = time(nullptr);
  if (now < 1704067200) return false;          // before 2024: never set
  localtime_r(&now, &t);
  return true;
}
void boardSetRtcFromSystem() {
  if (!S.rtc) return;
  time_t now = time(nullptr);
  struct tm u; gmtime_r(&now, &u);
  rtc.setDateTime(RTC_DateTime(u));
}
void sysSetTime(int y, int mo, int d, int h, int mi, int s) {
  applyTz();
  struct tm t = {}; t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d; t.tm_hour = h; t.tm_min = mi; t.tm_sec = s; t.tm_isdst = -1;
  time_t e = mktime(&t);
  struct timeval tv = {e, 0};
  settimeofday(&tv, nullptr);
  boardSetRtcFromSystem();
}
static void rtcToSystem() {
  RTC_DateTime d = rtc.getDateTime();
  if (d.getYear() < 2024 || d.getYear() > 2099) return;
  struct tm u = {}; u.tm_year = d.getYear() - 1900; u.tm_mon = d.getMonth() - 1; u.tm_mday = d.getDay();
  u.tm_hour = d.getHour(); u.tm_min = d.getMinute(); u.tm_sec = d.getSecond();
  // the RTC keeps UTC: turn it into seconds without the time zone
  setenv("TZ", "UTC0", 1); tzset();
  time_t e = mktime(&u);
  struct timeval tv = {e, 0};
  settimeofday(&tv, nullptr);
  setenv("TZ", TIMEZONES[cfg.tzIdx % N_TZ].posix, 1); tzset();
}

// ------------------------------------------------------------------ small things
uint32_t freeHeap() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }
uint32_t freePsram() { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM); }
uint32_t uptimeSecs() { return (uint32_t)(esp_timer_get_time() / 1000000ULL); }
const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "POWER ON";
    case ESP_RST_SW: return "RESTART";
    case ESP_RST_PANIC: return "CRASH";
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_BROWNOUT: return "LOW POWER";
    case ESP_RST_DEEPSLEEP: return "DEEP SLEEP";
    case ESP_RST_USB: return "USB";
    default: return "OTHER";
  }
}
void sysChargeCurrent(int ma) {
  if (!S.pmu) return;
  uint8_t v = ma <= 100 ? XPOWERS_AXP2101_CHG_CUR_100MA : ma <= 200 ? XPOWERS_AXP2101_CHG_CUR_200MA : ma <= 300 ? XPOWERS_AXP2101_CHG_CUR_300MA
            : ma <= 400 ? XPOWERS_AXP2101_CHG_CUR_400MA : ma <= 500 ? XPOWERS_AXP2101_CHG_CUR_500MA : ma <= 700 ? XPOWERS_AXP2101_CHG_CUR_700MA
            : XPOWERS_AXP2101_CHG_CUR_1000MA;
  pmu.setChargerConstantCurr(v);
}
void sysApplyRotation() { forceFull = true; }
void sysRestart() { cfgFlush(true); sndStop(); delay(200); ESP.restart(); }
void sysPowerOff() {
  cfgFlush(true);
  boardCrt(-1);
  boardPanel(false);
  delay(100);
  if (S.pmu) pmu.shutdown();
  esp_deep_sleep_start();                     // no power chip: sleep for ever (the reset button wakes it)
}
void boardSlow(bool slow) { setCpuFrequencyMhz(slow ? 80 : 240); }

// ------------------------------------------------------------------ the panel
static int lastBright = -1;
void sysBrightness(int pct) {
  if (!gfx) return;
  if (pct <= 0) { boardPanel(false); lastBright = 0; return; }
  if (lastBright <= 0) boardPanel(true);
  // brightness is not linear to the eye
  float f = pct / 100.0f;
  int v = (int)(255 * f * f * 0.85f + 255 * f * 0.15f);
  if (v < 3) v = 3;
  gfx->setBrightness(v);
  lastBright = pct;
}
void boardPanel(bool on) {
  if (!gfx) return;
  if (on) { gfx->displayOn(); } else { gfx->setBrightness(0); gfx->displayOff(); }
}

static void turn(const uint16_t* src, uint16_t* dst, int r) {
  // logical (x, y) -> physical: r1 (479-y, x), r2 (479-x, 479-y), r3 (y, 479-x)
  if (r == 2) { for (int i = 0, n = SCR_W * SCR_H; i < n; i++) dst[n - 1 - i] = src[i]; return; }
  for (int y = 0; y < SCR_H; y++) {
    const uint16_t* s = src + y * SCR_W;
    if (r == 1) for (int x = 0; x < SCR_W; x++) dst[x * SCR_W + (SCR_W - 1 - y)] = s[x];
    else for (int x = 0; x < SCR_W; x++) dst[(SCR_H - 1 - x) * SCR_W + y] = s[x];
  }
}
static const uint16_t* physical() {
  int r = cfg.rot < 4 ? cfg.rot : autoRot;
  if (r != rotNow) { rotNow = r; forceFull = true; }
  if (!r || !phys) return fb;
  turn(fb, phys, r);
  return phys;
}
static void pushRows(const uint16_t* img, int y0, int y1) {
  // the CO5300 wants windows that start on an even row and end on an odd one
  y0 &= ~1; y1 |= 1; if (y1 >= SCR_H) y1 = SCR_H - 1;
  gfx->draw16bitRGBBitmap(0, y0, (uint16_t*)img + y0 * SCR_W, SCR_W, y1 - y0 + 1);
}
void boardShow() {
  if (!gfx) return;
  const uint16_t* img = physical();
  if (!shadow || forceFull) {
    pushRows(img, 0, SCR_H - 1);
    if (shadow) memcpy(shadow, img, SCR_W * SCR_H * 2);
    forceFull = false;
    return;
  }
  // only the rows that changed, in bands
  int y = 0;
  while (y < SCR_H) {
    if (!memcmp(img + y * SCR_W, shadow + y * SCR_W, SCR_W * 2)) { y++; continue; }
    int y0 = y, gap = 0;
    while (y < SCR_H && gap < 8) {
      if (memcmp(img + y * SCR_W, shadow + y * SCR_W, SCR_W * 2)) gap = 0; else gap++;
      y++;
    }
    int y1 = y - 1 - gap;
    if (y1 < y0) y1 = y0;
    pushRows(img, y0, y1);
    int a = y0 & ~1, b = (y1 | 1) < SCR_H ? (y1 | 1) : SCR_H - 1;
    memcpy(shadow + a * SCR_W, img + a * SCR_W, (b - a + 1) * SCR_W * 2);
  }
}

// the CRT animation: the picture folds into a bright line, then a dot (off), or the other way round (on)
void boardCrt(int dir) {
  if (!gfx || !shadow || !phys) return;
  const uint16_t* src = dir > 0 ? physical() : shadow;
  static uint16_t* tmp = nullptr;
  if (!tmp) tmp = (uint16_t*)heap_caps_malloc(SCR_W * SCR_H * 2, MALLOC_CAP_SPIRAM);
  if (!tmp) return;
  if (src == phys) { memcpy(tmp, phys, SCR_W * SCR_H * 2); src = tmp; }   // phys gets reused below
  uint16_t* out = phys;
  const int N = 9;
  for (int k = 0; k <= N; k++) {
    float t = dir < 0 ? (float)k / N : 1.0f - (float)k / N;    // 0 = full picture, 1 = gone
    float sy = t < 0.55f ? 1.0f - t / 0.55f * 0.996f : 0.004f;
    float sx = t < 0.55f ? 1.0f : t < 0.85f ? 1.0f - (t - 0.55f) / 0.3f * 0.88f : 0.12f * (1.0f - (t - 0.85f) / 0.15f);
    int bright = (int)(t * 1.6f * 32); if (bright > 26) bright = 26;
    memset(out, 0, SCR_W * SCR_H * 2);
    int h = (int)(SCR_H * sy); if (h < 2) h = 2;
    int w = (int)(SCR_W * sx); if (w < 2 && t < 1) w = 2;
    int top = (SCR_H - h) / 2, left = (SCR_W - w) / 2;
    if (t < 1)
      for (int yy = 0; yy < h; yy++) {
        const uint16_t* s = src + (yy * SCR_H / h) * SCR_W;
        uint16_t* o = out + (top + yy) * SCR_W + left;
        for (int xx = 0; xx < w; xx++) {
          uint16_t c = s[xx * SCR_W / w];
          o[xx] = bright ? blend(c, 0xFFFF, bright) : c;
        }
      }
    gfx->draw16bitRGBBitmap(0, 0, out, SCR_W, SCR_H);
    delay(dir < 0 ? 18 : 12);
  }
  forceFull = true;
}

// ------------------------------------------------------------------ files: /sd = TF card, /c = the cube's own drive
int fsList(const char* dir, FileEnt* out, int max) {
  DIR* d = opendir(dir);
  if (!d) return -1;
  int n = 0; struct dirent* e;
  while ((e = readdir(d)) && n < max) {
    if (e->d_name[0] == '.') continue;
    snprintf(out[n].name, sizeof(out[n].name), "%s", e->d_name);
    char p[200]; snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
    struct stat st;
    if (stat(p, &st) == 0) { out[n].dir = S_ISDIR(st.st_mode); out[n].size = st.st_size; }
    else { out[n].dir = e->d_type == DT_DIR; out[n].size = 0; }
    n++;
  }
  closedir(d);
  return n;
}
bool fsRead(const char* path, uint8_t* buf, size_t max, size_t& n) {
  FILE* f = fopen(path, "rb"); if (!f) return false;
  n = fread(buf, 1, max, f); fclose(f); return true;
}
static void mkdirs(const char* path) {
  char p[200]; snprintf(p, sizeof(p), "%s", path);
  for (char* s = p + 1; *s; s++) if (*s == '/') { *s = 0; mkdir(p, 0777); *s = '/'; }
}
bool fsWrite(const char* path, const uint8_t* buf, size_t n) {
  mkdirs(path);
  FILE* f = fopen(path, "wb"); if (!f) return false;
  size_t w = fwrite(buf, 1, n, f); fclose(f); return w == n;
}
bool fsRemove(const char* path) { return remove(path) == 0; }
bool fsInfo(const char* drive, uint64_t& total, uint64_t& used) {
  if (!strncmp(drive, "/sd", 3)) {
    if (!S.sd) return false;
    if (sdSpi) { total = SD.totalBytes(); used = SD.usedBytes(); } else { total = SD_MMC.totalBytes(); used = SD_MMC.usedBytes(); }
    return true;
  }
  total = FFat.totalBytes(); used = FFat.usedBytes();
  return true;
}

// ------------------------------------------------------------------ motion
static float rawA[3] = {0, 0, 1}, rawG[3] = {0, 0, 0};
static uint32_t imuAt = 0;
static void imuSample() {
  if (!S.imu || millis() - imuAt < 8) return;
  imuAt = millis();
  float x, y, z;
  if (qmi.getAccelerometer(x, y, z)) { rawA[0] = x; rawA[1] = y; rawA[2] = z; }
  if (qmi.getGyroscope(x, y, z)) { rawG[0] = x; rawG[1] = y; rawG[2] = z; }
}
bool imuRead(float a[3], float g[3]) {
  if (!S.imu) return false;
  imuSample();
  for (int i = 0; i < 3; i++) {
    int m = cfg.axes[i], k = (m < 0 ? -m : m) - 1; float s = m < 0 ? -1.0f : 1.0f;
    if (k < 0 || k > 2) k = i;
    a[i] = rawA[k] * s * 9.81f;
    g[i] = rawG[k] * s;
  }
  return true;
}

// ------------------------------------------------------------------ start-up
void boardBegin() {
  cfgLoad();
  if (!cfg.code[0]) {        // the link code: 6 letters/digits, no look-alikes
    static const char A[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    for (int i = 0; i < 6; i++) cfg.code[i] = A[esp_random() % 32];
    cfg.code[6] = 0;
    cfgSave();
  }
  uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(S.mac, sizeof(S.mac), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  snprintf(S.host, sizeof(S.host), "CUBE-%02X%02X", mac[4], mac[5]);
  if (!cfg.name[0]) { snprintf(cfg.name, sizeof(cfg.name), "%s", S.host); cfgSave(); }

  Wire.begin(PIN_SDA, PIN_SCL, 400000);
  // power chip first: it feeds everything else
  S.pmu = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, PIN_SDA, PIN_SCL);
  if (S.pmu) {
    pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    pmu.clearIrqStatus();
    pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ | XPOWERS_AXP2101_PKEY_LONG_IRQ);
    pmu.setIrqLevelTime(XPOWERS_AXP2101_IRQ_TIME_1S5);
    pmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_6S);
    pmu.enableTemperatureMeasure(); pmu.enableBattDetection(); pmu.enableVbusVoltageMeasure();
    pmu.enableBattVoltageMeasure(); pmu.enableSystemVoltageMeasure();
    sysChargeCurrent(CHARGE_MA[cfg.chgIdx % N_CHARGE]);
  }
  // the screen, black, then the light
  bus = new Arduino_ESP32QSPI(PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3);
  gfx = new Arduino_CO5300(bus, PIN_LCD_RST, 0, SCR_W, SCR_H, 0, 0, 0, 0);
  gfx->begin();
  bus->writeC8D8(0x36, 0xA0);             // as in Waveshare's examples: the right way up in the case
  gfx->setBrightness(0);
  gfx->fillScreen(0);
  shadow = (uint16_t*)heap_caps_malloc(SCR_W * SCR_H * 2, MALLOC_CAP_SPIRAM);
  phys = (uint16_t*)heap_caps_malloc(SCR_W * SCR_H * 2, MALLOC_CAP_SPIRAM);
  if (shadow) memset(shadow, 0, SCR_W * SCR_H * 2);
  // touch
  touch.setPins(PIN_TP_RST, PIN_TP_INT);
  S.touch = touch.begin(Wire, CST92XX_SLAVE_ADDRESS, PIN_SDA, PIN_SCL);
  if (S.touch) { touch.setMaxCoordinates(SCR_W, SCR_H); touch.setSwapXY(true); touch.setMirrorXY(true, false); }
  pinMode(PIN_TP_INT, INPUT_PULLUP);
  // motion, clock
  S.imu = qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, PIN_SDA, PIN_SCL);
  if (S.imu) {
    qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_125Hz, SensorQMI8658::LPF_MODE_0);
    qmi.configGyroscope(SensorQMI8658::GYR_RANGE_512DPS, SensorQMI8658::GYR_ODR_112_1Hz, SensorQMI8658::LPF_MODE_3);
    qmi.enableAccelerometer(); qmi.enableGyroscope();
  }
  S.rtc = rtc.begin(Wire, PIN_SDA, PIN_SCL);
  if (S.rtc) rtcToSystem();
  applyTz();
  // buttons
  pinMode(PIN_KEY_PLUS, INPUT_PULLUP);
  pinMode(PIN_KEY_MINUS, INPUT_PULLUP);
  S.chipTemp = temperatureRead();
}

// the drives and the speaker: after the first picture is on the screen (the first start formats drive C:)
void boardDrives() {
  // drives: the cube's own flash (C:), the TF card (A:)
  if (!FFat.begin(true, "/c", 10, "ffat")) Serial.println("C: drive failed");
  mkdir("/c/apps", 0777); mkdir("/c/appdata", 0777);
  pinMode(PIN_SD_CS, OUTPUT); digitalWrite(PIN_SD_CS, HIGH);       // D3 high = SD mode
  SD_MMC.setPins(PIN_SD_SCK, PIN_SD_MOSI, PIN_SD_MISO);
  S.sd = SD_MMC.begin("/sd", true, false, 20000);
  if (!S.sd) {
    static SPIClass spi(HSPI);
    spi.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    S.sd = SD.begin(PIN_SD_CS, spi, 20000000, "/sd");
    sdSpi = S.sd;
  }
  if (S.sd) {
    uint64_t t = sdSpi ? SD.cardSize() : SD_MMC.cardSize();
    snprintf(S.sdInfo, sizeof(S.sdInfo), "%.1f GB", t / 1073741824.0);
  }
  // sound
  S.codec = sndBegin();
}

// ------------------------------------------------------------------ the loop: touch, buttons, power key, battery
struct Btn { int pin; bool down; uint32_t at; bool longDone; int shortKey, longKey; };
static Btn btns[2] = {{PIN_KEY_PLUS, false, 0, false, K_NEXT, K_OK}, {PIN_KEY_MINUS, false, 0, false, K_PREV, K_BACK}};
static bool touchDown = false;
static uint32_t pmuAt = 0, battAt = 0, rotAt = 0;
static float moveRef[3] = {0, 0, 0};

static void touchPoll() {
  if (!S.touch) return;
  // the chip pulls INT low while there's something to read; while a finger is down, read every time
  bool irq = digitalRead(PIN_TP_INT) == LOW;
  if (!irq && !touchDown) return;
  int16_t x[2], y[2];
  uint8_t n = touch.getPoint(x, y, 1);
  if (n) {
    int px = x[0], py = y[0], lx = px, ly = py;
    if (px < 0) px = 0; if (py < 0) py = 0; if (px > 479) px = 479; if (py > 479) py = 479;
    switch (rotNow) {                     // physical -> logical, the inverse of turn()
      case 1: lx = py; ly = 479 - px; break;
      case 2: lx = 479 - px; ly = 479 - py; break;
      case 3: lx = 479 - py; ly = px; break;
      default: lx = px; ly = py;
    }
    touchDown = true;
    uiTouch(true, lx, ly);
  } else if (touchDown) {
    touchDown = false;
    uiTouch(false, 0, 0);
  }
}

void boardPoll() {
  uint32_t now = millis();
  touchPoll();
  for (Btn& b : btns) {
    bool d = digitalRead(b.pin) == LOW;
    if (d && !b.down) { b.down = true; b.at = now; b.longDone = false; }
    else if (d && b.down && !b.longDone && now - b.at > 600) { b.longDone = true; uiKey(b.longKey); }
    else if (!d && b.down) { b.down = false; if (!b.longDone && now - b.at > 25) uiKey(b.shortKey); }
  }
  if (S.pmu && now - pmuAt > 80) {
    pmuAt = now;
    pmu.getIrqStatus();
    if (pmu.isPekeyShortPressIrq()) uiKey(K_PWR);
    if (pmu.isPekeyLongPressIrq()) uiKey(K_PWR_LONG);
    pmu.clearIrqStatus();
  }
  if (now - battAt > 2000) {
    battAt = now;
    if (S.pmu) {
      S.battery = pmu.isBatteryConnect();
      S.usb = pmu.isVbusIn();
      S.charging = pmu.isCharging();
      S.batMv = pmu.getBattVoltage();
      S.vbusMv = S.usb ? pmu.getVbusVoltage() : 0;
      S.batPct = S.battery ? pmu.getBatteryPercent() : -1;
      S.pmuTemp = pmu.getTemperature();
    }
    S.chipTemp = temperatureRead();
    cfgFlush();
  }
  // motion: lift to wake, turn the picture with the cube
  if (S.imu && now - rotAt > 100) {
    rotAt = now;
    float a[3], g[3];
    imuRead(a, g);
    if (!uiAwake() && cfg.moveWake) {
      float d = fabsf(a[0] - moveRef[0]) + fabsf(a[1] - moveRef[1]) + fabsf(a[2] - moveRef[2]);
      if (d > 3.0f) uiWake();
    }
    for (int i = 0; i < 3; i++) moveRef[i] = moveRef[i] * 0.9f + a[i] * 0.1f;
    if (cfg.rot == 4) {
      static int cand = 0; static uint32_t candAt = 0;
      int r = autoRot;
      if (a[1] > 7) r = 0; else if (a[1] < -7) r = 2; else if (a[0] > 7) r = 3; else if (a[0] < -7) r = 1;
      if (r != cand) { cand = r; candAt = now; }
      if (cand != autoRot && now - candAt > 600) { autoRot = cand; uiDirty(); }
    }
  }
  applyTz();
}
