// ERROR-OLED screens. See ui.h. Everything here runs in loop(), one thing at a time.
#include "ui.h"
#include "config.h"
#ifdef ARDUINO
#include <U8g2lib.h>
#else
#include <clib/u8g2.h>
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <strings.h>
#include <math.h>

#define OLED_VERSION "1.0"
#ifndef SERVER_NAME
#define SERVER_NAME "ERROR.BLUE"
#endif

// ======================================================================= UTF-8
static uint32_t u8next(const char*& p) {
  uint8_t c = (uint8_t)*p;
  if (!c) return 0;
  p++;
  if (c < 0x80) return c;
  int n = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
  if (!n) return 0xFFFD;
  uint32_t cp = c & (0x3F >> n);
  for (int i = 0; i < n; i++) {
    if (((uint8_t)*p & 0xC0) != 0x80) return 0xFFFD;
    cp = (cp << 6) | ((uint8_t)*p++ & 0x3F);
  }
  return cp;
}
static int u8len(const char* s) { int n = 0; while (*s) { u8next(s); n++; } return n; }
static int u8off(const char* s, int cps) { const char* p = s; while (cps-- > 0 && *p) u8next(p); return p - s; }
static int u8enc(char* o, uint32_t cp) {
  if (cp < 0x80) { o[0] = cp; return 1; }
  if (cp < 0x800) { o[0] = 0xC0 | (cp >> 6); o[1] = 0x80 | (cp & 0x3F); return 2; }
  o[0] = 0xE0 | (cp >> 12); o[1] = 0x80 | ((cp >> 6) & 0x3F); o[2] = 0x80 | (cp & 0x3F); return 3;
}
static void scopy(char* d, size_t n, const char* s) {     // UTF-8 safe copy
  if (!n) return;
  size_t l = strlen(s);
  if (l >= n) { l = n - 1; while (l && ((uint8_t)s[l] & 0xC0) == 0x80) l--; }
  memcpy(d, s, l); d[l] = 0;
}
static void upcase(char* s) { for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s -= 32; }
static bool ieq(const char* a, const char* b) {
  for (; *a && *b; a++, b++) { char x = *a, y = *b; if (x >= 'a' && x <= 'z') x -= 32; if (y >= 'a' && y <= 'z') y -= 32; if (x != y) return false; }
  return !*a && !*b;
}

// ======================================================================= state
static Sys* S;
static u8g2_t* G;
#define CFG (S->cfg())
static int W, H, COLS, ROWS, CW, CH, ASC, X0, Y0, spareX, spareY;
#define MAXC 32
#define MAXR 10
static uint32_t gc[MAXR][MAXC];
static uint8_t ga[MAXR][MAXC];
enum : uint8_t { A_INV = 1 };

enum Scr : uint8_t { SC_BOOT, SC_WHO, SC_INFO, SC_MENU, SC_DOS, SC_CHAT, SC_QUICK, SC_WIFI, SC_BT, SC_SETUP, SC_INPUT, SC_LOG, SC_PORTAL, SC_INTRO };
static uint8_t stk[10];
static int sp = 0;
static Scr cur() { return (Scr)stk[sp]; }

static bool dirty = true;
static uint32_t now = 0, lastDraw = 0, lastInput = 0;
static bool kbOn = false, langUa = false;
// the 3D welcome (see "the 3D welcome" below)
static uint32_t introAt = 0, introLast = 0;
static bool introReplay = false, starsReady = false;
static void introEnd();           // a Bluetooth keyboard is connected / types Cyrillic

// what error.blue told us
static OnlineSnap onA, onB;
static OnlineSnap* on = &onA;
static bool onHave = false;
static uint32_t onAt = 0;
static int onErr = 0;
static OnEvents onEv;
static ChatLog chat;
static int chatErr = 0;
static uint32_t tOnline = 0, tChat = 0, tBeacon = 0;
static bool tOnlineSet = false;

// pending chat messages (sent, waiting for the server)
static char pend[4][120];
static int npend = 0;
static uint32_t sayRetryAt = 0;

// toasts
static char toastQ[4][40];
static int toastN = 0;
static uint32_t toastAt = 0;

// glitches
static uint32_t nextGlitch = 20000, glitchUntil = 0;
static uint8_t glitchKind = 0, glitchSeed = 0;

// mouse pointer
static int px = 20, py = 20, pAccX = 0, pAccY = 0;
static uint32_t pAt = 0;
static uint8_t pButtons = 0;

// LED blink
static uint32_t ledUntil = 0;

static void redraw() { dirty = true; }

// ======================================================================= the text grid
static void layout() {
  W = u8g2_GetDisplayWidth(G);
  H = u8g2_GetDisplayHeight(G);
  bool big = CFG.font == 1;
  u8g2_SetFont(G, big ? u8g2_font_5x8_t_cyrillic : u8g2_font_4x6_t_cyrillic);
  u8g2_SetFontMode(G, 1);
  CW = big ? 5 : 4;
  CH = big ? 8 : 6;
  ASC = u8g2_GetAscent(G);
  COLS = W / CW; if (COLS > MAXC) COLS = MAXC;
  ROWS = H / CH; if (ROWS > MAXR) ROWS = MAXR;
  spareX = W - COLS * CW;
  spareY = H - ROWS * CH;
  X0 = spareX / 2;
  Y0 = spareY / 2;
}
static void gclear() { for (int r = 0; r < MAXR; r++) for (int c = 0; c < MAXC; c++) { gc[r][c] = ' '; ga[r][c] = 0; } }
static void gcell(int r, int c, uint32_t cp, uint8_t a) { if (r >= 0 && r < ROWS && c >= 0 && c < COLS) { gc[r][c] = cp; ga[r][c] = a; } }
// writes text at row r, column c; at most maxw cells; returns the column after it
static int gput(int r, int c, const char* s, uint8_t a = 0, int maxw = MAXC) {
  while (*s && maxw-- > 0) { uint32_t cp = u8next(s); gcell(r, c, cp, a); c++; }
  return c;
}
static void gattr(int r, uint8_t a, int c0 = 0, int c1 = MAXC) { for (int c = c0; c < c1 && c < COLS; c++) if (r >= 0 && r < ROWS) ga[r][c] = a; }
static void gright(int r, const char* s, uint8_t a = 0, int endCol = -1) {
  if (endCol < 0) endCol = COLS;
  int n = u8len(s);
  if (n > endCol) { s += u8off(s, n - endCol); n = endCol; }
  gput(r, endCol - n, s, a);
}
static void gcenter(int r, const char* s, uint8_t a = 0) { int n = u8len(s); gput(r, n >= COLS ? 0 : (COLS - n) / 2, s, a, COLS); }

static uint32_t fallback(uint32_t cp) {
  if (cp == 0x491) return 0x433;  // ґ -> г
  if (cp == 0x490) return 0x413;
  if (cp >= 0xC0 && cp <= 0xFF) {
    static const char base[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
    return (uint8_t)base[cp - 0xC0];
  }
  if (cp == 0x2018 || cp == 0x2019 || cp == 0x2BC) return '\'';
  if (cp == 0x201C || cp == 0x201D) return '"';
  if (cp == 0x2013 || cp == 0x2014) return '-';
  if (cp == 0x2026) return '.';
  return '?';
}

static void drawGrid() {
  // inverted cells first; they reach 1 pixel up so the letters don't touch the edge
  u8g2_SetDrawColor(G, 1);
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if (ga[r][c] & A_INV) {
        bool up = r == 0 || !(ga[r - 1][c] & A_INV);
        int x = X0 + c * CW, y = Y0 + r * CH - (up ? 1 : 0);
        u8g2_DrawBox(G, x, y < 0 ? 0 : y, CW, CH + (up && y >= 0 ? 1 : 0));
      }
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++) {
      int x = X0 + c * CW, y = Y0 + r * CH;
      uint32_t cp = gc[r][c];
      bool inv = ga[r][c] & A_INV;
      if (cp != ' ') {
        if (cp > 0xFFFF || !u8g2_IsGlyph(G, (uint16_t)cp)) cp = fallback(cp);
        u8g2_SetDrawColor(G, inv ? 0 : 1);
        u8g2_DrawGlyph(G, x, y + ASC, (uint16_t)cp);
      }
    }
  u8g2_SetDrawColor(G, 1);
}

// little pictures drawn in the status bar (x = left edge, y = top of row 0)
static void icon(int x, int y, const char* rows, int color) {
  u8g2_SetDrawColor(G, color);
  for (int j = 0; rows[0]; j++) {
    for (int i = 0; rows[i] && rows[i] != '/'; i++) if (rows[i] == '#') u8g2_DrawPixel(G, x + i, y + j);
    while (*rows && *rows != '/') rows++;
    if (*rows == '/') rows++;
  }
  u8g2_SetDrawColor(G, 1);
}

// ======================================================================= helpers
static bool netUp() { return S->netState() == NET_UP; }
static void blink(uint32_t ms) { ledUntil = now + ms; S->led(true); }
static void glitch(uint8_t kind) { if (!CFG.glitch) return; glitchKind = kind; glitchSeed = rand(); glitchUntil = now + 90 + rand() % 120; redraw(); }

void uiToast(const char* t) {
  char line[48];
  snprintf(line, sizeof(line), "* %s", t);
  if (S) S->serialOut(line);
  if (toastN == 4) { memmove(toastQ[0], toastQ[1], sizeof(toastQ[0]) * 3); toastN = 3; }
  scopy(toastQ[toastN++], sizeof(toastQ[0]), t);
  if (toastN == 1) toastAt = now;
  redraw();
}

static void fmtDur(char* b, size_t n, uint32_t s) {
  if (s < 60) snprintf(b, n, "%u SEC", (unsigned)s);
  else if (s < 3600) snprintf(b, n, "%u MIN", (unsigned)(s / 60));
  else snprintf(b, n, "%uH%02u", (unsigned)(s / 3600), (unsigned)(s / 60 % 60));
}
static void fmtAgo(char* b, size_t n, uint32_t atMs) {
  uint32_t s = (now - atMs) / 1000;
  if (s < 60) snprintf(b, n, "NOW");
  else if (s < 3600) snprintf(b, n, "%uM", (unsigned)(s / 60));
  else snprintf(b, n, "%uH", (unsigned)(s / 3600));
}

// status bar: row 0, inverted. Title left; mail / Bluetooth / WiFi icons and the clock right.
static void statusBar(const char* title) {
  gattr(0, A_INV);
  int h, m, s;
  char clk[8] = "--:--";
  if (S->clock(h, m, s)) snprintf(clk, sizeof(clk), "%02d%c%02d", h, (s & 1) ? ':' : ' ', m);
  gright(0, clk, A_INV);
  int iconCells = (14 + CW - 1) / CW + (langUa ? 3 : 0);
  gput(0, 0, title, A_INV, COLS - 5 - iconCells);
  if (langUa) gput(0, COLS - 5 - iconCells + 1, "UA", A_INV);
}
static void statusIcons() {
  int x = X0 + (COLS - 5) * CW - 1, y = Y0 + (CH - 5) / 2 + (CH >= 8 ? 1 : 0);
  // WiFi: four bars
  int lvl = 0;
  uint8_t st = S->netState();
  if (st == NET_UP) { int r = S->netRssi(); lvl = r > -55 ? 4 : r > -65 ? 3 : r > -75 ? 2 : 1; }
  x -= 4;
  for (int i = 0; i < 4; i++) {
    u8g2_SetDrawColor(G, 0);
    if (i < lvl) u8g2_DrawVLine(G, x + i, y + 4 - i, i + 1);
    else if (st == NET_CONNECTING ? ((now / 300) % 4 == (uint32_t)i) : (st != NET_UP && i == 0)) u8g2_DrawPixel(G, x + i, y + 4);
  }
  if (st == NET_FAIL || st == NET_OFF) { u8g2_DrawPixel(G, x + 1, y); u8g2_DrawPixel(G, x + 1, y + 1); u8g2_DrawPixel(G, x + 1, y + 3); }
  // Bluetooth: a keyboard or mouse is connected
  BtDev d[3];
  int nbt = S->btConnected(d, 3);
  const char* bst = S->btStatus();
  x -= 4;
  if (nbt > 0 || (bst[0] && (now / 400) % 2)) icon(x, y, "##./#.#/##./#.#/##.", 0);
  // mail: unread chat
  x -= 6;
  if (chat.code[0] && chatUnread(chat, CFG.name) > 0 && cur() != SC_CHAT) icon(x, y, "#####/##.##/#.#.#/#####", 0);
  u8g2_SetDrawColor(G, 1);
}

// lists: selection + scrolling
struct List { int sel, top; };
static void listFix(List& l, int n, int vis) {
  if (l.sel >= n) l.sel = n - 1;
  if (l.sel < 0) l.sel = 0;
  if (l.sel < l.top) l.top = l.sel;
  if (l.sel >= l.top + vis) l.top = l.sel - vis + 1;
  if (l.top > n - vis) l.top = n - vis;
  if (l.top < 0) l.top = 0;
}
static void scrollbar(int row0, int vis, int top, int n) {
  if (n <= vis) return;
  int y0 = Y0 + row0 * CH, h = vis * CH;
  int bh = h * vis / n; if (bh < 2) bh = 2;
  int by = y0 + (h - bh) * top / (n - vis);
  u8g2_SetDrawColor(G, 2);
  u8g2_DrawVLine(G, W - 1, by, bh);
  u8g2_SetDrawColor(G, 1);
}

