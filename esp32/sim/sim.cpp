// ERROR-OLED on a PC: runs the real screens (ui.cpp, model.cpp, hidparse.cpp) with a fake board.
// Network jobs go to a real server with curl (default http://127.0.0.1:8787, `npm run dev`).
// Reads a script of inputs from stdin and saves screenshots as PBM files:
//   wait MS | key NAME | type TEXT | btn short|long|double | mouse DX DY [BUTTONS] | wheel N
//   kb on|off | passkey N | serial TEXT | net up|off | shot NAME | hid HEXBYTES (a keyboard report)
// Build: see sim/build.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <clib/u8g2.h>
#include "../ErrorOled/ui.h"
#include "../ErrorOled/hidparse.h"

static uint32_t clockMs = 0;
static std::string server = "http://127.0.0.1:8787";
static std::string outDir = ".";

static std::string urlenc(const char* s) {
  std::string o; char b[4];
  for (; *s; s++) { unsigned char c = *s; if (isalnum(c) || c == '-' || c == '_' || c == '.') o += c; else { snprintf(b, 4, "%%%02X", c); o += b; } }
  return o;
}
static std::string shq(const std::string& s) { std::string o = "'"; for (char c : s) { if (c == '\'') o += "'\\''"; else o += c; } return o + "'"; }

static int curl(const std::string& method, const std::string& url, const std::string& body, std::string& out) {
  std::string cmd = "curl -s -m 5 -w '\\n%{http_code}' -X " + method + " " + shq(url);
  if (!body.empty()) cmd += " -H 'content-type: application/json' --data-binary " + shq(body);
  FILE* f = popen(cmd.c_str(), "r");
  if (!f) return -1;
  char buf[4096]; size_t n; out.clear();
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
  pclose(f);
  size_t nl = out.rfind('\n');
  int code = nl == std::string::npos ? -1 : atoi(out.c_str() + nl + 1);
  if (nl != std::string::npos) out.resize(nl);
  return code ? code : -1;
}

struct Job { uint8_t kind; std::string method, url, body; uint32_t at; };

