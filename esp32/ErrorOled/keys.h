// Keys and mouse moves, as the DOS screens see them.
// They come from a Bluetooth keyboard / mouse (hid.cpp), the USB serial console or the BOOT button.
#pragma once
#include <stdint.h>

enum KeyCode : uint16_t {
  K_NONE = 0,
  K_CHAR,          // a typed character: KeyEv.ch (Unicode)
  K_ENTER, K_ESC, K_BKSP, K_TAB, K_DEL,
  K_UP, K_DOWN, K_LEFT, K_RIGHT, K_PGUP, K_PGDN, K_HOME, K_END,
  K_F1, K_F2, K_F3, K_F4, K_F5, K_F6, K_F7, K_F8, K_F9, K_F10, K_F11, K_F12,
  K_MENU,          // the Windows / menu key
  K_LANG,          // Alt+Shift or Win+Space: switch EN <-> UA letters
};

enum : uint8_t { MOD_CTRL = 1, MOD_SHIFT = 2, MOD_ALT = 4, MOD_GUI = 8 };

struct KeyEv {
  uint16_t code;   // KeyCode
  uint32_t ch;     // for K_CHAR
  uint8_t mods;    // MOD_*
};

// the BOOT button
enum BtnEv : uint8_t { BTN_SHORT = 1, BTN_DOUBLE = 2, BTN_LONG = 3 };

// Bluetooth mouse buttons
enum : uint8_t { MB_LEFT = 1, MB_RIGHT = 2, MB_MIDDLE = 4 };