// ======================================================================= text input line
struct LineEdit { char b[420]; int cur; };   // cur = byte offset
static void leClear(LineEdit& e) { e.b[0] = 0; e.cur = 0; }
static void leSet(LineEdit& e, const char* s) { scopy(e.b, sizeof(e.b), s); e.cur = strlen(e.b); }
static bool leKey(LineEdit& e, const KeyEv& k, int maxCps) {
  int len = strlen(e.b);
  if (k.code == K_CHAR && !(k.mods & MOD_CTRL)) {
    if (k.ch < 32 || (maxCps && u8len(e.b) >= maxCps)) return true;
    char enc[4]; int n = u8enc(enc, k.ch);
    if (len + n >= (int)sizeof(e.b)) return true;
    memmove(e.b + e.cur + n, e.b + e.cur, len - e.cur + 1);
    memcpy(e.b + e.cur, enc, n);
    e.cur += n;
    return true;
  }
  if (k.code == K_BKSP) {
    if (!e.cur) return true;
    int p = e.cur - 1; while (p > 0 && ((uint8_t)e.b[p] & 0xC0) == 0x80) p--;
    memmove(e.b + p, e.b + e.cur, len - e.cur + 1); e.cur = p; return true;
  }
  if (k.code == K_DEL) {
    if (e.cur >= len) return true;
    const char* q = e.b + e.cur; u8next(q);
    memmove(e.b + e.cur, q, strlen(q) + 1); return true;
  }
  if (k.code == K_LEFT) { if (e.cur) { e.cur--; while (e.cur > 0 && ((uint8_t)e.b[e.cur] & 0xC0) == 0x80) e.cur--; } return true; }
  if (k.code == K_RIGHT) { if (e.cur < len) { const char* q = e.b + e.cur; u8next(q); e.cur = q - e.b; } return true; }
  if (k.code == K_HOME) { e.cur = 0; return true; }
  if (k.code == K_END) { e.cur = len; return true; }
  if (k.code == K_CHAR && (k.mods & MOD_CTRL) && (k.ch == 'u' || k.ch == 'U')) { leClear(e); return true; }
  return false;
}
// draws the line at row r from column c0, scrolled so the cursor shows; mask: passwords
static void leDraw(LineEdit& e, int r, int c0, bool mask, bool cursor) {
  int width = COLS - c0;
  int curCp = u8len(e.b) - u8len(e.b + e.cur);
  int total = u8len(e.b);
  int start = curCp - width + 1; if (start < 0) start = 0;
  if (total - start < width - 1 && start > 0) { start = total - width + 1; if (start < 0) start = 0; }
  const char* p = e.b + u8off(e.b, start);
  int c = c0, i = start;
  while (*p && c < COLS) {
    uint32_t cp = u8next(p);
    if (mask && i < total - 1) cp = '*';
    gcell(r, c++, cp, 0);
    i++;
  }
  if (cursor && (now / 500) % 2 == 0) { int cc = c0 + (curCp - start); if (cc < COLS) ga[r][cc] ^= A_INV; }
}

// ======================================================================= DOS console
struct CLine { char t[100]; char cmd[24]; };
#define CL_MAX 30
static CLine cl[CL_MAX];
static int clN = 0;
static int clScroll = 0;          // rows scrolled back
static LineEdit dosIn;
static char hist[8][64];
static int histN = 0, histPos = -1;
static int dosSuggest = -1;

static void cprint(const char* s, const char* cmd = nullptr) {
  if (clN == CL_MAX) { memmove(&cl[0], &cl[1], sizeof(CLine) * (CL_MAX - 1)); clN--; }
  scopy(cl[clN].t, sizeof(cl[0].t), s);
  scopy(cl[clN].cmd, sizeof(cl[0].cmd), cmd ? cmd : "");
  clN++;
  clScroll = 0;
  S->serialOut(s);
  redraw();
}
static void cprintf(const char* fmt, ...) {
  char b[128];
  va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
  cprint(b);
}
static int lineRows(const char* t) { int n = u8len(t); return n == 0 ? 1 : (n + COLS - 1) / COLS; }

// ======================================================================= screens: shared
static void enter(Scr s);
static void go(Scr s) { if (sp < 9) stk[++sp] = s; enter(s); redraw(); }
static void back() { if (sp > 0) sp--; else { stk[0] = SC_WHO; } enter(cur()); redraw(); }
static void homeScreen() { sp = 0; stk[0] = SC_WHO; enter(SC_WHO); redraw(); }
static void runCommand(const char* line);

// ---- text input (codes, names, passwords) with keyboard or BOOT-button letter wheel
struct InputReq {
  char title[20];
  char prompt[24];
  uint8_t charset;          // 0 = code (A-Z 0-9), 1 = name, 2 = anything
  uint8_t mask;
  int maxCps;
  void (*done)(const char* text);
};
static InputReq inReq;
static LineEdit inEd;
static int wheel = 0;
static const char* const CHARSETS[3] = {
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789",
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.",
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,-_!?@#$%&*+=/:;()[]<>'\"~^|\\",
};
static int wheelN() { return strlen(CHARSETS[inReq.charset]) + 2; }   // + DEL + OK
static void askText(const char* title, const char* prompt, uint8_t charset, bool mask, int maxCps, const char* start, void (*done)(const char*)) {
  scopy(inReq.title, sizeof(inReq.title), title);
  scopy(inReq.prompt, sizeof(inReq.prompt), prompt);
  inReq.charset = charset; inReq.mask = mask; inReq.maxCps = maxCps; inReq.done = done;
  leSet(inEd, start ? start : "");
  wheel = 0;
  go(SC_INPUT);
}
static void inputFinish() {
  char t[420];
  scopy(t, sizeof(t), inEd.b);
  void (*done)(const char*) = inReq.done;
  back();
  if (done) done(t);
}
static void drawInput() {
  statusBar(inReq.title);
  gput(1, 0, inReq.prompt, 0, COLS);
  gput(2, 0, ">", 0);
  leDraw(inEd, 2, 1, inReq.mask, true);
  if (!kbOn && ROWS >= 5) {
    // the letter wheel for the BOOT button
    const char* cs = CHARSETS[inReq.charset];
    int n = wheelN(), half = (COLS - 3) / 2;
    int mid = COLS / 2;
    for (int i = -half; i <= half; i++) {
      int k = ((wheel + i) % n + n) % n;
      int c = mid + i + (i < 0 ? -1 : i > 0 ? 1 : 0);
      if (i == 0) {
        gcell(3, c - 1, '[', 0);
        if (k < n - 2) gcell(3, c, (uint8_t)cs[k], 0);
        else gput(3, c, k == n - 2 ? "DEL" : "OK", 0);
        gcell(3, k < n - 2 ? c + 1 : c + (k == n - 2 ? 3 : 2), ']', 0);
      } else if (k < n - 2) gcell(3, c, (uint8_t)cs[k], 0);
      else gcell(3, c, k == n - 2 ? '<' : '!', 0);
    }
    gcenter(ROWS - 1, COLS >= 18 ? "TAP:NEXT HOLD:PICK" : "TAP=>  HOLD=OK");
  } else if (ROWS >= 5) {
    gcenter(ROWS - 1, COLS >= 18 ? "ENTER:OK ESC:BACK" : "ENTER=OK");
  }
}
static void keyInput(const KeyEv& k) {
  if (k.code == K_ENTER) { inputFinish(); return; }
  if (k.code == K_ESC) { back(); return; }
  if (inReq.charset == 0 && k.code == K_CHAR) {        // codes: capitals and digits only
    KeyEv u = k;
    if (u.ch >= 'a' && u.ch <= 'z') u.ch -= 32;
    if (!((u.ch >= 'A' && u.ch <= 'Z') || (u.ch >= '0' && u.ch <= '9'))) return;
    leKey(inEd, u, inReq.maxCps);
    return;
  }
  leKey(inEd, k, inReq.maxCps);
}
static void btnInput(BtnEv e) {
  int n = wheelN();
  if (e == BTN_SHORT) wheel = (wheel + 1) % n;
  else if (e == BTN_DOUBLE) back();
  else if (e == BTN_LONG) {
    if (wheel == n - 1) { inputFinish(); return; }
    if (wheel == n - 2) { KeyEv k = {K_BKSP, 0, 0}; leKey(inEd, k, 0); return; }
    KeyEv k = {K_CHAR, (uint8_t)CHARSETS[inReq.charset][wheel], 0};
    leKey(inEd, k, inReq.maxCps);
  }
}

// ======================================================================= WHO.EXE (the info screen)
struct WRow { int8_t game; int8_t player; };      // player -1 = a game header (grouped view)
static WRow wrows[ON_MAX_PLAYERS + ON_MAX_GAMES];
static int wn = 0;
static List whoL = {0, 0};
static bool whoActive = false;                   // the user is moving the selection
static uint32_t whoTouched = 0, whoPage = 0;
static int infoPlayer = -1, infoGame = -1;

static void buildWho() {
  wn = 0;
  if (CFG.whoByGame) {
    for (int g = 0; g < on->nGames; g++) {
      wrows[wn++] = {(int8_t)g, -1};
      for (int p = 0; p < on->nPlayers; p++) if (on->players[p].game == g) wrows[wn++] = {(int8_t)g, (int8_t)p};
    }
  } else {
    for (int p = 0; p < on->nPlayers; p++) wrows[wn++] = {(int8_t)on->players[p].game, (int8_t)p};
  }
}
static void drawWho() {
  buildWho();
  char t[48];
  snprintf(t, sizeof(t), onHave ? "WHO %u ON" : "WHO", (unsigned)on->total);
  statusBar(t);
  int vis = ROWS - 1;
  if (!onHave || wn == 0) {
    int r = ROWS >= 6 ? 2 : 1;
    if (!CFG.ssid[0] && S->netState() != NET_UP) { gcenter(r, "NO WIFI SET UP"); gcenter(r + 1, "C:\\>WIFI"); }
    else if (S->netState() == NET_CONNECTING) { gcenter(r, "CONNECTING..."); gcenter(r + 1, S->netSsid()); }
    else if (S->netState() != NET_UP) { gcenter(r, "NO WIFI"); gcenter(r + 1, S->netSsid()); }
    else if (!onHave) { gcenter(r, onErr ? "NET ERROR" : "LOADING..."); if (onErr) { snprintf(t, sizeof(t), "%d %s", onErr, S->netError()); gcenter(r + 1, t); } }
    else {
      gcenter(r, "NOBODY ONLINE");
      if (onEv.n) {
        const OnEvent& e = onEv.e[0];
        char ago[8]; fmtAgo(ago, sizeof(ago), e.atMs);
        snprintf(t, sizeof(t), "%s %s", e.joined ? "+" : "-", e.name);
        gcenter(r + 1, t);
        snprintf(t, sizeof(t), "%s %s", e.dos, ago);
        gcenter(r + 2, t);
      } else gcenter(r + 1, "ON " SERVER_NAME);
    }
    if (ROWS >= 6) { gput(ROWS - 1, 0, "C:\\>"); if ((now / 530) % 2) gcell(ROWS - 1, 4, ' ', A_INV); }
    return;
  }
  // nobody touching it: page through by itself every 4 s
  if (whoActive && now - whoTouched > 15000) whoActive = false;
  if (!whoActive && wn > vis && now - whoPage > 4000) {
    whoPage = now;
    whoL.top = whoL.top + vis >= wn ? 0 : whoL.top + vis;
    redraw();
  }
  if (whoActive) listFix(whoL, wn, vis);
  else if (whoL.top > wn - 1) whoL.top = 0;
  for (int i = 0; i < vis && whoL.top + i < wn; i++) {
    int r = 1 + i;
    const WRow& w = wrows[whoL.top + i];
    const OnGame& g = on->games[w.game];
    uint8_t a = whoActive && whoL.top + i == whoL.sel ? A_INV : 0;
    if (w.player < 0) {
      int c = gput(r, 0, g.dos, a);
      snprintf(t, sizeof(t), "%u", g.n);
      gright(r, t, a, COLS - 1);
      gattr(r, a, c, COLS - 1);
      if (a) gattr(r, a, 0, COLS - 1);
    } else {
      const OnPlayer& p = on->players[w.player];
      int ind = CFG.whoByGame ? 1 : 0;
      const char* right = CFG.whoByGame ? p.detail : g.dos;
      int rw = u8len(right);
      if (rw > COLS - 1 - ind - 4) rw = COLS - 1 - ind - 4;          // leave room for 3 letters of the name
      int nw = COLS - 1 - ind - (rw ? rw + 1 : 0);
      gput(r, ind, p.name, a, nw);
      if (rw) { const char* rp = right + u8off(right, u8len(right) - rw); gput(r, COLS - 1 - rw, rp, a, rw); }
      if (a) gattr(r, a, 0, COLS - 1);
    }
  }
  scrollbar(1, vis, whoL.top, wn);
}
static void openInfo(int idx) {
  if (idx < 0 || idx >= wn) return;
  infoPlayer = wrows[idx].player;
  infoGame = wrows[idx].game;
  go(SC_INFO);
}
static void whoMove(int d) {
  if (!wn) return;
  if (!whoActive) { whoActive = true; whoL.sel = d > 0 ? whoL.top : whoL.top; }
  else whoL.sel = (whoL.sel + d + wn) % wn;
  whoTouched = now;
  listFix(whoL, wn, ROWS - 1);
}
static void keyWho(const KeyEv& k) {
  buildWho();
  switch (k.code) {
    case K_UP: whoMove(-1); return;
    case K_DOWN: case K_TAB: whoMove(1); return;
    case K_PGUP: whoMove(-(ROWS - 1)); return;
    case K_PGDN: whoMove(ROWS - 1); return;
    case K_ENTER: if (whoActive) openInfo(whoL.sel); else whoMove(1); return;
    case K_ESC: if (whoActive) whoActive = false; else go(SC_MENU); return;
    case K_F5: tOnlineSet = false; return;
    case K_CHAR: break;
    default: return;
  }
  uint32_t c = k.ch;
  if (c == 'g' || c == 'G') { CFG.whoByGame = !CFG.whoByGame; S->saveCfg(); whoL = {0, 0}; }
  else if (c == 'r' || c == 'R') tOnlineSet = false;
  else if (c == 'c' || c == 'C') go(SC_CHAT);
  else if (c == 'd' || c == 'D' || c == ':' || c == '\\') go(SC_DOS);
  else if (c == 'l' || c == 'L') go(SC_LOG);
  else if (c == 'm' || c == 'M' || c == ' ') go(SC_MENU);
  else { go(SC_DOS); KeyEv kk = k; leKey(dosIn, kk, 60); }    // just start typing a command
}
static void btnWho(BtnEv e) {
  buildWho();
  if (e == BTN_SHORT) whoMove(1);
  else if (e == BTN_LONG) { if (whoActive) openInfo(whoL.sel); else go(SC_MENU); }
  else if (e == BTN_DOUBLE) { if (whoActive) whoActive = false; else go(SC_MENU); }
}

