// simpico.cpp: PICO-CUBE's network part (pnet.cpp) faked for the simulator: the cart list is
// ../../public/pico8/carts.json, downloads copy from ../../public (simsys.cpp's netDownload)
#include "../src/ui/ui.h"
#include "../../PicoCube/src/pico/pico.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>

volatile bool pnetSwitching = false;
// "key": "value" or "key": number, inside one object
static std::string field(const std::string& o, const char* k) {
  std::string q = std::string("\"") + k + "\"";
  size_t i = o.find(q); if (i == std::string::npos) return "";
  i = o.find(':', i + q.size()); if (i == std::string::npos) return "";
  i++; while (i < o.size() && o[i] == ' ') i++;
  if (o[i] == '"') { size_t e = i + 1; std::string v; while (e < o.size() && o[e] != '"') { if (o[e] == '\\') e++; v += o[e++]; } return v; }
  size_t e = i; while (e < o.size() && o[e] != ',' && o[e] != '}') e++;
  return o.substr(i, e - i);
}
int pnetCarts(PCart* out, int max) {
  FILE* f = fopen("../../public/pico8/carts.json", "rb"); if (!f) return -2;
  std::string s; char b[4096]; size_t n; while ((n = fread(b, 1, sizeof(b), f)) > 0) s.append(b, n); fclose(f);
  size_t i = s.find("\"carts\""); if (i == std::string::npos) return -2;
  int k = 0;
  while (k < max && (i = s.find('{', i)) != std::string::npos) {
    size_t e = s.find('}', i); if (e == std::string::npos) break;
    std::string o = s.substr(i, e - i + 1); i = e;
    PCart& p = out[k]; memset(&p, 0, sizeof(p));
    snprintf(p.file, sizeof(p.file), "%s", field(o, "file").c_str()); snprintf(p.name, sizeof(p.name), "%s", field(o, "name").c_str());
    snprintf(p.author, sizeof(p.author), "%s", field(o, "author").c_str()); snprintf(p.lic, sizeof(p.lic), "%s", field(o, "license").c_str());
    snprintf(p.ver, sizeof(p.ver), "%s", field(o, "v").c_str()); snprintf(p.about, sizeof(p.about), "%s", field(o, "about").c_str());
    p.size = atoi(field(o, "size").c_str());
    if (p.file[0]) k++;
  }
  return k;
}
void pnetCartsRefresh() {}
void pnetSwitch() { pnetSwitching = true; S.upd = UPD_LOADING; S.updPct = 37; snprintf(S.updMsg, sizeof(S.updMsg), "ERROR-CUBE 1.5.3"); }
