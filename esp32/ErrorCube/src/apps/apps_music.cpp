// apps_music.cpp: MUSIC (MP3 / WAV from the SD card or the cube's flash: library, now playing with a picture of
// the sound, seek, shuffle / repeat, keeps playing in the background) and VIDEO (MJPEG AVI, full screen).
// The decoding is in core/media.cpp; videos from a phone: error.blue/cube/video converts MP4 to AVI.
#include "../ui/ui.h"
#include "../core/holo.h"
#include "../core/g3d.h"
#include "../lib/cubeqr.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <vector>
#include <algorithm>
#ifdef ARDUINO
#include <esp_heap_caps.h>
static void* bigAlloc(size_t n) { void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); return p ? p : malloc(n); }
#else
static void* bigAlloc(size_t n) { return malloc(n); }
#endif

static bool isAudio(const char* n) { const char* e = strrchr(n, '.'); return e && (!strcasecmp(e, ".mp3") || !strcasecmp(e, ".wav")); }
static bool isVideo(const char* n) { const char* e = strrchr(n, '.'); return e && !strcasecmp(e, ".avi"); }
// a video from a phone or camera (MP4, MOV, WebM...): the cube plays it after the phone converts it once
bool isPhoneVideo(const char* n) {
  static const char* X[] = {".mp4", ".m4v", ".mov", ".mkv", ".webm", ".3gp", ".3g2", ".ogv", ".mpg", ".mpeg", ".wmv", ".flv", ".ts"};
  const char* e = strrchr(n, '.'); if (!e) return false;
  for (const char* x : X) if (!strcasecmp(e, x)) return true;
  return false;
}
// "A:\VIDEO\holiday.mp4" -> "A:\VIDEO\holiday.avi": the name the phone gives the converted video
// (the same rules as error.blue/cube/conv.js: no \ / : * ? " < > | or control characters, at most 60)
std::string aviTwin(const char* path) {
  std::string p = path; size_t sl = p.rfind('/'), dot = p.rfind('.');
  std::string dir = p.substr(0, sl), b = p.substr(sl + 1, dot != std::string::npos && dot > sl ? dot - sl - 1 : std::string::npos);
  for (auto& c : b) if (strchr("\\/:*?\"<>|", c) || (unsigned char)c < 32) c = '_';
  while (!b.empty() && b.back() == ' ') b.pop_back();
  while (!b.empty() && b[0] == ' ') b.erase(0, 1);
  if (b.size() > 60) b.resize(60);
  if (b.empty()) b = "video";
  return dir + "/" + b + ".avi";
}
static void fmtMs(char* o, uint32_t ms) { uint32_t s = ms / 1000; if (s >= 3600) sprintf(o, "%u:%02u:%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60), (unsigned)(s % 60)); else sprintf(o, "%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60)); }

// ------------------------------------------------------------------ VOLUME: a slider for the players (cfg.vol, like SETUP)
// x, y: where "VOL" starts; w: the whole width. Drag or tap anywhere on it.
static bool volDrag = false;
static void volSlider(int x, int y, int w) {
  drawText(FT, x, y, "VOL", C_DIM, TX_FLAT);
  int bx = x + 52, bw = w - 52 - 62;
  meter(bx, y + 3, bw, 18, cfg.vol, true);
  int kx = bx + bw * cfg.vol / 100;
  fillRect(kx - 3, y - 1, 7, 26, volDrag ? C_WARN : C_FG);       // the knob
  char v[8]; snprintf(v, sizeof(v), cfg.vol ? "%d%%" : "OFF", cfg.vol);
  drawTextR(FT, x + w, y, v, cfg.vol ? C_FG : C_WARN, TX_FLAT);
}
static bool volEv(const Ev& e, int x, int y, int w) {
  int bx = x + 52, bw = w - 52 - 62;
  bool on = e.y0 >= y - 8 && e.y0 < y + 30 && e.x0 >= x && e.x0 < x + w;
  if (on && (e.t == EV_DOWN || e.t == EV_MOVE)) {
    int v = (e.x - bx) * 100 / bw; v = v < 0 ? 0 : v > 100 ? 100 : v;
    if (e.x0 < bx) v = e.t == EV_DOWN ? (cfg.vol ? 0 : 60) : cfg.vol;   // tap on VOL: mute / back on
    cfg.vol = v; volDrag = true; uiDirty(); return true;
  }
  if (volDrag && (e.t == EV_UP || e.t == EV_TAP || e.t == EV_LONG)) { volDrag = false; cfgChanged(); uiDirty(); return true; }
  return on && (e.t == EV_TAP || e.t == EV_LONG);
}
// the same, standing up (video: on the right side)
static void volSliderV(int x, int y, int h) {
  blendRect(x - 10, y - 34, 64, h + 74, 0, 24);
  drawTextC(FT, x + 22, y - 30, "VOL", C_DIM, TX_FLAT);
  rect(x + 10, y, 24, h, C_FG);
  int f = (h - 4) * cfg.vol / 100;
  fillRect(x + 12, y + h - 2 - f, 20, f, C_FG);
  char v[8]; snprintf(v, sizeof(v), cfg.vol ? "%d" : "OFF", cfg.vol);
  drawTextC(FT, x + 22, y + h + 6, v, cfg.vol ? C_FG : C_WARN, TX_FLAT);
}
static bool volEvV(const Ev& e, int x, int y, int h) {
  bool on = e.x0 >= x - 10 && e.x0 < x + 54 && e.y0 >= y - 34 && e.y0 < y + h + 40;
  if (on && (e.t == EV_DOWN || e.t == EV_MOVE)) {
    int v = (y + h - e.y) * 100 / h; cfg.vol = v < 0 ? 0 : v > 100 ? 100 : v; volDrag = true; uiDirty(); return true;
  }
  if (volDrag && (e.t == EV_UP || e.t == EV_TAP || e.t == EV_LONG)) { volDrag = false; cfgChanged(); uiDirty(); return true; }
  return on && (e.t == EV_TAP || e.t == EV_LONG);
}