// ---- one person (or one game) in detail
static void drawInfo() {
  char t[64];
  if (infoGame < 0 || infoGame >= on->nGames) { statusBar("WHO"); gcenter(2, "GONE"); return; }
  const OnGame& g = on->games[infoGame];
  if (infoPlayer >= 0 && infoPlayer < on->nPlayers) {
    const OnPlayer& p = on->players[infoPlayer];
    statusBar(p.name);
    snprintf(t, sizeof(t), "IN %s.HTM", g.dos);
    gput(1, 0, t);
    gput(2, 0, g.title, 0, COLS);
    char d[16]; fmtDur(d, sizeof(d), p.secs + (now - onAt) / 1000);
    snprintf(t, sizeof(t), "FOR %s", d);
    gput(3, 0, t);
    const char* kind = !strcmp(g.folder, "online") ? "ONLINE GAME" : !strcmp(g.folder, "apps") ? "APP" : !strcmp(g.folder, "menu") ? "IN THE MENU" : "GAME";
    gput(4, 0, p.detail[0] ? p.detail : kind, 0, COLS);
  } else {
    statusBar(g.dos);
    gput(1, 0, g.title, 0, COLS);
    snprintf(t, sizeof(t), "%u ONLINE", g.n);
    gput(2, 0, t);
    int r = 3, c = 0;
    for (int p = 0; p < on->nPlayers && r < ROWS; p++) {
      if (on->players[p].game != infoGame) continue;
      int w = u8len(on->players[p].name);
      if (c && c + w > COLS) { r++; c = 0; if (r >= ROWS) break; }
      c = gput(r, c, on->players[p].name, 0, COLS - c) + 1;
    }
  }
}
static void keyInfo(const KeyEv& k) {
  if (k.code == K_ESC || k.code == K_ENTER || k.code == K_BKSP) { back(); return; }
  if (k.code == K_UP || k.code == K_DOWN) {
    buildWho();
    if (!wn) return;
    whoL.sel = (whoL.sel + (k.code == K_DOWN ? 1 : -1) + wn) % wn;
    infoPlayer = wrows[whoL.sel].player; infoGame = wrows[whoL.sel].game;
  }
}

// ======================================================================= MENU
struct MenuItem { const char* file; const char* what; uint8_t scr; char key; };
static const MenuItem MENU[] = {
  {"WHO.EXE", "ONLINE", SC_WHO, 'W'}, {"CHAT.EXE", "CHAT", SC_CHAT, 'C'}, {"C:\\>", "DOS", SC_DOS, 'D'},
  {"WIFI.EXE", "NET", SC_WIFI, 'W'}, {"BT.EXE", "KEYBOARD", SC_BT, 'B'}, {"SETUP.EXE", "", SC_SETUP, 'S'}, {"LOG.TXT", "", SC_LOG, 'L'},
};
static const int MENU_N = sizeof(MENU) / sizeof(MENU[0]);
static List menuL = {0, 0};
static void menuOpen(int i) {
  uint8_t s = MENU[i].scr;
  if (s == SC_WHO) homeScreen();
  else { sp = 0; stk[0] = SC_WHO; go((Scr)s); }
}
static void drawMenu() {
  statusBar("PROGRAMS");
  int vis = ROWS - 1;
  listFix(menuL, MENU_N, vis);
  for (int i = 0; i < vis && menuL.top + i < MENU_N; i++) {
    int k = menuL.top + i;
    uint8_t a = k == menuL.sel ? A_INV : 0;
    int fw = gput(1 + i, 0, MENU[k].file, a);
    int room = COLS - 1 - fw - 1, ww = u8len(MENU[k].what);
    if (room >= ww) gright(1 + i, MENU[k].what, a, COLS - 1);
    gattr(1 + i, a, 0, COLS - 1);
  }
  scrollbar(1, vis, menuL.top, MENU_N);
}
static void keyMenu(const KeyEv& k) {
  if (k.code == K_UP) menuL.sel = (menuL.sel + MENU_N - 1) % MENU_N;
  else if (k.code == K_DOWN || k.code == K_TAB) menuL.sel = (menuL.sel + 1) % MENU_N;
  else if (k.code == K_ENTER) menuOpen(menuL.sel);
  else if (k.code == K_ESC) back();
  else if (k.code == K_CHAR) {                // first letter: next program with that letter; opens it if it's the only one
    uint32_t c = k.ch; if (c >= 'a' && c <= 'z') c -= 32;
    int hits = 0, next = -1;
    for (int i = 0; i < MENU_N; i++) if ((uint8_t)MENU[i].key == c) hits++;
    for (int j = 1; j <= MENU_N && next < 0; j++) { int i = (menuL.sel + j) % MENU_N; if ((uint8_t)MENU[i].key == c) next = i; }
    if (next < 0) return;
    menuL.sel = next;
    if (hits == 1) menuOpen(next);
  }
}
static void btnMenu(BtnEv e) {
  if (e == BTN_SHORT) menuL.sel = (menuL.sel + 1) % MENU_N;
  else if (e == BTN_LONG) menuOpen(menuL.sel);
  else back();
}

// ======================================================================= C:\> DOS prompt
static const char* const SUGGEST[] = {"WHO", "CHAT", "DIR", "HELP", "LOG", "SAY HI!", "TIME", "MEM", "WIFI", "BT", "SETUP", "CLS", "VER"};
static const int SUGGEST_N = sizeof(SUGGEST) / sizeof(SUGGEST[0]);
static void drawDos(bool withPrompt) {
  // the prompt line, then output above it, newest at the bottom
  char pr[8] = "C:\\>";
  int promptRows = withPrompt ? 1 : 0;      // the input scrolls sideways when it gets long
  int avail = ROWS - promptRows;
  // lines from the bottom
  int skip = clScroll;
  int r = avail - 1;
  for (int i = clN - 1; i >= 0 && r >= 0; i--) {
    const char* t = cl[i].t;
    int nr = lineRows(t), len = u8len(t);
    for (int k = nr - 1; k >= 0 && r >= 0; k--) {
      if (skip > 0) { skip--; continue; }
      const char* p = t + u8off(t, k * COLS);
      int w = len - k * COLS; if (w > COLS) w = COLS;
      gput(r, 0, p, 0, w);
      r--;
    }
  }
  if (withPrompt) {
    gput(ROWS - 1, 0, pr);
    leDraw(dosIn, ROWS - 1, 4, false, true);
  }
}
static void dosRun() {
  char line[420];
  scopy(line, sizeof(line), dosIn.b);
  char echo[440];
  snprintf(echo, sizeof(echo), "C:\\>%s", line);
  cprint(echo);
  if (line[0]) {
    if (histN == 0 || strcmp(hist[0], line) != 0) {
      memmove(hist[1], hist[0], sizeof(hist[0]) * 7);
      scopy(hist[0], sizeof(hist[0]), line);
      if (histN < 8) histN++;
    }
  }
  histPos = -1;
  leClear(dosIn);
  dosSuggest = -1;
  runCommand(line);
}
static void keyDos(const KeyEv& k) {
  if (k.code == K_ENTER) { dosRun(); return; }
  if (k.code == K_ESC) { if (dosIn.b[0]) leClear(dosIn); else back(); return; }
  if (k.code == K_UP) { if (histPos + 1 < histN) { histPos++; leSet(dosIn, hist[histPos]); } return; }
  if (k.code == K_DOWN) { if (histPos > 0) { histPos--; leSet(dosIn, hist[histPos]); } else { histPos = -1; leClear(dosIn); } return; }
  if (k.code == K_PGUP) { clScroll += ROWS - 2; return; }
  if (k.code == K_PGDN) { clScroll -= ROWS - 2; if (clScroll < 0) clScroll = 0; return; }
  if (k.code == K_TAB) {                       // complete a command
    for (int i = 0; i < SUGGEST_N; i++)
      if (dosIn.b[0] && strncasecmp(SUGGEST[i], dosIn.b, strlen(dosIn.b)) == 0) { leSet(dosIn, SUGGEST[i]); return; }
    return;
  }
  if (k.code == K_CHAR && (k.mods & MOD_CTRL) && (k.ch == 'l' || k.ch == 'L')) { clN = 0; return; }
  leKey(dosIn, k, 200);
}
static void btnDos(BtnEv e) {
  if (e == BTN_SHORT) { dosSuggest = (dosSuggest + 1) % SUGGEST_N; leSet(dosIn, SUGGEST[dosSuggest]); }
  else if (e == BTN_LONG) { if (dosIn.b[0]) dosRun(); }
  else back();
}

