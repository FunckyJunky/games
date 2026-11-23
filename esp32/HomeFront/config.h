// HOME FRONT firmware settings.
// Board: Waveshare / diymore ESP32-S3-LCD-1.47 (or 1.47B), 172x320 ST7789, used sideways (320x172).
#pragma once

#define HF_VERSION      "1.10"

// error.blue: the device's web app lives at https://error.blue/thuisfront/dev, its connection is this WebSocket
#define HF_HOST         "error.blue"
#define HF_PORT         443
#define HF_WS_PATH      "/thuisfront/dev/ws"

// the setup WiFi the board makes when it has no WiFi: HOMEFRONT-XXXX (open), page at http://192.168.4.1
#define HF_AP_PREFIX    "HOMEFRONT-"

// screen (the same on the 1.47 and the 1.47B, only the backlight differs)
#define PIN_LCD_MOSI    45
#define PIN_LCD_SCLK    40
#define PIN_LCD_CS      42
#define PIN_LCD_DC      41
#define PIN_LCD_RST     39
// backlight: -1 = find out: the 1.47B has a QMI8658 motion sensor on SDA 48 / SCL 47 and its backlight on 46;
// the plain 1.47 has its backlight on 48
#define PIN_LCD_BL      -1
#define LCD_SPI_HZ      80000000
#define LCD_ROW_OFFSET  34          // the 172 visible lines sit at 34..205 of the chip's 240

#define PIN_BUTTON      0           // BOOT
#define PIN_RGB         38          // the RGB LED

// how long to try one WiFi network (ms), at full power and then at low power
#define WIFI_TRY_MS     12000
