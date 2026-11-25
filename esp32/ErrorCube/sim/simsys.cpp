// simsys.cpp: the board and the network, faked for the PC simulator (screenshots of every screen)
#include "../src/ui/ui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <math.h>
#include <string>

uint32_t simNow = 0;
uint32_t msNow() { return simNow; }
Cfg cfg;
SysState S;
uint16_t* vidBuf = nullptr;
void cfgSave() {}
void cfgReset() { cfg = Cfg(); }
std::string root = "/tmp/cubesim", appsDir = "";

void sysBrightness(int) {}
void sysPowerOff() { printf("POWER OFF\n"); }
void sysRestart() { printf("RESTART\n"); }
static time_t base = 1795628520;   // 2026-11-25 17:42 UTC = 18:42 in Amsterdam
static int64_t setOff = 0;
void sysSetTime(int y, int mo, int d, int h, int mi, int s) { struct tm t = {}; t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d; t.tm_hour = h; t.tm_min = mi; t.tm_sec = s; setOff = mktime(&t) - (base + simNow / 1000); }
bool sysLocalTime(struct tm& t) {
  setenv("TZ", TIMEZONES[cfg.tzIdx % N_TZ].posix, 1); tzset();
  time_t now = base + simNow / 1000 + setOff; localtime_r(&now, &t); return true;
}
float simTilt[3] = {0.3f, 9.7f, 0.4f};
float simGyro[3] = {0.4f, -0.2f, 0.1f};
bool imuRead(float a[3], float g[3]) { a[0] = simTilt[0]; a[1] = simTilt[1]; a[2] = simTilt[2]; g[0] = simGyro[0]; g[1] = simGyro[1]; g[2] = simGyro[2]; return true; }
uint32_t freeHeap() { return 214 * 1024; }
uint32_t freePsram() { return 6120 * 1024; }
uint32_t uptimeSecs() { return 3725 + simNow / 1000; }
const char* resetReason() { return "POWER ON"; }
void sysChargeCurrent(int) {}
void sysApplyRotation() {}
void snd(int) {}
void tone(int, int, int) {}
void sndStop() {}

