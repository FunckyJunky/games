# HOME FRONT firmware

For the ESP32-S3 board with the 1.47" 172x320 screen (Waveshare / diymore ESP32-S3-LCD-1.47 or 1.47B, the same
board as pocket.DOS). The board shows one web app from error.blue; you control it from your phone at
error.blue/thuisfront. More in the main README ("Home Front").

## What it does

1. Starts with a short BIOS screen.
2. Joins the strongest saved WiFi (up to 8). Each one at full power, then at low power (8.5 dBm); it remembers
   which one worked.
3. No WiFi saved, or none found: it makes the open WiFi **HOMEFRONT-XXXX**. Join it with your phone; the setup page
   opens by itself (or go to http://192.168.4.1). Pick your WiFi, type the password, JOIN. It tries the saved
   ones again every minute.
4. Connected: **CONNECTED** for 3 seconds, then it loads its web app from error.blue/thuisfront/dev
   (WebSocket `wss://error.blue/thuisfront/dev/ws?id=HF-XXXXXX`).
5. It saves the web app's state and text picture to flash, so after a restart it shows them again straight away,
   also without WiFi (a red dot in the corner = not connected to error.blue).

The setup page also works on your home WiFi at the board's IP or http://homefront-xxxx.local/ (press the button to
see the IP): WiFi, saved networks, the screen's name, HOME CODE, turn the picture round, restart.

BOOT button: press = info (name, id, WiFi, IP, version), hold 3 s = open the setup WiFi.
LED: blue breathing = setup WiFi is on, green blink = new text.

## Putting it on the board

- **Easiest:** Chrome or Edge on a computer, open **error.blue/thuisfront/flash**, press INSTALL, pick the port.
- **Arduino IDE:** esp32 by Espressif 3.x, library ArduinoJson 7. Tools: Board **ESP32S3 Dev Module**,
  USB CDC On Boot **Enabled**, Partition Scheme **Huge APP (3MB No OTA/1MB SPIFFS)**, PSRAM **OPI PSRAM**.
  Open HomeFront.ino, Upload.
- New build for the web flasher: compile with those settings, then copy `HomeFront.ino.bin` to
  public/thuisfront/flash/homefront.bin (and bootloader/partitions if they changed) and bump the version in
  manifest.json and config.h.

## Files

```
HomeFront.ino   start-up, which screen to show, button, LED, backlight
config.h        pins, version, error.blue address
lcd.*           ST7789 over SPI with DMA (esp_lcd), sideways 320x172; finds the backlight pin (46 on the 1.47B, 48 on the 1.47)
screen.*        all drawing: the same as public/thuisfront/dev/render.js (keep them in step)
network.cpp     saved WiFi, setup WiFi + page + captive DNS, the network task (core 0)
link.cpp        the WebSocket to error.blue, state, text pictures, video frames, saving to flash
hf.h            what the network task and the screen share
certs.h         root certificates for error.blue (Let's Encrypt, Google Trust Services)
font8x8.h       8x8 font (public domain)
```
