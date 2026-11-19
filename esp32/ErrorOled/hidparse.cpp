// HID report map parser + keyboard/mouse report decoder. See hidparse.h.
#include "hidparse.h"
#include <string.h>
#include "keymaps.h"

const char* const KEYMAP_NAMES[3] = {"US", "DE", "FR"};

// ---------------------------------------------------------------- report map
static void addField(HidMap& m, uint8_t rid, uint8_t kind, uint16_t off, uint8_t size, uint8_t count, uint16_t umin, int32_t lmin) {
  if (m.n >= sizeof(m.f) / sizeof(m.f[0]) || size == 0 || size > 32) return;
  HidField& f = m.f[m.n++];
  f.reportId = rid; f.kind = kind; f.bitOff = off; f.bitSize = size; f.count = count; f.usageMin = umin; f.logicalMin = lmin;
  if (kind <= HF_KB_BITMAP && kind != HF_KB_MODS) m.keyboard = true;
  if (kind == HF_M_X || kind == HF_M_Y) m.mouse = true;
}

bool hidParseMap(const uint8_t* d, size_t len, HidMap& out) {
  out = HidMap();
  struct Glob { uint16_t page; int32_t lmin, lmax; uint8_t rsize, rcount, rid; };
  Glob g = {0, 0, 0, 0, 0, 0}, stack[4];
  int sp = 0;
  uint32_t usages[16];
  uint8_t nus = 0;
  uint32_t umin = 0, umax = 0;
  bool range = false;
  uint16_t off[256];
  memset(off, 0, sizeof(off));

  size_t i = 0;
  while (i < len) {
    uint8_t b = d[i++];
    if (b == 0xFE) {                       // long item: skip it
      if (i + 1 >= len) break;
      i += 2 + d[i];
      continue;
    }
    uint8_t sz = b & 3;
    if (sz == 3) sz = 4;
    uint8_t type = (b >> 2) & 3, tag = b >> 4;
    if (i + sz > len) break;
    uint32_t u = 0;
    for (uint8_t k = 0; k < sz; k++) u |= (uint32_t)d[i + k] << (8 * k);
    int32_t sv = (int32_t)u;
    if (sz == 1) sv = (int8_t)u;
    else if (sz == 2) sv = (int16_t)u;
    i += sz;

    if (type == 0) {                                   // ---- main item
      if (tag == 0x8) {                                // Input
        uint16_t& o = off[g.rid];
        bool constant = u & 1, variable = u & 2;
        uint32_t first = range ? umin : (nus ? usages[0] : 0);
        uint16_t page = (first >> 16) ? (first >> 16) : g.page;
        uint16_t u0 = first & 0xFFFF;
        if (!constant) {
          if (variable) {
            if (page == 0x07 && g.rsize == 1) {
              if (u0 >= 0xE0 && u0 <= 0xE7) addField(out, g.rid, HF_KB_MODS, o, 1, g.rcount, u0, 0);
              else addField(out, g.rid, HF_KB_BITMAP, o, 1, g.rcount, u0, 0);
            } else if (page == 0x09 && g.rsize == 1) {
              addField(out, g.rid, HF_M_BUTTONS, o, 1, g.rcount > 8 ? 8 : g.rcount, u0, 0);
            } else {
              for (uint8_t e = 0; e < g.rcount; e++) {
                uint32_t us = range ? umin + e : (nus ? usages[e < nus ? e : nus - 1] : 0);
                uint16_t pg = (us >> 16) ? (us >> 16) : g.page, id = us & 0xFFFF;
                int kind = -1;
                if (pg == 0x01 && id == 0x30) kind = HF_M_X;
                else if (pg == 0x01 && id == 0x31) kind = HF_M_Y;
                else if (pg == 0x01 && id == 0x38) kind = HF_M_WHEEL;
                else if (pg == 0x0C && id == 0x238) kind = HF_M_PAN;
                if (kind >= 0) addField(out, g.rid, kind, o + e * g.rsize, g.rsize, 1, id, g.lmin);
              }
            }
          } else if (page == 0x07) {                   // array of key usages
            addField(out, g.rid, HF_KB_ARRAY, o, g.rsize, g.rcount, range ? u0 : 0, g.lmin);
          }
        }
        o += (uint16_t)g.rsize * g.rcount;
      } else if (tag == 0x9 || tag == 0xB) {
        // Output / Feature: not in input reports
      }
      nus = 0; range = false; umin = umax = 0;        // locals end with every main item
    } else if (type == 1) {                            // ---- global item
      switch (tag) {
        case 0x0: g.page = u; break;
        case 0x1: g.lmin = sv; break;
        case 0x2: g.lmax = g.lmin < 0 ? sv : (int32_t)u; break;
        case 0x7: g.rsize = u; break;
        case 0x8: g.rid = u; out.hasIds = true; break;
        case 0x9: g.rcount = u; break;
        case 0xA: if (sp < 4) stack[sp++] = g; break;
        case 0xB: if (sp > 0) g = stack[--sp]; break;
      }
    } else if (type == 2) {                            // ---- local item
      if (tag == 0x0) { if (nus < 16) usages[nus++] = u; }
      else if (tag == 0x1) { umin = u; range = true; }
      else if (tag == 0x2) { umax = u; range = true; }
    }
  }
  (void)umax;
  return out.keyboard || out.mouse;
}