// ------------------------------------------------------------------ the player (keeps going when MUSIC is closed)
static std::vector<std::string> plist;          // the playlist: the files of a folder
static int pidx = -1;
static uint8_t shuffle = 0, repeatMode = 1;     // repeat: 0 off, 1 all, 2 one
static std::vector<int> order;
static void makeOrder() {
  order.clear();
  for (int i = 0; i < (int)plist.size(); i++) order.push_back(i);
  if (shuffle) for (int i = order.size() - 1; i > 0; i--) { int j = rnd() % (i + 1); std::swap(order[i], order[j]); }
}
static void playAt(int i) {
  if (i < 0 || i >= (int)plist.size()) return;
  pidx = i;
  musPlay(plist[i].c_str());
  uiDirty();
}
void musicNext(int d) {
  if (plist.empty()) return;
  if ((int)order.size() != (int)plist.size()) makeOrder();
  int k = 0; for (int i = 0; i < (int)order.size(); i++) if (order[i] == pidx) k = i;
  k += d;
  if (k >= (int)order.size()) { if (repeatMode == 0 && d > 0) { musStop(); return; } k = 0; }
  if (k < 0) k = order.size() - 1;
  playAt(order[k]);
}
void musicToggle() {
  int st = musState();
  if (st == MED_PLAY) musPause(true);
  else if (st == MED_PAUSE) musPause(false);
  else if (!plist.empty()) playAt(pidx < 0 ? 0 : pidx);
  else openProg("MUSIC");
  uiDirty();
}
// a folder's music, starting at a file
void musicPlayFolder(const char* dir, const char* file) {
  static FileEnt* e = nullptr;
  if (!e) e = (FileEnt*)malloc(sizeof(FileEnt) * 200);
  if (!e) return;
  int n = fsList(dir, e, 200);
  plist.clear();
  for (int i = 0; i < n; i++) if (!e[i].dir && isAudio(e[i].name)) plist.push_back(std::string(dir) + "/" + e[i].name);
  std::sort(plist.begin(), plist.end(), [](const std::string& a, const std::string& b) { return strcasecmp(a.c_str(), b.c_str()) < 0; });
  makeOrder();
  int at = 0;
  for (int i = 0; i < (int)plist.size(); i++) if (file && plist[i].substr(plist[i].rfind('/') + 1) == file) at = i;
  playAt(at);
}
// every loop (apps_media.cpp): the song ended -> the next one
void musicTick() {
  static int last = MED_STOP;
  int st = musState();
  // the playlist goes on only for its own songs (not the alarm's)
  bool mine = pidx >= 0 && pidx < (int)plist.size() && !strcmp(musFile(), plist[pidx].c_str());
  if (mine && st == MED_END && last != MED_END) {
    if (repeatMode == 2) playAt(pidx); else musicNext(1);
  }
  if (st != last) uiDirty();
  last = st;
}