class FakeSys : public Sys {
 public:
  Settings s;
  uint8_t net = NET_UP;
  bool kb = false;
  uint32_t pk = 0;
  std::vector<Job> jobs;
  bool scanning = false;
  FakeSys() { settingsDefaults(s); strcpy(s.name, "OLED"); strcpy(s.ssid, "HOMENET"); }
  uint32_t ms() override { return clockMs; }
  bool clock(int& h, int& m, int& sec) override { uint32_t t = 20 * 3600 + 41 * 60 + clockMs / 1000; h = t / 3600 % 24; m = t / 60 % 60; sec = t % 60; return true; }
  Settings& cfg() override { return s; }
  void saveCfg() override {}
  void led(bool) override {}
  void serialOut(const char* l) override { fprintf(stderr, "  [serial] %s\n", l); }
  uint8_t netState() override { return net; }
  const char* netSsid() override { return s.ssid; }
  const char* netIp() override { return "192.168.1.42"; }
  int netRssi() override { return -58; }
  void wifiScan() override { scanAt = clockMs + 1500; }
  int wifiResults(WifiNet* o, int max) override {
    if (clockMs < scanAt) return -1;
    const char* names[] = {"HOMENET", "Livebox-3F2A", "FreeWifi", "iPhone van Alex", "SFR_88B0"};
    int rs[] = {-48, -63, -71, -55, -80};
    int n = 5 < max ? 5 : max;
    for (int i = 0; i < n; i++) { strcpy(o[i].ssid, names[i]); o[i].rssi = rs[i]; o[i].open = i == 2; }
    return n;
  }
  void wifiJoin(const char* ssid, const char* pass) override { strcpy(s.ssid, ssid); strcpy(s.pass, pass); net = NET_UP; }
  void portalStart() override {}
  void portalStop() override {}
  bool netBusy(uint8_t j) override { for (auto& x : jobs) if (x.kind == j) return true; return false; }
  void fetchOnline() override { jobs.push_back({JOB_ONLINE, "GET", server + "/api/online", "", clockMs + 150}); }
  void fetchChat(const char* code, uint32_t after, bool bg) override {
    char q[200]; snprintf(q, sizeof(q), "/api/chat/%s/log?after=%u&limit=20&name=%s%s", code, after, urlenc(s.name).c_str(), bg ? "&bg=1" : "");
    jobs.push_back({(uint8_t)(bg ? JOB_CHAT_BG : JOB_CHAT), "GET", server + q, "", clockMs + 150});
  }
  void sendChat(const char* code, const char* text) override {
    std::string b = std::string("{\"name\":\"") + s.name + "\",\"text\":\"" + text + "\"}";
    jobs.push_back({JOB_SAY, "POST", server + "/api/chat/" + code + "/say", b, clockMs + 250});
  }
  void beacon() override {
    std::string b = std::string("{\"sid\":\"oledsimABCDE\",\"path\":\"/oled\",\"name\":\"") + s.name + "\"}";
    jobs.push_back({JOB_BEACON, "POST", server + "/api/online", b, clockMs + 50});
  }
  const char* netError() override { return ""; }
  void btScan() override { scanning = true; btScanEnd = clockMs + 3000; }
  bool btScanning() override { if (scanning && clockMs > btScanEnd) scanning = false; return scanning; }
  int btResults(BtDev* o, int max) override {
    if (!scanning && !btScanEnd) return 0;
    int n = clockMs > btScanEnd - 2000 ? 2 : 0;
    if (n > max) n = max;
    if (n > 0) { strcpy(o[0].name, "Rii i8+ BT"); strcpy(o[0].addr, "aa:bb:cc:dd:ee:01"); o[0].type = BT_COMBO; o[0].rssi = -50; }
    if (n > 1) { strcpy(o[1].name, "Logi K380"); strcpy(o[1].addr, "aa:bb:cc:dd:ee:02"); o[1].type = BT_KEYBOARD; o[1].rssi = -62; }
    return n;
  }
  void btPair(const char*) override { kb = true; }
  void btForget() override { kb = false; }
  int btConnected(BtDev* o, int max) override { if (!kb || max < 1) return 0; strcpy(o[0].name, "Rii i8+ BT"); o[0].type = BT_COMBO; return 1; }
  uint32_t btPasskey() override { return pk; }
  const char* btStatus() override { return ""; }
  int btBonds() override { return kb ? 1 : 0; }
  uint32_t freeHeap() override { return 182 * 1024; }
  uint32_t totalHeap() override { return 312 * 1024; }
  const char* chipInfo() override { return "ESP32-C3 160MHZ"; }
  void reboot() override { fprintf(stderr, "  [reboot]\n"); }
  void runJobs() {
    for (size_t i = 0; i < jobs.size();) {
      if (jobs[i].at > clockMs) { i++; continue; }
      Job j = jobs[i];
      jobs.erase(jobs.begin() + i);
      std::string out;
      int code = curl(j.method, j.url, j.body, out);
      if (j.kind != JOB_BEACON) uiNetResult(j.kind, code, out.c_str(), out.size());
    }
  }
 private:
  uint32_t scanAt = 0, btScanEnd = 0;
};

static u8g2_t u8g2;
static FakeSys sys;

static void shot(const char* name) {
  int W = u8g2_GetDisplayWidth(&u8g2), H = u8g2_GetDisplayHeight(&u8g2);
  uint8_t* buf = u8g2_GetBufferPtr(&u8g2);
  int tw = u8g2_GetBufferTileWidth(&u8g2) * 8;
  std::string path = outDir + "/" + name + ".pbm";
  FILE* f = fopen(path.c_str(), "w");
  fprintf(f, "P1\n%d %d\n", W, H);
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) fputs((buf[(y / 8) * tw + x] >> (y % 8)) & 1 ? "1 " : "0 ", f);
    fputc('\n', f);
  }
  fclose(f);
  fprintf(stderr, "shot %s\n", path.c_str());
}

static void step(uint32_t ms) {
  uint32_t end = clockMs + ms;
  while (clockMs < end) { clockMs += 10; sys.runJobs(); uiTick(); }
}

static KeyEv named(const char* n) {
  KeyEv k = {K_NONE, 0, 0};
  struct { const char* n; uint16_t c; } t[] = {{"enter", K_ENTER}, {"esc", K_ESC}, {"bksp", K_BKSP}, {"tab", K_TAB}, {"up", K_UP}, {"down", K_DOWN},
    {"left", K_LEFT}, {"right", K_RIGHT}, {"pgup", K_PGUP}, {"pgdn", K_PGDN}, {"f1", K_F1}, {"f2", K_F2}, {"f5", K_F5}, {"menu", K_MENU}, {"lang", K_LANG}};
  for (auto& x : t) if (!strcmp(x.n, n)) k.code = x.c;
  if (k.code == K_NONE && n[0]) { k.code = K_CHAR; k.ch = (uint8_t)n[0]; }
  return k;
}

