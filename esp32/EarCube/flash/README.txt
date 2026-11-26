EAR-CUBE for the Waveshare ESP32-S3-Touch-AMOLED-2.16 (the ERROR-CUBE)

INSTALL
  Double-click flash.bat with the cube plugged in by USB (or: flash.bat COM7).
  Nothing is erased: your WiFi, settings and files stay.

FILES (offsets)
  bootloader.bin  0x0
  partitions.bin  0x8000   (16 MB: two 3 MB app slots + the 9 MB C: drive)
  boot_app0.bin   0xe000
  ear.bin         0x10000  (EAR-CUBE itself)

BACK TO ERROR-CUBE
  On the cube: SETUP > BACK TO ERROR-CUBE (over WiFi). Or error.blue/cube/flash.

More: https://error.blue/ear
