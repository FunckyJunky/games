// config.h: version, pins and the error.blue address for ERROR-CUBE
// Board: Waveshare ESP32-S3-Touch-AMOLED-2.16 (480x480 AMOLED CO5300, touch CST9220, AXP2101, PCF85063,
// QMI8658, ES8311 + ES7210, TF card). Pins from Waveshare's wiki and pin_config.h.
#pragma once

#define CUBE_VERSION "1.3"
#define CUBE_HOST "error.blue"
#define CUBE_PORT 443
#define FW_INFO_PATH "/cube/flash/firmware.json"     // newest firmware: {ver, path, size, md5, notes}
#define LINK_GAME "error-cube"                      // live link: /api/room/error-cube/<CODE>

// display (QSPI)
#define PIN_LCD_CS 12
#define PIN_LCD_SCLK 38
#define PIN_LCD_D0 4
#define PIN_LCD_D1 5
#define PIN_LCD_D2 6
#define PIN_LCD_D3 7
#define PIN_LCD_RST 39
// shared I2C: touch 0x5A, AXP2101 0x34, QMI8658 0x6B, PCF85063 0x51, ES8311 0x18, ES7210 0x40
#define PIN_SDA 15
#define PIN_SCL 14
#define PIN_TP_INT 11
#define PIN_TP_RST 40
// buttons: + / KEY (KEY3, low = pressed), BOOT / - (GPIO0), PWR goes to the AXP2101
#define PIN_KEY_PLUS 18
#define PIN_KEY_MINUS 0
// sound
#define PIN_I2S_MCLK 42
#define PIN_I2S_BCLK 9
#define PIN_I2S_WS 45
#define PIN_I2S_DOUT 8
#define PIN_I2S_DIN 10
#define PIN_PA 46
// TF card (SPI)
#define PIN_SD_MOSI 1
#define PIN_SD_SCK 2
#define PIN_SD_MISO 3
#define PIN_SD_CS 41
// interrupts
#define PIN_IMU_INT1 17
#define PIN_RTC_INT 13