static std::string real(const char* p) {
  if (!strncmp(p, "/sd", 3)) return root + "/sd" + (p + 3);
  if (!strncmp(p, "/c", 2)) return root + "/c" + (p + 2);
  return root + p;
}
std::string simRealPath(const char* p) { return real(p); }   // PICO-8 opens its files itself
int fsList(const char* dir, FileEnt* out, int max) {
  DIR* d = opendir(real(dir).c_str()); if (!d) return -1;
  int n = 0; struct dirent* e;
  while ((e = readdir(d)) && n < max) {
    if (e->d_name[0] == '.') continue;
    snprintf(out[n].name, sizeof(out[n].name), "%s", e->d_name);
    struct stat st; std::string f = real(dir) + "/" + e->d_name; stat(f.c_str(), &st);
    out[n].dir = S_ISDIR(st.st_mode); out[n].size = st.st_size; n++;
  }
  closedir(d); return n;
}
bool fsRead(const char* path, uint8_t* buf, size_t max, size_t& n) {
  FILE* f = fopen(real(path).c_str(), "rb"); if (!f) return false; n = fread(buf, 1, max, f); fclose(f); return true;
}
bool fsWrite(const char* path, const uint8_t* buf, size_t n) {
  std::string r = real(path); std::string d = r.substr(0, r.rfind('/')); std::string cmd = "mkdir -p '" + d + "'"; system(cmd.c_str());
  FILE* f = fopen(r.c_str(), "wb"); if (!f) return false; fwrite(buf, 1, n, f); fclose(f); return true;
}
bool fsRemove(const char* path) { return remove(real(path).c_str()) == 0; }
bool fsAppend(const char* path, const uint8_t* buf, size_t n, size_t at) {
  std::string r = real(path); if (!at) { std::string d = r.substr(0, r.rfind('/')); system(("mkdir -p '" + d + "'").c_str()); }
  FILE* f = fopen(r.c_str(), at ? "r+b" : "wb"); if (!f) return false; if (at) fseek(f, at, SEEK_SET);
  size_t w = fwrite(buf, 1, n, f); fclose(f); return w == n;
}
bool fsMkdir(const char* path) { return system(("mkdir -p '" + real(path) + "'").c_str()) == 0; }
bool fsRename(const char* a, const char* b) { return rename(real(a).c_str(), real(b).c_str()) == 0; }
bool fsRmdir(const char* path) { return rmdir(real(path).c_str()) == 0; }
long fsSize(const char* path) { struct stat st; if (stat(real(path).c_str(), &st) != 0) return -1; return S_ISDIR(st.st_mode) ? 0 : (long)st.st_size; }
bool fsCopy(const char* a, const char* b, int* pct) {
  FILE* x = fopen(real(a).c_str(), "rb"); if (!x) return false;
  std::string r = real(b); system(("mkdir -p '" + r.substr(0, r.rfind('/')) + "'").c_str());
  FILE* y = fopen(r.c_str(), "wb"); if (!y) { fclose(x); return false; }
  char buf[16384]; size_t n; while ((n = fread(buf, 1, sizeof(buf), x)) > 0) fwrite(buf, 1, n, y);
  fclose(x); fclose(y); if (pct) *pct = 100; return true;
}
void* fsOpen(const char* path, const char* mode) { return fopen(real(path).c_str(), mode); }
size_t fsReadF(void* f, void* buf, size_t n) { return f ? fread(buf, 1, n, (FILE*)f) : 0; }
bool fsSeekF(void* f, long pos) { return f && (pos < 0 ? fseek((FILE*)f, 0, SEEK_END) : fseek((FILE*)f, pos, SEEK_SET)) == 0; }
long fsTellF(void* f) { return f ? ftell((FILE*)f) : -1; }
void fsClose(void* f) { if (f) fclose((FILE*)f); }
// the speaker, pretend: it plays 44.1 thousand frames a second of simulated time
static int simQ = 0; static uint32_t simPlayed = 0; static bool simPause = false; static float simPh = 0;
int sndMediaRoom() { return 32767 - simQ; }
void sndMediaPut(const int16_t*, int frames, int rate) { simQ += (int)((long long)frames * 44100 / rate); if (simQ > 32767) simQ = 32767; }
void mediaStart() {}
void sndMediaClear() { simQ = 0; simPlayed = 0; simPause = false; }
uint32_t sndMediaPlayed() { return simPlayed; }
int sndMediaQueued() { return simQ; }
void sndMediaPause(bool p) { simPause = p; }
void sndMediaWave(int16_t* o, int n) { for (int i = 0; i < n; i++) o[i] = simQ ? (int16_t)(9000 * sinf(simPh + i * 0.21f) + 5000 * sinf(simPh * 1.7f + i * 0.05f)) : 0; simPh += 0.4f; }
void simAudioTick(int ms) { if (simPause) return; int k = 44100 * ms / 1000; if (k > simQ) k = simQ; simQ -= k; simPlayed += k; }
bool simSdIn = true;
bool sdMount() { S.sd = simSdIn; if (S.sd) strcpy(S.sdInfo, "29.7 GB SDHC"); else S.sdInfo[0] = 0; return S.sd; }
void sdEject() { S.sd = false; S.sdInfo[0] = 0; }
void sysApplyButtons() {}
void sysLowPower(bool) {}
void netOffline(bool on) { S.net = on ? NET_OFF : NET_UP; S.link = !on; }
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "stb_image.h"
bool jpegDecode(const uint8_t* d, size_t n, uint16_t* out, int ow, int oh, int mode, int* iw, int* ih) {
  int w, h, c; uint8_t* p = stbi_load_from_memory(d, (int)n, &w, &h, &c, 3);
  if (!p) return false;
  if (iw) *iw = w; if (ih) *ih = h;
  int dw = ow, dh = oh, ox = 0, oy = 0;
  if (mode != IMG_STRETCH) { float k = mode == IMG_FIT ? fminf((float)ow / w, (float)oh / h) : fmaxf((float)ow / w, (float)oh / h); dw = (int)(w * k); dh = (int)(h * k); ox = (ow - dw) / 2; oy = (oh - dh) / 2; }
  for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) {
    int X = x + ox, Y = y + oy; if (X < 0 || Y < 0 || X >= ow || Y >= oh) continue;
    const uint8_t* q = p + ((y * h / dh) * w + (x * w / dw)) * 3;
    out[Y * ow + X] = rgb3(q[0], q[1], q[2]);
  }
  free(p); return true;
}
bool fsInfo(const char* drive, uint64_t& total, uint64_t& used) { total = drive[1] == 's' ? 31914983424ULL : 10354688; used = drive[1] == 's' ? 2147483648ULL : 98304; return true; }