// ======================================================================= CHAT.EXE
static LineEdit chatIn;
static int chatScroll = 0;
static void setChannel(const char* code) {
  char c[9];
  scopy(c, sizeof(c), code);
  upcase(c);
  int n = strlen(c);
  bool ok = n >= 3;
  for (int i = 0; i < n; i++) if (!((c[i] >= 'A' && c[i] <= 'Z') || (c[i] >= '0' && c[i] <= '9'))) ok = false;
  if (!ok) { uiToast("CODE: 3-8 LETTERS"); return; }
  strcpy(CFG.channel, c);
  S->saveCfg();
  chatReset(chat, c);
  chatErr = 0;
  tChat = 0;
  npend = 0;
}
static void askedChannel(const char* t) { setChannel(t); if (CFG.channel[0] && cur() != SC_CHAT) go(SC_CHAT); }
static void askedName(const char* t) {
  if (!t[0]) return;
  scopy(CFG.name, sizeof(CFG.name), t);
  S->saveCfg();
  uiToast("NAME SAVED");
}
static void chatSend(const char* text) {
  if (!text[0]) return;
  if (!CFG.channel[0]) { uiToast("NO CHANNEL: CODE X"); return; }
  if (!netUp()) { uiToast("NO WIFI"); return; }
  if (npend >= 4) { uiToast("WAIT..."); return; }
  scopy(pend[npend++], sizeof(pend[0]), text);
  S->sendChat(CFG.channel, text);
  chatScroll = 0;
  redraw();
}
// one message as rows of text: "NAME text text" with word wrap
struct CRow { int msg; int start; int len; bool first; };  // msg >= 100: pending
static CRow crows[64];
static int crn = 0;
static void wrapMsg(int id, const char* name, const char* text) {
  int nameW = u8len(name) + 1;
  int width0 = COLS - nameW, start = 0, len = u8len(text);
  bool first = true;
  if (width0 < 4) width0 = COLS;              // very long names
  while (crn < 64) {
    int width = first ? width0 : COLS - 1;
    int take = len - start;
    if (take > width) {
      take = width;
      // break at a space if there is one in the last part of the line
      for (int k = width; k > width / 2; k--) {
        const char* p = text + u8off(text, start + k);
        if (*p == ' ') { take = k + 1; break; }
      }
    }
    crows[crn++] = {id, start, take, first};
    start += take;
    first = false;
    if (start >= len) break;
  }
}
static void buildChat() {
  crn = 0;
  for (int i = 0; i < chat.n; i++) wrapMsg(i, chat.m[i].name, chat.m[i].text);
  for (int i = 0; i < npend; i++) wrapMsg(100 + i, CFG.name, pend[i]);
}
static void drawChat() {
  char t[24];
  if (!CFG.channel[0]) {
    statusBar("ERROR-CHAT");
    gcenter(2, "NO CHANNEL YET");
    gcenter(3, kbOn ? "ENTER: TYPE CODE" : "HOLD: TYPE CODE");
    return;
  }
  snprintf(t, sizeof(t), "#%s %uON", CFG.channel, chat.whoN);
  statusBar(t);
  buildChat();
  int vis = ROWS - 2;
  int maxScroll = crn - vis; if (maxScroll < 0) maxScroll = 0;
  if (chatScroll > maxScroll) chatScroll = maxScroll;
  int first = crn - vis - chatScroll; if (first < 0) first = 0;
  if (crn == 0) gcenter(2, chatErr ? "NET ERROR" : (chat.lastId || tChat ? "NO MESSAGES YET" : "LOADING..."));
  int pad = crn - first < vis ? vis - (crn - first) : 0;     // few messages: keep them just above the input line
  for (int i = 0; i < vis && first + i < crn; i++) {
    const CRow& cr = crows[first + i];
    bool pending = cr.msg >= 100;
    const char* name = pending ? CFG.name : chat.m[cr.msg].name;
    const char* text = pending ? pend[cr.msg - 100] : chat.m[cr.msg].text;
    int c = 0;
    if (cr.first) {
      bool mine = strcmp(name, CFG.name) == 0;
      c = gput(1 + pad + i, 0, name, pending ? 0 : A_INV, COLS - 4);
      gcell(1 + pad + i, c++, pending ? '~' : (mine ? '>' : ':'), 0);
    } else c = 1;
    const char* p = text + u8off(text, cr.start);
    gput(1 + pad + i, c, p, 0, cr.len);
  }
  scrollbar(1, vis, first, crn > vis ? crn : vis);
  // the line you type in
  gcell(ROWS - 1, 0, '>', 0);
  if (!kbOn && !chatIn.b[0]) gput(ROWS - 1, 1, "HOLD:QUICK MSG", 0);
  else leDraw(chatIn, ROWS - 1, 1, false, true);
}
static void keyChat(const KeyEv& k) {
  if (!CFG.channel[0]) {
    if (k.code == K_ESC) back();
    else if (k.code == K_ENTER || k.code == K_CHAR) askText("ERROR-CHAT", "CHANNEL CODE:", 0, false, 8, "", askedChannel);
    return;
  }
  if (k.code == K_ESC) { if (chatIn.b[0]) leClear(chatIn); else back(); return; }
  if (k.code == K_ENTER) {
    const char* t = chatIn.b;
    if (t[0] == '/') {
      if (!strncasecmp(t, "/code ", 6) || !strncasecmp(t, "/join ", 6)) setChannel(t + 6);
      else if (!strncasecmp(t, "/nick ", 6) || !strncasecmp(t, "/name ", 6)) askedName(t + 6);
      else if (!strncasecmp(t, "/who", 4)) uiToast(chat.who[0] ? chat.who : "NOBODY");
      else if (!strncasecmp(t, "/quick", 6)) go(SC_QUICK);
      else if (!strncasecmp(t, "/exit", 5) || !strncasecmp(t, "/quit", 5)) { leClear(chatIn); back(); return; }
      else if (!strncasecmp(t, "/me ", 4)) chatSend(t);
      else uiToast("/CODE /NICK /WHO /ME");
    } else chatSend(t);
    leClear(chatIn);
    return;
  }
  if (k.code == K_UP && !chatIn.b[0]) { chatScroll++; return; }
  if (k.code == K_DOWN && !chatIn.b[0]) { if (chatScroll) chatScroll--; return; }
  if (k.code == K_PGUP) { chatScroll += ROWS - 3; return; }
  if (k.code == K_PGDN) { chatScroll -= ROWS - 3; if (chatScroll < 0) chatScroll = 0; return; }
  if (k.code == K_TAB) { go(SC_QUICK); return; }
  leKey(chatIn, k, 300);
}
static void btnChat(BtnEv e) {
  if (!CFG.channel[0]) { if (e == BTN_DOUBLE) back(); else askText("ERROR-CHAT", "CHANNEL CODE:", 0, false, 8, "", askedChannel); return; }
  if (e == BTN_SHORT) { chatScroll += ROWS - 3; buildChat(); if (chatScroll > crn - (ROWS - 2)) chatScroll = 0; }
  else if (e == BTN_LONG) go(SC_QUICK);
  else back();
}

// ---- quick messages (for the BOOT button)
static const char* const QUICK[] = {"HI!", "OK", "YES", "NO", "BRB", "GG!", "LOL", "ON MY WAY", "WHO'S UP?", "COME PLAY!", "GOOD NIGHT", "ПРИВІТ!"};
static const int QUICK_N = sizeof(QUICK) / sizeof(QUICK[0]);
static List quickL = {0, 0};
static void drawQuick() {
  statusBar("SEND:");
  int vis = ROWS - 1;
  listFix(quickL, QUICK_N, vis);
  for (int i = 0; i < vis && quickL.top + i < QUICK_N; i++) {
    int k = quickL.top + i;
    uint8_t a = k == quickL.sel ? A_INV : 0;
    gput(1 + i, 1, QUICK[k], a);
    gattr(1 + i, a, 0, COLS - 1);
  }
  scrollbar(1, vis, quickL.top, QUICK_N);
}
static void quickSend() { const char* t = QUICK[quickL.sel]; back(); chatSend(t); }
static void keyQuick(const KeyEv& k) {
  if (k.code == K_UP) quickL.sel = (quickL.sel + QUICK_N - 1) % QUICK_N;
  else if (k.code == K_DOWN || k.code == K_TAB) quickL.sel = (quickL.sel + 1) % QUICK_N;
  else if (k.code == K_ENTER) quickSend();
  else if (k.code == K_ESC) back();
}
static void btnQuick(BtnEv e) {
  if (e == BTN_SHORT) quickL.sel = (quickL.sel + 1) % QUICK_N;
  else if (e == BTN_LONG) quickSend();
  else back();
}

// ======================================================================= WIFI.EXE
static WifiNet nets[16];
static int netN = -1;
static List wifiL = {0, 0};
static char joinSsid[33];
static void wifiItemsFix() { listFix(wifiL, (netN > 0 ? netN : 0) + 2, ROWS - 2); }
static void askedPass(const char* p) { S->wifiJoin(joinSsid, p); uiToast("CONNECTING..."); }
static void wifiPick(int i) {
  int n = netN > 0 ? netN : 0;
  if (i == n) { S->portalStart(); go(SC_PORTAL); return; }
  if (i == n + 1 || n == 0) { S->wifiScan(); netN = -1; return; }
  scopy(joinSsid, sizeof(joinSsid), nets[i].ssid);
  if (nets[i].open) { S->wifiJoin(joinSsid, ""); uiToast("CONNECTING..."); }
  else if (!strcmp(CFG.ssid, joinSsid) && CFG.pass[0] && S->netState() != NET_FAIL) { S->wifiJoin(joinSsid, CFG.pass); uiToast("CONNECTING..."); }
  else askText("WIFI.EXE", "PASSWORD:", 2, true, 63, "", askedPass);
}
static void drawWifi() {
  int r = S->wifiResults(nets, 16);
  if (r >= 0) netN = r;
  statusBar("WIFI.EXE");
  char t[48];
  uint8_t st = S->netState();
  if (st == NET_UP) snprintf(t, sizeof(t), "ON %s", S->netSsid());
  else if (st == NET_CONNECTING) snprintf(t, sizeof(t), "JOIN %s", S->netSsid());
  else if (st == NET_FAIL) snprintf(t, sizeof(t), "FAILED %s", S->netSsid());
  else snprintf(t, sizeof(t), "OFF");
  gput(1, 0, t, 0, COLS);
  int vis = ROWS - 2;
  if (netN < 0) { gcenter(3, "SCANNING..."); return; }
  int n = netN + 2;
  wifiItemsFix();
  for (int i = 0; i < vis && wifiL.top + i < n; i++) {
    int k = wifiL.top + i, row = 2 + i;
    uint8_t a = k == wifiL.sel ? A_INV : 0;
    if (k < netN) {
      gput(row, 0, nets[k].ssid, a, COLS - 3);
      int lvl = nets[k].rssi > -55 ? 3 : nets[k].rssi > -70 ? 2 : 1;
      static const char* bars[] = {"", ".", ":", "|"};
      snprintf(t, sizeof(t), "%s%s", nets[k].open ? " " : "*", bars[lvl]);
      gright(row, t, a, COLS - 1);
    } else gput(row, 0, k == netN ? "PHONE SETUP" : "SCAN AGAIN", a);
    gattr(row, a, 0, COLS - 1);
  }
  scrollbar(2, vis, wifiL.top, n);
}
static void keyWifi(const KeyEv& k) {
  int n = (netN > 0 ? netN : 0) + 2;
  if (k.code == K_UP) wifiL.sel = (wifiL.sel + n - 1) % n;
  else if (k.code == K_DOWN || k.code == K_TAB) wifiL.sel = (wifiL.sel + 1) % n;
  else if (k.code == K_ENTER) wifiPick(wifiL.sel);
  else if (k.code == K_ESC) back();
  else if (k.code == K_F5 || (k.code == K_CHAR && (k.ch == 'r' || k.ch == 'R'))) { S->wifiScan(); netN = -1; }
}
static void btnWifi(BtnEv e) {
  int n = (netN > 0 ? netN : 0) + 2;
  if (e == BTN_SHORT) wifiL.sel = (wifiL.sel + 1) % n;
  else if (e == BTN_LONG) wifiPick(wifiL.sel);
  else back();
}
static void drawPortal() {
  statusBar("PHONE SETUP");
  gput(1, 0, "1 JOIN WIFI:");
  gput(2, 1, "ERROR-OLED");
  gput(3, 0, "2 OPEN:");
  gput(4, 1, "192.168.4.1");
  if (ROWS > 5) gput(5, 0, S->netState() == NET_UP ? "OK! ESC=DONE" : "ESC=STOP");
}
static void keyPortal(const KeyEv& k) { if (k.code == K_ESC || k.code == K_ENTER) { S->portalStop(); back(); } }
static void btnPortal(BtnEv e) { (void)e; S->portalStop(); back(); }

// ======================================================================= BT.EXE
static BtDev btFound[10], btConn[3];
static int btFoundN = 0, btConnN = 0;
static List btL = {0, 0};
static const char* btType(uint8_t t) { return t == BT_KEYBOARD ? "KB" : t == BT_MOUSE ? "MOUSE" : t == BT_COMBO ? "KB+M" : "?"; }
static int btItems() { return btConnN + 1 + btFoundN + (S->btBonds() ? 1 : 0); }
static void btPick(int i) {
  if (i < btConnN) { uiToast(btConn[i].name); return; }
  i -= btConnN;
  if (i == 0) { S->btScan(); return; }
  i--;
  if (i < btFoundN) { S->btPair(btFound[i].addr); uiToast("CONNECTING..."); return; }
  S->btForget();
  uiToast("FORGOT ALL");
}
static void drawBt() {
  btConnN = S->btConnected(btConn, 3);
  btFoundN = S->btResults(btFound, 10);
  statusBar("BT.EXE");
  const char* st = S->btStatus();
  if (st[0]) gput(1, 0, st, 0, COLS);
  else gput(1, 0, btConnN ? "CONNECTED:" : "NO KEYBOARD/MOUSE", 0, COLS);
  int n = btItems(), vis = ROWS - 2;
  listFix(btL, n, vis);
  for (int i = 0; i < vis && btL.top + i < n; i++) {
    int k = btL.top + i, row = 2 + i;
    uint8_t a = k == btL.sel ? A_INV : 0;
    if (k < btConnN) { gput(row, 0, btType(btConn[k].type), a); gput(row, 6, btConn[k].name, a, COLS - 7); }
    else if (k == btConnN) gput(row, 0, S->btScanning() ? "SCANNING..." : "SCAN", a);
    else if (k - btConnN - 1 < btFoundN) {
      const BtDev& d = btFound[k - btConnN - 1];
      gput(row, 0, d.name[0] ? d.name : d.addr, a, COLS - 6);
      gright(row, btType(d.type), a, COLS - 1);
    } else gput(row, 0, "FORGET ALL", a);
    gattr(row, a, 0, COLS - 1);
  }
  scrollbar(2, vis, btL.top, n);
}
static void keyBt(const KeyEv& k) {
  int n = btItems();
  if (k.code == K_UP) btL.sel = (btL.sel + n - 1) % n;
  else if (k.code == K_DOWN || k.code == K_TAB) btL.sel = (btL.sel + 1) % n;
  else if (k.code == K_ENTER) btPick(btL.sel);
  else if (k.code == K_ESC) back();
}
static void btnBt(BtnEv e) {
  int n = btItems();
  if (e == BTN_SHORT) btL.sel = (btL.sel + 1) % n;
  else if (e == BTN_LONG) btPick(btL.sel);
  else back();
}

