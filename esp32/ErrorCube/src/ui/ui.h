// ui.h: the ERROR-DOS interface on the cube: screens, touch + buttons, lists, the keyboard, dialogs,
// the quick panel, screen off / dim / always-on clock, glitches. Plain C++ (also runs in sim/).
#pragma once
#include <functional>
#include <vector>
#include <initializer_list>
#include <string>
#include <ctype.h>
#include "../core/gfx.h"
#include "../core/sys.h"

// ------------------------------------------------------------------ input
enum { EV_TAP = 1, EV_LONG, EV_DOWN, EV_MOVE, EV_UP, EV_SWIPE, EV_KEY };
// buttons: - = PREV, + = NEXT, hold + = OK, hold - = BACK, PWR short / long
enum { K_PREV = 1, K_NEXT, K_OK, K_BACK, K_PWR, K_PWR_LONG };
enum { SW_LEFT = 1, SW_RIGHT, SW_UP, SW_DOWN };
struct Ev { uint8_t t = 0; int16_t x = 0, y = 0, x0 = 0, y0 = 0; uint8_t dir = 0, key = 0; uint32_t ms = 0; };

void uiTouch(bool down, int x, int y);   // raw touch from the board (screen coordinates after rotation)
void uiKey(int key);                     // a button

// ------------------------------------------------------------------ layout: a text grid of 30 x 14 cells of 15 x 32
#define GX 15
#define GY 16
#define CW 15
#define LH 32
#define COLS 30
#define ROWS 14
static inline int colX(int c) { return GX + c * CW; }
static inline int rowY(int r) { return GY + r * LH; }
#define TOP_Y 56          // under the status bar
#define BOT_Y 430         // the F-key bar starts here

typedef std::function<void()> Act;

struct Screen {
  virtual ~Screen() {}
  virtual const char* name() = 0;        // "CLOCK.EXE": the status bar and the prompt
  virtual void enter() {}
  virtual void leave() {}
  virtual void draw() = 0;               // everything under the status bar
  virtual void tick() {}                 // every loop (about 30 times a second)
  virtual bool ev(const Ev&) { return false; }
  virtual bool fullScreen() { return false; }   // no status bar
  virtual bool keepAwake() { return false; }    // the screen doesn't go off by itself
  virtual bool noSwipeBack() { return false; }  // a swipe to the right isn't BACK here (PAINT)
};

void uiInit();
void uiStart();                          // after the boot screen
void uiTransition(int dir);              // the cube turns to the next picture: 0 up, 1 down, 2 left, 3 right
void uiReveal();                         // list rows type themselves in again
uint32_t uiRevealAt();
void uiReplaceRoot(Screen* s);           // a new menu at the bottom of the stack (the menu view changed)
void uiGlitch(int kind, uint32_t ms);    // a glitch now (-1 = any)
void uiOpen(Screen* s);
void uiBack();
void uiHome();
Screen* uiCurrent();
void uiDirty();
void redrawIn(uint32_t ms);              // draw again within ms (animations, blinking cursor)
bool uiFrame();                          // true = fb has a new picture for the panel
void uiLoop();                           // timers: screen off, alarm, glitches, notifications

// ------------------------------------------------------------------ screen on / off
bool uiAwake();                          // false = panel off (or the dim always-on clock)
bool uiAod();
void uiWake();
void uiSleep();
void uiActivity();
int uiBrightNow();                       // what the panel should show now (0 = off)
extern bool crtAnim;                     // the board plays the CRT on/off animation (cfg.crt)
extern int8_t crtWant;                   // 1 = play power-on, -1 = power-off, 0 = nothing

// ------------------------------------------------------------------ pieces to draw with
struct FKey { const char* k; const char* label; Act act; };
void hit(int x, int y, int w, int h, Act a, Act longAct = nullptr, bool focusable = true);
void statusBar(const char* title);
void pathLine(const char* path);         // "C:\SETUP\DISPLAY>" under the status bar
void fkeys(std::initializer_list<FKey> keys);
void fkeysBack();                        // just "ESC Back"
void toast(const char* msg, uint32_t ms = 2600);
void putText(int r, int c, const char* s, uint16_t col = 0, int mode = TX_GLOW);
void invBar(int x, int y, int w, int h, bool glow = true);
void meter(int x, int y, int w, int h, int pct, bool active);   // ████░░░ bar
void wifiIcon(int x, int y, int rssi, bool on);
void battIcon(int x, int y, int pct, bool chg);
int wrapText(const char* s, int width, char lines[][64], int maxLines);
void fmtTime(char* out, int h, int m, int s, bool secs);
void dosButton(int x, int y, int w, int h, const char* label, Act a, bool on = false, int f = FT);
void sndKey();

// ------------------------------------------------------------------ list (menus, settings, files...)
// Rows of 56 px; drag to scroll, tap to pick, - / + buttons move, hold + = OK.
struct ListView {
  int sel = 0, n = 0;
  float barY = -1;                       // the selection bar slides to the selected row
  float scroll = 0, vel = 0;
  int x = 15, y = 96, w = 450, h = 330, rowH = 56;
  bool dragging = false; int dragY0 = 0; float scroll0 = 0; uint32_t lastMove = 0; int lastY = 0;
  void setBox(int X, int Y, int W, int H) { x = X; y = Y; w = W; h = H; }
  void clampScroll();
  void showSel();
  // draw the rows: row(i, x, y, w, h, selected)
  void draw(std::function<void(int i, int x, int y, int w, int h, bool sel)> row, std::function<void(int i)> pick,
            std::function<void(int i, int dx)> side = nullptr);
  bool ev(const Ev& e, std::function<void(int i)> pick);
  void tick();
};

// a settings row: name and value, < > to change
struct Row {
  const char* name;
  std::function<void(char* out)> val;    // value text (nullptr = an action row)
  std::function<void(int d)> act;        // d = -1 / +1 (tap left / right of the value), 0 = tap / OK
  uint8_t kind = 0;                      // 0 action, 1 choice < >, 2 slider 0..100, 3 info only, 4 heading
  std::function<int()> pct = nullptr;    // slider value
  std::function<void(int)> setPct = nullptr;
};
void drawRows(ListView& L, std::vector<Row>& rows);
bool rowsEv(ListView& L, std::vector<Row>& rows, const Ev& e);

// ------------------------------------------------------------------ overlays
void kbOpen(const char* title, const char* init, int maxLen, std::function<void(const char*)> done, bool password = false);
bool kbIsOpen();
void ask(const char* title, const char* text, std::vector<const char*> buttons, std::function<void(int)> cb);
void quickOpen();
void powerMenu();
void alarmRing(const char* what);

// ------------------------------------------------------------------ programs (apps*.cpp)
struct Prog { const char* dos; const char* desc; int folder; Screen* (*make)(); };
extern std::vector<Prog> PROGS;
enum { F_APPS, F_ONLINE, F_SYSTEM, F_WEB };
void appsRegister();
void appsLoop();                         // the control page asked to open a web app / video
Screen* makeWebApp(const char* id, const char* name);
Screen* makeLive();
Screen* bootScreen();
Screen* introScreen();
Screen* makeHome3D();
Screen* homeScreen();
Screen* aodScreen();
Screen* findProg(const char* dos);
void openProg(const char* dos);
void clockTick();                        // alarm / timer, also while another program is open
extern uint32_t tmrEnd, tmrLeft;     // CLOCK's timer
extern bool tmrRun;
void cfgChanged();                       // apply look/brightness after a settings change, then save
const char* bootLine(int i);