void hidBootKeyboardMap(HidMap& m, uint8_t rid) {
  m = HidMap();
  m.hasIds = rid != 0;
  addField(m, rid, HF_KB_MODS, 0, 1, 8, 0xE0, 0);
  addField(m, rid, HF_KB_ARRAY, 16, 8, 6, 0, 0);
}

void hidBootMouseMap(HidMap& m, uint8_t rid) {
  m = HidMap();
  m.hasIds = rid != 0;
  addField(m, rid, HF_M_BUTTONS, 0, 1, 5, 1, 0);
  addField(m, rid, HF_M_X, 8, 8, 1, 0x30, -127);
  addField(m, rid, HF_M_Y, 16, 8, 1, 0x31, -127);
  addField(m, rid, HF_M_WHEEL, 24, 8, 1, 0x38, -127);
}

// ---------------------------------------------------------------- reports
static uint32_t bits(const uint8_t* d, size_t len, uint32_t off, uint8_t size) {
  uint32_t v = 0;
  for (uint8_t b = 0; b < size; b++) {
    uint32_t p = off + b;
    if ((p >> 3) >= len) break;
    if ((d[p >> 3] >> (p & 7)) & 1) v |= 1u << b;
  }
  return v;
}

static int32_t sbits(const uint8_t* d, size_t len, uint32_t off, uint8_t size, bool isSigned) {
  uint32_t v = bits(d, len, off, size);
  if (isSigned && size < 32 && (v & (1u << (size - 1)))) v |= ~0u << size;
  return (int32_t)v;
}

static void addKey(HidInput& o, uint32_t usage) {
  if (usage >= 0xE0 && usage <= 0xE7) { o.mods |= 1 << (usage - 0xE0); return; }
  if (usage < 4 || usage > 0xFF) return;          // 0 = none, 1..3 = errors
  if (o.nkeys < sizeof(o.keys)) o.keys[o.nkeys++] = usage;
}

void hidDecode(const HidMap& m, uint8_t rid, const uint8_t* d, size_t len, HidInput& o) {
  o = HidInput();
  for (uint8_t i = 0; i < m.n; i++) {
    const HidField& f = m.f[i];
    if (f.reportId != rid) continue;
    switch (f.kind) {
      case HF_KB_MODS:
        o.kb = true;
        for (uint8_t e = 0; e < f.count && e < 8; e++)
          if (bits(d, len, f.bitOff + e, 1)) o.mods |= 1 << ((f.usageMin + e - 0xE0) & 7);
        break;
      case HF_KB_ARRAY:
        o.kb = true;
        for (uint8_t e = 0; e < f.count; e++) {
          int32_t v = (int32_t)bits(d, len, f.bitOff + e * f.bitSize, f.bitSize);
          if (v == 0 && f.logicalMin == 0 && f.usageMin == 0) continue;
          uint32_t usage = f.usageMin + (v - f.logicalMin);
          if (usage == 1) { o.rollover = true; return; }   // too many keys at once: ignore this report
          addKey(o, usage);
        }
        break;
      case HF_KB_BITMAP:
        o.kb = true;
        for (uint8_t e = 0; e < f.count; e++)
          if (bits(d, len, f.bitOff + e, 1)) addKey(o, f.usageMin + e);
        break;
      case HF_M_BUTTONS:
        o.mouse = true;
        o.buttons = bits(d, len, f.bitOff, f.count);
        break;
      case HF_M_X: o.mouse = true; o.dx = sbits(d, len, f.bitOff, f.bitSize, f.logicalMin < 0); break;
      case HF_M_Y: o.mouse = true; o.dy = sbits(d, len, f.bitOff, f.bitSize, f.logicalMin < 0); break;
      case HF_M_WHEEL: o.mouse = true; o.wheel = sbits(d, len, f.bitOff, f.bitSize, f.logicalMin < 0); break;
      case HF_M_PAN: o.mouse = true; o.pan = sbits(d, len, f.bitOff, f.bitSize, f.logicalMin < 0); break;
    }
  }
}

// ---------------------------------------------------------------- keys
static const uint8_t HM_CTRL = 0x11, HM_SHIFT = 0x22, HM_ALT = 0x04, HM_ALTGR = 0x40, HM_GUI = 0x88;

static uint32_t upperCyr(uint32_t c) {
  if (c >= 0x430 && c <= 0x44F) return c - 0x20;
  if (c >= 0x450 && c <= 0x45F) return c - 0x50;
  if (c == 0x491) return 0x490;
  return c;
}