// ======================================================================= SETUP.EXE
enum { ST_NAME, ST_CHAN, ST_WEB, ST_FONT, ST_KEYS, ST_WHO, ST_POLL, ST_TOAST, ST_GLITCH, ST_INTRO, ST_LIGHT, ST_FLIP, ST_SAVER, ST_ONLINE, ST_TLS, ST_ABOUT, ST_REBOOT, ST_N };
static const char* const ST_LABEL[ST_N] = {"NAME", "CHAT", "WEB", "FONT", "KEYS", "WHO", "POLL", "POPUPS", "GLITCH", "INTRO", "LIGHT", "FLIP", "SAVER", "SHOW ME", "TLS", "ABOUT", "REBOOT"};
static List setL = {0, 0};
static const uint8_t LIGHTS[] = {255, 120, 40, 8};
void uiApplyDisplay() {
  u8g2_SetContrast(G, CFG.contrast);
  u8g2_SetDisplayRotation(G, CFG.flip ? U8G2_R2 : U8G2_R0);
  layout();
  redraw();
}
static void setValue(int i, char* b, size_t n) {
  const char* on = "ON", *off = "OFF";
  switch (i) {
    case ST_NAME: scopy(b, n, CFG.name); break;
    case ST_CHAN: scopy(b, n, CFG.channel[0] ? CFG.channel : "-"); break;
    case ST_WEB: scopy(b, n, netUp() ? S->netIp() : "-"); break;
    case ST_FONT: scopy(b, n, CFG.font ? "BIG" : "SMALL"); break;
    case ST_KEYS: scopy(b, n, CFG.keymap == 1 ? "DE" : CFG.keymap == 2 ? "FR" : "US"); break;
    case ST_WHO: scopy(b, n, CFG.whoByGame ? "GAMES" : "PEOPLE"); break;
    case ST_POLL: snprintf(b, n, "%uS", CFG.poll); break;
    case ST_TOAST: scopy(b, n, CFG.toasts ? on : off); break;
    case ST_GLITCH: scopy(b, n, CFG.glitch ? on : off); break;
    case ST_INTRO: scopy(b, n, CFG.intro ? on : off); break;
    case ST_LIGHT: { int k = 0; for (int j = 0; j < 4; j++) if (CFG.contrast >= LIGHTS[j]) { k = j; break; } snprintf(b, n, "%d/4", 4 - k); break; }
    case ST_FLIP: scopy(b, n, CFG.flip ? on : off); break;
    case ST_SAVER: scopy(b, n, CFG.saver ? on : off); break;
    case ST_ONLINE: scopy(b, n, CFG.beacon ? on : off); break;
    case ST_TLS: scopy(b, n, CFG.tlsCheck == 1 ? "CHECK" : CFG.tlsCheck == 2 ? "HTTP" : "SKIP"); break;
    default: b[0] = 0;
  }
}
static void setPick(int i) {
  switch (i) {
    case ST_NAME: askText("SETUP.EXE", "YOUR NAME:", 1, false, 16, CFG.name, askedName); return;
    case ST_CHAN: askText("SETUP.EXE", "CHAT CHANNEL:", 0, false, 8, CFG.channel, askedChannel); return;
    case ST_WEB: { char b[48]; if (netUp()) snprintf(b, sizeof(b), "OPEN http://%s/", S->netIp()); else snprintf(b, sizeof(b), "NO WIFI: USE PHONE SETUP"); uiToast(b); return; }
    case ST_FONT: CFG.font = !CFG.font; layout(); break;
    case ST_KEYS: CFG.keymap = (CFG.keymap + 1) % 3; break;
    case ST_WHO: CFG.whoByGame = !CFG.whoByGame; whoL = {0, 0}; break;
    case ST_POLL: CFG.poll = CFG.poll == 5 ? 10 : CFG.poll == 10 ? 20 : CFG.poll == 20 ? 30 : 5; break;
    case ST_TOAST: CFG.toasts = !CFG.toasts; break;
    case ST_GLITCH: CFG.glitch = !CFG.glitch; if (CFG.glitch) glitch(1); break;
    case ST_INTRO: CFG.intro = !CFG.intro; break;
    case ST_LIGHT: { int k = 0; for (int j = 0; j < 4; j++) if (CFG.contrast >= LIGHTS[j]) { k = j; break; } CFG.contrast = LIGHTS[(k + 1) % 4]; uiApplyDisplay(); break; }
    case ST_FLIP: CFG.flip = !CFG.flip; uiApplyDisplay(); break;
    case ST_SAVER: CFG.saver = !CFG.saver; break;
    case ST_ONLINE: CFG.beacon = !CFG.beacon; break;
    case ST_TLS: CFG.tlsCheck = CFG.tlsCheck == 1 ? 0 : CFG.tlsCheck == 0 ? 2 : 1; break;
    case ST_ABOUT: { char b[48]; snprintf(b, sizeof(b), "ERROR-OLED " OLED_VERSION " %s", S->chipInfo()); uiToast(b); return; }
    case ST_REBOOT: S->reboot(); return;
  }
  S->saveCfg();
}
static void drawSetup() {
  statusBar("SETUP.EXE");
  int vis = ROWS - 1;
  listFix(setL, ST_N, vis);
  char v[40];
  for (int i = 0; i < vis && setL.top + i < ST_N; i++) {
    int k = setL.top + i;
    uint8_t a = k == setL.sel ? A_INV : 0;
    gput(1 + i, 0, ST_LABEL[k], a);
    setValue(k, v, sizeof(v));
    int lw = u8len(ST_LABEL[k]) + 1;
    int vw = u8len(v); if (vw > COLS - 1 - lw) vw = COLS - 1 - lw;
    gput(1 + i, COLS - 1 - vw, v, a, vw);
    gattr(1 + i, a, 0, COLS - 1);
  }
  scrollbar(1, vis, setL.top, ST_N);
}
static void keySetup(const KeyEv& k) {
  if (k.code == K_UP) setL.sel = (setL.sel + ST_N - 1) % ST_N;
  else if (k.code == K_DOWN || k.code == K_TAB) setL.sel = (setL.sel + 1) % ST_N;
  else if (k.code == K_ENTER || k.code == K_RIGHT || (k.code == K_CHAR && k.ch == ' ')) setPick(setL.sel);
  else if (k.code == K_ESC) back();
}
static void btnSetup(BtnEv e) {
  if (e == BTN_SHORT) setL.sel = (setL.sel + 1) % ST_N;
  else if (e == BTN_LONG) setPick(setL.sel);
  else back();
}

// ======================================================================= LOG.TXT: who came and went
static List logL = {0, 0};
static void drawLog() {
  statusBar("LOG.TXT");
  int vis = ROWS - 1;
  if (!onEv.n) { gcenter(2, "NOTHING YET"); return; }
  listFix(logL, onEv.n, vis);
  char ago[8];
  for (int i = 0; i < vis && logL.top + i < onEv.n; i++) {
    const OnEvent& e = onEv.e[logL.top + i];
    int r = 1 + i;
    gcell(r, 0, e.joined ? '+' : '-', 0);
    gput(r, 1, e.name, 0, COLS - 1 - 9 - 4);
    fmtAgo(ago, sizeof(ago), e.atMs);
    char t[20]; snprintf(t, sizeof(t), "%s %s", e.dos, ago);
    gright(r, t);
  }
  scrollbar(1, vis, logL.top, onEv.n);
}
static void keyLog(const KeyEv& k) {
  int vis = ROWS - 1;
  if (k.code == K_UP && logL.top > 0) { logL.top--; logL.sel = logL.top; }
  else if (k.code == K_DOWN && logL.top + vis < onEv.n) { logL.top++; logL.sel = logL.top + vis - 1; }
  else if (k.code == K_ESC || k.code == K_ENTER) back();
}

// BT PAIR: waiting for a device with this name
static char btWant[30] = "";
static uint32_t btWantUntil = 0;
static bool hasText(const char* hay, const char* needle) {
  for (; *hay; hay++) {
    const char* a = hay; const char* b = needle;
    while (*a && *b) { char x = *a, y = *b; if (x >= 'a' && x <= 'z') x -= 32; if (y >= 'a' && y <= 'z') y -= 32; if (x != y) break; a++; b++; }
    if (!*b) return true;
  }
  return false;
}

// ======================================================================= DOS commands
static int splitArgs(char* s, char** av, int max) {
  int n = 0;
  while (*s && n < max) {
    while (*s == ' ') s++;
    if (!*s) break;
    if (*s == '"') { s++; av[n++] = s; while (*s && *s != '"') s++; }
    else { av[n++] = s; while (*s && *s != ' ') s++; }
    if (*s) *s++ = 0;
  }
  return n;
}
static void cmdHelp() {
  cprint("WHO   WHO'S ONLINE", "WHO");
  cprint("CHAT  ERROR-CHAT", "CHAT");
  cprint("SAY x SEND TO CHAT");
  cprint("CODE x CHANNEL");
  cprint("NAME x YOUR NAME");
  cprint("WIFI  NETWORK", "WIFI");
  cprint("WEB   SETUP PAGE", "WEB");
  cprint("BT    KEYBOARD", "BT");
  cprint("BT PAIR name");
  cprint("SETUP SETTINGS", "SETUP");
  cprint("LOG DIR MEM TIME");
  cprint("INTRO 3D WELCOME", "INTRO");
  cprint("VER CLS EXIT");
}
static void cmdDir() {
  char t[64];
  cprint(" VOLUME " SERVER_NAME);
  struct { const char* n; const char* e; const char* cmd; } f[] = {
    {"WHO", "EXE", "WHO"}, {"CHAT", "EXE", "CHAT"}, {"WIFI", "EXE", "WIFI"}, {"BT", "EXE", "BT"},
    {"SETUP", "EXE", "SETUP"}, {"LOG", "TXT", "LOG"}, {"QUICK", "TXT", "QUICK"},
  };
  for (auto& x : f) { snprintf(t, sizeof(t), "%-8s %s", x.n, x.e); cprint(t, x.cmd); }
  snprintf(t, sizeof(t), "%uK FREE", (unsigned)(S->freeHeap() / 1024));
  cprint(t);
}
static bool cmdGame(const char* w) {
  // typed a game's DOS name (TUSA or TUSA.HTM): who's in it
  char name[16];
  scopy(name, sizeof(name), w);
  upcase(name);
  char* dot = strchr(name, '.');
  if (dot) *dot = 0;
  for (int g = 0; g < on->nGames; g++) {
    if (strcmp(on->games[g].dos, name)) continue;
    char t[120];
    int k = snprintf(t, sizeof(t), "%u IN %s:", on->games[g].n, on->games[g].dos);
    for (int p = 0; p < on->nPlayers && k < (int)sizeof(t) - 20; p++)
      if (on->players[p].game == g) k += snprintf(t + k, sizeof(t) - k, " %s", on->players[p].name);
    cprint(t);
    return true;
  }
  return false;
}
static void runCommand(const char* line) {
  char buf[420];
  scopy(buf, sizeof(buf), line);
  char* av[8];
  int ac = splitArgs(buf, av, 8);
  if (!ac) return;
  char cmd[16];
  scopy(cmd, sizeof(cmd), av[0]);
  upcase(cmd);
  int cl0 = strlen(cmd);
  if (cl0 > 4 && !strcmp(cmd + cl0 - 4, ".EXE")) cmd[cl0 - 4] = 0;
  const char* rest = line;                     // everything after the command word
  while (*rest == ' ') rest++;
  while (*rest && *rest != ' ') rest++;
  while (*rest == ' ') rest++;

  if (!strcmp(cmd, "HELP") || !strcmp(cmd, "?")) cmdHelp();
  else if (!strcmp(cmd, "DIR")) cmdDir();
  else if (!strcmp(cmd, "CLS")) clN = 0;
  else if (!strcmp(cmd, "VER")) { cprint("ERROR-DOS " OLED_VERSION); cprintf("ERROR-OLED %s", S->chipInfo()); }
  else if (!strcmp(cmd, "MEM")) { cprintf("%uK FREE OF %uK", (unsigned)(S->freeHeap() / 1024), (unsigned)(S->totalHeap() / 1024)); cprintf("BIGGEST %uK", (unsigned)(S->maxBlock() / 1024)); }
  else if (!strcmp(cmd, "TIME") || !strcmp(cmd, "DATE")) { int h, m, s; if (S->clock(h, m, s)) cprintf("%02d:%02d:%02d", h, m, s); else cprint("NO TIME YET (WIFI)"); }
  else if (!strcmp(cmd, "IP") || !strcmp(cmd, "NET")) {
    static const char* const ST[] = {"OFF", "CONNECTING", "ON", "FAILED", "PHONE SETUP"};
    cprintf("WIFI %s %s", ST[S->netState() % 5], S->netSsid());
    if (netUp()) cprintf("IP %s %ddBm", S->netIp(), S->netRssi());
    if (S->netError()[0]) cprintf("LAST ERR %s", S->netError());
  }
  else if (!strcmp(cmd, "WHO")) { if (ac > 1 && cmdGame(av[1])) return; homeScreen(); }
  else if (!strcmp(cmd, "LOG")) go(SC_LOG);
  else if (!strcmp(cmd, "CHAT")) { if (ac > 1) setChannel(av[1]); go(SC_CHAT); }
  else if (!strcmp(cmd, "QUICK")) { if (CFG.channel[0]) go(SC_QUICK); else cprint("NO CHANNEL: CODE X"); }
  else if (!strcmp(cmd, "SAY")) {
    if (!rest[0]) cprint("SAY WHAT?");
    else if (!CFG.channel[0]) cprint("NO CHANNEL: CODE X");
    else { chatSend(rest); cprintf("SENT TO #%s", CFG.channel); }
  }
  else if (!strcmp(cmd, "CODE") || !strcmp(cmd, "CHANNEL") || !strcmp(cmd, "JOIN")) {
    if (ac > 1) { setChannel(av[1]); if (ieq(CFG.channel, av[1])) cprintf("CHANNEL #%s", CFG.channel); }
    else cprintf("CHANNEL #%s", CFG.channel[0] ? CFG.channel : "-");
  }
  else if (!strcmp(cmd, "NAME") || !strcmp(cmd, "NICK")) {
    if (rest[0]) { askedName(rest); cprintf("NAME %s", CFG.name); }
    else cprintf("NAME %s", CFG.name);
  }
  else if (!strcmp(cmd, "WIFI")) {
    if (ac >= 2) { S->wifiJoin(av[1], ac >= 3 ? av[2] : ""); cprintf("JOINING %s", av[1]); }
    else go(SC_WIFI);
  }
  else if (!strcmp(cmd, "PORTAL")) { S->portalStart(); go(SC_PORTAL); }
  else if (!strcmp(cmd, "INTRO")) { introReplay = true; introAt = now; introLast = 0; starsReady = false; go(SC_INTRO); }
  else if (!strcmp(cmd, "WEB")) {
    if (netUp()) { cprint("SETUP PAGE:"); cprintf("http://%s/", S->netIp()); cprint("http://error-oled.local/"); }
    else cprint("NO WIFI: TRY PORTAL");
  }
  else if (!strcmp(cmd, "BT")) {
    if (ac > 1 && ieq(av[1], "FORGET")) { S->btForget(); cprint("FORGOT ALL"); }
    else if (ac > 1 && ieq(av[1], "PAIR")) {
      // BT PAIR Bluetooth Keyboard: scan, then pair with the first device whose name has this in it
      const char* want = rest; while (*want && *want != ' ') want++; while (*want == ' ') want++;
      if (!*want) { cprint("BT PAIR <NAME>"); return; }
      scopy(btWant, sizeof(btWant), want);
      btWantUntil = now + 20000;
      S->btScan();
      cprintf("LOOKING FOR %s...", btWant);
    }
    else if (ac > 1 && ieq(av[1], "LIST")) {
      BtDev d[10]; int n = S->btResults(d, 10);
      for (int i = 0; i < n; i++) cprintf("%s %s", btType(d[i].type), d[i].name[0] ? d[i].name : d[i].addr);
      n = S->btConnected(d, 3);
      for (int i = 0; i < n; i++) cprintf("ON: %s %s", btType(d[i].type), d[i].name);
      if (S->btStatus()[0]) cprint(S->btStatus());
    }
    else { if (ac > 1 && ieq(av[1], "SCAN")) S->btScan(); go(SC_BT); }
  }
  else if (!strcmp(cmd, "SETUP")) go(SC_SETUP);
  else if (!strcmp(cmd, "FONT")) { CFG.font = ac > 1 ? ieq(av[1], "BIG") : !CFG.font; S->saveCfg(); layout(); }
  else if (!strcmp(cmd, "KEYS")) {
    if (ac > 1) for (int i = 0; i < 3; i++) if (ieq(av[1], i == 0 ? "US" : i == 1 ? "DE" : "FR")) { CFG.keymap = i; S->saveCfg(); }
    cprintf("KEYS %s", CFG.keymap == 1 ? "DE" : CFG.keymap == 2 ? "FR" : "US");
  }
  else if (!strcmp(cmd, "FLIP")) { CFG.flip = !CFG.flip; S->saveCfg(); uiApplyDisplay(); }
  else if (!strcmp(cmd, "GLITCH")) { CFG.glitch = ac > 1 ? !ieq(av[1], "OFF") : !CFG.glitch; S->saveCfg(); cprintf("GLITCH %s", CFG.glitch ? "ON" : "OFF"); glitch(2); }
  else if (!strcmp(cmd, "LIGHT")) { if (ac > 1) { int v = atoi(av[1]); CFG.contrast = v < 1 ? 1 : v > 100 ? 255 : v * 255 / 100; S->saveCfg(); uiApplyDisplay(); } cprintf("LIGHT %d%%", CFG.contrast * 100 / 255); }
  else if (!strcmp(cmd, "REBOOT")) S->reboot();
  else if (!strcmp(cmd, "EXIT")) homeScreen();
  else if (!strcmp(cmd, "MENU") || !strcmp(cmd, "START")) go(SC_MENU);
  else if (!strcmp(cmd, "ECHO")) cprint(rest);
  else if (!cmdGame(av[0])) cprint("Bad command or file name");
}

