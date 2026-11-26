// common.cpp: EAR-CUBE's tables and little helpers (plain C++: the simulator uses them too)
#include "ear.h"
#include "../core/sys.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

EarCfg ecfg;

// what Whisper is told to hear ("" = it finds out by itself)
const Lang LANGS[] = {
  {"", "AUTO"}, {"nl", "NEDERLANDS"}, {"en", "ENGLISH"}, {"de", "DEUTSCH"}, {"fr", "FRAN\xC3\x87" "AIS"},
  {"uk", "\xD0\xA3\xD0\x9A\xD0\xA0\xD0\x90\xD0\x87\xD0\x9D\xD0\xA1\xD0\xAC\xD0\x9A\xD0\x90"},   // УКРАЇНСЬКА
  {"ru", "\xD0\xA0\xD0\xA3\xD0\xA1\xD0\xA1\xD0\x9A\xD0\x98\xD0\x99"},                           // РУССКИЙ
  {"es", "ESPA\xC3\x91OL"}, {"it", "ITALIANO"}, {"pl", "POLSKI"}, {"pt", "PORTUGU\xC3\x8AS"}, {"tr", "T\xC3\x9CRK\xC3\x87" "E"},
};
const int N_LANGS = sizeof(LANGS) / sizeof(LANGS[0]);

const char* gainLabel(int step) {
  static const char* G[15] = {"0 dB", "3 dB", "6 dB", "9 dB", "12 dB", "15 dB", "18 dB", "21 dB", "24 dB", "27 dB",
                              "30 dB", "33 dB", "34.5 dB", "36 dB", "37.5 dB"};
  return G[step < 0 ? 0 : step > 14 ? 14 : step];
}

void earApName(char* out, size_t n) {
  if (ecfg.apName[0]) { snprintf(out, n, "%s", ecfg.apName); return; }
  const char* h = strchr(S.host, '-');                 // "CUBE-1A2B" -> "EAR-CUBE-1A2B"
  snprintf(out, n, "EAR-CUBE-%s", h ? h + 1 : S.host);
}

// "2026-11-26_22-55.txt" -> "26 NOV 2026 22:55", "LIVE-2026-11-26.txt" -> "LIVE 26 NOV 2026",
// "LIVE-2026-11-26_22-55.txt" -> "LIVE 26 NOV 22:55", anything else: the name without .txt
void earNiceName(const char* file, char* out, size_t n) {
  static const char* MON[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  const char* f = strrchr(file, '/'); f = f ? f + 1 : file;
  bool live = !strncmp(f, "LIVE-", 5);
  const char* d = live ? f + 5 : f;
  int y, mo, da, h = -1, mi = -1, k = 0;
  if (sscanf(d, "%4d-%2d-%2d%n", &y, &mo, &da, &k) == 3 && mo >= 1 && mo <= 12) {
    if (d[k] == '_') sscanf(d + k + 1, "%2d-%2d", &h, &mi);
    if (h >= 0 && mi >= 0) {
      if (live) snprintf(out, n, "LIVE %d %s %02d:%02d", da, MON[mo - 1], h, mi);
      else snprintf(out, n, "%d %s %d %02d:%02d", da, MON[mo - 1], y, h, mi);
    } else snprintf(out, n, "%s%d %s %d", live ? "LIVE " : "", da, MON[mo - 1], y);
    // a second one in the same minute: "..._22-55-2.txt"
    const char* dash = strrchr(d, '-'); const char* dot = strrchr(d, '.');
    if (h >= 0 && dash && dot && dot - dash == 2 && dash > d + k + 3) { size_t l = strlen(out); snprintf(out + l, n - l, " (%c)", dash[1]); }
    return;
  }
  snprintf(out, n, "%s", f);
  char* e = strrchr(out, '.'); if (e && !strcmp(e, ".txt")) *e = 0;
  for (char* p = out; *p; p++) *p = toupper((unsigned char)*p);
}