// ------------------------------------------------------------------ NOW PLAYING
// the pictures of the sound (1.5): tap the picture for the next one
static const char* VIS_NAMES[] = {"BARS", "SCOPE", "MIRROR", "CIRCLE", "PEAKS", "WATERFALL", "PULSE", "VU METER", "DOTS"};
static const int N_VIS = sizeof(VIS_NAMES) / sizeof(VIS_NAMES[0]);
struct NowPlaying : Screen {
  int16_t wave[256];
  float bands[24] = {0}, peaks[24] = {0}, coef[24], win[256]; bool ready = false;
  uint8_t fall[13][24] = {{0}}; uint32_t fallAt = 0;
  uint32_t rings[6] = {0}; float bassAvg = 0, vu = 0; uint32_t lastBeat = 0;
  bool dragSeek = false; uint32_t seekTo = 0;
  static const int VX = 40, VW = 400, VY = 166, VH = 102, VC = 217;    // the picture's box and its middle line
  const char* name() override { return "MUSIC.EXE"; }
  void tick() override { if (musState() == MED_PLAY) redrawIn(33); }
  // 24 bands from low to high (Goertzel at log-spaced frequencies, a Hann window), 0..1 on a 54 dB scale
  void spectrum(bool play) {
    if (!ready) {
      for (int b = 0; b < 24; b++) { float k = 1.4f * powf(64.0f, b / 23.0f); coef[b] = 2 * cosf(6.2831853f * k / 256); }
      for (int n = 0; n < 256; n++) win[n] = 0.5f - 0.5f * cosf(6.2831853f * n / 255);
      ready = true;
    }
    float sum = 0;
    for (int n = 0; n < 256; n++) sum += (float)wave[n] * wave[n];
    float rms = sqrtf(sum / 256) / 32767.0f, db = 20 * log10f(rms + 1e-6f);
    float v = (db + 40) / 40; v = v < 0 ? 0 : v > 1 ? 1 : v;
    vu = play ? (v > vu ? vu + (v - vu) * 0.6f : vu * 0.9f + v * 0.1f) : vu * 0.85f;
    for (int b = 0; b < 24; b++) {
      float s1 = 0, s2 = 0, c = coef[b];
      for (int n = 0; n < 256; n++) { float s0 = wave[n] * win[n] + c * s1 - s2; s2 = s1; s1 = s0; }
      float pw = s1 * s1 + s2 * s2 - c * s1 * s2;
      float mag = sqrtf(pw > 0 ? pw : 0), d = 20 * log10f(mag / (64 * 32767.0f) + 1e-9f) + b * (18.0f / 23);   // +3 dB an octave: the highs show too
      float e = (d + 54) / 54; e = e < 0 ? 0 : e > 1 ? 1 : e;
      bands[b] = play ? fmaxf(e, bands[b] * 0.8f) : bands[b] * 0.8f;
      peaks[b] = fmaxf(bands[b], peaks[b] - 0.018f);
    }
  }
  void ring(int cx, int cy, float r, uint16_t c, int a) {
    int n = r < 20 ? 24 : 48;
    for (int i = 0; i < n; i++) {
      float a0 = i * 6.2832f / n, a1 = (i + 1) * 6.2832f / n;
      glowLine(cx + cosf(a0) * r, cy + sinf(a0) * r, cx + cosf(a1) * r, cy + sinf(a1) * r, c, a);
    }
  }
  void drawVis(bool play) {
    uint32_t now = msNow();
    int mode = cfg.musVis % N_VIS, n = 24, bw = VW / n;
    clipSet(VX - 6, VY, VW + 12, VH);
    switch (mode) {
      case 0:   // BARS: the spectrum in segments, the wave behind it
        if (play) for (int i = 0; i + 2 < 256; i += 2) glowLine(VX + i * VW / 256, VC + wave[i] / 900, VX + (i + 2) * VW / 256, VC + wave[i + 2] / 900, C_DIM, 10);
        for (int i = 0; i < n; i++) {
          int h = 4 + (int)(bands[i] * 96);
          for (int y = 0; y < h; y += 5) fillRect(VX + i * bw + 2, VY + VH - y - 4, bw - 5, 3, y > 70 ? C_WARN : C_FG);
        }
        break;
      case 1: {  // SCOPE: the wave itself, like an oscilloscope
        for (int x = VX; x < VX + VW; x += 25) for (int y = VY + 6; y < VY + VH; y += 6) fillRect(x, y, 1, 2, C_DIM);
        hline(VX, VC, VW, C_DIM);
        for (int i = 0; i + 1 < 256; i++) {
          int y0 = VC + wave[i] / 640, y1 = VC + wave[i + 1] / 640;
          glowLine(VX + i * VW / 255, y0, VX + (i + 1) * VW / 255, y1, C_FG, 26);
        }
        break;
      }
      case 2:   // MIRROR: bars up from the middle line, their reflection under it
        for (int i = 0; i < n; i++) {
          int h = 2 + (int)(bands[i] * 46), x = VX + i * bw + 2;
          fillRect(x, VC - h, bw - 5, h, C_FG);
          blendRect(x, VC + 2, bw - 5, h * 3 / 4, C_FG, 9);
        }
        break;
      case 3: {  // CIRCLE: spokes round a ring that breathes with the bass
        float bass = (bands[0] + bands[1] + bands[2] + bands[3]) / 4;
        float r0 = 16 + bass * 8;
        ring(240, VC, r0, C_FG, 22);
        for (int i = 0; i < 48; i++) {
          int b = i < 24 ? i : 47 - i;
          float a = i * 6.2832f / 48 - 1.5708f, l = 3 + bands[b] * 30;
          glowLine(240 + cosf(a) * (r0 + 3), VC + sinf(a) * (r0 + 3), 240 + cosf(a) * (r0 + 3 + l), VC + sinf(a) * (r0 + 3 + l), b > 16 ? C_WARN : C_FG, 26);
        }
        break;
      }
      case 4:   // PEAKS: thin bars, the peak of each falls slowly
        for (int i = 0; i < n; i++) {
          int x = VX + i * bw + 4, h = (int)(bands[i] * 92), ph = (int)(peaks[i] * 92);
          fillRect(x, VY + VH - h, bw - 9, h, C_DIM);
          fillRect(x - 1, VY + VH - ph - 4, bw - 7, 3, C_FG);
        }
        break;
      case 5: {  // WATERFALL: each row is a moment, the newest on top, it runs down
        if (play && now - fallAt > 60) {
          fallAt = now;
          memmove(fall[1], fall[0], sizeof(fall) - sizeof(fall[0]));
          for (int i = 0; i < n; i++) fall[0][i] = (uint8_t)(bands[i] * 255);
        }
        for (int r = 0; r < 13; r++)
          for (int i = 0; i < n; i++) {
            int v = fall[r][i]; if (v < 20) continue;
            fillRect(VX + i * bw + 1, VY + 2 + r * 8, bw - 2, 7, mix(C_BG, v > 200 ? C_WARN : C_FG, v * 100 / 255));
          }
        break;
      }
      case 6: {  // PULSE: a ring goes out on every beat (the bass jumps)
        float bass = (bands[0] + bands[1] + bands[2]) / 3;
        if (play && bass > bassAvg * 1.18f + 0.04f && now - lastBeat > 200) {
          lastBeat = now;
          for (int k = 0; k < 6; k++) if (!rings[k] || now - rings[k] > 1200) { rings[k] = now | 1; break; }
        }
        bassAvg = bassAvg * 0.94f + bass * 0.06f;
        for (int k = 0; k < 6; k++) {
          if (!rings[k]) continue;
          uint32_t age = now - rings[k];
          if (age > 1200) { rings[k] = 0; continue; }
          float p = age / 1200.0f;
          ring(240, VC, 8 + p * 190, C_FG, (int)(30 * (1 - p)));
        }
        fillCircle(240, VC, 4 + (int)(bass * 16), C_FG);
        break;
      }
      case 7: {  // VU METER: a needle, like on an old amplifier
        const int px = 240, py = VY + VH + 40, R = 128;
        for (int t = 0; t <= 10; t++) {
          float a = -2.25f + t * (1.5f / 10);
          int l = t % 5 == 0 ? 12 : 6;
          glowLine(px + cosf(a) * (R - l), py + sinf(a) * (R - l), px + cosf(a) * R, py + sinf(a) * R, t >= 8 ? C_RED : C_FG, 22);
        }
        for (int t = 0; t < 30; t++) { float a = -2.25f + t * 0.05f; fillRect(px + cosf(a) * (R + 4), py + sinf(a) * (R + 4), 2, 2, t >= 24 ? C_RED : C_DIM); }
        float a = -2.25f + vu * 1.5f;
        glowLine(px, py, px + cosf(a) * (R - 4), py + sinf(a) * (R - 4), C_WARN, 30);
        drawText(FT, VX, VY + 4, "VU", C_DIM, TX_FLAT);
        break;
      }
      case 8: {  // DOTS: a grid of dots that light up with the bands
        for (int i = 0; i < n; i++)
          for (int r = 0; r < 10; r++) {
            bool on = bands[i] * 10 > r;
            int x = VX + i * bw + bw / 2, y = VY + VH - 6 - r * 10;
            if (on) fillCircle(x, y, 3, r > 7 ? C_WARN : C_FG); else fillRect(x - 1, y - 1, 2, 2, C_DIM);
          }
        break;
      }
    }
    clipReset();
  }
  void draw() override {
    char t[96];
    pathLine("C:\\MUSIC\\PLAYING>");
    int st = musState();
    // title and artist, the title in big letters (cut to fit)
    snprintf(t, sizeof(t), "%s", musTitle()[0] ? musTitle() : "NOTHING PLAYING"); utf8Cut(t, 21);
    drawTextC(FB, 240, 96, t, C_FG);
    snprintf(t, sizeof(t), "%s", musArtist()[0] ? musArtist() : (pidx >= 0 && !plist.empty() ? "" : "pick a song in the list")); utf8Cut(t, 28);
    drawTextC(FT, 240, 140, t, C_DIM, TX_FLAT);
    // the picture of the sound (tap it: the next one)
    sndMediaWave(wave, 256);
    spectrum(st == MED_PLAY);
    drawVis(st == MED_PLAY);
    // where in the song: drag on the bar to jump
    uint32_t len = musLenMs(), pos = dragSeek ? seekTo : musPosMs();
    meter(40, 284, 400, 20, len ? (int)((uint64_t)pos * 100 / len) : 0, true);
    char a[16], b[16]; fmtMs(a, pos); fmtMs(b, len);
    drawText(FT, 40, 304, a, C_FG, TX_FLAT); drawTextR(FT, 440, 304, b, C_DIM, TX_FLAT);
    if (st == MED_ERR) drawTextC(FT, 240, 304, "CAN'T PLAY THIS FILE", C_RED);
    else if (musKbps()) { snprintf(t, sizeof(t), "%d KBPS", musKbps()); drawTextC(FT, 240, 304, t, C_DIM, TX_FLAT); }
    // the buttons
    int by = 336;
    dosButton(40, by, 90, 46, "|<", [] { musicNext(-1); });
    dosButton(140, by, 120, 46, st == MED_PLAY ? "PAUSE" : "PLAY", [] { musicToggle(); }, st == MED_PLAY);
    dosButton(270, by, 90, 46, ">|", [] { musicNext(1); });
    static const char* RP[3] = {"RPT-", "RPT*", "RPT1"};
    dosButton(370, by, 70, 46, RP[repeatMode % 3], [] { repeatMode = (repeatMode + 1) % 3; toast(repeatMode == 0 ? "REPEAT OFF" : repeatMode == 1 ? "REPEAT ALL" : "REPEAT ONE", 900); }, repeatMode != 0);
    volSlider(40, 394, 400);
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {"S", shuffle ? "Shuffle*" : "Shuffle", [] { shuffle = !shuffle; makeOrder(); toast(shuffle ? "SHUFFLE ON" : "SHUFFLE OFF", 900); }},
           {"X", "Stop", [] { musStop(); }}, {"V", "Look", [] { cfg.musVis = (cfg.musVis + 1) % N_VIS; cfgChanged(); }}});
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_KEY) {
      if (e.key == K_NEXT) { musicNext(1); return true; }
      if (e.key == K_PREV) { musicNext(-1); return true; }
      if (e.key == K_OK) { musicToggle(); return true; }
      return false;
    }
    bool onBar = e.y0 >= 270 && e.y0 < 320 && e.x0 >= 30 && e.x0 < 450;
    if (onBar && musLenMs() && (e.t == EV_DOWN || e.t == EV_MOVE)) {
      int x = e.x < 40 ? 40 : e.x > 440 ? 440 : e.x;
      dragSeek = true; seekTo = (uint32_t)((uint64_t)(x - 40) * musLenMs() / 400); uiDirty(); return true;
    }
    if (dragSeek && (e.t == EV_UP || e.t == EV_TAP)) { dragSeek = false; musSeek(seekTo); uiDirty(); return true; }
    if (e.t == EV_TAP && e.y0 >= VY - 6 && e.y0 < VY + VH) {     // the picture: the next one
      cfg.musVis = (cfg.musVis + 1) % N_VIS; cfgChanged(); toast(VIS_NAMES[cfg.musVis], 900); uiDirty(); return true;
    }
    if (volEv(e, 40, 394, 400)) return true;
    if (e.t == EV_SWIPE && e.dir == SW_LEFT) { musicNext(1); return true; }
    return false;
  }
};

