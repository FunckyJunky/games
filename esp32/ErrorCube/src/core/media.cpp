// media.cpp: music (MP3, WAV) and video (MJPEG AVI with PCM sound) from files, played in the background.
// One pump (mediaPump) does a bit of work at a time: the board runs it in its own task, the simulator in its loop.
// The interface only sends requests (musPlay, vidSeek...) and reads the state, so the two never step on each other.
// MP3: minimp3 (public domain, src/lib/minimp3.h). JPEG: jpegDecode (sys.h).
#include "sys.h"
#include "gfx.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <new>
#ifdef ARDUINO
#include <Arduino.h>
#include <esp_heap_caps.h>
static void* big(size_t n) { void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); return p ? p : malloc(n); }
#else
static void* big(size_t n) { return malloc(n); }
#endif
// the decoder's 16 KB of working memory: in PSRAM, so the media task needs only a small stack (internal RAM is
// what WiFi and the secure connections to error.blue live on)
static void* mp3Scratch(size_t n) { static void* p = nullptr; if (!p) p = big(n); return p; }
#define MINIMP3_SCRATCH_PTR mp3Scratch
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#include "../lib/minimp3.h"

// ------------------------------------------------------------------ requests from the interface
enum { C_NONE, C_MPLAY, C_MSTOP, C_MSEEK, C_MPAUSE, C_MRESUME, C_VPLAY, C_VSTOP, C_VSEEK, C_VPAUSE, C_VRESUME };
static volatile int req = C_NONE;
static char reqPath[200];
static volatile uint32_t reqMs = 0;
struct Mus; static Mus& musGet();
static void ask(int c, const char* p = nullptr, uint32_t ms = 0) {
  musGet(); mediaStart();                          // the first time: the decoder's memory and its task
  for (int i = 0; i < 200 && req != C_NONE; i++) {   // the pump takes the last request first
#ifdef ARDUINO
    delay(2);
#else
    mediaPump();
#endif
  }
  if (p) snprintf(reqPath, sizeof(reqPath), "%s", p);
  reqMs = ms;
  req = c;
}

// ------------------------------------------------------------------ music
struct Mus {
  void* f = nullptr; int kind = 0;               // 1 MP3, 2 WAV
  long dataStart = 0, dataEnd = 0;
  uint32_t lenMs = 0, baseMs = 0;
  int rate = 44100, ch = 2, bits = 16, kbps = 0;
  mp3dec_t dec;
  uint8_t* in = nullptr; int inLen = 0, inPos = 0; bool eof = false, lenKnown = false;
  int16_t* pcm = nullptr; int pcmN = 0, pcmPos = 0;
  char file[200] = "", title[72] = "", artist[72] = "";
};
static Mus* musP = nullptr;                      // in PSRAM (7 KB)
static Mus& musGet() { if (!musP) { void* p = big(sizeof(Mus)); musP = new (p) Mus(); } return *musP; }
#define M musGet()
static volatile int mState = MED_STOP;
static const int INBUF = 16384;

int musState() { return mState; }
uint32_t musLenMs() { return M.lenMs; }
uint32_t musPosMs() {
  if (mState == MED_STOP || mState == MED_ERR) return 0;
  uint32_t p = M.baseMs + (uint32_t)((uint64_t)sndMediaPlayed() * 1000 / 44100);
  return M.lenMs && p > M.lenMs ? M.lenMs : p;
}
const char* musFile() { return M.file; }
const char* musTitle() { return M.title; }
const char* musArtist() { return M.artist; }
int musKbps() { return M.kbps; }
bool musPlay(const char* path) { ask(C_MPLAY, path); mState = MED_PLAY; return true; }
void musStop() { ask(C_MSTOP); }
void musPause(bool p) { ask(p ? C_MPAUSE : C_MRESUME); }
void musSeek(uint32_t ms) { ask(C_MSEEK, nullptr, ms); }

static uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
static uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t* p) { return p[0] | p[1] << 8; }

