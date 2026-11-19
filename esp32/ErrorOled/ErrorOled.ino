/*
  ERROR-OLED  —  a tiny ERROR-DOS computer for error.blue on an ESP32-C3 with a built-in OLED
  ------------------------------------------------------------------------------------------
  - WHO.EXE: who is online on error.blue right now, and in which game (the screen you see after start)
  - CHAT.EXE: read and write ERROR-CHAT (same channels as error.blue/apps/error-chat)
  - C:\> prompt with DIR, HELP, SAY, NAME, CODE, WIFI, BT, SETUP, MEM, TIME ...
  - Bluetooth keyboard + mouse (Bluetooth LE ones), or just the BOOT button:
        click        = next / down
        double-click = back (on WHO: the PROGRAMS menu)
        hold         = OK / open
  - Or type in the Arduino Serial Monitor (115200, "Newline"): every line is a DOS command,
    e.g.  WIFI "My Network" mypassword     NAME Alex     CODE HOONGY     SAY hi!

  Libraries (Arduino Library Manager):  U8g2 (olikraus), NimBLE-Arduino 2.x (h2zero), ArduinoJson 7 (bblanchon)
  Board: "ESP32C3 Dev Module" (esp32 core by Espressif 3.x)
         Tools -> USB CDC On Boot -> Enabled
         Tools -> Partition Scheme -> "Huge APP (3MB No OTA/1MB SPIFFS)"   (WiFi + Bluetooth + HTTPS is big)
  Your board: see config.h (screen size, pins).
*/
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include <esp_mac.h>
#include "config.h"
#include "sys.h"
#include "ui.h"
#include "net.h"
#include "hid.h"

#if defined(SCREEN_128X64)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
#elif defined(SCREEN_SH1106)
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
#else
U8G2_SSD1306_72X40_ER_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);
#endif

static Settings cfg;
static Preferences prefs;
static char sid[13];                         // this box on the who's-online list
static const uint32_t CFG_VERSION = 0xE0100000 | sizeof(Settings);

static void saveSettings() {
  prefs.putBytes("cfg", &cfg, sizeof(cfg));
  prefs.putUInt("ver", CFG_VERSION);
}

// ===================================================================== the board, for the screens
class Board : public Sys {
 public:
  uint32_t ms() override { return millis(); }
  bool clock(int& h, int& m, int& s) override {
    time_t t = time(nullptr);
    if (t < 1700000000) return false;        // no time from the internet yet
    struct tm tm;
    localtime_r(&t, &tm);
    h = tm.tm_hour; m = tm.tm_min; s = tm.tm_sec;
    return true;
  }
  Settings& cfg() override { return ::cfg; }
  void saveCfg() override { saveSettings(); }
  void led(bool on) override {
    if (LED_PIN >= 0) digitalWrite(LED_PIN, on ? LED_ON : !LED_ON);
  }
  void serialOut(const char* line) override { Serial.println(line); }

  uint8_t netState() override { return netStateNow(); }
  const char* netSsid() override { return netSsidNow(); }
  const char* netIp() override { return netIpNow(); }
  int netRssi() override { return netRssiNow(); }
  void wifiScan() override { netScan(); }
  int wifiResults(WifiNet* out, int max) override { return netScanResults(out, max); }
  void wifiJoin(const char* ssid, const char* pass) override { netJoin(ssid, pass); }
  void portalStart() override { netPortal(true); }
  void portalStop() override { netPortal(false); }

  bool netBusy(uint8_t job) override { return netJobBusy(job); }
  void fetchOnline() override { netRequest(JOB_ONLINE, "GET", "/api/online", ""); }
  void fetchChat(const char* code, uint32_t after, bool background) override {
    String p = String("/api/chat/") + code + "/log?limit=20&after=" + after + "&name=" + urlEncode(::cfg.name);
    if (background) p += "&bg=1";
    netRequest(background ? JOB_CHAT_BG : JOB_CHAT, "GET", p, "");
  }
  void sendChat(const char* code, const char* text) override {
    String b = String("{\"name\":\"") + jsonEscape(::cfg.name) + "\",\"text\":\"" + jsonEscape(text) + "\"}";
    netRequest(JOB_SAY, "POST", String("/api/chat/") + code + "/say", b);
  }
  void beacon() override {
    if (netJobBusy(JOB_BEACON)) return;
    String b = String("{\"sid\":\"") + sid + "\",\"path\":\"/oled\",\"name\":\"" + jsonEscape(::cfg.name) + "\"}";
    netRequest(JOB_BEACON, "POST", "/api/online", b);
  }
  const char* netError() override { return netErr(); }

