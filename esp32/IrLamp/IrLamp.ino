/*
  ============================================================
   ESP32 IR LEARNER + WI-FI REMOTE + error.blue LINK    v2.1
  ============================================================
   - IR receiver learns any remote button
   - Web page (phone / PC): name it, save it, tap it to send it again
   - Saved signals stay in flash (survive power-off and re-upload)
   - Download all names + values as a .txt file (and restore from it)
   - NEW in 2.0: joins your home Wi-Fi and keeps a live link to
     error.blue, so error.blue/lamp and the lamp games can
     send IR from anywhere
   - NEW in 2.1: alarm clock (sunrise wake-up light). The alarms live
     on the board and run without any page open; set them on
     error.blue/lamp/alarm. OFF on the real remote stops a running one.

   Made for: classic ESP32 (DevKit / WROOM). Works on other ESP32s,
             just change the pins below.
   Arduino core: "esp32" by Espressif, version 2.x or 3.x
   Libraries: none needed.  U8g2 only if USE_OLED is 1.

  ------------------------------------------------------------
   WIRING
  ------------------------------------------------------------
   IR RECEIVER (VS1838B / TSOP38238 / KY-022 module)
      OUT  -> GPIO 13
      VCC  -> 3V3
      GND  -> GND

   IR LED (transmitter)
      Simple (about 1-2 m range, KY-005 module or bare LED):
        GPIO 12 -> 100 ohm -> IR LED long leg (+)
        IR LED short leg (-) -> GND

      Better (5-10 m range, recommended):
        GPIO 12 -> 1k ohm -> base of NPN transistor (2N2222 / BC547)
        emitter -> GND
        collector -> IR LED short leg (-)
        IR LED long leg (+) -> 33 ohm -> 5V

  ------------------------------------------------------------
   USE
  ------------------------------------------------------------
   1. Upload, then connect your phone to Wi-Fi "IR-Remote"
      (password 12345678) and open http://192.168.4.1
   2. Press a button on any remote, aimed at the receiver.
      Type a name -> Save.  Tap saved buttons to send them.
   3. ONLINE: open "Wi-Fi settings", pick your home network,
      type its password, Connect. The card then shows a LINK CODE.
      Open error.blue/lamp and type that code (or tap the link).
   4. "Download .txt" gives you all names and values.
      "Restore .txt" puts them back after a wipe.

   Serial Monitor (115200, Newline): HELP, STATUS, CODE, NEWCODE,
   LINK ON / LINK OFF, WIFI "network name" password, WIFI OFF, ALARMS
  ============================================================
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <time.h>
#include <vector>
#include <algorithm>

#define FW_VERSION "2.1"

// ============================================================
//  SETTINGS
// ============================================================
#define IR_RX_PIN       13       // IR receiver OUT
#define IR_TX_PIN       12       // IR LED (or transistor base)
                                 // Don't swap these: GPIO 12 must be LOW at boot.
                                 // The IR LED keeps it low, the receiver would
                                 // pull it HIGH and the ESP32 wouldn't start.

#define IR_CARRIER_HZ   38000    // 38 kHz works for almost every remote
#define IR_DUTY_8BIT    85       // carrier duty (85/255 = 33 %)
#define IR_GAP_US       15000    // this much silence = end of a signal.
                                 // Air-conditioner remotes: set 50000
#define IR_MAX_EDGES    1000     // longest signal we can store
#define IR_MIN_EDGES    12       // shorter = noise / NEC "repeat" blip
#define IR_QUEUE_GAP_MS 220      // pause between commands that come from error.blue

#define MAX_SIGNALS     60
#define MAX_NAME_LEN    32

// The board always makes its own network (works without internet)...
#define AP_SSID   "IR-Remote"
#define AP_PASS   "12345678"     // at least 8 characters

// ...and joins your home Wi-Fi for error.blue. Set it on the web page
// (Wi-Fi settings) or in the Serial Monitor; or fill it in here.
#define HOME_SSID ""
#define HOME_PASS ""

#define MDNS_NAME "irremote"     // also reachable as http://irremote.local

// error.blue link: the board joins the room /api/room/ir-lamp/<LINK CODE>
#ifndef CLOUD_HOST
  #define CLOUD_HOST  "error.blue"
  #define CLOUD_PORT  443
#endif
#define CLOUD_ROOM    "ir-lamp"
#define DEVICE_NAME   "IR-LAMP"

// ESP32-C3 mini boards often have weak / unstable Wi-Fi at full power.
// Set 1 there. Classic ESP32 boards: keep 0 (full power, more range).
#define REDUCE_TX_POWER 0

// Only for the ESP32-C3 board with built-in 0.42" OLED (SDA 5, SCL 6).
// Keep 0 on a classic ESP32: its GPIO 6 is wired to the flash chip.
#define USE_OLED 0

#if USE_OLED
  #include <U8g2lib.h>
  #include <Wire.h>
  U8G2_SSD1306_72X40_ER_F_HW_I2C oled(U8G2_R0, /*reset=*/U8X8_PIN_NONE, /*SCL=*/6, /*SDA=*/5);
#endif

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  #define CORE_V3 1
#else
  #define CORE_V3 0
  #define IR_LEDC_CHANNEL 0
#endif

// Root certificates for error.blue's HTTPS (Cloudflare uses Let's Encrypt or Google Trust
// Services). If the check ever fails, the link goes on without it (shown as "TLS: skip").
static const char ROOT_CAS[] PROGMEM =
// ISRG Root X1
  "-----BEGIN CERTIFICATE-----\n"
  "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
  "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
  "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
  "WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
  "ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
  "MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
  "h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
  "0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
  "A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
  "T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
  "B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
  "B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
  "KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
  "OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
  "jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
  "qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
  "rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
  "HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
  "hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
  "ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
  "3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
  "NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
  "ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
  "TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
  "jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
  "oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
  "4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
  "mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
  "emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
  "-----END CERTIFICATE-----\n"
  // ISRG Root X2
  "-----BEGIN CERTIFICATE-----\n"
  "MIICGzCCAaGgAwIBAgIQQdKd0XLq7qeAwSxs6S+HUjAKBggqhkjOPQQDAzBPMQsw\n"
  "CQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFyY2gg\n"
  "R3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMjAeFw0yMDA5MDQwMDAwMDBaFw00\n"
  "MDA5MTcxNjAwMDBaME8xCzAJBgNVBAYTAlVTMSkwJwYDVQQKEyBJbnRlcm5ldCBT\n"
  "ZWN1cml0eSBSZXNlYXJjaCBHcm91cDEVMBMGA1UEAxMMSVNSRyBSb290IFgyMHYw\n"
  "EAYHKoZIzj0CAQYFK4EEACIDYgAEzZvVn4CDCuwJSvMWSj5cz3es3mcFDR0HttwW\n"
  "+1qLFNvicWDEukWVEYmO6gbf9yoWHKS5xcUy4APgHoIYOIvXRdgKam7mAHf7AlF9\n"
  "ItgKbppbd9/w+kHsOdx1ymgHDB/qo0IwQDAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0T\n"
  "AQH/BAUwAwEB/zAdBgNVHQ4EFgQUfEKWrt5LSDv6kviejM9ti6lyN5UwCgYIKoZI\n"
  "zj0EAwMDaAAwZQIwe3lORlCEwkSHRhtFcP9Ymd70/aTSVaYgLXTWNLxBo1BfASdW\n"
  "tL4ndQavEi51mI38AjEAi/V3bNTIZargCyzuFJ0nN6T5U6VR5CmD1/iQMVtCnwr1\n"
  "/q4AaOeMSQ+2b1tbFfLn\n"
  "-----END CERTIFICATE-----\n"
  // GTS Root R1
  "-----BEGIN CERTIFICATE-----\n"
  "MIIFVzCCAz+gAwIBAgINAgPlk28xsBNJiGuiFzANBgkqhkiG9w0BAQwFADBHMQsw\n"
  "CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU\n"
  "MBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAw\n"
  "MDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp\n"
  "Y2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwggIiMA0GCSqGSIb3DQEBAQUA\n"
  "A4ICDwAwggIKAoICAQC2EQKLHuOhd5s73L+UPreVp0A8of2C+X0yBoJx9vaMf/vo\n"
  "27xqLpeXo4xL+Sv2sfnOhB2x+cWX3u+58qPpvBKJXqeqUqv4IyfLpLGcY9vXmX7w\n"
  "Cl7raKb0xlpHDU0QM+NOsROjyBhsS+z8CZDfnWQpJSMHobTSPS5g4M/SCYe7zUjw\n"
  "TcLCeoiKu7rPWRnWr4+wB7CeMfGCwcDfLqZtbBkOtdh+JhpFAz2weaSUKK0Pfybl\n"
  "qAj+lug8aJRT7oM6iCsVlgmy4HqMLnXWnOunVmSPlk9orj2XwoSPwLxAwAtcvfaH\n"
  "szVsrBhQf4TgTM2S0yDpM7xSma8ytSmzJSq0SPly4cpk9+aCEI3oncKKiPo4Zor8\n"
  "Y/kB+Xj9e1x3+naH+uzfsQ55lVe0vSbv1gHR6xYKu44LtcXFilWr06zqkUspzBmk\n"
  "MiVOKvFlRNACzqrOSbTqn3yDsEB750Orp2yjj32JgfpMpf/VjsPOS+C12LOORc92\n"
  "wO1AK/1TD7Cn1TsNsYqiA94xrcx36m97PtbfkSIS5r762DL8EGMUUXLeXdYWk70p\n"
  "aDPvOmbsB4om3xPXV2V4J95eSRQAogB/mqghtqmxlbCluQ0WEdrHbEg8QOB+DVrN\n"
  "VjzRlwW5y0vtOUucxD/SVRNuJLDWcfr0wbrM7Rv1/oFB2ACYPTrIrnqYNxgFlQID\n"
  "AQABo0IwQDAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4E\n"
  "FgQU5K8rJnEaK0gnhS9SZizv8IkTcT4wDQYJKoZIhvcNAQEMBQADggIBAJ+qQibb\n"
  "C5u+/x6Wki4+omVKapi6Ist9wTrYggoGxval3sBOh2Z5ofmmWJyq+bXmYOfg6LEe\n"
  "QkEzCzc9zolwFcq1JKjPa7XSQCGYzyI0zzvFIoTgxQ6KfF2I5DUkzps+GlQebtuy\n"
  "h6f88/qBVRRiClmpIgUxPoLW7ttXNLwzldMXG+gnoot7TiYaelpkttGsN/H9oPM4\n"
  "7HLwEXWdyzRSjeZ2axfG34arJ45JK3VmgRAhpuo+9K4l/3wV3s6MJT/KYnAK9y8J\n"
  "ZgfIPxz88NtFMN9iiMG1D53Dn0reWVlHxYciNuaCp+0KueIHoI17eko8cdLiA6Ef\n"
  "MgfdG+RCzgwARWGAtQsgWSl4vflVy2PFPEz0tv/bal8xa5meLMFrUKTX5hgUvYU/\n"
  "Z6tGn6D/Qqc6f1zLXbBwHSs09dR2CQzreExZBfMzQsNhFRAbd03OIozUhfJFfbdT\n"
  "6u9AWpQKXCBfTkBdYiJ23//OYb2MI3jSNwLgjt7RETeJ9r/tSQdirpLsQBqvFAnZ\n"
  "0E6yove+7u7Y/9waLd64NnHi/Hm3lCXRSHNboTXns5lndcEZOitHTtNCjv0xyBZm\n"
  "2tIMPNuzjsmhDYAPexZ3FL//2wmUspO8IFgV6dtxQ/PeEMMA3KgqlbbC1j+Qa3bb\n"
  "bP6MvPJwNQzcmRk13NfIRmPVNnGuV/u3gm3c\n"
  "-----END CERTIFICATE-----\n"
  // GTS Root R4
  "-----BEGIN CERTIFICATE-----\n"
  "MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD\n"
  "VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG\n"
  "A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw\n"
  "WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz\n"
  "IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi\n"
  "AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi\n"
  "QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR\n"
  "HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW\n"
  "BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D\n"
  "9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8\n"
  "p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD\n"
  "-----END CERTIFICATE-----\n";