void netWifi(bool on) { S.net = on ? NET_UP : NET_OFF; }
void netScan() {}
int netScanResults(WifiNet* out, int max) {
  const char* N[] = {"HOME-WIFI", "Ziggo4821", "KPN-Guest", "iPhone van Alex"}; int R[] = {-52, -67, -74, -81};
  for (int i = 0; i < 4 && i < max; i++) { snprintf(out[i].ssid, 33, "%s", N[i]); out[i].rssi = R[i]; out[i].open = i == 2; out[i].saved = i == 0; }
  return 4;
}
void netJoin(const char*, const char*) {}
int netSaved(char (*ssids)[33], int max) { if (max) strcpy(ssids[0], "HOME-WIFI"); return 1; }
void netForget(const char*) {}
void netPortal(bool on) { S.portal = on; strcpy(S.portalName, "ERROR-CUBE-4F2A"); }
void netLinkNewCode() { strcpy(cfg.code, "QX7K2M"); }
void netLinkState() {}
void netUpdateCheck() { S.upd = UPD_LATEST; strcpy(S.updVer, CUBE_VERSION); }
void netUpdateInstall() {}
bool netInstallFile(const char*) { return false; }
void netSetFocus(int) {}
bool netWho(WhoList& w) {
  w = WhoList(); w.ok = true; w.at = simNow > 2000 ? simNow - 2000 : 1; w.total = 7; w.ng = 4;
  const char* T[][3] = {{"Mistwalkers", "MIST", "VITO, VIKA"}, {"Tusa u Berlini", "TUSA", "OLEKSIY"}, {"ERROR-CHAT", "CHAT", "ALEX, GUEST"}, {"ERROR-CUBE", "CUBE", "CUBE-4F2A, OLED-77"}};
  int N[] = {2, 1, 2, 2};
  for (int i = 0; i < 4; i++) { snprintf(w.g[i].title, 28, "%s", T[i][0]); snprintf(w.g[i].dos, 9, "%s", T[i][1]); snprintf(w.g[i].players, 200, "%s", T[i][2]); w.g[i].n = N[i]; }
  return true;
}
bool netChat(ChatLog& c) {
  c = ChatLog(); c.ok = true; c.at = 5;
  struct { const char* n; const char* t; bool me, sys; } M[] = {
    {"", "VIKA joined", false, true}, {"VIKA", "привіт! хто грає сьогодні?", false, false}, {"VITO", "mistwalkers at 20:00?", false, false},
    {"CUBE", "the cube is online :)", true, false}, {"VIKA", "ok! I bring the camera", false, false}, {"VITO", "GG!", false, false}};
  for (int i = 0; i < 6; i++) { c.m[i].id = i + 1; snprintf(c.m[i].name, 17, "%s", M[i].n); snprintf(c.m[i].text, 204, "%s", M[i].t); c.m[i].me = M[i].me; c.m[i].sys = M[i].sys; }
  c.n = 6; strcpy(c.who, "VIKA VITO CUBE"); return true;
}
void netChatSay(const char*) {}
void netChatReset() {}
int netApps(AppInfo* out, int max) {
  // the real list from the repo
  std::string p = appsDir + "/apps.json"; FILE* f = fopen(p.c_str(), "rb"); if (!f) return -2;
  static char buf[16384]; size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); buf[n] = 0;
  int k = 0; char* s = buf;
  while ((s = strstr(s, "\"id\"")) && k < max) {
    AppInfo& a = out[k]; memset(&a, 0, sizeof(a));
    auto field = [&](const char* key, char* dst, int len) {
      char pat[16]; snprintf(pat, sizeof(pat), "\"%s\"", key); char* q = strstr(s, pat); char* end = strchr(s, '}');
      if (!q || q > end) return; q = strchr(q + strlen(pat), '"'); if (!q) return; q++; char* e = strchr(q, '"'); int l = e - q; if (l >= len) l = len - 1; memcpy(dst, q, l); dst[l] = 0; };
    field("id", a.id, 24); field("name", a.name, 24); field("info", a.info, 48); field("file", a.file, 64); field("v", a.v, 8);
    k++; s = strchr(s, '}'); if (!s) break;
  }
  return k;
}
void netAppsRefresh() {}
int netAppGet(const char*) { return 2; }
char* netAppSource(const char* id, size_t* n) {
  AppInfo a[32]; int k = netApps(a, 32);
  for (int i = 0; i < k; i++) if (!strcmp(a[i].id, id)) {
    std::string p = appsDir + "/" + a[i].file; FILE* f = fopen(p.c_str(), "rb"); if (!f) return nullptr;
    fseek(f, 0, SEEK_END); long l = ftell(f); fseek(f, 0, SEEK_SET); char* b = (char*)malloc(l + 1); fread(b, 1, l, f); fclose(f); b[l] = 0; *n = l; return b;
  }
  return nullptr;
}
void netAppSend(const char* id, const char* json) { printf("APP %s -> phone: %s\n", id, json); }
bool netAppMsg(char*, size_t) { return false; }
void netFetch(int, const char*) {}
bool netFetchDone(int&, int&, char*&) { return false; }
void netReportProg(const char*) {}
