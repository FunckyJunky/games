DREAM-CUBE for the Waveshare ESP32-S3-Touch-AMOLED-2.16 (the ERROR-CUBE)
It listens, and turns what's said into a little animated 3D world on the screen.

INSTALL
  Double-click flash.bat with the cube plugged in by USB (or: flash.bat COM7).
  Nothing is erased: your WiFi, settings and files stay.

FIRST START
  No WiFi yet? Join the cube's own WiFi (DREAM-CUBE-XXXX, the password is on the cube:
  SETUP > THE CUBE'S OWN WIFI, or scan the QR code there), open http://192.168.4.1
  and pick your home WiFi + password.
  Control page: https://error.blue/dream (the cube's code: SETUP > CONTROL PAGE).

FILES (offsets)
  bootloader.bin  0x0
  partitions.bin  0x8000   (16 MB: two 3 MB app slots + the 9 MB C: drive)
  boot_app0.bin   0xe000
  dream.bin       0x10000  (DREAM-CUBE itself)

BACK TO ERROR-CUBE
  On the cube: SETUP > BACK TO ERROR-CUBE (over WiFi). Or error.blue/cube/flash.

More: https://error.blue/dream