// an ID3v2 text frame into plain text (Latin-1 / UTF-16 / UTF-8 -> UTF-8, roughly)
static void id3Text(const uint8_t* d, int n, char* out, int max) {
  int o = 0;
  if (n < 1) { out[0] = 0; return; }
  int enc = d[0]; d++; n--;
  if (enc == 1 || enc == 2) {
    bool le = true; if (n >= 2 && d[0] == 0xFE && d[1] == 0xFF) { le = false; d += 2; n -= 2; } else if (n >= 2 && d[0] == 0xFF && d[1] == 0xFE) { d += 2; n -= 2; }
    if (enc == 2) le = false;
    for (int i = 0; i + 1 < n && o < max - 4; i += 2) {
      uint32_t c = le ? d[i] | d[i + 1] << 8 : d[i] << 8 | d[i + 1];
      if (!c) break;
      if (c < 0x80) out[o++] = c; else if (c < 0x800) { out[o++] = 0xC0 | c >> 6; out[o++] = 0x80 | (c & 63); }
      else { out[o++] = 0xE0 | c >> 12; out[o++] = 0x80 | ((c >> 6) & 63); out[o++] = 0x80 | (c & 63); }
    }
  } else if (enc == 3) {
    for (int i = 0; i < n && d[i] && o < max - 1; i++) out[o++] = d[i];
  } else {
    for (int i = 0; i < n && d[i] && o < max - 3; i++) { uint8_t c = d[i]; if (c < 0x80) out[o++] = c; else { out[o++] = 0xC0 | c >> 6; out[o++] = 0x80 | (c & 63); } }
  }
  out[o] = 0;
  while (o > 0 && out[o - 1] == ' ') out[--o] = 0;
}
static void id3Read(void* f, long& start) {
  uint8_t h[10];
  fsSeekF(f, 0);
  if (fsReadF(f, h, 10) != 10 || memcmp(h, "ID3", 3)) { start = 0; return; }
  uint32_t sz = (h[6] & 127) << 21 | (h[7] & 127) << 14 | (h[8] & 127) << 7 | (h[9] & 127);
  start = 10 + sz + ((h[5] & 0x10) ? 10 : 0);
  int ver = h[3];
  int n = sz > 8192 ? 8192 : sz;
  uint8_t* t = (uint8_t*)malloc(n);
  if (!t) return;
  n = fsReadF(f, t, n);
  int p = 0;
  while (p + 10 <= n) {
    if (ver == 2) {   // ID3v2.2: 3-letter ids, 6-byte headers
      if (!t[p]) break;
      uint32_t fs = t[p + 3] << 16 | t[p + 4] << 8 | t[p + 5];
      if (p + 6 + (int)fs > n) break;
      if (!memcmp(t + p, "TT2", 3)) id3Text(t + p + 6, fs, M.title, sizeof(M.title));
      if (!memcmp(t + p, "TP1", 3)) id3Text(t + p + 6, fs, M.artist, sizeof(M.artist));
      p += 6 + fs; continue;
    }
    if (!t[p]) break;
    uint32_t fs = ver >= 4 ? ((t[p + 4] & 127) << 21 | (t[p + 5] & 127) << 14 | (t[p + 6] & 127) << 7 | (t[p + 7] & 127)) : be32(t + p + 4);
    if (fs > (uint32_t)n || p + 10 + (int)fs > n) break;
    if (!memcmp(t + p, "TIT2", 4)) id3Text(t + p + 10, fs, M.title, sizeof(M.title));
    if (!memcmp(t + p, "TPE1", 4)) id3Text(t + p + 10, fs, M.artist, sizeof(M.artist));
    p += 10 + fs;
  }
  free(t);
}

static void musClose() { if (M.f) { fsClose(M.f); M.f = nullptr; } M.pcmN = M.pcmPos = 0; M.inLen = M.inPos = 0; }

