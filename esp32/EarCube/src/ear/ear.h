// ear.h: EAR-CUBE's own parts and what they share.
//   mic.cpp       the two microphones (ES7210): 16 kHz mono, levels, "is someone speaking", cut into pieces at pauses
//   scribe.cpp    LIVE and RECORD: the pieces waiting for error.blue, the text that comes back, transcripts on the card
//   ../net/enet.cpp  error.blue: speech to text (/api/stt), updates, the way back to ERROR-CUBE
//   screens.cpp   the cube's screens        web.cpp   the page on the cube's own WiFi (and on the home WiFi)
// Plain C++ apart from mic.cpp / scribe.cpp / web.cpp / enet.cpp: the screens also run in the PC simulator (sim/simear.cpp).
#pragma once
#include <stdint.h>
#include <stddef.h>

#define EAR_RATE 16000                 // what goes to error.blue: 16 kHz, mono, 16 bit (Whisper's own rate)

// ------------------------------------------------------------------ settings (NVS "ear"; ERROR-CUBE's cfg stays as it is)
struct EarCfg {
  uint32_t magic = 0xEA700001;
  uint8_t lang = 0;          // LANGS[]: 0 = find out by itself
  uint8_t task = 0;          // 0 write it as spoken, 1 write it in English (Whisper translates)
  uint8_t sens = 1;          // what counts as speech: 0 LOW (loud only), 1 NORMAL, 2 HIGH, 3 EVERYTHING
  uint8_t gain = 10;         // microphone gain, ES7210 steps: 0..14 = 0, 3, 6 ... 33, 34.5, 36, 37.5 dB (10 = 30 dB)
  uint8_t mics = 0;          // 0 both, 1 left, 2 right
  uint8_t pause = 1;         // a pause this long ends a piece: 0 SHORT, 1 NORMAL, 2 LONG
  uint8_t keepAudio = 0;     // RECORD: keep the sound too (a WAV next to the text, TF card only)
  uint8_t stamps = 1;        // a time in front of every line
  uint8_t liveKeep = 0;      // LIVE: also save what it hears (A:\EAR\LIVE-date.txt)
  uint8_t hotspot = 1;       // the cube's own WiFi: always on
  char apName[24] = "";      // "" = EAR-CUBE-XXXX
  char apPass[24] = "";      // "" = open; else 8 or more characters
  uint8_t liveLines = 1;     // LIVE text: 0 small (more lines), 1 normal
  // new settings go here, at the end
};
extern EarCfg ecfg;
void earCfgLoad();
void earCfgSave();
struct Lang { const char* code; const char* name; };
extern const Lang LANGS[];
extern const int N_LANGS;
const char* gainLabel(int step);       // "30 dB"
void earApName(char* out, size_t n);   // the hotspot's name now

// ------------------------------------------------------------------ mic.cpp
bool micBegin();                       // ES7210 set up, the task runs (false = no microphones found)
bool micOk();
void micGain(int step);                // ES7210 gain (ecfg.gain)
void micRun(int mode);                 // 0 just levels, 1 LIVE (short pieces), 2 RECORD (long pieces)
void micFlush();                       // stop: the piece so far, then a FLUSH marker, then nothing more (mode 0)
int micLevel(int ch);                  // 0..100: 0 = what's used, 1 left, 2 right (peak, falls slowly)
float micDb();                         // the sound now (dBFS, -90..0)
float micFloorDb();                    // the background noise it learnt
float micGateDb();                     // above this counts as speech
bool micSpeech();                      // someone is speaking now
uint32_t micPos();                     // 16 kHz samples since the start (the ring's write position)
int micCopy(uint32_t from, int16_t* out, int n);   // samples from the ring; -1 = already overwritten
struct Piece { uint32_t from, to, speechMs; uint8_t flush; };
bool micPiece(Piece& p, uint32_t waitMs);          // the next finished piece

// ------------------------------------------------------------------ scribe.cpp
enum { EAR_IDLE, EAR_LIVE, EAR_REC };
void scribeBegin();
void earWant(int mode);                // start / stop (from anywhere: the screens, the web page); EAR_IDLE = stop
int earMode();                         // what runs now
const char* earStartError();           // why the last start didn't happen ("" = it did)
uint32_t earSecs();                    // how long this session has been running
const char* earFile();                 // the transcript being written (RECORD; LIVE with SAVE LIVE TEXT), "" = none
const char* earLastFile();             // the last one written
int earWaiting();                      // pieces waiting for error.blue
int earDoneCount();                    // pieces written in this session
uint32_t earWords();                   // words in this session
bool earSending();                     // a piece is on its way to error.blue now
enum { ST_READY, ST_LISTEN, ST_HEAR, ST_SEND, ST_OFFLINE, ST_LIMIT, ST_ERROR, ST_NOMIC };
int earStatus();                       // for the screen and the page
const char* earStatusText(int st);
const char* earErr();                  // the last problem with error.blue
// the text that came back (LIVE and RECORD), newest last
struct EarLine { uint32_t n; char at[12]; char text[500]; };
uint32_t earLineSeq();                 // the newest line's number (0 = none)
int earLines(EarLine* out, int max, uint32_t after);   // the lines after number `after` (oldest first)
void earClearLines();
bool earSaveLive(char* path, size_t n);   // LIVE: the lines on screen into a new transcript
// transcripts: A:\EAR (TF card) and C:\ear
struct EarFile { char path[96]; char name[48]; uint32_t size; bool hasWav; bool live; };
int earFiles(EarFile* out, int max);   // newest first
const char* earDir();                  // where a new one goes ("/sd/EAR" or "/c/ear")
void earNiceName(const char* file, char* out, size_t n);   // "2026-11-26_22-55.txt" -> "26 NOV 2026 22:55"
// ------------------------------------------------------------------ ../net/enet.cpp
void enetSwitch();                     // back to ERROR-CUBE: download it, restart in it (S.upd / S.updMsg like an update)
extern volatile bool enetSwitching;
uint32_t enetSttMs();                  // how long error.blue took for the last piece

// for enet.cpp: the next piece for error.blue, and what came back
struct SttJob { const uint8_t* wav; size_t len; const char* lang; uint8_t task; uint32_t id; };
bool scribeNext(SttJob& j);
void scribeDone(uint32_t id, int code, const char* text, const char* err);
bool scribeWants();                    // something waits (and it's time to try again)