KeyEv KeyTracker::translate(uint8_t usage, uint8_t hm) const {
  KeyEv k = {K_NONE, 0, 0};
  if (hm & HM_CTRL) k.mods |= MOD_CTRL;
  if (hm & HM_SHIFT) k.mods |= MOD_SHIFT;
  if (hm & (HM_ALT | HM_ALTGR)) k.mods |= MOD_ALT;
  if (hm & HM_GUI) k.mods |= MOD_GUI;
  switch (usage) {
    case 0x28: case 0x58: k.code = K_ENTER; return k;
    case 0x29: k.code = K_ESC; return k;
    case 0x2A: k.code = K_BKSP; return k;
    case 0x2B: k.code = K_TAB; return k;
    case 0x4C: k.code = K_DEL; return k;
    case 0x4A: k.code = K_HOME; return k;
    case 0x4D: k.code = K_END; return k;
    case 0x4B: k.code = K_PGUP; return k;
    case 0x4E: k.code = K_PGDN; return k;
    case 0x4F: k.code = K_RIGHT; return k;
    case 0x50: k.code = K_LEFT; return k;
    case 0x51: k.code = K_DOWN; return k;
    case 0x52: k.code = K_UP; return k;
    case 0x65: k.code = K_MENU; return k;
  }
  if (usage >= 0x3A && usage <= 0x45) { k.code = K_F1 + (usage - 0x3A); return k; }
  if (usage == 0x2C && (hm & HM_GUI)) { k.code = K_LANG; return k; }
  if (usage >= 0x54 && usage <= 0x63) {           // number pad (as if Num Lock is on)
    static const char kp[] = "/*-+\0" "1234567890.";
    char c = kp[usage - 0x54];
    if (!c) return k;
    k.code = K_CHAR; k.ch = (uint8_t)c; return k;
  }
  int idx = -1;
  if (usage >= 0x04 && usage <= 0x38) idx = usage - 0x04;
  else if (usage == 0x64) idx = KEYMAP_KEYS - 1;
  if (idx < 0) return k;
  uint8_t lay = ukrainian ? 3 : layout;
  if (hm & HM_ALTGR) {
    for (const AltGrKey& a : ALTGR)
      if (a.layout == lay && a.usage == usage) {
        k.code = K_CHAR; k.ch = (hm & HM_SHIFT) ? upperCyr(a.ch) : a.ch; k.mods &= ~MOD_ALT; return k;
      }
  }
  static const uint16_t* const LO[4] = {KM_US, KM_DE, KM_FR, KM_UA};
  static const uint16_t* const HI[4] = {KM_US_SHIFT, KM_DE_SHIFT, KM_FR_SHIFT, KM_UA_SHIFT};
  uint32_t lo = LO[lay][idx], hi = HI[lay][idx];
  if (!lo) return k;
  bool letter = hi != lo && (upperCyr(lo) == hi || (lo >= 'a' && lo <= 'z') || (lo >= 0xE0 && lo <= 0xFE));
  bool shift = (hm & HM_SHIFT) != 0;
  if (caps && letter) shift = !shift;
  k.code = K_CHAR;
  k.ch = shift ? hi : lo;
  if (hm & HM_ALTGR) k.mods &= ~MOD_ALT;
  return k;
}

void KeyTracker::report(const HidInput& in, uint32_t now, void (*emit)(const KeyEv&)) {
  if (in.rollover) return;
  // Alt+Shift pressed together (and nothing else): switch EN <-> UA
  bool combo = (in.mods & HM_ALT) && (in.mods & HM_SHIFT) && in.nkeys == 0;
  bool had = (mods & HM_ALT) && (mods & HM_SHIFT);
  if (combo && !had) { KeyEv k = {K_LANG, 0, 0}; emit(k); }
  bool still = false;
  for (uint8_t i = 0; i < in.nkeys; i++) {
    uint8_t u = in.keys[i];
    if (u == repeatUsage) still = true;
    bool was = false;
    for (uint8_t j = 0; j < nprev; j++) if (prev[j] == u) { was = true; break; }
    if (was) continue;
    if (u == 0x39) { caps = !caps; continue; }     // Caps Lock
    KeyEv k = translate(u, in.mods);
    if (k.code == K_NONE) continue;
    emit(k);
    bool repeats = k.code == K_CHAR || k.code == K_BKSP || k.code == K_DEL || (k.code >= K_UP && k.code <= K_PGDN);
    repeatUsage = repeats ? u : 0;
    repeatAt = now + 450;
    still = repeats;
  }
  if (!still) repeatUsage = 0;
  memcpy(prev, in.keys, in.nkeys);
  nprev = in.nkeys;
  mods = in.mods;
}

void KeyTracker::tick(uint32_t now, void (*emit)(const KeyEv&)) {
  if (!repeatUsage || (int32_t)(now - repeatAt) < 0) return;
  KeyEv k = translate(repeatUsage, mods);
  if (k.code != K_NONE) emit(k);
  repeatAt = now + 45;
}