// ======================================================================= dispatch
static void enter(Scr s) {
  if (s == SC_WIFI) { netN = -1; wifiL = {0, 0}; S->wifiScan(); }
  if (s == SC_BT) btL = {0, 0};
  if (s == SC_CHAT) {
    if (strcmp(chat.code, CFG.channel)) chatReset(chat, CFG.channel);
    tChat = 0;                       // fetch now
    chatScroll = 0;
  }
  if (s == SC_WHO) whoActive = false;
  if (s == SC_DOS) clScroll = 0;
}

void uiKey(const KeyEv& k) {
  lastInput = now;
  redraw();
  if (cur() == SC_INTRO) { introEnd(); return; }
  if (S->btPasskey()) return;
  if (toastN && k.code == K_ESC && now - toastAt < 3000) { toastAt = now - 4000; return; }   // Esc closes a pop-up first
  // keys that work everywhere
  if (k.code == K_MENU || (k.code == K_CHAR && (k.mods & MOD_GUI))) { go(SC_MENU); return; }
  if (k.code == K_F1) { if (cur() != SC_DOS) go(SC_DOS); cmdHelp(); return; }
  if (k.code == K_DEL && (k.mods & MOD_CTRL) && (k.mods & MOD_ALT)) { S->reboot(); return; }
  if (k.code == K_F2) { go(SC_CHAT); return; }
  switch (cur()) {
    case SC_BOOT: homeScreen(); break;
    case SC_WHO: keyWho(k); break;
    case SC_INFO: keyInfo(k); break;
    case SC_MENU: keyMenu(k); break;
    case SC_DOS: keyDos(k); break;
    case SC_CHAT: keyChat(k); break;
    case SC_QUICK: keyQuick(k); break;
    case SC_WIFI: keyWifi(k); break;
    case SC_PORTAL: keyPortal(k); break;
    case SC_BT: keyBt(k); break;
    case SC_SETUP: keySetup(k); break;
    case SC_INPUT: keyInput(k); break;
    case SC_LOG: keyLog(k); break;
    case SC_INTRO: break;
  }
}

void uiButton(BtnEv e) {
  lastInput = now;
  redraw();
  if (cur() == SC_INTRO) { introEnd(); return; }
  switch (cur()) {
    case SC_BOOT: homeScreen(); break;
    case SC_WHO: btnWho(e); break;
    case SC_INFO: back(); break;
    case SC_MENU: btnMenu(e); break;
    case SC_DOS: btnDos(e); break;
    case SC_CHAT: btnChat(e); break;
    case SC_QUICK: btnQuick(e); break;
    case SC_WIFI: btnWifi(e); break;
    case SC_PORTAL: btnPortal(e); break;
    case SC_BT: btnBt(e); break;
    case SC_SETUP: btnSetup(e); break;
    case SC_INPUT: btnInput(e); break;
    case SC_LOG: if (e == BTN_SHORT) { logL.top = (logL.top + 1) % (onEv.n ? onEv.n : 1); } else back(); break;
    case SC_INTRO: break;
  }
}

// a click on the text grid
static void click(int r, int c) {
  (void)c;
  if (r == 0 && cur() != SC_DOS && cur() != SC_BOOT) { go(SC_MENU); return; }
  KeyEv enterK = {K_ENTER, 0, 0};
  switch (cur()) {
    case SC_WHO: buildWho(); if (whoL.top + r - 1 < wn) { whoActive = true; whoTouched = now; whoL.sel = whoL.top + r - 1; openInfo(whoL.sel); } break;
    case SC_INFO: back(); break;
    case SC_MENU: if (menuL.top + r - 1 < MENU_N) { menuL.sel = menuL.top + r - 1; menuOpen(menuL.sel); } break;
    case SC_QUICK: if (quickL.top + r - 1 < QUICK_N) { quickL.sel = quickL.top + r - 1; quickSend(); } break;
    case SC_SETUP: if (setL.top + r - 1 < ST_N) { setL.sel = setL.top + r - 1; setPick(setL.sel); } break;
    case SC_WIFI: if (r >= 2 && netN >= 0 && wifiL.top + r - 2 < netN + 2) { wifiL.sel = wifiL.top + r - 2; wifiPick(wifiL.sel); } break;
    case SC_BT: if (r >= 2 && btL.top + r - 2 < btItems()) { btL.sel = btL.top + r - 2; btPick(btL.sel); } break;
    case SC_CHAT: if (!CFG.channel[0]) keyChat(enterK); else if (r == ROWS - 1 && !kbOn) go(SC_QUICK); break;
    case SC_DOS: {
      // which console line is under the pointer? (same walk as drawDos)
      int promptRows = 1, row = ROWS - promptRows - 1, skip = clScroll;
      for (int i = clN - 1; i >= 0 && row >= 0; i--) {
        int nr = lineRows(cl[i].t);
        for (int k = nr - 1; k >= 0 && row >= 0; k--) {
          if (skip > 0) { skip--; continue; }
          if (row == r) { if (cl[i].cmd[0]) { leSet(dosIn, cl[i].cmd); dosRun(); } return; }
          row--;
        }
      }
      break;
    }
    case SC_INPUT: inputFinish(); break;
    case SC_BOOT: homeScreen(); break;
    default: break;
  }
}

void uiMouse(int dx, int dy, int wheelD, uint8_t buttons) {
  lastInput = now;
  // small screen: slow the pointer down
  pAccX += dx; pAccY += dy;
  int div = W > 100 ? 2 : 3;
  px += pAccX / div; py += pAccY / div;
  pAccX %= div; pAccY %= div;
  if (px < 0) px = 0;
  if (py < 0) py = 0;
  if (px > W - 1) px = W - 1;
  if (py > H - 1) py = H - 1;
  if (dx || dy) pAt = now;
  uint8_t pressed = buttons & ~pButtons;
  pButtons = buttons;
  if (pressed && cur() == SC_INTRO) { introEnd(); return; }
  if (pressed & MB_LEFT) {
    pAt = now;
    if (S->btPasskey()) return;
    int r = (py - Y0) / CH, c = (px - X0) / CW;
    if (r >= 0 && r < ROWS) click(r, c);
  }
  if (pressed & MB_RIGHT) { KeyEv k = {K_ESC, 0, 0}; uiKey(k); }
  if (pressed & MB_MIDDLE) go(SC_MENU);
  if (wheelD) {
    KeyEv k = {(uint16_t)(wheelD > 0 ? K_UP : K_DOWN), 0, 0};
    int n = wheelD > 0 ? wheelD : -wheelD;
    if (n > 4) n = 4;
    for (int i = 0; i < n; i++) {
      if (cur() == SC_DOS) { if (wheelD > 0) clScroll++; else if (clScroll) clScroll--; }
      else if (cur() == SC_CHAT) { if (wheelD > 0) chatScroll++; else if (chatScroll) chatScroll--; }
      else uiKey(k);
    }
  }
  redraw();
}

void uiSerialLine(const char* line) {
  lastInput = now;
  redraw();
  if (cur() == SC_INTRO) introEnd();
  if (cur() == SC_CHAT && line[0] != '/' && line[0] != '\\') { chatSend(line); return; }
  if (cur() == SC_INPUT) { leSet(inEd, line); inputFinish(); return; }
  const char* l = line[0] == '/' || line[0] == '\\' ? line + 1 : line;
  if (cur() != SC_DOS) go(SC_DOS);
  leSet(dosIn, l);
  dosRun();
}

void uiSetLang(bool ua) { langUa = ua; uiToast(ua ? "KEYS: UA" : "KEYS: EN"); }

// ======================================================================= network answers
void uiNetResult(uint8_t job, int status, const char* body, size_t len) {
  redraw();
  if (job == JOB_ONLINE) {
    if (status != 200) { onErr = status; return; }
    OnlineSnap* next = on == &onA ? &onB : &onA;
    if (!parseOnline(body, len, *next, CFG.name)) { onErr = -2; return; }
    onErr = 0;
    int changes = 0;
    if (onHave) changes = diffOnline(*on, *next, onEv, now);
    on = next;
    onHave = true;
    onAt = now;
    if (changes) {
      glitch(3);
      blink(300);
      if (CFG.toasts)
        for (int i = changes - 1; i >= 0 && i < onEv.n; i--) {
          char t[48]; const OnEvent& e = onEv.e[i];
          snprintf(t, sizeof(t), "%s%s %s", e.joined ? "+" : "-", e.name, e.dos);
          uiToast(t);
        }
    }
    return;
  }
  if (job == JOB_CHAT || job == JOB_CHAT_BG) {
    if (status != 200) { chatErr = status; return; }
    uint32_t had = chat.lastId;
    bool firstLoad = had == 0;
    int added = parseChatLog(body, len, chat);
    if (added < 0) { chatErr = -2; return; }
    chatErr = 0;
    if (cur() == SC_CHAT) chat.readId = chat.lastId;
    else if (firstLoad) chat.readId = chat.lastId;        // old messages aren't news
    if (added > 0 && !firstLoad) {
      int fromOthers = 0;
      for (int i = 0; i < chat.n; i++) if (chat.m[i].id > had && strcmp(chat.m[i].name, CFG.name)) fromOthers++;
      if (fromOthers) {
        blink(500);
        if (cur() != SC_CHAT && CFG.toasts) {
          for (int i = 0; i < chat.n; i++)
            if (chat.m[i].id > had && strcmp(chat.m[i].name, CFG.name)) {
              char t[60]; snprintf(t, sizeof(t), "%s:%s", chat.m[i].name, chat.m[i].text);
              uiToast(t);
            }
          glitch(1);
        }
      }
    }
    return;
  }
  if (job == JOB_SAY) {
    if (status == 429 && npend) { sayRetryAt = now + 900; return; }     // too fast: send it again in a moment
    if (npend) { memmove(pend[0], pend[1], sizeof(pend[0]) * 3); npend--; }
    if (status == 200) { parseChatSay(body, len, chat); if (cur() == SC_CHAT) chat.readId = chat.lastId; tChat = now - 2000; }
    else { char t[32]; snprintf(t, sizeof(t), status == 429 ? "TOO FAST" : "NOT SENT (%d)", status); uiToast(t); }
    return;
  }
}