static bool musOpen(const char* path) {
  musClose();
  sndMediaClear();
  snprintf(M.file, sizeof(M.file), "%s", path);
  M.title[0] = M.artist[0] = 0; M.lenMs = 0; M.baseMs = 0; M.kbps = 0; M.eof = false; M.lenKnown = false;
  if (!M.in) M.in = (uint8_t*)big(INBUF);
  if (!M.pcm) M.pcm = (int16_t*)big(MINIMP3_MAX_SAMPLES_PER_FRAME * 2 * sizeof(int16_t));
  M.f = fsOpen(path, "rb");
  if (!M.f || !M.in || !M.pcm) return false;
  fsSeekF(M.f, 0); long end = 0;
  { // the file's length
    fsSeekF(M.f, -1); end = fsTellF(M.f);           // -1: the end
  }
  const char* e = strrchr(path, '.');
  if (e && !strcasecmp(e, ".wav")) {
    M.kind = 2;
    uint8_t h[12]; fsSeekF(M.f, 0);
    if (fsReadF(M.f, h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) return false;
    long pos = 12; bool fmt = false;
    for (;;) {
      uint8_t c[8];
      fsSeekF(M.f, pos);
      if (fsReadF(M.f, c, 8) != 8) return false;
      uint32_t sz = le32(c + 4);
      if (!memcmp(c, "fmt ", 4)) {
        uint8_t fm[16]; fsReadF(M.f, fm, 16);
        if (le16(fm) != 1) return false;            // PCM only
        M.ch = le16(fm + 2); M.rate = le32(fm + 4); M.bits = le16(fm + 14); fmt = true;
        if ((M.bits != 16 && M.bits != 8) || M.ch < 1 || M.ch > 2) return false;
      } else if (!memcmp(c, "data", 4)) {
        if (!fmt) return false;
        M.dataStart = pos + 8; M.dataEnd = M.dataStart + sz; if (M.dataEnd > end) M.dataEnd = end;
        break;
      }
      pos += 8 + sz + (sz & 1);
    }
    M.kbps = M.rate * M.ch * M.bits / 1000;
    M.lenMs = (uint32_t)((uint64_t)(M.dataEnd - M.dataStart) * 1000 / (M.rate * M.ch * (M.bits / 8)));
    fsSeekF(M.f, M.dataStart);
    const char* n = strrchr(path, '/'); snprintf(M.title, sizeof(M.title), "%s", n ? n + 1 : path);
    char* d = strrchr(M.title, '.'); if (d) *d = 0;
    return true;
  }
  M.kind = 1;
  id3Read(M.f, M.dataStart);
  M.dataEnd = end;
  if (end > 128) { uint8_t t[3]; fsSeekF(M.f, end - 128); if (fsReadF(M.f, t, 3) == 3 && !memcmp(t, "TAG", 3)) M.dataEnd = end - 128; }
  fsSeekF(M.f, M.dataStart);
  mp3dec_init(&M.dec);
  if (!M.title[0]) { const char* n = strrchr(path, '/'); snprintf(M.title, sizeof(M.title), "%s", n ? n + 1 : path); char* d = strrchr(M.title, '.'); if (d) *d = 0; }
  return true;
}

static bool musRefill() {
  if (M.eof) return false;
  if (M.inPos > 0) { memmove(M.in, M.in + M.inPos, M.inLen - M.inPos); M.inLen -= M.inPos; M.inPos = 0; }
  long left = M.dataEnd - fsTellF(M.f);
  int want = INBUF - M.inLen; if (left < want) want = left > 0 ? (int)left : 0;
  int n = want > 0 ? (int)fsReadF(M.f, M.in + M.inLen, want) : 0;
  if (n <= 0) { M.eof = true; return false; }
  M.inLen += n;
  return true;
}

// the next piece of sound into M.pcm (stereo); false = the end
static bool musDecode() {
  M.pcmN = M.pcmPos = 0;
  if (M.kind == 2) {
    int bpf = M.ch * M.bits / 8, frames = 1024;
    long left = (M.dataEnd - fsTellF(M.f)) / bpf;
    if (left <= 0) return false;
    if (frames > left) frames = (int)left;
    uint8_t* raw = M.in;
    int n = (int)fsReadF(M.f, raw, frames * bpf) / bpf;
    if (n <= 0) return false;
    for (int i = 0; i < n; i++) {
      int16_t l, r;
      if (M.bits == 16) { l = (int16_t)le16(raw + i * bpf); r = M.ch == 2 ? (int16_t)le16(raw + i * bpf + 2) : l; }
      else { l = (raw[i * bpf] - 128) << 8; r = M.ch == 2 ? (raw[i * bpf + 1] - 128) << 8 : l; }
      M.pcm[i * 2] = l; M.pcm[i * 2 + 1] = r;
    }
    M.pcmN = n;
    return true;
  }
  for (int guard = 0; guard < 64; guard++) {
    if (M.inLen - M.inPos < 4096) musRefill();
    if (M.inLen - M.inPos <= 0) return false;
    mp3dec_frame_info_t info;
    int n = mp3dec_decode_frame(&M.dec, M.in + M.inPos, M.inLen - M.inPos, M.pcm, &info);
    if (!info.frame_bytes) {
      if (!musRefill()) { M.inPos = M.inLen; return false; }
      continue;
    }
    if (n > 0 && !M.lenKnown) {
      // the length: a Xing / Info frame count (VBR), else from the bit rate
      M.lenKnown = true;
      const uint8_t* fr = M.in + M.inPos;
      int lim = info.frame_bytes < 200 ? info.frame_bytes : 200;
      for (int i = 4; i + 12 < lim; i++)
        if (!memcmp(fr + i, "Xing", 4) || !memcmp(fr + i, "Info", 4)) {
          if (be32(fr + i + 4) & 1) M.lenMs = (uint32_t)((uint64_t)be32(fr + i + 8) * n * 1000 / info.hz);
          break;
        }
      if (!M.lenMs && info.bitrate_kbps) M.lenMs = (uint32_t)((uint64_t)(M.dataEnd - M.dataStart) * 8 / info.bitrate_kbps);
    }
    M.inPos += info.frame_bytes;
    if (n <= 0) continue;
    M.rate = info.hz; M.kbps = info.bitrate_kbps;
    if (info.channels == 1) for (int i = n - 1; i >= 0; i--) { M.pcm[i * 2] = M.pcm[i]; M.pcm[i * 2 + 1] = M.pcm[i]; }
    M.pcmN = n;
    return true;
  }
  return false;
}

static void musSeekNow(uint32_t ms) {
  if (!M.f) return;
  if (M.lenMs && ms > M.lenMs) ms = M.lenMs;
  long pos;
  if (M.kind == 2) { int bpf = M.ch * M.bits / 8; pos = M.dataStart + (long)((uint64_t)ms * M.rate / 1000) * bpf; }
  else { pos = M.lenMs ? M.dataStart + (long)((double)(M.dataEnd - M.dataStart) * ms / M.lenMs) : M.dataStart; mp3dec_init(&M.dec); }
  fsSeekF(M.f, pos);
  M.inLen = M.inPos = 0; M.eof = false; M.pcmN = M.pcmPos = 0;
  sndMediaClear();
  M.baseMs = ms;
}

static bool musPump() {
  if (mState != MED_PLAY || !M.f) return false;
  if (M.pcmPos >= M.pcmN) {
    if (!musDecode()) {
      if (sndMediaQueued() <= 0) { mState = MED_END; musClose(); }
      return false;
    }
  }
  int left = M.pcmN - M.pcmPos;
  int need = (int)((int64_t)left * 44100 / (M.rate ? M.rate : 44100)) + 8;
  if (sndMediaRoom() < need) return false;          // the speaker has enough for now
  sndMediaPut(M.pcm + M.pcmPos * 2, left, M.rate);
  M.pcmPos = M.pcmN;
  return true;
}

// ------------------------------------------------------------------ video: MJPEG AVI
uint16_t* vidFrame = nullptr;
volatile uint32_t vidFrameSeq = 0;
static uint16_t* vidBack = nullptr;
struct VF { uint8_t* d; uint32_t n; uint32_t t; };
struct Vid {
  void* f = nullptr;
  long movi = 0, moviEnd = 0, idx = 0, pos = 0; int idxN = 0;
  uint32_t usPerFrame = 66666, lenMs = 0, baseMs = 0, t0 = 0, pausedAt = 0;
  int w = 0, h = 0, total = 0, arate = 0, ach = 1, abits = 16, frameNo = 0;
  VF q[4]; int qn = 0;
  int16_t* au = nullptr; int auN = 0; bool auPending = false, eof = false;
} V;
static volatile int vState = MED_STOP;
int vidState() { return vState; }
uint32_t vidLenMs() { return V.lenMs; }
static uint32_t vidClock() {
  if (V.arate) return V.baseMs + (uint32_t)((uint64_t)sndMediaPlayed() * 1000 / 44100);
  return V.baseMs + ((vState == MED_PAUSE ? V.pausedAt : msNow()) - V.t0);
}
uint32_t vidPosMs() { return vState == MED_STOP ? 0 : vidClock(); }
int vidInfo(int* w, int* h, int* fps) { if (w) *w = V.w; if (h) *h = V.h; if (fps) *fps = V.usPerFrame ? 1000000 / V.usPerFrame : 0; return V.arate; }
static char vidErr[48] = "";
bool vidPlay(const char* path, char* err) {
  ask(C_VPLAY, path);
  for (int i = 0; i < 300 && req != C_NONE; i++) {
#ifdef ARDUINO
    delay(5);
#else
    mediaPump();
#endif
  }
  if (vState == MED_ERR) { if (err) strcpy(err, vidErr); return false; }
  return true;
}
void vidStop() { ask(C_VSTOP); }
void vidPause(bool p) { ask(p ? C_VPAUSE : C_VRESUME); }
void vidSeek(uint32_t ms) { ask(C_VSEEK, nullptr, ms); }

static void vidFlush() { for (int i = 0; i < V.qn; i++) free(V.q[i].d); V.qn = 0; V.auPending = false; }
static void vidClose() { vidFlush(); if (V.f) { fsClose(V.f); V.f = nullptr; } }

static bool vidOpen(const char* path) {
  vidClose();
  sndMediaClear();
  V = Vid();
  V.f = fsOpen(path, "rb");
  if (!V.f) { strcpy(vidErr, "CAN'T OPEN IT"); return false; }
  uint8_t h[12];
  if (fsReadF(V.f, h, 12) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "AVI ", 4)) { strcpy(vidErr, "NOT AN AVI (CONVERT IT FIRST)"); return false; }
  long pos = 12, fileEnd = 12 + le32(h + 4) - 4;
  bool mjpg = false; int stype = 0;
  while (pos < fileEnd + 8) {
    uint8_t c[12];
    fsSeekF(V.f, pos);
    if (fsReadF(V.f, c, 8) != 8) break;
    uint32_t sz = le32(c + 4);
    if (!memcmp(c, "LIST", 4)) {
      fsReadF(V.f, c + 8, 4);
      if (!memcmp(c + 8, "movi", 4)) { V.movi = pos + 12; V.moviEnd = pos + 8 + sz; pos += 8 + sz + (sz & 1); continue; }
      pos += 12;                                   // hdrl, strl: look inside
      continue;
    }
    uint8_t d[64]; int n = sz < 64 ? sz : 64;
    fsReadF(V.f, d, n);
    if (!memcmp(c, "avih", 4) && n >= 40) { V.usPerFrame = le32(d); V.total = le32(d + 16); V.w = le32(d + 32); V.h = le32(d + 36); }
    else if (!memcmp(c, "strh", 4) && n >= 4) stype = !memcmp(d, "vids", 4) ? 1 : !memcmp(d, "auds", 4) ? 2 : 0;
    else if (!memcmp(c, "strf", 4)) {
      if (stype == 1 && n >= 20) { V.w = le32(d + 4); int hh = (int32_t)le32(d + 8); V.h = hh < 0 ? -hh : hh; mjpg = !memcmp(d + 16, "MJPG", 4) || !memcmp(d + 16, "mjpg", 4); }
      if (stype == 2 && n >= 16 && le16(d) == 1) { V.ach = le16(d + 2); V.arate = le32(d + 4); V.abits = le16(d + 14); }
    } else if (!memcmp(c, "idx1", 4)) { V.idx = pos + 8; V.idxN = sz / 16; }
    pos += 8 + sz + (sz & 1);
  }
  if (!V.movi) { strcpy(vidErr, "NO VIDEO IN THE FILE"); return false; }
  if (!mjpg) { strcpy(vidErr, "ONLY MJPEG AVI (CONVERT IT)"); return false; }
  if (V.abits != 16 || V.ach < 1 || V.ach > 2) V.arate = 0;
  if (!V.usPerFrame) V.usPerFrame = 66666;
  V.lenMs = (uint32_t)((uint64_t)V.total * V.usPerFrame / 1000);
  if (!vidFrame) vidFrame = (uint16_t*)big(SCR_W * SCR_H * 2);
  if (!vidBack) vidBack = (uint16_t*)big(SCR_W * SCR_H * 2);
  if (!V.au) V.au = (int16_t*)big(16384 * 2 * sizeof(int16_t));
  if (!vidFrame || !vidBack || !V.au) { strcpy(vidErr, "NO MEMORY"); return false; }
  memset(vidFrame, 0, SCR_W * SCR_H * 2); memset(vidBack, 0, SCR_W * SCR_H * 2);
  V.pos = V.movi; V.t0 = msNow();
  return true;
}

