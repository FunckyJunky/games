// Reads what a Bluetooth keyboard or mouse sends.
// Every HID device describes its reports in a "report map"; hidParseMap() finds the
// keyboard keys, mouse buttons, X/Y and wheel in it, hidDecode() pulls them out of
// each report, and KeyTracker turns pressed keys into typed characters (US/DE/FR + UA).
// No Arduino code here, so it can be tested on a PC.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "keys.h"

enum HidKind : uint8_t {
  HF_KB_MODS,      // 8 modifier bits (Ctrl, Shift, Alt, GUI; left then right)
  HF_KB_ARRAY,     // pressed keys as a list of usages (the usual 6-key report)
  HF_KB_BITMAP,    // pressed keys as one bit per key ("N-key rollover")
  HF_M_BUTTONS,
  HF_M_X, HF_M_Y, HF_M_WHEEL, HF_M_PAN,
};

struct HidField {
  uint8_t reportId;
  uint8_t kind;          // HidKind
  uint16_t bitOff;       // where it starts in the report (after the report id byte)
  uint8_t bitSize;       // bits per value
  uint8_t count;         // how many values
  uint16_t usageMin;     // for arrays and bitmaps
  int32_t logicalMin;    // < 0: values are signed
};

struct HidMap {
  HidField f[24];
  uint8_t n = 0;
  bool hasIds = false;
  bool keyboard = false, mouse = false;
};

// false if no keyboard or mouse was found in the map
bool hidParseMap(const uint8_t* d, size_t len, HidMap& out);
// the classic "boot" layouts, for devices whose map can't be read
void hidBootKeyboardMap(HidMap& out, uint8_t reportId);
void hidBootMouseMap(HidMap& out, uint8_t reportId);

struct HidInput {
  bool kb = false, mouse = false;
  bool rollover = false;      // the keyboard said "too many keys": skip
  uint8_t mods = 0;          // HID modifier bits
  uint8_t keys[16];          // pressed key usages
  uint8_t nkeys = 0;
  uint8_t buttons = 0;
  int16_t dx = 0, dy = 0, wheel = 0, pan = 0;
};
// reportId: from the characteristic's Report Reference (0 when the map has none)
void hidDecode(const HidMap& m, uint8_t reportId, const uint8_t* data, size_t len, HidInput& out);

enum Keymap : uint8_t { KM_LAYOUT_US = 0, KM_LAYOUT_DE = 1, KM_LAYOUT_FR = 2 };
extern const char* const KEYMAP_NAMES[3];

// Turns keyboard reports into key presses, with auto-repeat while a key is held.
class KeyTracker {
 public:
  uint8_t layout = KM_LAYOUT_US;
  bool ukrainian = false;    // Cyrillic letters on (K_LANG / Alt+Shift switches)
  bool caps = false;
  // feed one keyboard report; calls emit() for every new key press
  void report(const HidInput& in, uint32_t nowMs, void (*emit)(const KeyEv&));
  // call often: repeats the held key
  void tick(uint32_t nowMs, void (*emit)(const KeyEv&));
  void releaseAll() { nprev = 0; repeatUsage = 0; mods = 0; }
 private:
  KeyEv translate(uint8_t usage, uint8_t hidMods) const;
  uint8_t prev[16];
  uint8_t nprev = 0;
  uint8_t mods = 0;
  uint8_t repeatUsage = 0;
  uint32_t repeatAt = 0;
};