// ============================================================
//  DATA
// ============================================================
struct IrSignal {
  String name;
  String protocol;
  uint16_t bits = 0;
  String code;                    // hex value, e.g. 0x20DF10EF
  std::vector<uint16_t> raw;      // mark, space, mark, ... in microseconds
};

std::vector<IrSignal> saved;      // the saved buttons
IrSignal last;                    // most recent received signal
bool     haveLast     = false;
bool     lastOverflow = false;
uint32_t lastSeq      = 0;        // increases with every new signal

WebServer server(80);
Preferences prefs;
String apIpStr = "192.168.4.1";
String staSsid, staPass;          // home Wi-Fi
String linkCode;                  // error.blue room code
bool   cloudOn = true;

static const char* DB_FILE = "/signals.txt";
static const char* DB_TMP  = "/signals.tmp";

// the last NEC codes sent or received: error.blue pages rebuild the lamp's state from them
static uint32_t hist[24];
static uint8_t  histHead = 0, histN = 0;
void histPush(uint32_t c) { hist[histHead] = c; histHead = (histHead + 1) % 24; if (histN < 24) histN++; }

void cloudSay(const String& s);   // further down
void cloudTx(const IrSignal& s, const char* from);
bool alarmRunning();
void stopAlarms(const char* why);
String alarmHelloPart();
void alarmCommand(const char* t, const char* d);
String tzStr = "";                // POSIX time zone for the alarms (set from error.blue/lamp/alarm)

// ============================================================
//  OLED
// ============================================================
void oledShow(const String& l1, const String& l2, const String& l3, const String& l4) {
#if USE_OLED
  oled.clearBuffer();
  oled.drawStr(0, 7,  l1.substring(0, 14).c_str());
  oled.drawStr(0, 17, l2.substring(0, 14).c_str());
  oled.drawStr(0, 28, l3.substring(0, 14).c_str());
  oled.drawStr(0, 39, l4.substring(0, 14).c_str());
  oled.sendBuffer();
#else
  (void)l1; (void)l2; (void)l3; (void)l4;
#endif
}

void oledBegin() {
#if USE_OLED
  oled.begin();
  oled.setBusClock(400000);
  oled.setFont(u8g2_font_5x7_tr);
  oledShow("IR Remote", "starting...", "", "");
#endif
}

// ============================================================
//  IR RECEIVE  (interrupt records the time between edges)
// ============================================================
volatile uint16_t rxBuf[IR_MAX_EDGES];
volatile uint16_t rxLen      = 0;
volatile uint32_t rxLastEdge = 0;   // last edge inside the current signal
volatile uint32_t rxLastAny  = 0;   // last edge of any kind
volatile bool     rxActive   = false;
volatile bool     rxReady    = false;
volatile bool     rxPaused   = false;
volatile bool     rxOverflow = false;
portMUX_TYPE rxMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR irISR() {
  portENTER_CRITICAL_ISR(&rxMux);
  uint32_t now  = micros();
  uint32_t idle = now - rxLastAny;
  rxLastAny = now;

  if (!rxReady && !rxPaused) {
    if (!rxActive) {
      // A new signal starts with the line going LOW after a quiet period
      if (idle > IR_GAP_US && digitalRead(IR_RX_PIN) == LOW) {
        rxActive   = true;
        rxLen      = 0;
        rxOverflow = false;
        rxLastEdge = now;
      }
    } else {
      uint32_t d = now - rxLastEdge;
      if (d > IR_GAP_US) {             // loop() was late: close this signal
        rxActive = false;
        rxReady  = true;
      } else {
        rxLastEdge = now;
        if (rxLen < IR_MAX_EDGES) rxBuf[rxLen++] = (uint16_t)d;
        else rxOverflow = true;
      }
    }
  }
  portEXIT_CRITICAL_ISR(&rxMux);
}

// ---------- decoder: turns timings into protocol + hex value ----------
static bool near(uint32_t v, uint32_t target, uint32_t pct = 25) {
  uint32_t tol = target * pct / 100;
  return v + tol >= target && v <= target + tol;
}

static String bitsToHex(const std::vector<uint8_t>& b) {
  static const char HEX_CHARS[] = "0123456789ABCDEF";
  String h = "0x";
  uint8_t nib = 0;
  size_t cnt = (4 - b.size() % 4) % 4;   // leading zero padding
  for (uint8_t bit : b) {
    nib = (uint8_t)((nib << 1) | (bit & 1));
    if (++cnt == 4) { h += HEX_CHARS[nib]; nib = 0; cnt = 0; }
  }
  return h;
}

// For unknown protocols: a hash so different buttons get different values
static String rawHash(const std::vector<uint16_t>& r) {
  uint32_t h = 2166136261UL;
  for (size_t i = 0; i + 2 < r.size(); i++) {
    uint32_t a = r[i], b = r[i + 2];
    uint32_t v = (b * 10 < a * 8) ? 0 : (a * 10 < b * 8) ? 2 : 1;
    h = (h * 16777619UL) ^ v;
  }
  char buf[12];
  snprintf(buf, sizeof(buf), "0x%08lX", (unsigned long)h);
  return String(buf);
}

void decodeSignal(IrSignal& s) {
  const std::vector<uint16_t>& r = s.raw;
  const size_t n = r.size();
  s.protocol = "RAW";
  s.bits = 0;
  s.code = rawHash(r);
  if (n < 2 + 2 * 8) return;

  // Data runs from index 2 to the first long gap (spaces are odd indexes)
  size_t end = n;
  for (size_t i = 3; i < n; i += 2) {
    if (r[i] > 5000) { end = i; break; }
  }

  uint32_t mMin = 65535, mMax = 0, sMin = 65535, sMax = 0, mSum = 0, sSum = 0;
  size_t mCnt = 0, sCnt = 0;
  for (size_t i = 2; i < end; i++) {
    uint32_t v = r[i];
    if ((i & 1) == 0) { mMin = min(mMin, v); mMax = max(mMax, v); mSum += v; mCnt++; }
    else              { sMin = min(sMin, v); sMax = max(sMax, v); sSum += v; sCnt++; }
  }
  if (mCnt < 8 || sCnt < 7) return;

  const bool mVar = mMax * 10 > mMin * 16;    // marks have 2 lengths?
  const bool sVar = sMax * 10 > sMin * 16;    // spaces have 2 lengths?

  // Header = first mark+space, unless they look like normal data bits
  const bool hasHeader = !(r[0] * 10 <= mMax * 14 && r[0] * 10 >= mMin * 6 &&
                           r[1] * 10 <= sMax * 14 && r[1] * 10 >= sMin * 6);
  const size_t start = hasHeader ? 2 : 0;
  const uint32_t hm = r[0], hs = r[1];

  std::vector<uint8_t> bitv;
  bool pulseDistance;

  if (sVar && !mVar) {                       // NEC, Samsung, Panasonic, most AC...
    pulseDistance = true;
    uint32_t th = (sMin + sMax) / 2;
    for (size_t i = start + 1; i < end; i += 2) bitv.push_back(r[i] > th ? 1 : 0);
  } else if (mVar && !sVar) {                // Sony
    pulseDistance = false;
    uint32_t th = (mMin + mMax) / 2;
    for (size_t i = start; i < end; i += 2) bitv.push_back(r[i] > th ? 1 : 0);
  } else if (!mVar && !sVar) {               // every bit the same value
    pulseDistance = true;
    uint8_t one = (sSum / sCnt) * 10 > (mSum / mCnt) * 18 ? 1 : 0;
    for (size_t i = start + 1; i < end; i += 2) bitv.push_back(one);
  } else {
    return;                                  // RC5 / RC6 / unknown -> RAW
  }

  const size_t nb = bitv.size();
  if (nb < 8) return;

  String p;
  if (pulseDistance) {
    if      (hasHeader && near(hm, 9000) && near(hs, 4500) && nb == 32) p = "NEC";
    else if (hasHeader && near(hm, 4500) && near(hs, 4500) && nb == 32) p = "SAMSUNG";
    else if (hasHeader && near(hm, 3500) && near(hs, 1750) && nb == 48) p = "PANASONIC";
    else if (hasHeader && near(hm, 8400) && near(hs, 4200) && nb == 16) p = "JVC";
    else p = "PULSE_DISTANCE";
  } else {
    if (hasHeader && near(hm, 2400) && (nb == 12 || nb == 15 || nb == 20)) p = "SONY";
    else p = "PULSE_WIDTH";
  }
  if (end < n) p += "+";                     // more data after a gap (AC remotes)

  s.protocol = p;
  s.bits = (uint16_t)nb;
  s.code = bitsToHex(bitv);
}

bool isNec32(const IrSignal& s) { return s.protocol == "NEC" && s.bits == 32; }
uint32_t codeValue(const String& c) { return (uint32_t)strtoul(c.c_str(), nullptr, 16); }   // "0x..." works too
String hex8(uint32_t v) { char b[10]; snprintf(b, sizeof(b), "%08lX", (unsigned long)v); return String(b); }

int findMatch(const IrSignal& s) {
  for (size_t i = 0; i < saved.size(); i++) {
    if (saved[i].protocol == s.protocol && saved[i].code == s.code && saved[i].bits == s.bits)
      return (int)i;
  }
  return -1;
}

