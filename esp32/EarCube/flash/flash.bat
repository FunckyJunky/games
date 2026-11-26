@echo off
setlocal EnableDelayedExpansion
title EAR-CUBE flasher
rem ===========================================================================
rem  flash.bat: puts EAR-CUBE on the Waveshare ESP32-S3-Touch-AMOLED-2.16 over USB.
rem  Keeps the cube's WiFi, settings and files (C:\ and the TF card): nothing is erased.
rem    flash.bat          finds the cube's COM port by itself
rem    flash.bat COM7     uses COM7
rem  Needs esptool: the Arduino IDE's (esp32 boards installed) is found by itself,
rem  or put esptool.exe next to this file, or "pip install esptool".
rem ===========================================================================
set APP=ear.bin
set NAME=EAR-CUBE
cd /d "%~dp0"
for %%f in (bootloader.bin partitions.bin boot_app0.bin %APP%) do (
  if not exist "%%f" ( echo %%f is missing: keep all the files from the zip together. & goto fail )
)

rem ---- find esptool
set ESPTOOL=
if exist "%~dp0esptool.exe" set "ESPTOOL=%~dp0esptool.exe"
if not defined ESPTOOL (
  for /d %%d in ("%LOCALAPPDATA%\Arduino15\packages\esp32\tools\esptool_py\*") do (
    if exist "%%d\esptool.exe" set "ESPTOOL=%%d\esptool.exe"
  )
)
if not defined ESPTOOL (
  for /d %%d in ("%USERPROFILE%\Documents\Arduino\hardware\espressif\esp32\tools\esptool*") do (
    if exist "%%d\esptool.exe" set "ESPTOOL=%%d\esptool.exe"
  )
)
if not defined ESPTOOL (
  py -m esptool version >nul 2>&1 && set "ESPTOOL=py -m esptool"
)
if not defined ESPTOOL (
  python -m esptool version >nul 2>&1 && set "ESPTOOL=python -m esptool"
)
if not defined ESPTOOL (
  echo Can't find esptool.
  echo  - install the esp32 boards in the Arduino IDE, or
  echo  - run:  pip install esptool    or
  echo  - put esptool.exe next to this file.
  goto fail
)
if exist "%ESPTOOL%" ( set "RUN="%ESPTOOL%"" ) else ( set "RUN=%ESPTOOL%" )

set PORT=
if not "%~1"=="" set "PORT=--port %~1"

echo.
echo  %NAME% ^> the cube
echo  esptool: %ESPTOOL%
if defined PORT ( echo  port:    %~1 ) else ( echo  port:    found by itself )
echo.
echo  1. Plug the cube in with a USB-C cable that carries data.
echo  2. Close the Arduino Serial Monitor (it keeps the port busy).
echo.
pause

%RUN% --chip esp32s3 %PORT% --baud 921600 write_flash 0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 %APP%
if errorlevel 1 (
  echo.
  echo  It didn't work. Try this: hold BOOT ^(the right button, "BOOT/-"^), press and let go of PWR,
  echo  let go of BOOT, then run flash.bat again. Or give the port: flash.bat COM7
  goto fail
)
echo.
echo  DONE. The cube restarts in %NAME%.
echo  No internet yet? Join its WiFi (EAR-CUBE-XXXX, password in SETUP ^> HOTSPOT on the cube),
echo  open http://192.168.4.1 and use the WIFI tab.
echo.
pause
exit /b 0
:fail
echo.
pause
exit /b 1
