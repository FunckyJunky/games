// What ERROR-OLED knows: settings, who is online where, the chat, recent comings and goings.
// No Arduino code here (only ArduinoJson), so it also runs in the PC simulator.
#pragma once
#include <stdint.h>
#include <stddef.h>

#define NAME_B 34          // 16 letters; Cyrillic letters take 2 bytes each

struct Settings {
  char name[NAME_B];       // your handle in ERROR-CHAT and on the WHO list
  char channel[9];         // ERROR-CHAT channel code (3-8 letters/digits)
  char ssid[33];
  char pass[65];
  uint8_t font;            // 0 = small (more text), 1 = big
  uint8_t contrast;        // OLED brightness 1..255
  uint8_t poll;            // seconds between WHO refreshes while you look at it
  uint8_t keymap;          // keyboard layout: 0 US, 1 DE, 2 FR
  uint8_t whoByGame;       // WHO list: 0 = one line per person, 1 = grouped by game
  uint8_t flip;            // screen upside down
  uint8_t glitch;          // ERROR-DOS style screen glitches
  uint8_t toasts;          // pop-ups when someone comes online / writes
  uint8_t beacon;          // show this box as online on error.blue
  uint8_t tlsCheck;        // check error.blue's certificate
  uint8_t saver;           // burn-in protection: shift the picture, dim when idle
};
void settingsDefaults(Settings& s);

// ---- GET /api/online
struct OnGame {
  char id[28];
  char title[64];          // full name (UTF-8, can be Cyrillic)
  char dos[9];             // 8-letter DOS name, as in the ERROR-DOS menu
  char folder[8];          // games / online / apps / menu
  uint8_t n;
};
struct OnPlayer {
  char name[NAME_B];
  char detail[52];         // e.g. S01E04 (Вгадай що?!) or FLIGHT.EXE (glasses)
  uint32_t secs;           // how long they've been in this game
  uint8_t game;            // index into OnlineSnap.games
};
#define ON_MAX_GAMES 20
#define ON_MAX_PLAYERS 48
struct OnlineSnap {
  uint8_t nGames, nPlayers;
  uint16_t total;
  OnGame games[ON_MAX_GAMES];
  OnPlayer players[ON_MAX_PLAYERS];
};
// hideName: leave out this box itself (game "error-oled" with this name)
bool parseOnline(const char* json, size_t len, OnlineSnap& out, const char* hideName);

// someone came or went
struct OnEvent {
  uint32_t atMs;
  uint8_t joined;
  char name[NAME_B];
  char dos[9];
};
#define ON_MAX_EVENTS 16
struct OnEvents {
  OnEvent e[ON_MAX_EVENTS];  // newest first
  uint8_t n;
};
// compares two lists; adds to ev and returns how many changed
int diffOnline(const OnlineSnap& before, const OnlineSnap& after, OnEvents& ev, uint32_t nowMs);

// ---- ERROR-CHAT
struct ChatMsg {
  uint32_t id;
  uint32_t at;             // seconds since 1970 (from the server)
  char name[NAME_B];
  char text[420];
};
#define CHAT_MAX 24
struct ChatLog {
  char code[9];            // channel these messages belong to
  ChatMsg m[CHAT_MAX];     // oldest first
  uint8_t n;
  uint32_t lastId;         // newest message id we have
  uint32_t readId;         // newest message you've seen on the CHAT screen
  char who[220];           // "Vika, Vito, OLED"
  uint8_t whoN;
  uint32_t serverNow;      // server time (s) of the last answer
};
void chatReset(ChatLog& c, const char* code);
// GET /api/chat/<CODE>/log  -> adds new messages; returns how many, -1 on bad JSON
int parseChatLog(const char* json, size_t len, ChatLog& c);
// POST /api/chat/<CODE>/say -> the saved message; returns 1 if added
int parseChatSay(const char* json, size_t len, ChatLog& c);
int chatUnread(const ChatLog& c, const char* me);