void pollReceiver() {
  static uint16_t tmp[IR_MAX_EDGES];
  uint16_t n = 0;
  bool ready = false, ovf = false;

  portENTER_CRITICAL(&rxMux);
  if (rxActive && (uint32_t)(micros() - rxLastEdge) > IR_GAP_US) {
    rxActive = false;
    rxReady  = true;
  }
  if (rxReady) {
    n = rxLen;
    for (uint16_t i = 0; i < n; i++) tmp[i] = rxBuf[i];
    ovf = rxOverflow;
    rxReady = false;
    ready = true;
  }
  portEXIT_CRITICAL(&rxMux);

  if (!ready) return;
  if ((n & 1) == 0 && n > 0) n--;            // a signal ends with a mark
  if (n < IR_MIN_EDGES) return;

  uint16_t shorts = 0;                        // reject electrical noise
  for (uint16_t i = 0; i < n; i++) if (tmp[i] < 100) shorts++;
  if (shorts * 10 > n) return;

  last.name = "";
  last.raw.assign(tmp, tmp + n);
  decodeSignal(last);
  haveLast = true;
  lastOverflow = ovf;
  lastSeq++;

  Serial.printf("IR received: %s  %s  (%u bits, %u pulses)\n",
                last.protocol.c_str(), last.code.c_str(), last.bits, (unsigned)n);

  // tell error.blue (the lamp page follows the real remote too)
  if (isNec32(last)) {
    uint32_t c = codeValue(last.code);
    histPush(c);
    if (c == 0x00F740BF && alarmRunning()) stopAlarms("remote");   // OFF on the remote: good morning
    cloudSay("{\"t\":\"rx\",\"c\":\"" + hex8(c) + "\"}");
  } else {
    cloudSay("{\"t\":\"rx\",\"p\":\"" + last.protocol + "\",\"c\":\"" + last.code + "\"}");
  }

  int m = findMatch(last);
  oledShow("IR Remote", apIpStr, last.protocol + " " + String(last.bits) + "b",
           m >= 0 ? saved[m].name : last.code);
}

// ============================================================
//  IR SEND  (38 kHz carrier from the LEDC PWM hardware)
// ============================================================
void irCarrier(bool on) {
#if CORE_V3
  ledcWrite(IR_TX_PIN, on ? IR_DUTY_8BIT : 0);
#else
  ledcWrite(IR_LEDC_CHANNEL, on ? IR_DUTY_8BIT : 0);
#endif
}

void irTxBegin() {
#if CORE_V3
  ledcAttach(IR_TX_PIN, IR_CARRIER_HZ, 8);
#else
  ledcSetup(IR_LEDC_CHANNEL, IR_CARRIER_HZ, 8);
  ledcAttachPin(IR_TX_PIN, IR_LEDC_CHANNEL);
#endif
  irCarrier(false);
}

uint8_t repeatsFor(const IrSignal& s) {
  return s.protocol.startsWith("SONY") ? 3 : 1;   // Sony devices want 3 copies
}

void irSendRaw(const std::vector<uint16_t>& r, uint8_t repeats) {
  if (r.empty()) return;
  rxPaused = true;                                // don't "learn" our own LED

  uint32_t frameUs = 0;
  for (uint16_t v : r) frameUs += v;

  for (uint8_t k = 0; k < repeats; k++) {
    // Highest priority while sending so Wi-Fi can't stretch the timing
    UBaseType_t oldPrio = uxTaskPriorityGet(NULL);
    vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);

    uint32_t t = micros();
    for (size_t i = 0; i < r.size(); i++) {
      irCarrier((i & 1) == 0);                    // even = mark (LED on), odd = space
      t += r[i];
      while ((int32_t)(micros() - t) < 0) { }
    }
    irCarrier(false);

    vTaskPrioritySet(NULL, oldPrio);
    if (k + 1 < repeats) {
      uint32_t gap = frameUs < 35000 ? 45000 - frameUs : 10000;
      delayMicroseconds(gap);
    }
  }

  delay(40);                                      // let reflections die out
  portENTER_CRITICAL(&rxMux);
  rxActive = false;
  rxReady  = false;
  rxPaused = false;
  portEXIT_CRITICAL(&rxMux);
}

// Any NEC code, without learning it first (error.blue sends these)
void necRaw(uint32_t code, std::vector<uint16_t>& r) {
  r.clear();
  r.reserve(67);
  r.push_back(9000); r.push_back(4500);
  for (int b = 31; b >= 0; b--) { r.push_back(560); r.push_back(((code >> b) & 1) ? 1690 : 560); }
  r.push_back(560);
}

// ---------- queue: commands from error.blue go out one by one ----------
struct IrCmd { uint8_t kind; uint32_t val; char from[12]; };   // kind 0 = NEC code, 1 = saved button
static IrCmd    irq[32];
static uint8_t  irqHead = 0, irqN = 0;
static uint32_t lastTxAt = 0;

bool queuePush(uint8_t kind, uint32_t val, const char* from) {
  if (irqN >= 32) return false;
  IrCmd& c = irq[(irqHead + irqN) % 32];
  c.kind = kind; c.val = val;
  strncpy(c.from, from ? from : "", sizeof(c.from) - 1); c.from[sizeof(c.from) - 1] = 0;
  irqN++;
  return true;
}
void queueClear() { irqN = 0; }

void queueStep() {
  if (!irqN || (uint32_t)(millis() - lastTxAt) < IR_QUEUE_GAP_MS) return;
  IrCmd c = irq[irqHead];
  irqHead = (irqHead + 1) % 32;
  irqN--;
  if (c.kind == 0) {
    std::vector<uint16_t> r;
    necRaw(c.val, r);
    irSendRaw(r, 1);
    histPush(c.val);
    cloudSay("{\"t\":\"tx\",\"c\":\"" + hex8(c.val) + "\",\"f\":\"" + String(c.from) + "\",\"q\":" + String(irqN) + "}");
    Serial.printf("error.blue: sent NEC %08lX\n", (unsigned long)c.val);
  } else if (c.val < saved.size()) {
    irSendRaw(saved[c.val].raw, repeatsFor(saved[c.val]));
    cloudTx(saved[c.val], c.from);
    Serial.printf("error.blue: sent \"%s\"\n", saved[c.val].name.c_str());
  }
  lastTxAt = millis();
}

// ============================================================
//  STORAGE  (LittleFS text file, one signal per line)
// ============================================================
String rawCsv(const std::vector<uint16_t>& r) {
  String o;
  o.reserve(r.size() * 5);
  for (size_t i = 0; i < r.size(); i++) {
    if (i) o += ',';
    o += String(r[i]);
  }
  return o;
}

void parseCsv(const char* p, std::vector<uint16_t>& out) {
  while (*p && out.size() < IR_MAX_EDGES) {
    while (*p == ' ') p++;
    char* e;
    unsigned long v = strtoul(p, &e, 10);
    if (e == p) break;
    out.push_back((uint16_t)v);
    if (*e != ',') break;
    p = e + 1;
  }
}

bool saveAll() {
  File f = LittleFS.open(DB_TMP, "w");
  if (!f) return false;
  f.print("IRV1\n");
  for (const IrSignal& s : saved) {
    f.print(s.name);     f.print('\t');
    f.print(s.protocol); f.print('\t');
    f.print(s.bits);     f.print('\t');
    f.print(s.code);     f.print('\t');
    f.print(rawCsv(s.raw));
    f.print('\n');
  }
  f.close();
  LittleFS.remove(DB_FILE);
  return LittleFS.rename(DB_TMP, DB_FILE);
}

void loadAll() {
  saved.clear();
  if (!LittleFS.exists(DB_FILE)) return;
  File f = LittleFS.open(DB_FILE, "r");
  if (!f) return;
  String line = f.readStringUntil('\n');
  line.trim();
  if (line != "IRV1") { f.close(); return; }

  while (f.available() && saved.size() < MAX_SIGNALS) {
    line = f.readStringUntil('\n');
    line.replace("\r", "");
    int t1 = line.indexOf('\t');
    int t2 = line.indexOf('\t', t1 + 1);
    int t3 = line.indexOf('\t', t2 + 1);
    int t4 = line.indexOf('\t', t3 + 1);
    if (t1 < 0 || t2 < 0 || t3 < 0 || t4 < 0) continue;

    IrSignal s;
    s.name     = line.substring(0, t1);
    s.protocol = line.substring(t1 + 1, t2);
    s.bits     = (uint16_t)line.substring(t2 + 1, t3).toInt();
    s.code     = line.substring(t3 + 1, t4);
    String rawStr = line.substring(t4 + 1);
    parseCsv(rawStr.c_str(), s.raw);
    if (!s.raw.empty()) saved.push_back(s);
  }
  f.close();
}