// ------------------------------------------------------------------ MUSIC: the library
struct Music : Screen {
  char dir[120] = "";
  FileEnt ents[160]; int n = 0;
  ListView L;
  const char* name() override { return "MUSIC.EXE"; }
  void load() {
    n = 0;
    if (!dir[0]) {
      // start in a MUSIC folder on the card if there is one
      sdMount();
      if (S.sd && fsSize("/sd/MUSIC") >= 0) strcpy(dir, "/sd/MUSIC");
      else if (S.sd) strcpy(dir, "/sd");
      else { fsMkdir("/c/music"); strcpy(dir, "/c/music"); }
    }
    strcpy(ents[0].name, ".."); ents[0].dir = true; ents[0].size = 0; n = 1;
    static FileEnt* tmp = (FileEnt*)bigAlloc(sizeof(FileEnt) * 160);
    if (!tmp) return;
    int k = fsList(dir, tmp, 159);
    for (int i = 0; i < k; i++) if (tmp[i].dir || isAudio(tmp[i].name)) ents[n++] = tmp[i];
    for (int a = 1; a < n; a++) for (int b = a + 1; b < n; b++) {
      bool sw = ents[a].dir != ents[b].dir ? ents[b].dir : strcasecmp(ents[a].name, ents[b].name) > 0;
      if (sw) { FileEnt t = ents[a]; ents[a] = ents[b]; ents[b] = t; }
    }
    L.n = n; L.sel = n > 1 ? 1 : 0; L.scroll = 0;
    uiReveal();
  }
  void enter() override { L.setBox(15, 92, 450, 312); L.rowH = 52; if (!n) load(); }
  void up() {
    if (!strcmp(dir, "/sd") || !strcmp(dir, "/c")) { strcpy(dir, !strcmp(dir, "/sd") ? "/c" : "/sd"); load(); return; }
    char* s = strrchr(dir, '/'); if (s && s != dir) *s = 0; load();
  }
  void open(int i) {
    if (i == 0) { up(); return; }
    FileEnt& f = ents[i];
    char p[200]; snprintf(p, sizeof(p), "%s/%s", dir, f.name);
    if (f.dir) { snprintf(dir, sizeof(dir), "%s", p); load(); return; }
    musicPlayFolder(dir, f.name);
    uiOpen(new NowPlaying());
  }
  void draw() override {
    char path[40]; snprintf(path, sizeof(path), "%c:%s>", dir[1] == 's' ? 'A' : 'C', strchr(dir + 1, '/') ? strchr(dir + 1, '/') : "\\");
    for (char* q = path; *q; q++) if (*q == '/') *q = '\\';
    utf8Cut(path, 28);
    pathLine(path);
    L.n = n;
    L.draw([&](int i, int x, int y, int w, int h, bool sel) {
      FileEnt& f = ents[i];
      int ty = y + (h - LH) / 2;
      if (sel) invBar(x + 2, y + 3, w - 4, h - 6);
      uint16_t c = sel ? C_BG : C_FG; int m = sel ? TX_FLAT : TX_GLOW;
      char nm[64]; snprintf(nm, sizeof(nm), "%s%s", f.dir && i ? "\xE2\x96\x92 " : "", i == 0 ? (!strcmp(dir, "/sd") ? ".. (FLASH)" : !strcmp(dir, "/c") ? ".. (SD CARD)" : "..") : f.name);
      char* d = strrchr(nm, '.'); if (d && !f.dir) *d = 0;
      bool now = !f.dir && musFile()[0] && strstr(musFile(), f.name) && musState() != MED_STOP;
      if (now) { char t2[64]; snprintf(t2, sizeof(t2), "\xE2\x96\xBA %s", nm); strcpy(nm, t2); }
      utf8Cut(nm, 24);
      drawText(FT, x + 10, ty, nm, c, m);
      if (!f.dir) { char sz[16]; snprintf(sz, sizeof(sz), "%.1fM", f.size / 1048576.0); drawTextR(FT, x + w - 10, ty, sz, sel ? C_BG : C_DIM, TX_FLAT); }
    }, nullptr);
    if (n <= 1) drawTextC(FT, 240, 250, "NO MP3 / WAV HERE", C_DIM, TX_FLAT);
    char b[48];
    if (musState() == MED_PLAY || musState() == MED_PAUSE) { snprintf(b, sizeof(b), "%s %s", musState() == MED_PLAY ? "\xE2\x96\xBA" : "||", musTitle()); utf8Cut(b, 28); }
    else snprintf(b, sizeof(b), "Put songs in A:\\MUSIC");
    drawText(FT, colX(0), rowY(12) + 6, b, C_DIM, TX_FLAT);
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {"P", "Playing", [] { uiOpen(new NowPlaying()); }}, {"R", "Ref", [this] { load(); }}});
  }
  void tick() override { L.tick(); }
  bool ev(const Ev& e) override {
    if (e.t == EV_SWIPE && e.dir == SW_LEFT) { uiOpen(new NowPlaying()); return true; }
    if (e.t == EV_LONG && e.y0 >= L.y && e.y0 < L.y + L.h) {      // hold a song: play it, or wake up to it
      int i = (int)((e.y0 - L.y + L.scroll) / L.rowH);
      if (i > 0 && i < n && !ents[i].dir) {
        L.sel = i;
        std::string p = std::string(dir) + "/" + ents[i].name;
        ask(ents[i].name, "", {"PLAY", "ALARM SONG", "CANCEL"}, [this, p, i](int k) {
          if (k == 0) open(i);
          else if (k == 1) { snprintf(cfg.alarmSong, sizeof(cfg.alarmSong), "%s", p.c_str()); cfgChanged(); toast("ALARM SONG SET"); }
        });
        return true;
      }
    }
    return L.ev(e, [this](int i) { open(i); uiDirty(); });
  }
};
Screen* makeMusic() { return new Music(); }
Screen* makeNowPlaying() { return new NowPlaying(); }