static KeyTracker kt;
static void emitKey(const KeyEv& k) {
  if (k.code == K_LANG) { kt.ukrainian = !kt.ukrainian; uiSetLang(kt.ukrainian); return; }
  uiKey(k);
}

int main(int argc, char** argv) {
  bool big = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--big")) big = true;
    else if (!strcmp(argv[i], "--out") && i + 1 < argc) outDir = argv[++i];
    else if (!strcmp(argv[i], "--server") && i + 1 < argc) server = argv[++i];
    else if (!strcmp(argv[i], "--font") && i + 1 < argc) sys.s.font = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--name") && i + 1 < argc) strcpy(sys.s.name, argv[++i]);
    else if (!strcmp(argv[i], "--chan") && i + 1 < argc) strcpy(sys.s.channel, argv[++i]);
    else if (!strcmp(argv[i], "--nowifi")) { sys.s.ssid[0] = 0; sys.net = NET_OFF; }
  }
  if (big) u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_empty, u8x8_dummy_cb);
  else u8g2_Setup_ssd1306_i2c_72x40_er_f(&u8g2, U8G2_R0, u8x8_byte_empty, u8x8_dummy_cb);
  u8g2_InitDisplay(&u8g2);
  sys.s.glitch = 0;            // screenshots without random glitches
  uiBegin(&u8g2, &sys);
  char line[512];
  while (fgets(line, sizeof(line), stdin)) {
    line[strcspn(line, "\r\n")] = 0;
    char* a = strchr(line, ' ');
    char* arg = a ? a + 1 : (char*)"";
    if (a) *a = 0;
    if (!strcmp(line, "wait")) step(atoi(arg));
    else if (!strcmp(line, "key")) { uiKey(named(arg)); step(40); }
    else if (!strcmp(line, "type")) { const char* p = arg; while (*p) { KeyEv k = {K_CHAR, 0, 0}; unsigned char c = *p; uint32_t cp = c; int n = 0;
        if (c >= 0xF0) { cp = c & 7; n = 3; } else if (c >= 0xE0) { cp = c & 15; n = 2; } else if (c >= 0xC0) { cp = c & 31; n = 1; }
        p++; while (n-- > 0 && *p) cp = (cp << 6) | (*p++ & 0x3F);
        k.ch = cp; uiKey(k); step(20); } }
    else if (!strcmp(line, "btn")) { uiButton(!strcmp(arg, "long") ? BTN_LONG : !strcmp(arg, "double") ? BTN_DOUBLE : BTN_SHORT); step(40); }
    else if (!strcmp(line, "mouse")) { int dx = 0, dy = 0, b = 0; sscanf(arg, "%d %d %d", &dx, &dy, &b); uiMouse(dx, dy, 0, b); step(20); if (b) { uiMouse(0, 0, 0, 0); step(20); } }
    else if (!strcmp(line, "wheel")) { uiMouse(0, 0, atoi(arg), 0); step(20); }
    else if (!strcmp(line, "kb")) { sys.kb = !strcmp(arg, "on"); step(300); }
    else if (!strcmp(line, "passkey")) { sys.pk = atoi(arg); step(300); }
    else if (!strcmp(line, "serial")) { uiSerialLine(arg); step(40); }
    else if (!strcmp(line, "net")) { sys.net = !strcmp(arg, "up") ? NET_UP : NET_OFF; step(40); }
    else if (!strcmp(line, "hid")) {        // a raw boot-keyboard report, e.g. "hid 02 00 0B 00 00 00 00 00"
      uint8_t r[16]; int n = 0; char* p = arg;
      while (*p && n < 16) { r[n++] = strtol(p, &p, 16); while (*p == ' ') p++; }
      HidMap m; hidBootKeyboardMap(m, 0); HidInput in; hidDecode(m, 0, r, n, in); kt.report(in, clockMs, emitKey); step(40);
    }
    else if (!strcmp(line, "shot")) { step(30); shot(arg); }
    else if (!strcmp(line, "sleep")) { usleep(atoi(arg) * 1000); }
  }
  return 0;
}