// ============================================================
//  WEB PAGE
// ============================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>IR Remote</title>
<style>
:root{--bg:#0b0f14;--card:#141c26;--line:#24334a;--txt:#e8eef5;--mut:#8a9bb0;--acc:#29d3ff;--ok:#3ddc84;--bad:#ff5c6c;--warn:#ffb84d}
*{box-sizing:border-box}[hidden]{display:none!important}
body{margin:0;background:var(--bg);color:var(--txt);font:16px/1.4 -apple-system,system-ui,"Segoe UI",Roboto,sans-serif}
.wrap{max-width:560px;margin:0 auto;padding:16px 14px 90px}
header{display:flex;align-items:center;justify-content:space-between;margin:4px 0 16px}
h1{font-size:21px;margin:0}
.pill{font-size:13px;color:var(--mut);display:flex;align-items:center;gap:7px}
.dot{width:9px;height:9px;border-radius:50%;background:var(--ok);box-shadow:0 0 8px var(--ok);animation:pulse 1.6s infinite}
.off .dot{background:var(--bad);box-shadow:0 0 8px var(--bad);animation:none}
@keyframes pulse{50%{opacity:.3}}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin-bottom:14px}
.card h2{font-size:12px;text-transform:uppercase;letter-spacing:1px;color:var(--mut);margin:0 0 10px;display:flex;justify-content:space-between;align-items:center;gap:8px}
#lastInfo{text-transform:none;letter-spacing:0;color:var(--ok);font-size:12px}
.code{font:600 23px/1.2 ui-monospace,Menlo,Consolas,monospace;color:var(--acc);word-break:break-all}
.meta{color:var(--mut);font-size:13px;margin-top:4px}
.flash{animation:flash .9s}
@keyframes flash{0%{background:#1b4256}100%{background:var(--card)}}
.row{display:flex;gap:8px;margin-top:12px}
input,select{flex:1;min-width:0;background:#0d141c;border:1px solid var(--line);color:var(--txt);border-radius:10px;padding:11px 12px;font-size:16px}
input:focus,select:focus{outline:none;border-color:var(--acc)}
button,.btn{background:var(--acc);color:#04202b;border:0;border-radius:10px;padding:11px 16px;font-weight:700;font-size:15px;cursor:pointer;text-decoration:none;display:inline-block;text-align:center;font-family:inherit}
.ghost{background:transparent;color:var(--acc);border:1px solid var(--line)}
.small{padding:5px 11px;font-size:12px;letter-spacing:0;text-transform:none}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px}
.sig{background:#1a2532;border:1px solid var(--line);border-radius:12px;padding:13px 12px;min-height:66px;cursor:pointer;user-select:none;-webkit-user-select:none;transition:transform .08s,border-color .2s}
.sig:active{transform:scale(.96)}
.sig b{display:block;font-size:16px;word-break:break-word}
.sig small{display:block;color:var(--mut);font:12px ui-monospace,Menlo,monospace;margin-top:3px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.sig.sent{border-color:var(--ok);box-shadow:0 0 0 2px var(--ok) inset}
.tools{display:none;gap:6px;margin-top:9px}
.edit .tools{display:flex}
.edit .sig{cursor:default}
.tools span{flex:1;text-align:center;font-size:12px;padding:6px 0;border-radius:8px;background:#0d141c;border:1px solid var(--line);color:var(--txt);cursor:pointer}
.tools .del{color:var(--bad)}
.empty{color:var(--mut);font-size:14px}
.foot{display:flex;gap:8px;flex-wrap:wrap}.foot .btn{flex:1}
.kv{display:flex;justify-content:space-between;gap:10px;font-size:14px;padding:5px 0;border-bottom:1px solid #1c2735}
.kv span{color:var(--mut)}.kv b{font-weight:600;text-align:right;word-break:break-all}
.lc{font:700 34px/1 ui-monospace,Menlo,Consolas,monospace;letter-spacing:6px;color:var(--acc);text-align:center;margin:14px 0 4px}
.st-ok{color:var(--ok)}.st-bad{color:var(--bad)}.st-wait{color:var(--warn)}
details{margin-top:12px;border-top:1px solid var(--line);padding-top:10px}
summary{cursor:pointer;color:var(--acc);font-weight:600}
#toast{position:fixed;left:50%;bottom:20px;transform:translate(-50%,120px);background:#1f2b38;border:1px solid var(--line);padding:10px 16px;border-radius:12px;transition:transform .25s;font-size:14px;max-width:90%;text-align:center}
#toast.show{transform:translate(-50%,0)}
#toast.err{border-color:var(--bad);color:#ffd0d5}
</style></head><body><div class="wrap">

<header><h1>&#128225; IR Remote</h1>
<div class="pill" id="status"><span class="dot"></span><span id="stxt">Listening</span></div></header>

<div class="card" id="lastCard">
  <h2><span>Last received</span><span id="lastInfo"></span></h2>
  <div id="lastEmpty" class="empty">Point a remote at the receiver and press a button.</div>
  <div id="lastBox" hidden>
    <div class="code" id="lcode"></div>
    <div class="meta" id="lmeta"></div>
    <div class="row">
      <input id="name" maxlength="32" placeholder="Name it, e.g. TV Power" autocomplete="off">
      <button id="saveBtn">Save</button>
    </div>
    <div class="row"><button class="ghost" id="testBtn" style="flex:1">&#9654; Test send</button></div>
  </div>
</div>

<div class="card">
  <h2><span>My buttons</span><button class="ghost small" id="editBtn">Edit</button></h2>
  <div class="grid" id="grid"></div>
  <div class="empty" id="gridEmpty">No saved buttons yet.</div>
</div>

<div class="card" id="netCard">
  <h2><span>Online &middot; error.blue</span><span id="netPill" class="st-wait">&hellip;</span></h2>
  <div class="kv"><span>Home Wi-Fi</span><b id="staTxt">&hellip;</b></div>
  <div class="kv"><span>error.blue link</span><b id="cloudTxt">&hellip;</b></div>
  <div id="codeBox" hidden>
    <div class="lc" id="linkCode">------</div>
    <div class="meta" style="text-align:center">LINK CODE &middot; type it on error.blue/lamp</div>
    <div class="row"><a class="btn" id="openLamp" target="_blank" style="flex:1">Open error.blue/lamp</a><button class="ghost" id="newCode">New code</button></div>
  </div>
  <details id="wifiBox"><summary>Wi-Fi settings</summary>
    <div class="row"><select id="ssidSel"><option value="">Pick a network&hellip;</option></select><button class="ghost" id="scanBtn">Scan</button></div>
    <div class="row"><input id="ssid" placeholder="Network name" autocomplete="off" autocapitalize="off"></div>
    <div class="row"><input id="pass" type="password" placeholder="Password" autocomplete="off"><button id="wifiSave">Connect</button></div>
    <div class="row"><button class="ghost" id="wifiForget" style="flex:1">Forget network</button><button class="ghost" id="cloudToggle" style="flex:1">Link off</button></div>
  </details>
</div>

<div class="foot">
  <a class="btn" href="/export.txt">&#11015; Download .txt</a>
  <a class="btn ghost" href="/export.txt?view=1" target="_blank">View .txt</a>
  <button class="ghost" id="restoreBtn">&#11014; Restore .txt</button>
  <input type="file" id="restoreFile" accept=".txt,text/plain" hidden>
</div>
</div>
<div id="toast"></div>

<script>
const $=id=>document.getElementById(id);
let seq=-1,force=false,list=[],edit=false,tt,st=null;

function toast(m,err){const t=$('toast');t.textContent=m;t.className='show'+(err?' err':'');
  clearTimeout(tt);tt=setTimeout(()=>t.classList.remove('show'),2600);}

async function api(url,data){
  const o=data?{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:new URLSearchParams(data)}:{cache:'no-store'};
  const r=await fetch(url,o);let j={};try{j=await r.json()}catch(e){}
  if(!r.ok||j.ok===false)throw new Error(j.error||('Error '+r.status));
  return j;
}

function online(on){$('status').classList.toggle('off',!on);$('stxt').textContent=on?'Listening':'Disconnected';}

async function poll(){
  try{
    const j=await api('/api/last');online(true);
    if(j.seq!==seq||force){const anim=seq>=0&&j.seq!==seq;seq=j.seq;force=false;showLast(j,anim);}
  }catch(e){online(false);}
  setTimeout(poll,700);
}

function showLast(j,anim){
  $('lastEmpty').hidden=j.has;$('lastBox').hidden=!j.has;
  if(!j.has){$('lastInfo').textContent='';return;}
  $('lcode').textContent=j.code;
  $('lmeta').textContent=j.protocol+(j.bits?' · '+j.bits+' bits':'')+' · '+j.len+' pulses'+(j.overflow?' · TOO LONG, cut off':'');
  $('lastInfo').textContent=j.match?('✓ saved as "'+j.match+'"'):'';
  if(anim){const c=$('lastCard');c.classList.remove('flash');void c.offsetWidth;c.classList.add('flash');
    if(navigator.vibrate)navigator.vibrate(30);}
}

function render(){
  const g=$('grid');g.innerHTML='';$('gridEmpty').hidden=list.length>0;
  list.forEach(s=>{
    const b=document.createElement('div');b.className='sig';
    const n=document.createElement('b');n.textContent=s.name;
    const sm=document.createElement('small');sm.textContent=s.protocol+' '+s.code;
    const tl=document.createElement('div');tl.className='tools';
    const rn=document.createElement('span');rn.textContent='Rename';
    const dl=document.createElement('span');dl.textContent='Delete';dl.className='del';
    tl.append(rn,dl);b.append(n,sm,tl);g.append(b);
    b.onclick=()=>{if(!edit)send(s.id,b);};
    rn.onclick=async e=>{e.stopPropagation();const nn=prompt('New name',s.name);if(nn===null)return;
      try{list=(await api('/api/rename',{id:s.id,name:nn})).signals;render();force=true;}catch(err){toast(err.message,1);}};
    dl.onclick=async e=>{e.stopPropagation();if(!confirm('Delete "'+s.name+'"?'))return;
      try{list=(await api('/api/delete',{id:s.id})).signals;render();force=true;}catch(err){toast(err.message,1);}};
  });
}

async function send(id,el){
  el.classList.add('sent');
  try{await api('/api/send',{id:id});}catch(e){toast(e.message,1);}
  setTimeout(()=>el.classList.remove('sent'),350);
}

$('saveBtn').onclick=async()=>{
  const n=$('name').value.trim();const d={name:n};if(seq>=0)d.seq=seq;
  try{const j=await api('/api/save',d);list=j.signals;render();$('name').value='';force=true;
    toast('Saved "'+list[list.length-1].name+'"');}catch(e){toast(e.message,1);}
};
$('name').onkeydown=e=>{if(e.key==='Enter')$('saveBtn').click();};
$('testBtn').onclick=async()=>{try{await api('/api/sendlast',{});toast('Sent');}catch(e){toast(e.message,1);}};
$('editBtn').onclick=()=>{edit=!edit;$('grid').classList.toggle('edit',edit);$('editBtn').textContent=edit?'Done':'Edit';};

// ---------- online: home Wi-Fi + error.blue ----------
function cls(el,c){el.className=c;}
function showStatus(s){
  st=s;
  const sta=$('staTxt');
  if(!s.ssid){sta.textContent='not set — open Wi-Fi settings';cls(sta,'st-wait');}
  else if(s.sta){sta.textContent=s.ssid+' · '+s.ip+' · '+s.rssi+' dBm';cls(sta,'st-ok');}
  else{sta.textContent=s.ssid+' · '+(s.stamsg||'connecting…');cls(sta,s.stamsg&&s.stamsg.indexOf('connecting')<0?'st-bad':'st-wait');}
  const c=$('cloudTxt');let pill='offline',pc='st-bad';
  if(!s.cloud){c.textContent='switched off';cls(c,'st-wait');pill='off';pc='st-wait';}
  else if(s.up){c.textContent='online · '+s.host+(s.tls==='skip'?' (TLS: skip)':'');cls(c,'st-ok');pill='● online';pc='st-ok';}
  else if(!s.sta){c.textContent='needs home Wi-Fi';cls(c,'st-wait');}
  else{c.textContent='connecting…'+(s.err?' ('+s.err+')':'');cls(c,'st-wait');pill='connecting';pc='st-wait';}
  $('netPill').textContent=pill;cls($('netPill'),pc);
  $('codeBox').hidden=!s.code;
  $('linkCode').textContent=s.code||'';
  $('openLamp').href='https://error.blue/lamp/?code='+encodeURIComponent(s.code||'');
  $('cloudToggle').textContent=s.cloud?'Link off':'Link on';
  if(!s.ssid&&!$('wifiBox').dataset.opened){$('wifiBox').open=true;$('wifiBox').dataset.opened=1;}
}
async function statusLoop(){
  try{showStatus(await api('/api/status'));}catch(e){}
  setTimeout(statusLoop,2500);
}
$('scanBtn').onclick=async()=>{
  $('scanBtn').textContent='…';
  try{const j=await api('/api/scan');const s=$('ssidSel');s.innerHTML='<option value="">'+j.nets.length+' networks found</option>';
    j.nets.forEach(n=>{const o=document.createElement('option');o.value=n.s;
      o.textContent=n.s+'  '+(n.r>-60?'▂▄▆':n.r>-75?'▂▄':'▂')+(n.l?' 🔒':'');s.append(o);});
  }catch(e){toast(e.message,1);}
  $('scanBtn').textContent='Scan';
};
$('ssidSel').onchange=()=>{if($('ssidSel').value){$('ssid').value=$('ssidSel').value;$('pass').focus();}};
$('wifiSave').onclick=async()=>{
  const ssid=$('ssid').value.trim();if(!ssid){toast('Type or pick a network',1);return;}
  try{await api('/api/wifi',{ssid:ssid,pass:$('pass').value});$('pass').value='';toast('Connecting to '+ssid+'…');}catch(e){toast(e.message,1);}
};
$('wifiForget').onclick=async()=>{if(!confirm('Forget the home Wi-Fi?'))return;
  try{await api('/api/wifi',{ssid:''});toast('Forgotten');}catch(e){toast(e.message,1);}};
$('cloudToggle').onclick=async()=>{try{showStatus(await api('/api/cloud',{on:st&&st.cloud?0:1}));}catch(e){toast(e.message,1);}};
$('newCode').onclick=async()=>{if(!confirm('Make a new link code? Pages that use the old one lose the lamp.'))return;
  try{showStatus(await api('/api/cloud',{newcode:1}));toast('New code');}catch(e){toast(e.message,1);}};
$('restoreBtn').onclick=()=>$('restoreFile').click();
$('restoreFile').onchange=async()=>{
  const f=$('restoreFile').files[0];if(!f)return;
  try{const r=await fetch('/api/restore',{method:'POST',headers:{'Content-Type':'text/plain'},body:await f.text()});
    const j=await r.json();if(!r.ok||j.ok===false)throw new Error(j.error||'Error');
    list=j.signals;render();toast('Restored '+j.added+' button'+(j.added==1?'':'s')+(j.skipped?' ('+j.skipped+' already there)':''));
  }catch(e){toast(e.message,1);}
  $('restoreFile').value='';
};

api('/api/list').then(j=>{list=j.signals;render();}).catch(()=>{});
poll();statusLoop();
</script></body></html>)rawliteral";

// ============================================================
//  WEB API
// ============================================================
String jsonEsc(const String& s) {
  String o;
  o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if ((uint8_t)c < 0x20) {
      char b[8];
      snprintf(b, sizeof(b), "\\u%04x", (unsigned)(uint8_t)c);
      o += b;
    }
    else o += c;
  }
  return o;
}

String listJson() {
  String o = "[";
  for (size_t i = 0; i < saved.size(); i++) {
    const IrSignal& s = saved[i];
    if (i) o += ',';
    o += "{\"id\":" + String((int)i) +
         ",\"name\":\"" + jsonEsc(s.name) +
         "\",\"protocol\":\"" + s.protocol +
         "\",\"bits\":" + String(s.bits) +
         ",\"code\":\"" + s.code +
         "\",\"len\":" + String((int)s.raw.size()) + "}";
  }
  o += "]";
  return o;
}

String cleanName(String n) {
  n.replace("\t", " ");
  n.replace("\r", " ");
  n.replace("\n", " ");
  n.trim();
  if (n.length() > MAX_NAME_LEN) {
    size_t cut = MAX_NAME_LEN;
    while (cut > 0 && (((uint8_t)n[cut]) & 0xC0) == 0x80) cut--;   // don't split UTF-8
    n = n.substring(0, cut);
    n.trim();
  }
  return n;
}

void sendJson(int code, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

void sendError(int code, const String& msg) {
  sendJson(code, "{\"ok\":false,\"error\":\"" + jsonEsc(msg) + "\"}");
}

void sendList() {
  sendJson(200, "{\"ok\":true,\"signals\":" + listJson() + "}");
}

int argId() {
  if (!server.hasArg("id")) return -1;
  long id = server.arg("id").toInt();
  if (id < 0 || id >= (long)saved.size()) return -1;
  return (int)id;
}

void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleLast() {
  String o = "{\"ok\":true,\"seq\":" + String(lastSeq) + ",\"has\":" + (haveLast ? "true" : "false");
  if (haveLast) {
    o += ",\"protocol\":\"" + last.protocol +
         "\",\"bits\":" + String(last.bits) +
         ",\"code\":\"" + last.code +
         "\",\"len\":" + String((int)last.raw.size()) +
         ",\"overflow\":" + (lastOverflow ? "true" : "false");
    int m = findMatch(last);
    if (m >= 0) o += ",\"match\":\"" + jsonEsc(saved[m].name) + "\"";
  }
  o += "}";
  sendJson(200, o);
}

void handleSave() {
  if (!haveLast) return sendError(400, "Nothing received yet");
  if (server.hasArg("seq") && (uint32_t)server.arg("seq").toInt() != lastSeq)
    return sendError(409, "A newer signal just came in - check it and press Save again");
  if (saved.size() >= MAX_SIGNALS) return sendError(400, "Memory full - delete some buttons first");

  IrSignal s = last;
  s.name = cleanName(server.arg("name"));
  if (s.name.length() == 0) s.name = "Button " + String((int)saved.size() + 1);
  saved.push_back(s);
  if (!saveAll()) { saved.pop_back(); return sendError(500, "Could not write to flash"); }

  Serial.printf("Saved \"%s\"\n", s.name.c_str());
  oledShow("IR Remote", apIpStr, "Saved:", s.name);
  sendList();
}

void handleSend() {
  int id = argId();
  if (id < 0) return sendError(400, "Unknown button");
  irSendRaw(saved[id].raw, repeatsFor(saved[id]));
  cloudTx(saved[id], "local");
  Serial.printf("Sent \"%s\"\n", saved[id].name.c_str());
  oledShow("IR Remote", apIpStr, "Sent:", saved[id].name);
  sendJson(200, "{\"ok\":true}");
}

void handleSendLast() {
  if (!haveLast) return sendError(400, "Nothing received yet");
  irSendRaw(last.raw, repeatsFor(last));
  cloudTx(last, "local");
  oledShow("IR Remote", apIpStr, "Sent test", last.code);
  sendJson(200, "{\"ok\":true}");
}

void handleRename() {
  int id = argId();
  if (id < 0) return sendError(400, "Unknown button");
  String n = cleanName(server.arg("name"));
  if (n.length() == 0) return sendError(400, "Name can't be empty");
  String old = saved[id].name;
  saved[id].name = n;
  if (!saveAll()) { saved[id].name = old; return sendError(500, "Could not write to flash"); }
  sendList();
}

void handleDelete() {
  int id = argId();
  if (id < 0) return sendError(400, "Unknown button");
  saved.erase(saved.begin() + id);
  if (!saveAll()) return sendError(500, "Could not write to flash");
  sendList();
}

String padRight(const String& s, unsigned w) {
  String o = s;
  while (o.length() < w) o += ' ';
  return o;
}

void handleExport() {
  if (!server.hasArg("view"))
    server.sendHeader("Content-Disposition", "attachment; filename=\"ir_signals.txt\"");
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/plain; charset=utf-8", "");

  String t = "IR REMOTE - SAVED SIGNALS\r\n";
  t += "Buttons saved: " + String((int)saved.size()) + "\r\n\r\n";
  t += " #   " + padRight("Name", MAX_NAME_LEN) + "  " + padRight("Protocol", 16) + "  Bits  Code\r\n";
  t += "---  ";
  for (int i = 0; i < MAX_NAME_LEN; i++) t += '-';
  t += "  ----------------  ----  ------------------\r\n";
  server.sendContent(t);

  for (size_t i = 0; i < saved.size(); i++) {
    const IrSignal& s = saved[i];
    t = padRight(String((int)i + 1), 3) + "  " + padRight(s.name, MAX_NAME_LEN) + "  " +
        padRight(s.protocol, 16) + "  " + padRight(String(s.bits), 4) + "  " + s.code + "\r\n";
    server.sendContent(t);
  }

  t = "\r\n\r\nRAW TIMINGS  (microseconds: mark,space,mark,space,...  carrier " +
      String(IR_CARRIER_HZ / 1000) + " kHz)\r\n";
  t += "==========================================================================\r\n\r\n";
  server.sendContent(t);

  for (size_t i = 0; i < saved.size(); i++) {
    const IrSignal& s = saved[i];
    t = "#" + String((int)i + 1) + "  " + s.name + "   [" + s.protocol + " " + s.code + "]\r\n";
    t += rawCsv(s.raw) + "\r\n\r\n";
    server.sendContent(t);
  }
  server.sendContent("");   // end of response
}

// Puts buttons back from a downloaded ir_signals.txt (the RAW TIMINGS part)
void handleRestore() {
  String body = server.arg("plain");
  if (body.length() == 0) return sendError(400, "That file is empty");
  int added = 0, skipped = 0;
  int pos = 0;
  const int len = body.length();
  while (pos < len) {
    int nl = body.indexOf('\n', pos);
    if (nl < 0) nl = len;
    String line = body.substring(pos, nl);
    pos = nl + 1;
    line.replace("\r", "");
    if (line.length() < 3 || line[0] != '#') continue;          // "#12  Name   [NEC 0x00F720DF]"
    int sp = line.indexOf(' ');
    int br = line.lastIndexOf("   [");
    if (sp < 0 || br < sp) continue;
    String name = line.substring(sp, br);
    int nl2 = body.indexOf('\n', pos);                          // the next line: the timings
    if (nl2 < 0) nl2 = len;
    String rawLine = body.substring(pos, nl2);
    pos = nl2 + 1;
    IrSignal s;
    parseCsv(rawLine.c_str(), s.raw);
    if (s.raw.size() < IR_MIN_EDGES) continue;
    if ((s.raw.size() & 1) == 0) s.raw.pop_back();
    decodeSignal(s);
    s.name = cleanName(name);
    if (s.name.length() == 0) s.name = "Button " + String((int)saved.size() + 1);
    if (findMatch(s) >= 0) { skipped++; continue; }
    if (saved.size() >= MAX_SIGNALS) break;
    saved.push_back(s);
    added++;
  }
  if (added == 0 && skipped == 0) return sendError(400, "No signals found in that file");
  if (added && !saveAll()) return sendError(500, "Could not write to flash");
  Serial.printf("Restored %d buttons (%d already there)\n", added, skipped);
  sendJson(200, "{\"ok\":true,\"added\":" + String(added) + ",\"skipped\":" + String(skipped) +
                ",\"signals\":" + listJson() + "}");
}

// ============================================================
//  HOME WI-FI  (the board's own network stays on as well)
// ============================================================
static bool     staWasUp = false, staTrying = false;
static uint32_t staTryStart = 0, staNextTry = 0, staUpAt = 0;
static String   staMsg = "";

void staStart() {
  staWasUp = false; staTrying = false; staNextTry = 0; staMsg = "";
  WiFi.disconnect(false);
  if (staSsid.length()) staMsg = "connecting...";
}

void staStep() {
  if (staSsid.length() == 0) return;
  wl_status_t s = WiFi.status();
  if (s == WL_CONNECTED) {
    if (!staWasUp) {
      staWasUp = true; staTrying = false; staUpAt = millis(); staMsg = "";
      Serial.printf("Home Wi-Fi: connected to %s, IP %s\n", staSsid.c_str(), WiFi.localIP().toString().c_str());
      configTzTime(tzStr.length() ? tzStr.c_str() : "UTC0", "pool.ntp.org", "time.google.com");   // the certificate check (and the alarms) need the date
      oledShow("IR Remote", WiFi.localIP().toString(), staSsid, "online");
    }
    return;
  }
  if (staWasUp) {                                   // lost it: try again at once
    staWasUp = false; staTrying = false; staNextTry = millis(); staMsg = "connecting...";
    Serial.println("Home Wi-Fi: lost the connection");
  }
  if (staTrying) {
    if (millis() - staTryStart > 20000) {           // give up for a minute (keeps our own network steady)
      staMsg = (s == WL_NO_SSID_AVAIL) ? "network not found" :
               (s == WL_CONNECT_FAILED) ? "wrong password?" : "can't connect";
      Serial.printf("Home Wi-Fi: %s (%d), next try in 60 s\n", staMsg.c_str(), (int)s);
      WiFi.disconnect(false);
      staTrying = false;
      staNextTry = millis() + 60000;
    }
    return;
  }
  if ((int32_t)(millis() - staNextTry) >= 0) {
    WiFi.begin(staSsid.c_str(), staPass.c_str());
    staTrying = true; staTryStart = millis();
    if (staMsg.length() == 0 || staMsg == "connecting...") staMsg = "connecting...";
  }
}

void wifiBegin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.setAutoReconnect(false);                     // staStep() decides when to try
#if REDUCE_TX_POWER
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
#endif
  apIpStr = WiFi.softAPIP().toString();
  staStart();
  if (MDNS.begin(MDNS_NAME)) MDNS.addService("http", "tcp", 80);
  Serial.printf("Own Wi-Fi: %s  open http://%s  or  http://%s.local\n", AP_SSID, apIpStr.c_str(), MDNS_NAME);
  if (staSsid.length()) Serial.printf("Home Wi-Fi: %s\n", staSsid.c_str());
  else Serial.println("Home Wi-Fi: not set (web page -> Wi-Fi settings, or WIFI \"name\" password here)");
}

void setHomeWifi(const String& ssid, const String& pass) {
  staSsid = ssid; staPass = pass;
  prefs.putString("ssid", staSsid);
  prefs.putString("pass", staPass);
  staStart();
}

// ============================================================
//  error.blue LINK  (WebSocket to the room /api/room/ir-lamp/<CODE>)
// ============================================================
WiFiClientSecure tls;
static bool     wsUp = false, tlsSkip = false;
static uint32_t wsNextTry = 0, wsBackoff = 2000, wsLastPing = 0, wsLastRx = 0, helloAt = 0;
static String   cloudErr = "", myId = "";
static char     wsMsg[12289];
static size_t   wsMsgLen = 0;
static bool     wsMsgBig = false;

String newLinkCode() {
  static const char A[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  String c;
  for (int i = 0; i < 6; i++) c += A[esp_random() % 32];
  return c;
}

static bool wsReadExact(uint8_t* p, size_t n, uint32_t ms = 4000) {
  uint32_t t0 = millis();
  while (n) {
    int a = tls.available();
    if (a > 0) {
      int r = tls.read(p, min((size_t)a, n));
      if (r > 0) { p += r; n -= r; t0 = millis(); continue; }
    }
    if (!tls.connected()) return false;
    if (millis() - t0 > ms) return false;
    delay(1);
  }
  return true;
}

static bool wsSend(uint8_t op, const uint8_t* d, size_t n) {
  uint8_t h[14];
  size_t hl = 2;
  h[0] = 0x80 | op;
  if (n < 126) h[1] = 0x80 | n;
  else if (n < 65536) { h[1] = 0x80 | 126; h[2] = n >> 8; h[3] = n & 255; hl = 4; }
  else return false;
  uint32_t m = esp_random();
  memcpy(h + hl, &m, 4); hl += 4;
  const uint8_t* mk = h + hl - 4;
  if (tls.write(h, hl) != hl) return false;
  uint8_t buf[256];
  for (size_t o = 0; o < n; o += sizeof(buf)) {
    size_t k = min(sizeof(buf), n - o);
    for (size_t i = 0; i < k; i++) buf[i] = d[o + i] ^ mk[(o + i) & 3];
    if (tls.write(buf, k) != k) return false;
  }
  return true;
}
static bool wsText(const String& s) { return wsSend(1, (const uint8_t*)s.c_str(), s.length()); }

void wsDrop(const char* why) {
  if (wsUp) Serial.printf("LINK: %s\n", why);
  tls.stop();
  wsUp = false;
  myId = "";
}

void cloudSay(const String& s) {
  if (wsUp && !wsText(s)) { wsDrop("send failed"); wsNextTry = millis() + 2000; }
}

void cloudTx(const IrSignal& s, const char* from) {
  if (isNec32(s)) {
    uint32_t c = codeValue(s.code);
    histPush(c);
    cloudSay("{\"t\":\"tx\",\"c\":\"" + hex8(c) + "\",\"f\":\"" + String(from) + "\"}");
  } else {
    cloudSay("{\"t\":\"tx\",\"p\":\"" + s.protocol + "\",\"code\":\"" + s.code + "\",\"f\":\"" + String(from) + "\"}");
  }
}

// "who am I": what error.blue pages need to show the lamp
void sendHello() {
  String s;
  s.reserve(400 + saved.size() * 60);
  s = "{\"t\":\"dev\",\"n\":\"" DEVICE_NAME "\",\"v\":\"" FW_VERSION "\",\"ip\":\"" + WiFi.localIP().toString() +
      "\",\"rssi\":" + String(WiFi.RSSI()) + ",\"q\":" + String(irqN) + ",\"gap\":" + String(IR_QUEUE_GAP_MS) + ",\"b\":[";
  for (size_t i = 0; i < saved.size(); i++) {
    if (i) s += ',';
    s += "[\"" + jsonEsc(saved[i].name) + "\",\"" + saved[i].protocol + "\",\"" + saved[i].code + "\"]";
  }
  s += "],\"h\":[";
  for (uint8_t i = 0; i < histN; i++) {
    if (i) s += ',';
    s += "\"" + hex8(hist[(histHead + 24 - histN + i) % 24]) + "\"";
  }
  s += "]," + alarmHelloPart() + "}";
  cloudSay(s);
}

// tiny JSON readers for the few fields we need
static bool jStr(const char* s, const char* key, char* out, size_t n) {
  char pat[24];
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char* p = strstr(s, pat);
  if (!p) return false;
  p += strlen(pat);
  while (*p == ' ') p++;
  if (*p != '"') return false;
  p++;
  size_t k = 0;
  while (*p && *p != '"' && k + 1 < n) { if (*p == '\\' && p[1]) p++; out[k++] = *p++; }
  out[k] = 0;
  return true;
}
static bool jNum(const char* s, const char* key, long& v) {
  char pat[24];
  snprintf(pat, sizeof(pat), "\"%s\":", key);
  const char* p = strstr(s, pat);
  if (!p) return false;
  p += strlen(pat);
  while (*p == ' ' || *p == '"') p++;
  char* e;
  v = strtol(p, &e, 10);
  return e != p;
}

// a message from a page: {"t":"ir","c":"00F720DF","r":1} / {"t":"btn","i":3} / {"t":"clr"} / {"t":"hi"}
void cloudCommand(const char* d, const char* from) {
  char t[8];
  if (!jStr(d, "t", t, sizeof(t))) return;
  if (!strcmp(t, "ir")) {
    char c[16];
    if (!jStr(d, "c", c, sizeof(c))) return;
    uint32_t code = (uint32_t)strtoul(c, nullptr, 16);
    long r = 1;
    jNum(d, "r", r);
    r = constrain(r, 1L, 10L);
    for (long i = 0; i < r; i++) if (!queuePush(0, code, from)) { cloudSay("{\"t\":\"full\"}"); break; }
  } else if (!strcmp(t, "btn")) {
    long i;
    if (jNum(d, "i", i) && i >= 0 && i < (long)saved.size()) queuePush(1, (uint32_t)i, from);
  } else if (!strcmp(t, "clr")) {
    queueClear();
  } else if (!strcmp(t, "hi")) {
    helloAt = millis() + 20;
  } else if (!strcmp(t, "al") || !strcmp(t, "tz") || !strcmp(t, "run") || !strcmp(t, "stop")) {
    alarmCommand(t, d);
  }
}

void cloudMessage(const char* m) {
  char type[12];
  if (!jStr(m, "type", type, sizeof(type))) return;
  if (!strcmp(type, "msg")) {
    char from[16] = "";
    jStr(m, "from", from, sizeof(from));
    const char* d = strstr(m, "\"data\":");
    if (d) cloudCommand(d + 7, from);
  } else if (!strcmp(type, "welcome")) {
    char id[16];
    if (jStr(m, "id", id, sizeof(id))) myId = id;
    cloudErr = "";
    helloAt = millis() + 50;
    Serial.printf("LINK: online in room %s/%s\n", CLOUD_ROOM, linkCode.c_str());
  } else if (!strcmp(type, "join")) {
    helloAt = millis() + 300;                       // a page opened: tell it about the lamp
  } else if (!strcmp(type, "error")) {
    char e[48];
    if (jStr(m, "message", e, sizeof(e))) cloudErr = e;
  }
}

// one frame (control frames answered here); false = the connection broke
static bool wsReadFrame() {
  uint8_t h[2];
  if (!wsReadExact(h, 2)) return false;
  uint8_t op = h[0] & 15;
  bool fin = h[0] & 0x80, masked = h[1] & 0x80;
  uint64_t n = h[1] & 127;
  if (n == 126) { uint8_t e[2]; if (!wsReadExact(e, 2)) return false; n = (uint16_t)(e[0] << 8 | e[1]); }
  else if (n == 127) { uint8_t e[8]; if (!wsReadExact(e, 8)) return false; n = 0; for (int i = 0; i < 8; i++) n = n << 8 | e[i]; }
  uint8_t mk[4] = { 0, 0, 0, 0 };
  if (masked && !wsReadExact(mk, 4)) return false;
  if (n > 1000000) return false;
  if (op >= 8) {                                    // close, ping, pong
    uint8_t c[126];
    if (n > sizeof(c) || !wsReadExact(c, n)) return false;
    if (op == 8) return false;
    if (op == 9) wsSend(10, c, n);
    return true;
  }
  if (op) { wsMsgLen = 0; wsMsgBig = false; }       // op 0 = more of the last message
  uint8_t tmp[256];
  while (n) {                                       // keep what fits, skip the rest
    size_t k = min((uint64_t)sizeof(tmp), n);
    if (!wsReadExact(tmp, k)) return false;
    for (size_t i = 0; i < k; i++) {
      uint8_t b = masked ? tmp[i] ^ mk[i & 3] : tmp[i];
      if (wsMsgLen < sizeof(wsMsg) - 1) wsMsg[wsMsgLen++] = (char)b; else wsMsgBig = true;
    }
    n -= k;
  }
  if (fin) {
    wsMsg[wsMsgLen] = 0;
    if (!wsMsgBig) cloudMessage(wsMsg);
  }
  return true;
}

bool wsOpen() {
#if defined(CLOUD_PLAIN)
  (void)tlsSkip;
#else
  if (tlsSkip) tls.setInsecure(); else tls.setCACert(ROOT_CAS);
  tls.setHandshakeTimeout(12);
#endif
  if (!tls.connect(CLOUD_HOST, CLOUD_PORT)) {
    char e[100] = "";
    tls.lastError(e, sizeof(e));
    String es = e; es.toUpperCase();
    Serial.printf("LINK: can't reach %s (%s)\n", CLOUD_HOST, e);
    cloudErr = es.length() ? String(e).substring(0, 40) : String("no connection");
    if (!tlsSkip && (es.indexOf("X509") >= 0 || es.indexOf("VERIFY") >= 0 || es.indexOf("CERT") >= 0)) {
      tlsSkip = true;
      Serial.println("LINK: certificate check failed, going on without it");
    }
    return false;
  }
  uint8_t key[16];
  for (int i = 0; i < 16; i += 4) { uint32_t r = esp_random(); memcpy(key + i, &r, 4); }
  static const char* B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char k64[25]; int o = 0;
  for (int i = 0; i < 16; i += 3) {
    uint32_t v = key[i] << 16 | (i + 1 < 16 ? key[i + 1] << 8 : 0) | (i + 2 < 16 ? key[i + 2] : 0);
    k64[o++] = B64[v >> 18 & 63]; k64[o++] = B64[v >> 12 & 63];
    k64[o++] = i + 1 < 16 ? B64[v >> 6 & 63] : '='; k64[o++] = i + 2 < 16 ? B64[v & 63] : '=';
  }
  k64[o] = 0;
  String req = String("GET /api/room/") + CLOUD_ROOM + "/" + linkCode + "?name=" + DEVICE_NAME +
               " HTTP/1.1\r\nHost: " + CLOUD_HOST + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n" +
               "Sec-WebSocket-Key: " + k64 + "\r\nSec-WebSocket-Version: 13\r\nUser-Agent: IR-LAMP/" FW_VERSION "\r\n\r\n";
  tls.print(req);
  String line; bool first = true, ok = false;
  uint32_t t0 = millis();
  while (millis() - t0 < 10000) {
    if (!tls.available()) { if (!tls.connected()) break; delay(2); continue; }
    char c = tls.read();
    if (c == '\r') continue;
    if (c != '\n') { if (line.length() < 200) line += c; continue; }
    if (first) { ok = line.indexOf(" 101") > 0; first = false; if (!ok) { Serial.printf("LINK: server said %s\n", line.c_str()); cloudErr = line.substring(0, 40); } }
    else if (!line.length()) break;
    line = "";
  }
  if (!ok) { tls.stop(); return false; }
  wsLastRx = wsLastPing = millis();
  wsMsgLen = 0;
  return true;
}

void cloudStep() {
  bool can = cloudOn && linkCode.length() && WiFi.status() == WL_CONNECTED;
  if (!can) { if (wsUp) wsDrop("offline"); return; }
  if (!wsUp) {
    if ((int32_t)(millis() - wsNextTry) < 0) return;
#if !defined(CLOUD_PLAIN)
    // the certificate check needs today's date: give the clock up to 15 s after Wi-Fi came up
    if (!tlsSkip && time(nullptr) < 1600000000 && millis() - staUpAt < 15000) return;
#endif
    Serial.printf("LINK: connecting to %s ...\n", CLOUD_HOST);
    wsUp = wsOpen();
    if (!wsUp) { wsNextTry = millis() + wsBackoff; wsBackoff = min(wsBackoff * 2, (uint32_t)60000); return; }
    wsBackoff = 2000;
    return;
  }
  uint32_t t0 = millis();
  while (tls.available() >= 2 && millis() - t0 < 40) {
    if (!wsReadFrame()) { wsDrop("connection lost"); wsNextTry = millis() + 2000; return; }
    wsLastRx = millis();
  }
  if (!tls.connected()) { wsDrop("closed"); wsNextTry = millis() + 2000; return; }
  if (millis() - wsLastPing > 25000) {
    wsLastPing = millis();
    if (!wsText("{\"type\":\"ping\"}")) { wsDrop("ping failed"); wsNextTry = millis() + 2000; return; }
  }
  if (millis() - wsLastRx > 80000) { wsDrop("no answer"); wsNextTry = millis() + 2000; return; }
  if (helloAt && (int32_t)(millis() - helloAt) >= 0) { helloAt = 0; sendHello(); }
}

void handleStatus() {
  bool sta = WiFi.status() == WL_CONNECTED;
  String o = "{\"ok\":true,\"v\":\"" FW_VERSION "\",\"ap\":\"" AP_SSID "\",\"apip\":\"" + apIpStr +
             "\",\"ssid\":\"" + jsonEsc(staSsid) + "\",\"sta\":" + (sta ? "true" : "false") +
             ",\"ip\":\"" + (sta ? WiFi.localIP().toString() : String("")) + "\",\"rssi\":" + String(sta ? WiFi.RSSI() : 0) +
             ",\"stamsg\":\"" + jsonEsc(staMsg) + "\",\"cloud\":" + (cloudOn ? "true" : "false") +
             ",\"up\":" + (wsUp ? "true" : "false") + ",\"code\":\"" + linkCode + "\",\"host\":\"" CLOUD_HOST "\"" +
             ",\"tls\":\"" + (tlsSkip ? "skip" : "ok") + "\",\"err\":\"" + jsonEsc(cloudErr) + "\"}";
  sendJson(200, o);
}

void handleScan() {
  int n = WiFi.scanNetworks(false, false);
  if (n < 0) return sendError(503, "Busy - try again in a moment");
  std::vector<int> idx;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i).length() == 0) continue;
    bool dup = false;
    for (int j : idx) if (WiFi.SSID(j) == WiFi.SSID(i)) { dup = true; break; }
    if (!dup) idx.push_back(i);
  }
  String o = "{\"ok\":true,\"nets\":[";
  for (size_t k = 0; k < idx.size() && k < 30; k++) {
    int i = idx[k];
    if (k) o += ',';
    o += "{\"s\":\"" + jsonEsc(WiFi.SSID(i)) + "\",\"r\":" + String(WiFi.RSSI(i)) +
         ",\"l\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "0" : "1") + "}";
  }
  o += "]}";
  WiFi.scanDelete();
  sendJson(200, o);
}

void handleWifi() {
  String ssid = server.arg("ssid");
  ssid.trim();
  if (ssid.length() > 32) return sendError(400, "Network name is too long");
  setHomeWifi(ssid, server.arg("pass"));
  Serial.printf("Home Wi-Fi set to \"%s\"\n", ssid.c_str());
  sendJson(200, "{\"ok\":true}");
}

void handleCloud() {
  if (server.hasArg("on")) {
    cloudOn = server.arg("on").toInt() != 0;
    prefs.putBool("cloud", cloudOn);
    if (!cloudOn) wsDrop("switched off");
    wsNextTry = 0;
  }
  if (server.hasArg("newcode")) {
    linkCode = newLinkCode();
    prefs.putString("code", linkCode);
    wsDrop("new code");
    wsNextTry = 0;
    Serial.printf("New link code: %s\n", linkCode.c_str());
  }
  handleStatus();
}

// ============================================================
//  ALARMS  (sunrise wake-up light: the board runs them itself)
// ============================================================
// Kept in /alarms.txt as one text: alarms joined by '^', each  id|on|hh|mm|days|steps|meta
//   days:  bit 0 = Sunday .. bit 6 = Saturday; 0 = once (it switches itself off afterwards)
//   steps: offset:code,offset:code,...  seconds from the alarm time (negative = before it), NEC code in hex
//   meta:  the page's own settings (URL-encoded), kept as they are
// error.blue/lamp/alarm writes the whole list ({t:"al", a}), the board runs it with its own clock.
struct AlarmStep { int32_t off; uint32_t code; };
struct Alarm {
  uint16_t id = 0; bool on = true; uint8_t hh = 7, mm = 0, days = 0;
  std::vector<AlarmStep> steps;
  String meta;
  time_t runE = 0, doneE = 0;      // the alarm time being run now / the last one finished
  uint16_t next = 0;
};
std::vector<Alarm> alarms;
Alarm program;                     // a one-off from a page (try a sunrise, fall asleep), id 0
bool programOn = false;
static const char* AL_FILE = "/alarms.txt";
#define MAX_ALARMS 8
#define MAX_STEPS  64

void parseSteps(const char* s, std::vector<AlarmStep>& out) {
  out.clear();
  while (*s && out.size() < MAX_STEPS) {
    char* e;
    long off = strtol(s, &e, 10);
    if (e == s || *e != ':') break;
    s = e + 1;
    uint32_t code = (uint32_t)strtoul(s, &e, 16);
    if (e == s) break;
    out.push_back({ (int32_t)off, code });
    s = e;
    if (*s == ',') s++; else break;
  }
  std::stable_sort(out.begin(), out.end(), [](const AlarmStep& a, const AlarmStep& b) { return a.off < b.off; });
}

bool parseAlarm(const String& line, Alarm& a) {
  int p[6], from = 0;
  for (int i = 0; i < 6; i++) { int q = line.indexOf('|', from); if (q < 0) return false; p[i] = q; from = q + 1; }
  a.id   = (uint16_t)line.substring(0, p[0]).toInt();
  a.on   = line.substring(p[0] + 1, p[1]).toInt() != 0;
  a.hh   = (uint8_t)constrain((int)line.substring(p[1] + 1, p[2]).toInt(), 0, 23);
  a.mm   = (uint8_t)constrain((int)line.substring(p[2] + 1, p[3]).toInt(), 0, 59);
  a.days = (uint8_t)(line.substring(p[3] + 1, p[4]).toInt() & 127);
  String st = line.substring(p[4] + 1, p[5]);
  parseSteps(st.c_str(), a.steps);
  a.meta = line.substring(p[5] + 1);
  if (a.meta.length() > 400) a.meta = a.meta.substring(0, 400);
  return a.id > 0;
}

String stepsText(const std::vector<AlarmStep>& v) {
  String o;
  for (size_t i = 0; i < v.size(); i++) { if (i) o += ','; o += String((long)v[i].off) + ":" + hex8(v[i].code); }
  return o;
}

String alarmsText() {
  String o;
  for (size_t i = 0; i < alarms.size(); i++) {
    const Alarm& a = alarms[i];
    if (i) o += '^';
    o += String(a.id) + "|" + (a.on ? "1" : "0") + "|" + String(a.hh) + "|" + String(a.mm) + "|" + String(a.days) + "|" + stepsText(a.steps) + "|" + a.meta;
  }
  return o;
}

bool setAlarms(const String& txt, bool save) {
  std::vector<Alarm> nv;
  int from = 0;
  while (from < (int)txt.length() && nv.size() < MAX_ALARMS) {
    int q = txt.indexOf('^', from);
    if (q < 0) q = txt.length();
    Alarm a;
    if (parseAlarm(txt.substring(from, q), a)) {
      for (const Alarm& o : alarms) if (o.id == a.id && o.hh == a.hh && o.mm == a.mm) { a.runE = o.runE; a.doneE = o.doneE; a.next = o.next; }
      nv.push_back(a);
    }
    from = q + 1;
  }
  alarms = nv;
  if (save) {
    File f = LittleFS.open(AL_FILE, "w");
    if (!f) return false;
    f.print(alarmsText());
    f.close();
  }
  return true;
}

void loadAlarms() {
  if (!LittleFS.exists(AL_FILE)) return;
  File f = LittleFS.open(AL_FILE, "r");
  if (!f) return;
  String t = f.readString();
  f.close();
  setAlarms(t, false);
  Serial.printf("Loaded %u alarms\n", (unsigned)alarms.size());
}

bool alarmRunning() {
  if (programOn) return true;
  for (const Alarm& a : alarms) if (a.runE) return true;
  return false;
}

String nowText() {
  time_t now = time(nullptr);
  if (now < 1600000000) return String("");
  struct tm tm;
  localtime_r(&now, &tm);
  char b[24];
  strftime(b, sizeof(b), "%a %H:%M:%S", &tm);
  return String(b);
}

// the part of the hello about alarms: board time, time zone, the list, what runs now
String alarmHelloPart() {
  time_t now = time(nullptr);
  int run = -1;
  if (programOn) run = 0;
  for (const Alarm& a : alarms) if (a.runE) run = a.id;
  return "\"ts\":" + String((unsigned long)(now > 1600000000 ? now : 0)) + ",\"lt\":\"" + nowText() + "\",\"tz\":\"" + jsonEsc(tzStr) +
         "\",\"run\":" + String(run) + ",\"al\":\"" + jsonEsc(alarmsText()) + "\"";
}

void sayAlarms() { cloudSay("{\"t\":\"alarms\"," + alarmHelloPart() + "}"); }

void finishAlarm(Alarm& a, const char* why) {
  cloudSay("{\"t\":\"alarm\",\"s\":\"" + String(why) + "\",\"i\":" + String(a.id) + "}");
  Serial.printf("Alarm %u: %s\n", a.id, why);
  a.doneE = a.runE; a.runE = 0; a.next = 0;
  if (&a == &program) { programOn = false; return; }
  if (!a.days && a.on) { a.on = false; setAlarms(alarmsText(), true); }   // a one-time alarm switches itself off
  sayAlarms();
}

void stopAlarms(const char* why) {
  bool any = false;
  for (Alarm& a : alarms) if (a.runE) { finishAlarm(a, why); any = true; }
  if (programOn) { finishAlarm(program, why); any = true; }
  if (any) queueClear();
}

// the alarm time on the day 'shift' days from today (local time, summer time handled by the TZ)
time_t alarmEpoch(time_t now, int shift, int hh, int mm, int& wday) {
  struct tm tm;
  localtime_r(&now, &tm);
  tm.tm_mday += shift; tm.tm_hour = hh; tm.tm_min = mm; tm.tm_sec = 0; tm.tm_isdst = -1;
  time_t e = mktime(&tm);
  struct tm t2;
  localtime_r(&e, &t2);
  wday = t2.tm_wday;
  return e;
}

void runSteps(Alarm& a, time_t now) {
  while (a.next < a.steps.size() && now >= a.runE + a.steps[a.next].off) {
    if (!queuePush(0, a.steps[a.next].code, "alarm")) break;
    a.next++;
  }
  if (a.next >= a.steps.size()) finishAlarm(a, "done");
}

void alarmStep() {
  static uint32_t last = 0;
  if (millis() - last < 500) return;
  last = millis();
  time_t now = time(nullptr);
  if (now < 1600000000) return;                      // no clock yet (needs home Wi-Fi once)
  for (Alarm& a : alarms) {
    if (!a.on || a.steps.empty()) continue;
    if (!a.runE) {
      for (int sh = -1; sh <= 1; sh++) {
        int wd;
        time_t E = alarmEpoch(now, sh, a.hh, a.mm, wd);
        if (a.days && !((a.days >> wd) & 1)) continue;
        if (E == a.doneE) continue;
        time_t s0 = E + a.steps.front().off, s1 = E + a.steps.back().off;
        if (now < s0 || now > s1 + 2) continue;
        a.runE = E; a.next = 0;
        while (a.next < a.steps.size() && E + a.steps[a.next].off < now - 90) a.next++;   // started late: skip what's long past
        Serial.printf("Alarm %u %02u:%02u: starting\n", a.id, a.hh, a.mm);
        cloudSay("{\"t\":\"alarm\",\"s\":\"start\",\"i\":" + String(a.id) + "}");
        break;
      }
    }
    if (a.runE) runSteps(a, now);
  }
  if (programOn) runSteps(program, now);
}

void alarmCommand(const char* t, const char* d) {
  static char buf[8192];
  if (!strcmp(t, "al")) {
    if (!jStr(d, "a", buf, sizeof(buf))) buf[0] = 0;
    setAlarms(String(buf), true);
    Serial.printf("Alarms set from error.blue: %u\n", (unsigned)alarms.size());
    sayAlarms();
  } else if (!strcmp(t, "tz")) {
    char z[72];
    if (!jStr(d, "z", z, sizeof(z)) || !z[0]) return;
    tzStr = z;
    prefs.putString("tz", tzStr);
    setenv("TZ", z, 1); tzset();
    Serial.printf("Time zone: %s (now %s)\n", z, nowText().c_str());
    sayAlarms();
  } else if (!strcmp(t, "run")) {
    if (!jStr(d, "s", buf, sizeof(buf))) return;
    time_t now = time(nullptr);
    if (now < 1600000000) { cloudSay("{\"t\":\"alarm\",\"s\":\"noclock\",\"i\":0}"); return; }
    if (programOn) finishAlarm(program, "replaced");
    program = Alarm();
    parseSteps(buf, program.steps);
    if (program.steps.empty()) return;
    program.runE = now;
    programOn = true;
    cloudSay("{\"t\":\"alarm\",\"s\":\"start\",\"i\":0}");
  } else if (!strcmp(t, "stop")) {
    stopAlarms("stopped");
  }
}

void printAlarms() {
  Serial.printf("Board time: %s  TZ: %s\n", nowText().length() ? nowText().c_str() : "(no clock yet)", tzStr.c_str());
  for (const Alarm& a : alarms)
    Serial.printf("  #%u %s %02u:%02u days=%u steps=%u%s\n", a.id, a.on ? "ON " : "off", a.hh, a.mm, a.days, (unsigned)a.steps.size(), a.runE ? "  RUNNING" : "");
}

// ============================================================
//  SERIAL MONITOR COMMANDS (115200 baud, "Newline")
// ============================================================
void printStatus() {
  Serial.printf("IR-LAMP %s | own Wi-Fi %s (%s) | home Wi-Fi %s %s %s | error.blue %s | code %s\n",
                FW_VERSION, AP_SSID, apIpStr.c_str(),
                staSsid.length() ? staSsid.c_str() : "(not set)",
                WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "",
                staMsg.c_str(),
                !cloudOn ? "off" : wsUp ? "online" : "offline", linkCode.c_str());
}

void runCommand(String line) {
  if (line.equalsIgnoreCase("ALARMS")) { printAlarms(); return; }
  line.trim();
  String up = line; up.toUpperCase();
  if (up == "HELP") {
    Serial.println("STATUS | CODE | NEWCODE | LINK ON | LINK OFF | WIFI \"network name\" password | WIFI OFF");
  } else if (up == "STATUS") {
    printStatus();
  } else if (up == "CODE") {
    Serial.printf("Link code: %s  ->  https://error.blue/lamp/?code=%s\n", linkCode.c_str(), linkCode.c_str());
  } else if (up == "NEWCODE") {
    linkCode = newLinkCode(); prefs.putString("code", linkCode); wsDrop("new code"); wsNextTry = 0;
    Serial.printf("New link code: %s\n", linkCode.c_str());
  } else if (up == "LINK ON" || up == "LINK OFF") {
    cloudOn = up == "LINK ON"; prefs.putBool("cloud", cloudOn);
    if (!cloudOn) wsDrop("switched off");
    wsNextTry = 0;
    printStatus();
  } else if (up == "WIFI OFF") {
    setHomeWifi("", "");
    Serial.println("Home Wi-Fi forgotten");
  } else if (up.startsWith("WIFI ")) {
    String rest = line.substring(5); rest.trim();
    String ssid, pass;
    if (rest.startsWith("\"")) {
      int q = rest.indexOf('"', 1);
      if (q < 0) { Serial.println("Missing closing \""); return; }
      ssid = rest.substring(1, q);
      pass = rest.substring(q + 1);
    } else {
      int sp = rest.indexOf(' ');
      ssid = sp < 0 ? rest : rest.substring(0, sp);
      pass = sp < 0 ? String("") : rest.substring(sp + 1);
    }
    pass.trim();
    setHomeWifi(ssid, pass);
    Serial.printf("Home Wi-Fi set to \"%s\", connecting...\n", ssid.c_str());
  } else if (line.length()) {
    Serial.println("Unknown command. Type HELP");
  }
}

void serialStep() {
  static String line;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') { if (line.length()) runCommand(line); line = ""; }
    else if (line.length() < 160) line += c;
  }
}

// ============================================================
//  SETUP / LOOP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== ESP32 IR Remote " FW_VERSION " ===");

  oledBegin();

  if (!LittleFS.begin(true)) Serial.println("LittleFS failed - signals won't be saved");
  loadAll();
  Serial.printf("Loaded %u saved signals\n", (unsigned)saved.size());

  prefs.begin("irlamp", false);
  staSsid = prefs.getString("ssid", HOME_SSID);
  staPass = prefs.getString("pass", HOME_PASS);
  cloudOn = prefs.getBool("cloud", true);
  linkCode = prefs.getString("code", "");
  tzStr = prefs.getString("tz", "");
  if (tzStr.length()) { setenv("TZ", tzStr.c_str(), 1); tzset(); }
  loadAlarms();
  if (linkCode.length() < 3) { linkCode = newLinkCode(); prefs.putString("code", linkCode); }

  irTxBegin();
  pinMode(IR_RX_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(IR_RX_PIN), irISR, CHANGE);

  wifiBegin();
  Serial.printf("Link code: %s  ->  https://error.blue/lamp/?code=%s\n", linkCode.c_str(), linkCode.c_str());

  server.on("/",             HTTP_GET,  handleRoot);
  server.on("/api/last",     HTTP_GET,  handleLast);
  server.on("/api/list",     HTTP_GET,  sendList);
  server.on("/api/save",     HTTP_POST, handleSave);
  server.on("/api/send",     HTTP_POST, handleSend);
  server.on("/api/sendlast", HTTP_POST, handleSendLast);
  server.on("/api/rename",   HTTP_POST, handleRename);
  server.on("/api/delete",   HTTP_POST, handleDelete);
  server.on("/api/restore",  HTTP_POST, handleRestore);
  server.on("/api/status",   HTTP_GET,  handleStatus);
  server.on("/api/scan",     HTTP_GET,  handleScan);
  server.on("/api/wifi",     HTTP_POST, handleWifi);
  server.on("/api/cloud",    HTTP_POST, handleCloud);
  server.on("/export.txt",   HTTP_GET,  handleExport);
  server.on("/favicon.ico",  HTTP_GET,  []() { server.send(204); });
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();

  oledShow("IR Remote", apIpStr, AP_SSID, "Listening...");
}

void loop() {
  server.handleClient();
  pollReceiver();
  staStep();
  cloudStep();
  alarmStep();
  queueStep();
  serialStep();
  delay(2);
}
