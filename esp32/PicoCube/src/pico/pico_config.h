// pico_config.h: PICO-CUBE's name, version and update address (core/config.h includes this when it's there)
#pragma once
#define PICO_CUBE 1
#define FW_NAME "PICO-CUBE"
#define CUBE_VERSION "1.0"
#define FW_INFO_PATH "/pico8/flash/firmware.json"     // newest PICO-CUBE: {ver, path, size, md5, notes}
#define CARTS_PATH "/pico8/carts.json"                // the carts on error.blue: {carts: [{file, name, author, ...}]}
#define THEME_COLOR 6                                 // SCHEMES[6]: "PICO-8" (white, pink, dark blue)
