// ===================== ERROR-OLED settings =====================
// Change these to match your board. Everything else can be set on the box itself
// (SETUP.EXE, WIFI.EXE), from your phone (PHONE SETUP) or over the USB serial monitor.
#pragma once

// ---- the screen: uncomment ONE ----
#define SCREEN_72X40        // ESP32-C3 board with the built-in 0.42" OLED (72x40)
// #define SCREEN_128X64    // 0.96" SSD1306 OLED (128x64)
// #define SCREEN_SH1106    // 1.3" SH1106 OLED (128x64)

#define OLED_SDA      5     // built-in OLED: SDA 5, SCL 6
#define OLED_SCL      6
#define BUTTON_PIN    9     // the BOOT button (pressed = LOW)
#define LED_PIN       8     // the small blue LED; -1 if your board has none
#define LED_ON        LOW   // that LED lights up when the pin is LOW

// ---- WiFi (optional: you can also pick it on the box or from your phone) ----
#define WIFI_SSID     ""
#define WIFI_PASS     ""

// ---- error.blue ----
#define SERVER_HOST   "error.blue"
#define SERVER_NAME   "ERROR.BLUE"
#define CHAT_CHANNEL  ""    // ERROR-CHAT channel to start with, e.g. "HOONGY" (or type CODE xxxx)

// time zone for the clock (Paris / Amsterdam / Berlin; see "POSIX TZ" for others)
#define TIMEZONE      "CET-1CEST,M3.5.0,M10.5.0/3"

// name of the box on Bluetooth and of its own WiFi in PHONE SETUP
#define DEVICE_NAME   "ERROR-OLED"