// read the next chunk: audio straight to the speaker (or kept until there's room), video into the queue
static bool vidRead() {
  if (V.eof || V.qn >= 4) return false;
  if (V.auPending) {
    int need = (int)((int64_t)V.auN * 44100 / V.arate) + 8;
    if (sndMediaRoom() < need) return false;
    sndMediaPut(V.au, V.auN, V.arate); V.auPending = false;
    return true;
  }
  for (int guard = 0; guard < 8; guard++) {
    if (V.pos + 8 > V.moviEnd) { V.eof = true; return false; }
    uint8_t c[12];
    fsSeekF(V.f, V.pos);
    if (fsReadF(V.f, c, 8) != 8) { V.eof = true; return false; }
    uint32_t sz = le32(c + 4);
    if (!memcmp(c, "LIST", 4)) { V.pos += 12; continue; }          // 'rec ' groups: look inside
    long next = V.pos + 8 + sz + (sz & 1);
    if (c[2] == 'w' && c[3] == 'b' && V.arate) {
      int bpf = V.ach * 2, frames = sz / bpf; if (frames > 16384) frames = 16384;
      uint8_t* raw = (uint8_t*)V.au;
      if (V.ach == 1) {                                             // mono: read into the second half, spread to stereo
        fsReadF(V.f, raw + frames * 2, frames * 2);
        int16_t* m = (int16_t*)(raw + frames * 2);
        for (int i = 0; i < frames; i++) { V.au[i * 2] = m[i]; V.au[i * 2 + 1] = m[i]; }
      } else fsReadF(V.f, raw, frames * 4);
      V.auN = frames; V.auPending = true;
      V.pos = next;
      int need = (int)((int64_t)frames * 44100 / V.arate) + 8;
      if (sndMediaRoom() >= need) { sndMediaPut(V.au, frames, V.arate); V.auPending = false; }
      return true;
    }
    if ((c[2] == 'd' && (c[3] == 'c' || c[3] == 'b')) && sz > 0 && sz < 600000) {
      uint8_t* d = (uint8_t*)big(sz);
      if (!d) { V.pos = next; return true; }
      fsReadF(V.f, d, sz);
      V.q[V.qn++] = {d, sz, (uint32_t)((uint64_t)V.frameNo * V.usPerFrame / 1000)};
      V.frameNo++;
      V.pos = next;
      return true;
    }
    V.pos = next;                                                   // something else: skip it
  }
  return true;
}