// ------------------------------------------------------------------ VIDEO: full screen
struct VideoPlay : Screen {
  std::string path; char err[48] = ""; uint32_t seen = 0, overlayAt = 0; bool started = false;
  VideoPlay(const std::string& p) : path(p) {}
  const char* name() override { return "VIDEO"; }
  bool fullScreen() override { return true; }
  bool keepAwake() override { return vidState() == MED_PLAY; }
  bool noSwipeBack() override { return true; }
  void enter() override { if (!started) { started = true; if (!vidPlay(path.c_str(), err)) {} overlayAt = msNow(); } }
  void leave() override {}
  ~VideoPlay() { vidStop(); }
  void tick() override {
    if (vidFrameSeq != seen) { seen = vidFrameSeq; uiDirty(); }
    if (msNow() - overlayAt < 3000) redrawIn(250);
    if (vidState() == MED_END) { uiBack(); }
  }
  void draw() override {
    if (vidFrame && !err[0]) memcpy(fb, vidFrame, SCR_W * SCR_H * 2); else gfxClearTo(0);
    if (err[0]) { drawTextC(FT, 220, 200, err, C_RED); drawTextC(FT, 240, 250, "error.blue/cube/video", C_DIM, TX_FLAT); return; }
    bool show = msNow() - overlayAt < 3000 || vidState() == MED_PAUSE;
    if (!show) return;
    blendRect(0, 396, 480, 84, 0, 24);
    const char* nm = strrchr(path.c_str(), '/'); char t[48]; snprintf(t, sizeof(t), "%s", nm ? nm + 1 : path.c_str()); utf8Cut(t, 20);
    drawText(FT, 16, 400, t, C_FG, TX_FLAT);
    uint32_t len = vidLenMs(), pos = vidPosMs();
    char a[16], b[16], ab[40]; fmtMs(a, pos); fmtMs(b, len); snprintf(ab, sizeof(ab), "%s / %s", a, b);
    drawTextR(FT, 464, 400, ab, C_DIM, TX_FLAT);
    meter(16, 440, 448, 20, len ? (int)((uint64_t)pos * 100 / len) : 0, true);
    if (vidState() == MED_PAUSE) { fillRect(206, 196, 22, 70, C_FG); fillRect(252, 196, 22, 70, C_FG); }
    volSliderV(410, 110, 220);
  }
  bool ev(const Ev& e) override {
    if (e.t == EV_KEY) {
      if (e.key == K_OK) vidPause(vidState() == MED_PLAY);
      else if (e.key == K_NEXT) vidSeek(vidPosMs() + 10000);
      else if (e.key == K_PREV) vidSeek(vidPosMs() > 10000 ? vidPosMs() - 10000 : 0);
      else uiBack();
      overlayAt = msNow(); uiDirty(); return true;
    }
    if (msNow() - overlayAt < 3000 || vidState() == MED_PAUSE) {
      if (volEvV(e, 410, 110, 220)) { overlayAt = msNow(); return true; }
    }
    if (e.t == EV_TAP) {
      if (e.y > 430 && vidLenMs()) { vidSeek((uint32_t)((uint64_t)(e.x - 16 < 0 ? 0 : e.x - 16) * vidLenMs() / 448)); }
      else if (msNow() - overlayAt < 3000) vidPause(vidState() == MED_PLAY);
      overlayAt = msNow(); uiDirty(); return true;
    }
    if (e.t == EV_SWIPE && e.dir == SW_RIGHT) { uiBack(); return true; }
    if (e.t == EV_SWIPE && e.dir == SW_LEFT) { vidSeek(vidPosMs() + 30000); overlayAt = msNow(); return true; }
    return false;
  }
};
void videoOpen(const char* path) {
  musStop();
  uiOpen(new VideoPlay(path));
}

