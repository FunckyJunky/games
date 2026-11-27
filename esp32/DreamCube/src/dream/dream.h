// dream.h: DREAM-CUBE's own parts and what they share.
//   mic.cpp       the two microphones (ES7210, shared with EAR-CUBE): 16 kHz, "someone speaks", pieces at the pauses
//   listen.cpp    the pieces to error.blue (speech to text), the words gathered, a scene asked for now and then,
//                 the scenes kept (to go back), the control page's commands
//   ../net/enet.cpp  error.blue: speech to text, scenes, the control page, updates, the way back to ERROR-CUBE
//   r3d.cpp models.cpp scene.cpp  the 3D (world.h)
//   screens.cpp   the cube's screens        web.cpp   the page on the cube's own WiFi
// Plain C++ apart from mic.cpp / listen.cpp / web.cpp / enet.cpp: the screens also run in the PC simulator.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "dream_config.h"
#include "world.h"

#define EAR_RATE 16000

// ------------------------------------------------------------------ settings (NVS "dream"; ERROR-CUBE's cfg stays as it is)
struct DreamCfg {
  uint32_t magic = 0xD8EA0001;
  uint8_t lang = 0;          // LANGS[]: 0 = find out by itself
  uint8_t sens = 1;          // what counts as speech: 0 LOW, 1 NORMAL, 2 HIGH, 3 EVERYTHING
  uint8_t gain = 10;         // microphone gain (ES7210 steps, 10 = 30 dB)
  uint8_t mics = 0;          // 0 both, 1 left, 2 right
  uint8_t pause = 0;         // a pause this long ends a piece: 0 SHORT, 1 NORMAL, 2 LONG
  uint8_t listen = 1;        // listening (else paused)
  uint8_t often = 1;         // a new scene: 0 OFTEN, 1 NORMAL, 2 CALM
  uint8_t mind = 0;          // 0 QUICK (the small AI), 1 SMART (the big one, slower)
  uint8_t words = 1;         // what it heard, small at the bottom
  uint8_t title = 1;         // the scene's name at the top
  uint8_t cam = 2;           // the camera turns: 0 STILL, 1 SLOW, 2 NORMAL, 3 FAST
  uint8_t soft = 1;          // smooth pixels (else sharp)
  uint8_t detail = 1;        // the place's own trees, houses... (0 = fewer, faster)
  uint8_t wake = 1;          // a new scene turns the screen on
  uint8_t hotspot = 1;       // the cube's own WiFi: always on
  char apName[24] = "";      // "" = DREAM-CUBE-XXXX
  char apPass[24] = "";      // "" = open; else 8 or more characters
  uint8_t link = 1;          // the control page at error.blue/dream
  char code[9] = "";         // its link code (made at the first start)
  uint8_t fps = 0;           // show the frame rate (for testing)
  // new settings go here, at the end
};
extern DreamCfg dcfg;
void dreamCfgLoad();
void dreamCfgSave();
struct Lang { const char* code; const char* name; };
extern const Lang LANGS[];
extern const int N_LANGS;
const char* gainLabel(int step);
void dreamApName(char* out, size_t n);       // the hotspot's name now
void dreamNewCode();                         // a new link code (the old page link stops working)

// ------------------------------------------------------------------ mic.cpp (shared with EAR-CUBE)
bool micBegin();
bool micOk();
void micGain(int step);
void micRun(int mode);                       // 0 just levels, 1 short pieces, 2 long pieces
void micFlush();
int micLevel(int ch);
float micDb();
float micFloorDb();
float micGateDb();
bool micSpeech();
uint32_t micPos();
int micCopy(uint32_t from, int16_t* out, int n);
struct Piece { uint32_t from, to, speechMs; uint8_t flush; };
bool micPiece(Piece& p, uint32_t waitMs);

// ------------------------------------------------------------------ listen.cpp
void dreamBegin();
void dreamLoop();                            // the main loop: commands from the page, settings applied
void dreamListen(bool on);                   // listen / pause (saved)
bool dreamListening();
enum { ST_READY, ST_LISTEN, ST_HEAR, ST_SEND, ST_THINK, ST_OFFLINE, ST_LIMIT, ST_ERROR, ST_NOMIC, ST_PAUSED };
int dreamStatus();
const char* dreamStatusText(int st);
const char* dreamErr();                      // the last problem with error.blue
void dreamSay(const char* text);             // typed words: a scene for them, now
const char* dreamHeard();                    // the last words heard (for the screen)
uint32_t dreamHeardAt();                     // when (ms)
int dreamWordsWaiting();                     // words heard since the last scene
bool dreamThinking();                        // a scene is being made on error.blue
uint32_t dreamScenes();                      // scenes made since the start
// the scenes kept (40, newest first): browse back with PREV / NEXT
int dreamCount();
int dreamPos();                              // the one shown: 0 = the newest
void dreamShowAt(int pos);
bool dreamTitleAt(int pos, char* out, size_t n, char* when = nullptr, size_t wn = 0);
bool dreamPoll(SceneDef& d, bool& fresh);    // the main loop: a scene to show now (fresh = new, not browsing)
// for enet.cpp
struct SttJob { const uint8_t* wav; size_t len; const char* lang; uint32_t id; };
bool scribeNext(SttJob& j);
void scribeDone(uint32_t id, int code, const char* text, const char* lang, const char* err);
bool scribeWants();
int scribeWaiting();
bool dreamWants();                           // enough words for a new scene (and nothing on its way)
bool dreamNext(char* text, size_t n, char* prev, size_t pn, char* lang, size_t ln);
void dreamDone(int code, const char* json, const char* err);
void dreamCmd(int n, const char* c, const char* a);   // from the control page (the net task)
void dreamState(char* out, size_t n);        // how the cube is, as JSON (for the control page)
// ------------------------------------------------------------------ ../net/enet.cpp
void enetSwitch();                           // back to ERROR-CUBE (S.upd / S.updMsg like an update)
extern volatile bool enetSwitching;
uint32_t enetLinkAt();                       // when the control page was last reached (ms, 0 = never)
uint32_t enetDreamMs();                      // how long the last scene took