// ======================================================================= boot: ERROR-BIOS
static int bootStep = 0;
static uint32_t bootNext = 0, bootMem = 0;
static void bootTick() {
  if (now < bootNext) return;
  char t[64];
  switch (bootStep) {
    case 0: cprint("ERROR-BIOS " OLED_VERSION); glitch(2); bootNext = now + 300; break;
    case 1: cprint(S->chipInfo()); bootNext = now + 200; break;
    case 2: cprint("MEMORY 0K"); bootMem = 0; bootNext = now + 20; break;
    case 3: {
      uint32_t total = S->freeHeap() / 1024;
      bootMem += total / 12 + 1;
      if (bootMem < total) { snprintf(cl[clN - 1].t, sizeof(cl[0].t), "MEMORY %uK", (unsigned)bootMem); bootNext = now + 25; redraw(); return; }
      snprintf(cl[clN - 1].t, sizeof(cl[0].t), "MEMORY %uK OK", (unsigned)total);
      S->serialOut(cl[clN - 1].t);
      bootNext = now + 200;
      break;
    }
    case 4: snprintf(t, sizeof(t), "OLED %dX%d", W, H); cprint(t); bootNext = now + 150; break;
    case 5: snprintf(t, sizeof(t), "WIFI %s", CFG.ssid[0] ? CFG.ssid : "NOT SET"); cprint(t); bootNext = now + 150; break;
    case 6: snprintf(t, sizeof(t), "BT %d PAIRED", S->btBonds()); cprint(t); bootNext = now + 150; break;
    case 7: cprint("STARTING ERROR-DOS..."); bootNext = now + 500; break;
    case 8: cprint("C:\\>"); bootNext = now + 300; break;
    case 9: case 10: case 11: {
      const char* w = "WHO";
      int k = bootStep - 8;
      snprintf(cl[clN - 1].t, sizeof(cl[0].t), "C:\\>%.*s", k, w);
      bootNext = now + 120;
      redraw();
      break;
    }
    default:
      glitch(1);
      homeScreen();
      if (!CFG.ssid[0] && S->netState() != NET_UP) go(SC_WIFI);
      return;
  }
  bootStep++;
  redraw();
}


// ======================================================================= the 3D welcome
// When the box starts: the screen switches on like an old CRT, stars rush past, "ERROR" in
// solid 3D block letters flies in spinning, "error.blue" types itself underneath and a
// LOADING ERROR-DOS bar fills up. Then the ERROR-BIOS text boot starts. Any key or button skips it.
#define INTRO_MS 4900

// block letters on a 4 x 5 grid, in tenths (y up). Each letter: outline + holes.
static const int8_t L_E[] = {0,0, 40,0, 40,10, 10,10, 10,20, 30,20, 30,30, 10,30, 10,40, 40,40, 40,50, 0,50};
static const int8_t L_R[] = {0,0, 10,0, 10,20, 18,20, 29,0, 41,0, 29,21, 40,26, 40,50, 0,50};
static const int8_t L_RH[] = {10,30, 30,30, 30,40, 10,40};
static const int8_t L_O[] = {7,0, 33,0, 40,7, 40,43, 33,50, 7,50, 0,43, 0,7};
static const int8_t L_OH[] = {10,10, 30,10, 30,40, 10,40};
struct LPoly { const int8_t* p; uint8_t n; };
struct Letter { LPoly poly[2]; uint8_t np; };
static const Letter LET_E = {{{L_E, 12}, {nullptr, 0}}, 1};
static const Letter LET_R = {{{L_R, 10}, {L_RH, 4}}, 2};
static const Letter LET_O = {{{L_O, 8}, {L_OH, 4}}, 2};
static const Letter* const WORD[5] = {&LET_E, &LET_R, &LET_R, &LET_O, &LET_R};