// ------------------------------------------------------------------ an MP4: converted once by the phone, then it plays
// A QR code opens the cube's own page (http://IP/cv), which converts the video in the phone's browser and sends the
// AVI back next to it (then asks the cube to play it). Sent from error.blue/cube instead, it arrives the same way.
struct ConvWait : Screen {
  std::string path, twin, twinName; uint32_t seenFile;
  ConvWait(const char* p) : path(p), twin(aviTwin(p)) {
    twinName = twin.substr(twin.rfind('/') + 1);
    seenFile = S.fileAt;
    snprintf(S.convPath, sizeof(S.convPath), "%s", p);
    S.convAt = 0; S.convPct = 0; S.convMsg[0] = 0;
  }
  const char* name() override { return "VIDEO.EXE"; }
  bool keepAwake() override { return S.convAt && msNow() - S.convAt < 15000; }
  void draw() override {
    pathLine(path.compare(0, 3, "/sd") ? "C:\\VIDEO\\MP4>" : "A:\\VIDEO\\MP4>");
    const char* nm = strrchr(path.c_str(), '/'); char t[64]; snprintf(t, sizeof(t), "%s", nm ? nm + 1 : path.c_str()); utf8Cut(t, 28);
    drawTextC(FT, 240, 90, t, C_FG);
    bool net = S.net == NET_UP && !cfg.offline && S.ip[0];
    if (!net) {
      drawTextC(FT, 240, 150, "MP4 plays after one quick", C_FG, TX_FLAT);
      drawTextC(FT, 240, 180, "conversion on your phone.", C_FG, TX_FLAT);
      drawTextC(FT, 240, 236, cfg.offline ? "TURN OFFLINE MODE OFF" : "CONNECT THE CUBE TO WIFI", C_WARN);
      drawTextC(FT, 240, 290, "or on the phone:", C_DIM, TX_FLAT);
      drawTextC(FT, 240, 320, "error.blue/cube/video", C_FG, TX_FLAT);
      fkeysBack(); redrawIn(1000); return;
    }
    char url[48]; snprintf(url, sizeof(url), "http://%s/cv", S.ip);
    static uint8_t qr[cubeqr_BUFFER_LEN_FOR_VERSION(4)], tmp[cubeqr_BUFFER_LEN_FOR_VERSION(4)];
    if (cubeqr_encodeText(url, tmp, qr, cubeqr_Ecc_MEDIUM, 1, 4, cubeqr_Mask_AUTO, true)) {
      int n = cubeqr_getSize(qr), sc = 190 / (n + 4), sz = sc * (n + 4), x0 = 240 - sz / 2, y0 = 118;
      fillRect(x0, y0, sz, sz, C_FG);
      for (int y = 0; y < n; y++) for (int x = 0; x < n; x++)
        if (cubeqr_getModule(qr, x, y)) fillRect(x0 + (x + 2) * sc, y0 + (y + 2) * sc, sc, sc, C_BG);
    }
    drawTextC(FT, 240, 318, url, C_FG, TX_FLAT);
    bool busy = S.convAt && msNow() - S.convAt < 10000;
    if (busy) {
      meter(40, 352, 400, 18, S.convPct, true);
      char m[48]; snprintf(m, sizeof(m), "PHONE: %s", S.convMsg[0] ? S.convMsg : "WORKING"); utf8Cut(m, 34);
      drawTextC(FT, 240, 376, m, C_FG, TX_FLAT);
    } else {
      drawTextC(FT, 240, 350, "Scan it with your phone", C_DIM, TX_FLAT);
      drawTextC(FT, 240, 378, "(same WiFi). It converts", C_DIM, TX_FLAT);
      drawTextC(FT, 240, 406, "it once, then it plays.", C_DIM, TX_FLAT);
    }
    fkeysBack();
    redrawIn(500);
  }
  void tick() override {
    // from error.blue/cube (FILES upload): the converted file arrived
    if (S.fileAt != seenFile) {
      seenFile = S.fileAt;
      if (!strcasecmp(S.fileName, twinName.c_str()) && fsSize(twin.c_str()) > 0) { std::string t = twin; uiBack(); videoOpen(t.c_str()); return; }
    }
    // from the cube's own page: it says DONE and asks the cube to play the AVI (which opens on top of this)
    if (S.convPct >= 100 && fsSize(twin.c_str()) > 0) { S.convPct = 0; uiBack(); return; }
  }
};
// a video file: the AVI plays; an MP4 (MOV...) plays its converted twin if there is one, else asks the phone
void videoOrConvert(const char* path) {
  if (!isPhoneVideo(path)) { videoOpen(path); return; }
  std::string t = aviTwin(path);
  if (fsSize(t.c_str()) > 0) { videoOpen(t.c_str()); return; }
  // or the same name in A:\VIDEO (sent with error.blue/cube/video)
  std::string v = "/sd/VIDEO" + t.substr(t.rfind('/'));
  if (fsSize(v.c_str()) > 0) { videoOpen(v.c_str()); return; }
  uiOpen(new ConvWait(path));
}

