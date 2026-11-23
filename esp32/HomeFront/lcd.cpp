// ST7789 over SPI with DMA (ESP-IDF esp_lcd). The 172x320 panel is used sideways: 320 wide, 172 high.
#include <Arduino.h>
#include <Wire.h>
#include "esp_lcd_panel_io.h"
#include "driver/spi_master.h"
#include "freertos/semphr.h"
#include "config.h"
#include "lcd.h"

#define BAND 20                                 // lines per DMA piece
static esp_lcd_panel_io_handle_t io = nullptr;
static uint16_t* bufs[2];
static SemaphoreHandle_t freeBufs;
static int blPin = 48;
static bool blOn = false;

static bool IRAM_ATTR doneCb(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*) {
  BaseType_t woke = pdFALSE;
  xSemaphoreGiveFromISR(freeBufs, &woke);
  return woke == pdTRUE;
}

static void cmd(uint8_t c, const uint8_t* d = nullptr, int n = 0) { esp_lcd_panel_io_tx_param(io, c, d, n); }
static void cmd1(uint8_t c, uint8_t a) { cmd(c, &a, 1); }

uint16_t lcdColor(int r, int g, int b) {
  r = constrain(r, 0, 255); g = constrain(g, 0, 255); b = constrain(b, 0, 255);
  // the start-up list sets RAMCTRL (B0h) to little endian (0xE8), so no byte swap here
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// which board? The 1.47B answers on I2C 0x6B/0x6A (its motion sensor) at SDA 48 / SCL 47.
static int findBacklight() {
  if (PIN_LCD_BL >= 0) return PIN_LCD_BL;
  bool b = false;
  Wire.begin(48, 47, 100000);
  for (uint8_t a : { 0x6B, 0x6A }) { Wire.beginTransmission(a); if (Wire.endTransmission() == 0) b = true; }
  Wire.end();
  pinMode(48, INPUT); pinMode(47, INPUT);
  Serial.printf("LCD: board %s, backlight on GPIO %d\n", b ? "1.47B (motion sensor found)" : "1.47", b ? 46 : 48);
  return b ? 46 : 48;
}
int lcdBacklightPin() { return blPin; }

void lcdBacklight(int percent) {
  percent = constrain(percent, 0, 100);
  int duty = percent ? map(percent, 1, 100, 12, 255) : 0;
  ledcWrite(blPin, duty);
  blOn = percent > 0;
}

void lcdFlip(bool flip) { cmd1(0x36, flip ? 0xA0 : 0x60); }   // MADCTL: sideways (MV) + mirror one way or the other

void lcdBegin(bool flip) {
  blPin = findBacklight();
  ledcAttach(blPin, 5000, 8);
  ledcWrite(blPin, 0);

  spi_bus_config_t bus = {};
  bus.mosi_io_num = PIN_LCD_MOSI; bus.miso_io_num = -1; bus.sclk_io_num = PIN_LCD_SCLK;
  bus.quadwp_io_num = -1; bus.quadhd_io_num = -1;
  bus.max_transfer_sz = LCD_W * BAND * 2 + 16;
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

  freeBufs = xSemaphoreCreateCounting(2, 2);
  esp_lcd_panel_io_spi_config_t cfg = {};
  cfg.cs_gpio_num = PIN_LCD_CS; cfg.dc_gpio_num = PIN_LCD_DC;
  cfg.spi_mode = 0; cfg.pclk_hz = LCD_SPI_HZ; cfg.trans_queue_depth = 4;
  cfg.on_color_trans_done = doneCb; cfg.user_ctx = nullptr;
  cfg.lcd_cmd_bits = 8; cfg.lcd_param_bits = 8;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &cfg, &io));
  for (int i = 0; i < 2; i++) bufs[i] = (uint16_t*)heap_caps_malloc(LCD_W * BAND * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

  pinMode(PIN_LCD_RST, OUTPUT);
  digitalWrite(PIN_LCD_RST, HIGH); delay(10);
  digitalWrite(PIN_LCD_RST, LOW); delay(30);
  digitalWrite(PIN_LCD_RST, HIGH); delay(120);

  // the panel maker's start-up list (Waveshare's ST7789 demo for this board)
  cmd(0x11); delay(120);
  lcdFlip(flip);
  cmd1(0x3A, 0x05);                                         // 16-bit colour
  { const uint8_t d[] = { 0x00, 0xE8 }; cmd(0xB0, d, 2); }
  { const uint8_t d[] = { 0x0C, 0x0C, 0x00, 0x33, 0x33 }; cmd(0xB2, d, 5); }
  cmd1(0xB7, 0x35); cmd1(0xBB, 0x35); cmd1(0xC0, 0x2C); cmd1(0xC2, 0x01); cmd1(0xC3, 0x13); cmd1(0xC4, 0x20); cmd1(0xC6, 0x0F);
  { const uint8_t d[] = { 0xA4, 0xA1 }; cmd(0xD0, d, 2); }
  cmd1(0xD6, 0xA1);
  { const uint8_t d[] = { 0xF0, 0x00, 0x04, 0x04, 0x04, 0x05, 0x29, 0x33, 0x3E, 0x38, 0x12, 0x12, 0x28, 0x30 }; cmd(0xE0, d, 14); }
  { const uint8_t d[] = { 0xF0, 0x07, 0x0A, 0x0D, 0x0B, 0x07, 0x28, 0x33, 0x3E, 0x36, 0x14, 0x14, 0x29, 0x32 }; cmd(0xE1, d, 14); }
  cmd(0x21);                                                // the IPS panel wants inverted colours
  cmd(0x11); delay(20);
  cmd(0x29);

  // start black
  memset(bufs[0], 0, LCD_W * BAND * 2);
  { const uint8_t ca[] = { 0, 0, (LCD_W - 1) >> 8, (LCD_W - 1) & 255 }; cmd(0x2A, ca, 4); }
  { const uint8_t ra[] = { 0, LCD_ROW_OFFSET, (LCD_ROW_OFFSET + LCD_H - 1) >> 8, (LCD_ROW_OFFSET + LCD_H - 1) & 255 }; cmd(0x2B, ra, 4); }
  for (int y = 0; y < LCD_H; y += BAND) {
    xSemaphoreTake(freeBufs, portMAX_DELAY);
    int n = min(BAND, LCD_H - y);
    esp_lcd_panel_io_tx_color(io, y ? 0x3C : 0x2C, bufs[0], LCD_W * n * 2);
  }
}