static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
static void dpix(int x, int y, int level) {          // a pixel of a grey (level 0..16)
  if (x < 0 || y < 0 || x >= W || y >= H) return;
  if (BAYER[y & 3][x & 3] < level) u8g2_DrawPixel(G, x, y);
}
static void ditherBox(int x0, int y0, int w, int h, int level) {
  for (int y = y0; y < y0 + h; y++) for (int x = x0; x < x0 + w; x++) dpix(x, y, level);
}
// fade the whole picture towards black (level 16 = untouched, 0 = black)
static void fadeBuffer(int level) {
  uint8_t* buf = u8g2_GetBufferPtr(G);
  int tw = u8g2_GetBufferTileWidth(G) * 8;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      if (BAYER[y & 3][x & 3] >= level) buf[(y / 8) * tw + x] &= ~(1 << (y & 7));
}
static void cline(int x0, int y0, int x1, int y1) {
  if ((x0 < 0 && x1 < 0) || (y0 < 0 && y1 < 0) || (x0 >= W && x1 >= W) || (y0 >= H && y1 >= H)) return;
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
  for (int guard = 0; guard < 400; guard++) {
    if (x0 >= 0 && y0 >= 0 && x0 < W && y0 < H) u8g2_DrawPixel(G, x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * e;
    if (e2 >= dy) { e += dy; x0 += sx; }
    if (e2 <= dx) { e += dx; y0 += sy; }
  }
}
static float easeOut(float t) { if (t < 0) return 0; if (t > 1) return 1; t = 1 - t; return 1 - t * t * t; }

// fill the polygons (even-odd, so holes stay open) with a grey
static void fillPolys(const float* xs, const float* ys, const uint8_t* cnt, int np, int level) {
  float ymin = 1e9, ymax = -1e9;
  int total = 0;
  for (int k = 0; k < np; k++) total += cnt[k];
  for (int i = 0; i < total; i++) { if (ys[i] < ymin) ymin = ys[i]; if (ys[i] > ymax) ymax = ys[i]; }
  int y0 = (int)ceilf(ymin - 0.5f), y1 = (int)floorf(ymax - 0.5f);
  if (y0 < 0) y0 = 0;
  if (y1 > H - 1) y1 = H - 1;
  for (int y = y0; y <= y1; y++) {
    float yc = y + 0.5f, xi[32];
    int n = 0, base = 0;
    for (int k = 0; k < np; k++) {
      for (int i = 0; i < cnt[k]; i++) {
        int j = (i + 1) % cnt[k];
        float ax = xs[base + i], ay = ys[base + i], bx = xs[base + j], by = ys[base + j];
        if ((ay <= yc && by > yc) || (by <= yc && ay > yc)) if (n < 32) xi[n++] = ax + (yc - ay) * (bx - ax) / (by - ay);
      }
      base += cnt[k];
    }
    for (int a = 1; a < n; a++) { float v = xi[a]; int b = a - 1; while (b >= 0 && xi[b] > v) { xi[b + 1] = xi[b]; b--; } xi[b + 1] = v; }
    for (int a = 0; a + 1 < n; a += 2)
      for (int x = (int)ceilf(xi[a] - 0.5f); x <= (int)floorf(xi[a + 1] - 0.5f); x++) dpix(x, y, level);
  }
}

// "ERROR" in 3D: yaw/pitch in radians, dist = how far away (units), cy = screen centre y
static void drawWord(float yaw, float pitch, float dist, float cx, float cy, float F) {
  float cyw = cosf(yaw), syw = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
  const float D = 0.7f;                                       // half the depth of the letters
  float facing = cyw * cp;                                    // > 0: the front faces us
  float af = fabsf(facing);
  int level = af > 0.9f ? 16 : (int)(af * 16);                // turned away = darker grey
  if (level < 3) level = 3;
  for (int li = 0; li < 5; li++) {
    const Letter& L = *WORD[li];
    float fx[24], fy[24], bx[24], by[24];
    uint8_t cnt[2];
    int k = 0;
    for (int pi = 0; pi < L.np; pi++) {
      cnt[pi] = L.poly[pi].n;
      for (int i = 0; i < L.poly[pi].n; i++, k++) {
        float X = (L.poly[pi].p[i * 2] + li * 52 - 124) / 10.0f, Y = (L.poly[pi].p[i * 2 + 1] - 25) / 10.0f;
        for (int side = 0; side < 2; side++) {
          float Z = side ? D : -D;
          float x1 = X * cyw + Z * syw, z1 = -X * syw + Z * cyw;
          float y2 = Y * cp - z1 * sp, z2 = Y * sp + z1 * cp + dist;
          if (z2 < 0.5f) z2 = 0.5f;
          float sx = cx + x1 * F / z2, sy = cy - y2 * F / z2;
          if (side) { bx[k] = sx; by[k] = sy; } else { fx[k] = sx; fy[k] = sy; }
        }
      }
    }
    // the face turned towards us is solid; the other one and the sides are lines behind it
    float* vx = facing >= 0 ? fx : bx; float* vy = facing >= 0 ? fy : by;
    float* hx = facing >= 0 ? bx : fx; float* hy = facing >= 0 ? by : fy;
    u8g2_SetDrawColor(G, 1);
    int base = 0;
    for (int pi = 0; pi < L.np; pi++) {
      for (int i = 0; i < cnt[pi]; i++) {
        int a = base + i, b = base + (i + 1) % cnt[pi];
        cline((int)hx[a], (int)hy[a], (int)hx[b], (int)hy[b]);
        cline((int)vx[a], (int)vy[a], (int)hx[a], (int)hy[a]);
      }
      base += cnt[pi];
    }
    // clear the face area first so lines behind it don't show through, then fill it grey and outline it
    u8g2_SetDrawColor(G, 0);
    fillPolys(vx, vy, cnt, L.np, 17);
    u8g2_SetDrawColor(G, 1);
    fillPolys(vx, vy, cnt, L.np, level);
    base = 0;
    for (int pi = 0; pi < L.np; pi++) {
      for (int i = 0; i < cnt[pi]; i++) {
        int a = base + i, b = base + (i + 1) % cnt[pi];
        cline((int)vx[a], (int)vy[a], (int)vx[b], (int)vy[b]);
      }
      base += cnt[pi];
    }
  }
}

struct Star { float x, y, z; };
static Star stars[44];
static void starReset(Star& s, bool anywhere) {
  s.x = (rand() % 2001 - 1000) / 1000.0f;
  s.y = (rand() % 2001 - 1000) / 1000.0f;
  s.z = anywhere ? 0.08f + (rand() % 920) / 1000.0f : 1.0f;
}

static void drawIntro() {
  uint32_t t = now - introAt;
  float dt = (now - introLast) / 1000.0f;
  if (dt > 0.1f || introLast == 0) dt = 0.03f;
  introLast = now;
  float cx = W / 2.0f, cyText = H * 0.40f;
  float F = W * 0.42f * 30.0f / 12.4f;                      // "ERROR" fills ~85% of the width when it lands
  u8g2_SetDrawColor(G, 1);

  // 1. the CRT switches on: a dot becomes a line, the line opens into a white flash that fades
  if (t < 130) {
    int w = (int)(W * t / 130.0f) | 1;
    u8g2_DrawHLine(G, (W - w) / 2, H / 2, w);
    return;
  }
  if (t < 320) {
    float p = (t - 130) / 190.0f;
    int h = (int)(H * p) | 1;
    ditherBox(0, (H - h) / 2, W, h, (int)(16 * (1 - p)) + 1);
  }

  // 2. stars rushing past
  if (!starsReady) { for (auto& s : stars) starReset(s, true); starsReady = true; }
  if (t > 200 && t < 2900) {
    float speed = t < 1200 ? 0.4f + (t - 200) / 1000.0f * 1.6f : 2.0f - (t - 1200) / 1700.0f * 1.7f;
    int show = t < 2100 ? 44 : (int)(44 * (2900 - t) / 800.0f);
    for (int i = 0; i < 44; i++) {
      Star& s = stars[i];
      float zOld = s.z;
      s.z -= speed * dt;
      if (s.z < 0.06f) { starReset(s, false); continue; }
      if (i >= show) continue;
      float k1 = 1 / s.z, k0 = 1 / zOld;
      int x1 = (int)(cx + s.x * k1 * W * 0.22f), y1 = (int)(H / 2 + s.y * k1 * H * 0.30f);
      int x0 = (int)(cx + s.x * k0 * W * 0.22f), y0 = (int)(H / 2 + s.y * k0 * H * 0.30f);
      if (x1 < -4 || x1 > W + 4 || y1 < -4 || y1 > H + 4) { starReset(s, false); continue; }
      if (speed > 1.0f) cline(x0, y0, x1, y1); else if (x1 >= 0 && y1 >= 0 && x1 < W && y1 < H) u8g2_DrawPixel(G, x1, y1);
    }
  }

  // 3. "WELCOME TO" while the word flies in
  if (t > 450 && t < 2250) {
    static const char* w = "WELCOME TO";
    int n = (t - 450) / 60; if (n > 10) n = 10;
    int x = (W - 10 * CW) / 2;
    for (int i = 0; i < n; i++) u8g2_DrawGlyph(G, x + i * CW, ASC, (uint8_t)w[i]);
  }

  // 4. "ERROR" in 3D: spins in from far away, then sways gently
  if (t > 500) {
    float yaw, pitch, dist;
    if (t < 2300) {
      float p = easeOut((t - 500) / 1800.0f);
      dist = 260 - 230 * p;
      yaw = (1 - p) * 3 * 3.14159f;
      pitch = (1 - p) * 0.9f * sinf(t / 180.0f);
    } else {
      float u = (t - 2300) / 1000.0f;
      dist = 30;
      yaw = 0.32f * sinf(u * 2.2f) * (u < 0.5f ? u * 2 : 1);
      pitch = 0.13f * sinf(u * 1.5f) * (u < 0.5f ? u * 2 : 1);
    }
    drawWord(yaw, pitch, dist, cx, cyText, F);
    if (t > 2300 && t < 2390) glitch(2);                     // it lands with a flicker
  }

  // 5. error.blue types itself in, with a blinking cursor
  int capY = (int)(H * 0.66f);
  if (t > 2450) {
    static const char* d = "error.blue";
    int n = (t - 2450) / 75; if (n > 10) n = 10;
    int x = (W - 10 * CW) / 2;
    for (int i = 0; i < n; i++) u8g2_DrawGlyph(G, x + i * CW, capY + ASC, (uint8_t)d[i]);
    if ((n < 10 || (t / 260) % 2) && t < 4500) u8g2_DrawBox(G, x + n * CW, capY, CW - 1, CH - 1);
  }

  // 6. the loading bar, with the text drawn inverted over the part that's filled
  if (t > 3100) {
    int bh = CH + 1, by = H - bh, bw = W;
    float p = (t - 3100) / 1300.0f;
    if (p > 1) p = 1;
    float q = p < 0.3f ? p * 1.4f : p < 0.45f ? 0.42f : p < 0.8f ? 0.42f + (p - 0.45f) * 1.3f : 0.875f + (p - 0.8f) * 0.625f;   // stutters like a real load
    if (q > 1) q = 1;
    u8g2_DrawFrame(G, 0, by, bw, bh);
    u8g2_DrawBox(G, 1, by + 1, (int)((bw - 2) * q), bh - 2);
    const char* txt = W >= 18 * CW ? "LOADING ERROR-DOS" : "LOADING DOS";
    int tw = strlen(txt) * CW;
    u8g2_SetDrawColor(G, 2);
    u8g2_DrawStr(G, (bw - tw) / 2, by + 1 + ASC, txt);
    u8g2_SetDrawColor(G, 1);
  }

  // 7. out: torn bands, then a dithered fade to black
  if (t > 4500) {
    float p = (t - 4500) / 400.0f;
    if (p > 1) p = 1;
    fadeBuffer((int)(16 * (1 - p)));
  }
}

static void introEnd() {
  introLast = 0;
  starsReady = false;
  if (introReplay) { introReplay = false; back(); }
  else { stk[0] = SC_BOOT; sp = 0; bootNext = now + 150; }
  redraw();
}

// ======================================================================= drawing + timers
static void applyGlitch() {
  if (!glitchUntil) return;
  if (now > glitchUntil) { glitchUntil = 0; return; }
  uint8_t* buf = u8g2_GetBufferPtr(G);
  int tw = u8g2_GetBufferTileWidth(G) * 8, th = u8g2_GetBufferTileHeight(G);
  srand(glitchSeed + (now / 40));
  int page = rand() % th;
  uint8_t* row = buf + page * tw;
  switch (glitchKind) {
    case 1: {                             // tear: a band slides sideways
      int k = 1 + rand() % 5;
      memmove(row + k, row, tw - k);
      memset(row, 0, k);
      break;
    }
    case 2:                               // a band flips colours
      for (int x = 0; x < tw; x++) row[x] ^= 0xFF;
      break;
    case 3: {                             // static
      for (int x = 0; x < tw; x++) row[x] ^= (uint8_t)(rand() & rand());
      int p2 = rand() % th;
      int k = 1 + rand() % 3;
      memmove(buf + p2 * tw + k, buf + p2 * tw, tw - k);
      break;
    }
  }
}

static void drawPointer() {
  if (!pAt || now - pAt > 4000) return;
  static const char* arrow = "#..../##.../###../####./#####/##.../#.#..";
  u8g2_SetDrawColor(G, 2);
  int y = 0;
  for (const char* p = arrow; *p; y++) {
    for (int x = 0; *p && *p != '/'; x++, p++) if (*p == '#') u8g2_DrawPixel(G, px + x, py + y);
    if (*p == '/') p++;
  }
  u8g2_SetDrawColor(G, 1);
}

static void drawPasskey(uint32_t pk) {
  u8g2_SetDrawColor(G, 0);
  u8g2_DrawBox(G, 0, 0, W, H);
  u8g2_SetDrawColor(G, 1);
  u8g2_DrawFrame(G, 0, 0, W, H);
  gclear();
  gcenter(0, "TYPE ON KEYBOARD");
  gcenter(ROWS - 1, "THEN ENTER");
  drawGrid();
  char d[12];
  snprintf(d, sizeof(d), "%06u", (unsigned)(pk % 1000000));
  u8g2_SetFont(G, W > 100 ? u8g2_font_10x20_tn : u8g2_font_7x14B_tn);
  int w = u8g2_GetStrWidth(G, d);
  u8g2_DrawStr(G, (W - w) / 2, H / 2 + (W > 100 ? 8 : 6), d);
  layout();
}

static void render() {
  if (cur() == SC_INTRO) {
    u8g2_ClearBuffer(G);
    drawIntro();
    applyGlitch();
    u8g2_SendBuffer(G);
    return;
  }
  gclear();
  switch (cur()) {
    case SC_BOOT: drawDos(false); break;
    case SC_WHO: drawWho(); break;
    case SC_INFO: drawInfo(); break;
    case SC_MENU: drawMenu(); break;
    case SC_DOS: drawDos(true); break;
    case SC_CHAT: drawChat(); break;
    case SC_QUICK: drawQuick(); break;
    case SC_WIFI: drawWifi(); break;
    case SC_PORTAL: drawPortal(); break;
    case SC_BT: drawBt(); break;
    case SC_SETUP: drawSetup(); break;
    case SC_INPUT: drawInput(); break;
    case SC_LOG: drawLog(); break;
    case SC_INTRO: break;
  }
  // a pop-up on the bottom row
  if (toastN) {
    if (now - toastAt > 3200) {
      memmove(toastQ[0], toastQ[1], sizeof(toastQ[0]) * 3);
      toastN--;
      toastAt = now;
    }
    if (toastN) {
      int r = ROWS - 1;
      for (int c = 0; c < COLS; c++) { gc[r][c] = ' '; ga[r][c] = A_INV; }
      const char* t = toastQ[0];
      int n = u8len(t);
      // long ones scroll
      int shift = 0;
      if (n > COLS) { int s = (now - toastAt) / 180 - 4; if (s < 0) s = 0; shift = s > n - COLS ? n - COLS : s; }
      gput(r, 0, t + u8off(t, shift), A_INV, COLS);
    }
  }
  u8g2_ClearBuffer(G);
  drawGrid();
  if (cur() != SC_DOS && cur() != SC_BOOT) statusIcons();
  drawPointer();
  uint32_t pk = S->btPasskey();
  if (pk) drawPasskey(pk);
  applyGlitch();
  u8g2_SendBuffer(G);
}

void uiBegin(u8g2_t* g, Sys* sys) {
  G = g;
  S = sys;
  now = S->ms();
  srand(now ^ 0x5EED);
  chatReset(chat, CFG.channel);
  memset(&onA, 0, sizeof(onA));
  memset(&onB, 0, sizeof(onB));
  memset(&onEv, 0, sizeof(onEv));
  uiApplyDisplay();
  sp = 0;
  stk[0] = CFG.intro ? SC_INTRO : SC_BOOT;
  introAt = now;
  bootNext = now + 200;
  lastInput = now;
  nextGlitch = now + 15000 + rand() % 30000;
}

void uiTick() {
  now = S->ms();
  if (cur() == SC_INTRO && now - introAt >= INTRO_MS) introEnd();
  if (cur() == SC_BOOT) bootTick();
  // network: refresh what's on screen more often
  if (netUp()) {
    Scr s = cur();
    uint32_t onEvery = (s == SC_WHO || s == SC_INFO) ? CFG.poll * 1000u : 30000u;
    if (!S->netBusy(JOB_ONLINE) && (!tOnlineSet || now - tOnline >= onEvery)) { S->fetchOnline(); tOnline = now; tOnlineSet = true; }
    if (CFG.channel[0]) {
      bool fg = s == SC_CHAT || s == SC_QUICK;
      uint32_t every = fg ? 3000u : 30000u;
      if (!S->netBusy(JOB_CHAT) && !S->netBusy(JOB_CHAT_BG) && (tChat == 0 || now - tChat >= every)) {
        if (strcmp(chat.code, CFG.channel)) chatReset(chat, CFG.channel);
        S->fetchChat(CFG.channel, chat.lastId, !fg);
        tChat = now ? now : 1;
      }
    }
    if (CFG.beacon && (tBeacon == 0 || now - tBeacon >= 25000)) { S->beacon(); tBeacon = now ? now : 1; }
  }
  if (btWant[0]) {
    BtDev d[10];
    int n = S->btResults(d, 10);
    for (int i = 0; i < n; i++)
      if (hasText(d[i].name, btWant)) { cprintf("PAIRING %s", d[i].name); S->btPair(d[i].addr); btWant[0] = 0; break; }
    if (btWant[0] && (int32_t)(now - btWantUntil) > 0) { cprintf("%s NOT FOUND", btWant); btWant[0] = 0; }
  }
  {
    static uint32_t lastPk = 0;
    static uint8_t lastNet = 255;
    static bool saidWho = false;
    static int lastErr = 0;
    static char lastBt[28] = "";
    uint32_t pk = S->btPasskey();
    if (pk != lastPk) { lastPk = pk; if (pk) { char t[48]; snprintf(t, sizeof(t), "KEYBOARD CODE: TYPE %06u + ENTER", (unsigned)(pk % 1000000)); S->serialOut(t); } }
    uint8_t ns = S->netState();
    if (ns != lastNet) {
      lastNet = ns;
      char t[64];
      if (ns == NET_UP) { snprintf(t, sizeof(t), "WIFI ON %s %s", S->netSsid(), S->netIp()); S->serialOut(t); snprintf(t, sizeof(t), "SETUP PAGE: http://%s/", S->netIp()); }
      else if (ns == NET_CONNECTING) snprintf(t, sizeof(t), "WIFI JOINING %s", S->netSsid());
      else if (ns == NET_FAIL) snprintf(t, sizeof(t), "WIFI FAILED %s", S->netSsid());
      else t[0] = 0;
      if (t[0]) S->serialOut(t);
    }
    if (onHave && !saidWho) { saidWho = true; char t[40]; snprintf(t, sizeof(t), "WHO: %u ONLINE", (unsigned)on->total); S->serialOut(t); }
    if (onErr != lastErr) { lastErr = onErr; if (onErr) { char t[80]; snprintf(t, sizeof(t), "NET ERROR %d %s", onErr, S->netError()); S->serialOut(t); } }
    const char* bs = S->btStatus();
    if (strcmp(bs, lastBt)) { scopy(lastBt, sizeof(lastBt), bs); if (bs[0]) { char t[40]; snprintf(t, sizeof(t), "BT %s", bs); S->serialOut(t); } }
  }
  if (sayRetryAt && now >= sayRetryAt) { sayRetryAt = 0; if (npend && netUp()) S->sendChat(CFG.channel, pend[0]); }
  // the LED
  if (ledUntil && now > ledUntil) { ledUntil = 0; S->led(false); }
  // random glitches
  if (CFG.glitch && now > nextGlitch) {
    glitch(1 + rand() % 3);
    nextGlitch = now + 20000 + rand() % 50000;
  }
  // burn-in care: move the picture a pixel now and then, dim when nobody uses it
  if (CFG.saver) {
    uint32_t minute = now / 60000;
    int nx = spareX ? (int)(minute % (spareX + 1)) : 0;
    int ny = spareY ? (int)((minute / 2) % (spareY + 1)) : 0;
    if (nx != X0 || ny != Y0) { X0 = nx; Y0 = ny; redraw(); }
    static bool dim = false;
    bool want = now - lastInput > 10 * 60000u && !toastN;
    if (want != dim) { dim = want; u8g2_SetContrast(G, dim ? (CFG.contrast > 8 ? 8 : 1) : CFG.contrast); }
  }
  // draw: right away when something changed, else 4x a second (clock, cursor, paging)
  uint32_t every = cur() == SC_INTRO ? 25 : glitchUntil || toastN || (pAt && now - pAt < 4000) ? 40 : 250;
  if (now - lastDraw >= (dirty ? 30u : every)) {
    lastDraw = now;
    dirty = false;
    kbOn = false;
    BtDev d[3];
    int n = S->btConnected(d, 3);
    for (int i = 0; i < n; i++) if (d[i].type & BT_KEYBOARD) kbOn = true;
    render();
  }
}