static bool vidPump() {
  if (vState != MED_PLAY || !V.f) return false;
  uint32_t clk = vidClock();
  if (V.qn && V.q[0].t <= clk + 15) {
    VF f = V.q[0];
    memmove(V.q, V.q + 1, sizeof(VF) * (V.qn - 1)); V.qn--;
    bool late = f.t + 160 < clk && V.qn > 0;                       // behind: skip this picture to catch up
    if (!late && jpegDecode(f.d, f.n, vidBack, SCR_W, SCR_H, IMG_FIT)) {
      uint16_t* t = vidFrame; vidFrame = vidBack; vidBack = t;
      vidFrameSeq++;
    }
    free(f.d);
    return true;
  }
  bool audioLow = V.arate && sndMediaQueued() < 44100 * 4 / 10;
  if (V.qn < 4 && (audioLow || !V.arate || V.qn < 2)) { if (vidRead()) return true; }
  if (V.eof && !V.qn && !V.auPending && (!V.arate || sndMediaQueued() <= 0)) { vState = MED_END; vidClose(); }
  return false;
}

static void vidSeekNow(uint32_t ms) {
  if (!V.f) return;
  if (V.lenMs && ms + 500 >= V.lenMs) ms = V.lenMs > 1500 ? V.lenMs - 1500 : 0;   // past the end: the last second
  int target = (int)((uint64_t)ms * 1000 / V.usPerFrame);
  long at = V.movi; int frame = 0;
  if (V.idx && V.idxN) {
    // the index: the target frame's chunk (offsets from 'movi', or from the file's start)
    uint8_t e[16]; long base = -1; int vids = 0;
    for (int i = 0; i < V.idxN; i++) {
      fsSeekF(V.f, V.idx + i * 16);
      if (fsReadF(V.f, e, 16) != 16) break;
      uint32_t off = le32(e + 8);
      if (base < 0) base = off < (uint32_t)V.movi ? V.movi - 4 : 0;
      if (e[2] == 'd') { if (vids == target) { at = base + off; frame = vids; break; } vids++; }
    }
  } else { target = 0; ms = 0; }
  vidFlush();
  sndMediaClear();
  V.pos = at; V.frameNo = frame; V.eof = false;
  V.baseMs = (uint32_t)((uint64_t)frame * V.usPerFrame / 1000);
  V.t0 = msNow();
  (void)ms;
}