void lcdPush(const uint8_t* fb, const uint16_t* lut, const uint16_t* lutDim, const uint16_t* plane) {
  { const uint8_t ca[] = { 0, 0, (LCD_W - 1) >> 8, (LCD_W - 1) & 255 }; cmd(0x2A, ca, 4); }   // waits for the last frame
  { const uint8_t ra[] = { 0, LCD_ROW_OFFSET, (LCD_ROW_OFFSET + LCD_H - 1) >> 8, (LCD_ROW_OFFSET + LCD_H - 1) & 255 }; cmd(0x2B, ra, 4); }
  int b = 0;
  for (int y = 0; y < LCD_H; y += BAND, b ^= 1) {
    xSemaphoreTake(freeBufs, portMAX_DELAY);                // this buffer is free again
    int n = min(BAND, LCD_H - y);
    uint16_t* out = bufs[b];
    for (int r = 0; r < n; r++) {
      const uint8_t* src = fb + (y + r) * LCD_W;
      bool dimRow = lutDim && ((y + r) & 1);
      const uint16_t* L = dimRow ? lutDim : lut;
      uint16_t* o = out + r * LCD_W;
      if (!plane) {
        for (int x = 0; x < LCD_W; x += 4) { o[x] = L[src[x]]; o[x + 1] = L[src[x + 1]]; o[x + 2] = L[src[x + 2]]; o[x + 3] = L[src[x + 3]]; }
      } else {
        const uint16_t* P = plane + (y + r) * LCD_W;
        for (int x = 0; x < LCD_W; x++) {
          uint8_t i = src[x];
          o[x] = i != 255 ? L[i] : dimRow ? (uint16_t)((P[x] >> 1) & 0x7BEF) : P[x];
        }
      }
    }
    esp_lcd_panel_io_tx_color(io, y ? 0x3C : 0x2C, out, LCD_W * n * 2);
  }
}