struct Video : Screen {
  char dir[120] = "";
  FileEnt ents[160]; int n = 0;
  ListView L;
  const char* name() override { return "VIDEO.EXE"; }
  void load() {
    if (!dir[0]) { sdMount(); strcpy(dir, S.sd ? (fsSize("/sd/VIDEO") >= 0 ? "/sd/VIDEO" : "/sd") : "/c"); }
    strcpy(ents[0].name, ".."); ents[0].dir = true; n = 1;
    static FileEnt* tmp = (FileEnt*)bigAlloc(sizeof(FileEnt) * 160);
    if (!tmp) return;
    int k = fsList(dir, tmp, 159);
    for (int i = 0; i < k && n < 160; i++) {
      if (isPhoneVideo(tmp[i].name)) {          // its converted twin is in the list already: show only that
        std::string tw = aviTwin((std::string("/x/") + tmp[i].name).c_str()).substr(3);
        bool twin = false;
        for (int j = 0; j < k; j++) if (!strcasecmp(tmp[j].name, tw.c_str())) twin = true;
        if (!twin) ents[n++] = tmp[i];
      } else if (tmp[i].dir || isVideo(tmp[i].name)) ents[n++] = tmp[i];
    }
    for (int a = 1; a < n; a++) for (int b = a + 1; b < n; b++) {
      bool sw = ents[a].dir != ents[b].dir ? ents[b].dir : strcasecmp(ents[a].name, ents[b].name) > 0;
      if (sw) { FileEnt t = ents[a]; ents[a] = ents[b]; ents[b] = t; }
    }
    L.n = n; L.sel = n > 1 ? 1 : 0; L.scroll = 0; uiReveal();
  }
  void enter() override { L.setBox(15, 92, 450, 312); L.rowH = 52; if (!n) load(); }
  void open(int i) {
    if (i == 0) { if (!strcmp(dir, "/sd") || !strcmp(dir, "/c")) { strcpy(dir, !strcmp(dir, "/sd") ? "/c" : "/sd"); } else { char* s = strrchr(dir, '/'); if (s && s != dir) *s = 0; } load(); return; }
    char p[200]; snprintf(p, sizeof(p), "%s/%s", dir, ents[i].name);
    if (ents[i].dir) { snprintf(dir, sizeof(dir), "%s", p); load(); return; }
    videoOrConvert(p);
  }
  void draw() override {
    char path[40]; snprintf(path, sizeof(path), "%c:%s>", dir[1] == 's' ? 'A' : 'C', strchr(dir + 1, '/') ? strchr(dir + 1, '/') : "\\");
    for (char* q = path; *q; q++) if (*q == '/') *q = '\\';
    utf8Cut(path, 28);
    pathLine(path);
    L.n = n;
    L.draw([&](int i, int x, int y, int w, int h, bool sel) {
      FileEnt& f = ents[i];
      int ty = y + (h - LH) / 2;
      if (sel) invBar(x + 2, y + 3, w - 4, h - 6);
      uint16_t c = sel ? C_BG : C_FG; int m = sel ? TX_FLAT : TX_GLOW;
      char nm[64]; snprintf(nm, sizeof(nm), "%s%s", f.dir && i ? "\xE2\x96\x92 " : "", i == 0 ? ".." : f.name); utf8Cut(nm, 22);
      drawText(FT, x + 10, ty, nm, c, m);
      if (!f.dir) {
        char sz[16]; snprintf(sz, sizeof(sz), isPhoneVideo(f.name) ? "MP4 %.0fM" : "%.1fM", f.size / 1048576.0);
        drawTextR(FT, x + w - 10, ty, sz, sel ? C_BG : isPhoneVideo(f.name) ? C_WARN : C_DIM, TX_FLAT);
      }
    }, nullptr);
    if (n <= 1) drawTextC(FT, 240, 250, "NO VIDEOS HERE", C_DIM, TX_FLAT);
    drawText(FT, colX(0), rowY(12) + 6, "MP4? tap it, phone converts", C_DIM, TX_FLAT);
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {"R", "Refresh", [this] { load(); }}});
  }
  void tick() override { L.tick(); }
  bool ev(const Ev& e) override { return L.ev(e, [this](int i) { open(i); uiDirty(); }); }
};
Screen* makeVideo() { return new Video(); }
