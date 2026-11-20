// Reading error.blue's answers into ERROR-OLED's lists. See model.h.
#include "model.h"
#include <string.h>
#include <stdio.h>
#include <ArduinoJson.h>

// copy UTF-8 text without cutting a letter in half
static void copyText(char* dst, size_t size, const char* src) {
  if (!size) return;
  if (!src) src = "";
  size_t n = strlen(src);
  if (n >= size) {
    n = size - 1;
    while (n > 0 && ((uint8_t)src[n] & 0xC0) == 0x80) n--;
  }
  memcpy(dst, src, n);
  dst[n] = 0;
}

void settingsDefaults(Settings& s) {
  memset(&s, 0, sizeof(s));
  strcpy(s.name, "OLED");
  s.font = 0;
  s.contrast = 255;
  s.poll = 10;
  s.keymap = 0;
  s.whoByGame = 0;
  s.flip = 0;
  s.glitch = 1;
  s.toasts = 1;
  s.beacon = 1;
  s.tlsCheck = 1;
  s.saver = 1;
  s.intro = 1;
}

bool parseOnline(const char* json, size_t len, OnlineSnap& out, const char* hideName) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return false;
  JsonArray games = doc["games"];
  if (games.isNull()) return false;
  out.nGames = out.nPlayers = 0;
  out.total = 0;
  for (JsonObject g : games) {
    if (out.nGames >= ON_MAX_GAMES) break;
    const char* id = g["id"] | "";
    OnGame& G = out.games[out.nGames];
    copyText(G.id, sizeof(G.id), id);
    copyText(G.title, sizeof(G.title), g["title"] | "");
    copyText(G.dos, sizeof(G.dos), g["dos"] | "?");
    copyText(G.folder, sizeof(G.folder), g["folder"] | "");
    G.n = 0;
    bool oled = strcmp(id, "error-oled") == 0;
    JsonArray players = g["players"];
    for (JsonObject p : players) {
      const char* name = p["name"] | "guest";
      if (oled && hideName && strcmp(name, hideName) == 0) continue;     // that's us
      if (out.nPlayers >= ON_MAX_PLAYERS) break;
      OnPlayer& P = out.players[out.nPlayers++];
      copyText(P.name, sizeof(P.name), name);
      copyText(P.detail, sizeof(P.detail), p["detail"] | "");
      P.secs = p["secs"] | 0;
      P.game = out.nGames;
      G.n++;
    }
    if (G.n) { out.total += G.n; out.nGames++; }
  }
  return true;
}

static bool hasPlayer(const OnlineSnap& s, const char* name, const char* gameId) {
  for (int i = 0; i < s.nPlayers; i++)
    if (strcmp(s.players[i].name, name) == 0 && strcmp(s.games[s.players[i].game].id, gameId) == 0) return true;
  return false;
}

static void addEvent(OnEvents& ev, bool joined, const char* name, const char* dos, uint32_t now) {
  memmove(&ev.e[1], &ev.e[0], sizeof(OnEvent) * (ON_MAX_EVENTS - 1));
  OnEvent& e = ev.e[0];
  e.atMs = now;
  e.joined = joined;
  copyText(e.name, sizeof(e.name), name);
  copyText(e.dos, sizeof(e.dos), dos);
  if (ev.n < ON_MAX_EVENTS) ev.n++;
}

int diffOnline(const OnlineSnap& a, const OnlineSnap& b, OnEvents& ev, uint32_t now) {
  int n = 0;
  for (int i = 0; i < a.nPlayers; i++) {
    const OnPlayer& p = a.players[i];
    const OnGame& g = a.games[p.game];
    if (!hasPlayer(b, p.name, g.id)) { addEvent(ev, false, p.name, g.dos, now); n++; }
  }
  for (int i = 0; i < b.nPlayers; i++) {
    const OnPlayer& p = b.players[i];
    const OnGame& g = b.games[p.game];
    if (!hasPlayer(a, p.name, g.id)) { addEvent(ev, true, p.name, g.dos, now); n++; }
  }
  return n;
}

void chatReset(ChatLog& c, const char* code) {
  memset(&c, 0, sizeof(c));
  copyText(c.code, sizeof(c.code), code);
}

static bool addMsg(ChatLog& c, JsonObject m) {
  uint32_t id = m["id"] | 0;
  if (!id || id <= c.lastId) return false;
  if (c.n == CHAT_MAX) { memmove(&c.m[0], &c.m[1], sizeof(ChatMsg) * (CHAT_MAX - 1)); c.n--; }
  ChatMsg& M = c.m[c.n++];
  M.id = id;
  double at = m["at"] | 0.0;
  M.at = (uint32_t)(at / 1000.0);
  copyText(M.name, sizeof(M.name), m["name"] | "?");
  copyText(M.text, sizeof(M.text), m["text"] | "");
  c.lastId = id;
  return true;
}

int parseChatLog(const char* json, size_t len, ChatLog& c) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return -1;
  if (!doc["msgs"].is<JsonArray>()) return -1;
  const char* code = doc["code"] | "";
  if (c.code[0] && strcmp(code, c.code) != 0) return 0;     // an answer for a channel we just left
  int added = 0;
  JsonArray msgs = doc["msgs"];
  for (JsonObject m : msgs) added += addMsg(c, m);
  // the channel was wiped (7 days quiet): start over
  uint32_t last = doc["last"] | 0;
  if (last < c.lastId) { char code[9]; strcpy(code, c.code); chatReset(c, code); }
  c.who[0] = 0;
  c.whoN = 0;
  JsonArray who = doc["who"];
  for (JsonVariant w : who) {
    const char* name = w | "";
    size_t have = strlen(c.who);
    if (have + strlen(name) + 3 < sizeof(c.who)) {
      if (have) strcat(c.who, ", ");
      strcat(c.who, name);
    }
    c.whoN++;
  }
  double now = doc["now"] | 0.0;
  c.serverNow = (uint32_t)(now / 1000.0);
  return added;
}

int parseChatSay(const char* json, size_t len, ChatLog& c) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return 0;
  if (!(doc["ok"] | false)) return 0;
  return addMsg(c, doc["msg"].as<JsonObject>()) ? 1 : 0;
}

int chatUnread(const ChatLog& c, const char* me) {
  int n = 0;
  for (int i = 0; i < c.n; i++)
    if (c.m[i].id > c.readId && strcmp(c.m[i].name, me) != 0) n++;
  return n;
}
