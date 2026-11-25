// sim.cpp: runs the cube's interface on a PC and writes screenshots.
//   sh sim/build.sh && printf 'wait 6000\nshot home\n' | ./sim/cubesim --out /tmp/shots
// Commands (one per line): wait MS | tap X Y | hold X Y MS | swipe X0 Y0 X1 Y1 | key prev|next|ok|back|pwr | shot NAME
//   set FIELD VALUE (color, glow, scan, glitch, boot, h24) | open PROG | vid (a test video frame) | app ID
#include "../src/ui/ui.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>

extern uint32_t simNow;
extern std::string root, appsDir;
static std::string outDir = "/tmp/shots";
extern float simTilt[3];
static int recN = 0;
static void dump(const std::string& p) {
  FILE* f = fopen(p.c_str(), "wb");
  fprintf(f, "P6 480 480 255\n");
  for (int i = 0; i < 480 * 480; i++) {
    uint16_t c = fb[i]; uint8_t rgb[3] = {(uint8_t)((c >> 11) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}
static void rec(int ms) {
  // frames every 40 ms (25 a second) into outDir/rec/NNNN.ppm
  system(("mkdir -p " + outDir + "/rec").c_str());
  for (int t = 0; t < ms; t += 40) {
    extern void step(uint32_t); step(40);
    char p[32]; snprintf(p, sizeof(p), "/rec/%04d.ppm", recN++);
    dump(outDir + p);
  }
}

void step(uint32_t ms) {
  uint32_t end = simNow + ms;
  extern void simAudioTick(int);
  while (simNow < end) { simNow += 16; simAudioTick(16); for (int k = 0; k < 12; k++) if (!mediaPumpWork()) break; btnPoll(); uiLoop(); uiFrame(); }
}
static void shot(const char* name) {
  uiDirty(); uiFrame();
  std::string p = outDir + "/" + name + ".ppm";
  FILE* f = fopen(p.c_str(), "wb");
  fprintf(f, "P6 480 480 255\n");
  for (int i = 0; i < 480 * 480; i++) {
    uint16_t c = fb[i]; uint8_t rgb[3] = {(uint8_t)((c >> 11) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  printf("shot %s\n", p.c_str());
}
int main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--out") && i + 1 < argc) outDir = argv[++i];
    else if (!strcmp(argv[i], "--apps") && i + 1 < argc) appsDir = argv[++i];
  }
  system(("mkdir -p " + outDir + " " + root + "/sd/PICS " + root + "/c").c_str());
  FILE* t = fopen((root + "/sd/README.TXT").c_str(), "w"); if (t) { fputs("ERROR-CUBE TF card.\nPut UPDATE.BIN here to install firmware.\n", t); fclose(t); }
  S.touch = S.pmu = S.rtc = S.imu = S.codec = S.sd = true;
  strcpy(S.sdInfo, "29.7 GB FAT32");
  S.battery = true; S.batPct = 87; S.batMv = 4012; S.usb = true; S.charging = true; S.vbusMv = 5040; S.pmuTemp = 31.5; S.chipTemp = 38.2;
  S.net = NET_UP; strcpy(S.ssid, "HOME-WIFI"); strcpy(S.ip, "192.168.1.42"); S.rssi = -54; strcpy(S.mac, "34:85:18:4F:2A:10"); strcpy(S.host, "CUBE-4F2A");
  S.link = true; S.panels = 1; S.timeOk = true;
  strcpy(cfg.code, "QX7K2M"); strcpy(cfg.name, "ALEX");
  appsRegister();
  uiInit();
  uiOpen(cfg.boot == 0 && cfg.intro ? introScreen() : bootScreen());
  char line[256]; static char reqp[200];
  while (fgets(line, sizeof(line), stdin)) {
    char cmd[32] = ""; int a = 0, b = 0, c = 0, d = 0; char s[64] = "";
    sscanf(line, "%31s", cmd);
    if (!strcmp(cmd, "wait")) { sscanf(line, "%*s %d", &a); step(a); }
    else if (!strcmp(cmd, "tap")) { sscanf(line, "%*s %d %d", &a, &b); uiTouch(true, a, b); step(50); uiTouch(false, a, b); step(120); }
    else if (!strcmp(cmd, "hold")) { sscanf(line, "%*s %d %d %d", &a, &b, &c); uiTouch(true, a, b); step(c); uiTouch(false, a, b); step(100); }
    else if (!strcmp(cmd, "swipe")) {
      sscanf(line, "%*s %d %d %d %d", &a, &b, &c, &d);
      uiTouch(true, a, b); for (int i = 1; i <= 8; i++) { step(20); uiTouch(true, a + (c - a) * i / 8, b + (d - b) * i / 8); } uiTouch(false, c, d); step(150);
    }
    else if (!strcmp(cmd, "drag")) {   // slow drag (scrolls, draws)
      sscanf(line, "%*s %d %d %d %d", &a, &b, &c, &d);
      uiTouch(true, a, b); for (int i = 1; i <= 30; i++) { step(30); uiTouch(true, a + (c - a) * i / 30, b + (d - b) * i / 30); } step(600); uiTouch(false, c, d); step(150);
    }
    else if (!strcmp(cmd, "key")) {
      sscanf(line, "%*s %63s", s);
      int k = !strcmp(s, "prev") ? K_PREV : !strcmp(s, "next") ? K_NEXT : !strcmp(s, "ok") ? K_OK : !strcmp(s, "back") ? K_BACK : !strcmp(s, "pwr") ? K_PWR : K_PWR_LONG;
      uiKey(k); step(150);
    }
    else if (!strcmp(cmd, "shot")) { sscanf(line, "%*s %63s", s); shot(s); }
    else if (!strcmp(cmd, "rec")) { sscanf(line, "%*s %d", &a); rec(a); }
    else if (!strcmp(cmd, "fopen")) { sscanf(line, "%*s %190s", reqp); snprintf(S.openPath, sizeof(S.openPath), "%s", reqp); S.openAt = simNow | 1; step(300); }
    else if (!strcmp(cmd, "btn")) { sscanf(line, "%*s %d %d", &a, &b); btnRaw(a, b); step(20); }      // btn 0|1 (- / +) 1|0 (down / up)
    else if (!strcmp(cmd, "pwr")) { sscanf(line, "%*s %d", &a); btnPwr(a); step(20); }                 // pwr 0 press, 1 hold
    else if (!strcmp(cmd, "tilt")) { float x, z; sscanf(line, "%*s %f %f", &x, &z); simTilt[0] = x; simTilt[2] = z; }
    else if (!strcmp(cmd, "accel")) { float x, y, z; sscanf(line, "%*s %f %f %f", &x, &y, &z); simTilt[0] = x; simTilt[1] = y; simTilt[2] = z; }
    else if (!strcmp(cmd, "gyro")) { extern float simGyro[3]; sscanf(line, "%*s %f %f %f", &simGyro[0], &simGyro[1], &simGyro[2]); }
    else if (!strcmp(cmd, "glitch")) { sscanf(line, "%*s %d %d", &a, &b); uiGlitch(a, b); }
    else if (!strcmp(cmd, "dragrec")) {   // a slow drag, recorded
      sscanf(line, "%*s %d %d %d %d", &a, &b, &c, &d);
      system(("mkdir -p " + outDir + "/rec").c_str());
      uiTouch(true, a, b);
      for (int i = 1; i <= 20; i++) { step(40); uiTouch(true, a + (c - a) * i / 20, b + (d - b) * i / 20); char p[32]; snprintf(p, sizeof(p), "/rec/%04d.ppm", recN++); dump(outDir + p); }
      uiTouch(false, c, d);
    }
    else if (!strcmp(cmd, "open")) { sscanf(line, "%*s %63s", s); openProg(s); step(200); }
    else if (!strcmp(cmd, "app")) { sscanf(line, "%*s %63s", s); strcpy(S.wantApp, s); S.wantAppAt = simNow + 1; step(300); }
    else if (!strcmp(cmd, "vid")) {
      if (!vidBuf) vidBuf = (uint16_t*)malloc(480 * 480 * 2);
      for (int y = 0; y < 480; y++) for (int x = 0; x < 480; x++) vidBuf[y * 480 + x] = rgb3(x / 2, y / 2, 128 + ((x / 40 + y / 40) & 1) * 80);
      S.vidAt = simNow + 1; S.vidSeq++; step(200);
    }
    else if (!strcmp(cmd, "set")) {
      sscanf(line, "%*s %31s %d", s, &a);
      if (!strcmp(s, "color")) cfg.color = a; else if (!strcmp(s, "glow")) cfg.glow = a; else if (!strcmp(s, "scan")) cfg.scan = a;
      else if (!strcmp(s, "glitch")) cfg.glitch = a; else if (!strcmp(s, "boot")) cfg.boot = a; else if (!strcmp(s, "h24")) cfg.h24 = a;
      else if (!strcmp(s, "lens")) cfg.lens = a; else if (!strcmp(s, "view")) cfg.view = a;
      else if (!strcmp(s, "holo")) cfg.holo = a; else if (!strcmp(s, "holoSensor")) cfg.holoSensor = a;
      else if (!strcmp(s, "aod")) cfg.aod = a; else if (!strcmp(s, "aodStyle")) cfg.aodStyle = a; else if (!strcmp(s, "offline")) cfg.offline = a;
      else if (!strcmp(s, "btnDouble")) cfg.btnDouble = a; else if (!strcmp(s, "btnRepeat")) cfg.btnRepeat = a;
      else if (!strcmp(s, "holoCentre")) cfg.holoCentre = a; else if (!strcmp(s, "holoBack")) cfg.holoBack = a;
      else if (!strcmp(s, "holoDepth")) cfg.holoDepth = a; else if (!strcmp(s, "holoSmooth")) cfg.holoSmooth = a;
      else if (!strcmp(s, "upd")) { S.upd = a; strcpy(S.updVer, "1.1"); S.updPct = 42; strcpy(S.updNotes, "Faster chat, new LEVEL look"); }
      cfgChanged();
    }
  }
  return 0;
}