// ------------------------------------------------------------------ the pump
bool mediaPumpWork();
void mediaPump() { mediaPumpWork(); }
bool mediaPumpWork() {
  int r = req;
  if (r != C_NONE) {
    switch (r) {
      case C_MPLAY:
        if (vState != MED_STOP) { vidClose(); vState = MED_STOP; }
        mState = musOpen(reqPath) ? MED_PLAY : MED_ERR;
        if (mState == MED_ERR) musClose();
        break;
      case C_MSTOP: musClose(); sndMediaClear(); mState = MED_STOP; M.file[0] = 0; break;
      case C_MSEEK: musSeekNow(reqMs); if (mState == MED_END) mState = MED_PLAY; break;
      case C_MPAUSE: if (mState == MED_PLAY) { mState = MED_PAUSE; sndMediaPause(true); } break;
      case C_MRESUME: if (mState == MED_PAUSE) { mState = MED_PLAY; sndMediaPause(false); } break;
      case C_VPLAY:
        if (mState != MED_STOP) { musClose(); mState = MED_STOP; M.file[0] = 0; }
        vState = vidOpen(reqPath) ? MED_PLAY : MED_ERR;
        if (vState == MED_ERR) vidClose();
        break;
      case C_VSTOP: vidClose(); sndMediaClear(); vState = MED_STOP; break;
      case C_VSEEK: vidSeekNow(reqMs); if (vState == MED_END) vState = MED_PLAY; break;
      case C_VPAUSE: if (vState == MED_PLAY) { vState = MED_PAUSE; V.pausedAt = msNow(); sndMediaPause(true); } break;
      case C_VRESUME: if (vState == MED_PAUSE) { vState = MED_PLAY; V.t0 += msNow() - V.pausedAt; sndMediaPause(false); } break;
    }
    req = C_NONE;
    return true;
  }
  bool w = musPump();
  w = vidPump() || w;
  return w;
}