  void btScan() override { hidScan(); }
  bool btScanning() override { return hidScanning(); }
  int btResults(BtDev* out, int max) override { return hidResults(out, max); }
  void btPair(const char* addr) override { hidPair(addr); }
  void btForget() override { hidForget(); }
  int btConnected(BtDev* out, int max) override { return hidConnected(out, max); }
  uint32_t btPasskey() override { return hidPasskey(); }
  const char* btStatus() override { return hidStatus(); }
  int btBonds() override { return hidBonds(); }

  uint32_t freeHeap() override { return ESP.getFreeHeap(); }
  uint32_t totalHeap() override { return ESP.getHeapSize(); }
  const char* chipInfo() override {
    static char b[32];
    snprintf(b, sizeof(b), "%s %uMHZ", ESP.getChipModel(), (unsigned)ESP.getCpuFreqMHz());
    for (char* p = b; *p; p++) *p = toupper(*p);
    return b;
  }
  void reboot() override { display.clearDisplay(); delay(100); ESP.restart(); }
};
static Board board;

// ===================================================================== BOOT button
// click = right away; a second click within 350 ms = double-click; hold 0.6 s = long
static void buttonLoop() {
  static bool down = false, longSent = false;
  static uint32_t downAt = 0, lastClick = 0, changeAt = 0;
  bool pressed = digitalRead(BUTTON_PIN) == LOW;
  uint32_t now = millis();
  if (pressed != down && now - changeAt > 25) {          // debounce
    changeAt = now;
    down = pressed;
    if (down) { downAt = now; longSent = false; }
    else if (!longSent) {
      if (lastClick && now - lastClick < 350) { uiButton(BTN_DOUBLE); lastClick = 0; }
      else { uiButton(BTN_SHORT); lastClick = now; }
    }
  }
  if (down && !longSent && now - downAt > 600) { longSent = true; lastClick = 0; uiButton(BTN_LONG); }
}

// ===================================================================== USB serial: type DOS commands
static void serialLoop() {
  static char line[300];
  static size_t n = 0;
  while (Serial.available()) {
    int c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') { line[n] = 0; if (n) uiSerialLine(line); n = 0; continue; }
    if (n < sizeof(line) - 1) line[n++] = (char)c;
  }
}

// ===================================================================== start
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  if (LED_PIN >= 0) { pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, !LED_ON); }

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  const char* abc = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789";
  for (int i = 0; i < 12; i++) sid[i] = abc[(mac[i % 6] * 31 + esp_random()) % 56];
  sid[12] = 0;

  prefs.begin("erroroled", false);
  settingsDefaults(cfg);
  if (prefs.getUInt("ver", 0) != CFG_VERSION || prefs.getBytes("cfg", &cfg, sizeof(cfg)) != sizeof(cfg)) {
    settingsDefaults(cfg);
    // each box gets its own name, e.g. OLED-3F, so two boxes don't look like one
    snprintf(cfg.name, sizeof(cfg.name), "OLED-%02X", mac[5]);
    strncpy(cfg.channel, CHAT_CHANNEL, sizeof(cfg.channel) - 1);
    saveSettings();
  }

  display.setBusClock(400000);
  display.begin();
  display.enableUTF8Print();

  netBegin(&cfg, saveSettings);
  hidBegin(DEVICE_NAME);
  uiBegin(display.getU8g2(), &board);

  Serial.println();
  Serial.println("ERROR-OLED: type DOS commands here, e.g.  HELP   WIFI \"My Network\" password   NAME Alex   CODE HOONGY");
}

void loop() {
  buttonLoop();
  serialLoop();
  netLoop();
  hidLoop(millis(), cfg.keymap);
  uiTick();
  delay(5);
}
