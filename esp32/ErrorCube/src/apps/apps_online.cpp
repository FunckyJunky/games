// apps_online.cpp: WHO (who's online on error.blue), CHAT (ERROR-CHAT), LINK (the phone control page)
#include "../ui/ui.h"
#include "../lib/cubeqr.h"
#include <string.h>
#include <stdio.h>

static void ago(char* out, size_t n, uint32_t at) {
  if (!at) { snprintf(out, n, "..."); return; }
  uint32_t s = (msNow() - at) / 1000;
  if (s < 5) snprintf(out, n, "NOW"); else if (s < 120) snprintf(out, n, "%us AGO", (unsigned)s); else snprintf(out, n, "%um AGO", (unsigned)(s / 60));
}
static void noNet(int y) {
  const char* m = !cfg.wifiOn ? "WIFI IS OFF" : S.net == NET_UP ? "TALKING TO ERROR.BLUE..." : S.net == NET_JOINING ? "JOINING WIFI..." : "NO WIFI: SEE WIFI.EXE";
  drawTextC(FT, 240, y, m, C_DIM, TX_FLAT);
}

// ================================================================== WHO
struct Who : Screen {
  WhoList w; ListView L; bool have = false;
  const char* name() override { return "WHO.EXE"; }
  void enter() override { netSetFocus(FOCUS_WHO); L.setBox(15, 92, 450, 312); L.rowH = 52; }
  void leave() override { netSetFocus(FOCUS_NONE); }
  void tick() override {
    static uint32_t last = 0;
    static WhoList& t = *(WhoList*)calloc(1, sizeof(WhoList));
    if (netWho(t) && t.at != last) { last = t.at; w = t; have = true; L.n = w.ng; uiDirty(); }
    L.tick();
    redrawIn(1000);
  }
  void draw() override {
    char p[40]; snprintf(p, sizeof(p), "C:\\ONLINE\\WHO>");
    drawText(FT, colX(0), TOP_Y, p, C_FG);
    if (have) {
      char a[24]; ago(a, sizeof(a), w.at);
      char t[40]; snprintf(t, sizeof(t), "%d ON  %s", w.total, a);
      drawTextR(FT, 465, TOP_Y, t, C_DIM, TX_FLAT);
    }
    if (!have || !w.ok) { noNet(220); fkeysBack(); return; }
    if (!w.ng) {
      drawTextC(FT, 240, 200, "NOBODY IS ONLINE", C_FG);
      drawTextC(FT, 240, 236, "(except you)", C_DIM, TX_FLAT);
    }
    L.n = w.ng;
    L.draw([&](int i, int x, int y, int ww, int h, bool sel) {
      WhoGame& g = w.g[i];
      int ty = y + (h - LH) / 2;
      if (sel) invBar(x + 2, y + 3, ww - 4, h - 6);
      uint16_t c = sel ? C_BG : C_FG;
      char nm[16]; snprintf(nm, sizeof(nm), "%-8s", g.dos[0] ? g.dos : "?");
      drawText(FT, x + 10, ty, nm, c, sel ? TX_FLAT : TX_GLOW);
      char n[8]; snprintf(n, sizeof(n), "%2d", g.n);
      drawText(FT, x + 10 + CW * 9, ty, n, c, sel ? TX_FLAT : TX_GLOW);
      char pl[64]; snprintf(pl, sizeof(pl), "%s", g.players); utf8Cut(pl, 16);
      drawText(FT, x + 10 + CW * 12, ty, pl, sel ? C_BG : C_DIM, TX_FLAT);
    }, nullptr);
    fkeys({{"ESC", "Back", [] { uiBack(); }}, {"\xE2\x86\xB5", "Who", [this] { show(L.sel); }}});
  }
  void show(int i) {
    if (i < 0 || i >= w.ng) return;
    char t[300]; snprintf(t, sizeof(t), "%s\n%s", w.g[i].title, w.g[i].players);
    ask(w.g[i].dos, t, {"OK"}, nullptr);
  }
  bool ev(const Ev& e) override { return L.ev(e, [this](int i) { show(i); }); }
};
Screen* makeWho() { return new Who(); }

// ================================================================== CHAT
static const char* QUICK[] = {"HI!", "OK", "BRB", "GG!", "LOL", "ПРИВІТ!", "ДЯКУЮ!", "BYE!"};
struct Chat : Screen {
  ChatLog log; bool have = false, quick = false;
  Scroll sc; bool pinned = true;        // pinned: stays at the newest (bottom)
  struct L2 { std::string s; uint8_t kind; };   // 0 text, 1 name line, 2 system, 3 mine
  std::vector<L2> lines;
  const char* name() override { return "CHAT.EXE"; }
  void enter() override { netSetFocus(FOCUS_CHAT); S.chatUnread = 0; }
  void leave() override { netSetFocus(FOCUS_NONE); }
  void layout() {
    sc.x = 15; sc.y = 96; sc.w = 450; sc.h = 9 * LH; sc.view = sc.h; sc.total = lines.size() * LH;
    if (pinned) sc.pos = sc.maxPos();
    sc.clamp();
  }
  void build() {
    pinned = sc.pos >= sc.maxPos() - 2;
    lines.clear();
    for (int i = 0; i < log.n; i++) {
      ChatMsg& m = log.m[i];
      char full[260];
      if (m.sys) snprintf(full, sizeof(full), "* %s", m.text);
      else snprintf(full, sizeof(full), "<%s> %s", m.name, m.text);
      char wl[8][64];
      int n = wrapText(full, 29, wl, 8);
      for (int k = 0; k < n; k++) lines.push_back({wl[k], (uint8_t)(m.sys ? 2 : m.me ? 3 : 0)});
    }
    layout();
  }
  void tick() override {
    static uint32_t last = 0;
    static ChatLog& t = *(ChatLog*)calloc(1, sizeof(ChatLog));
    if (netChat(t) && t.at != last) {
      last = t.at; log = t; have = true; build(); S.chatUnread = 0; uiDirty();
    }
    sc.tick();
    redrawIn(1000);
  }
  void say(const char* t) { if (t && t[0]) { netChatSay(t); snd(SND_OK); pinned = true; sc.vel = 0; layout(); } }
  void draw() override {
    char p[40]; snprintf(p, sizeof(p), "#%s", cfg.chan);
    drawText(FT, colX(0), TOP_Y, p, C_FG);
    if (have && log.who[0]) { char w[64]; snprintf(w, sizeof(w), "%s", log.who); utf8Cut(w, 28 - utf8Len(p)); drawTextR(FT, 465, TOP_Y, w, C_DIM, TX_FLAT); }
    hline(15, 90, 450, C_DIM);
    if (quick) {
      for (int i = 0; i < 8; i++) {
        const char* q = QUICK[i];
        dosButton(20 + (i % 2) * 226, 100 + (i / 2) * 70, 214, 58, q, [this, q] { say(q); quick = false; uiDirty(); });
      }
    } else if (!have || !log.ok) noNet(220);
    else {
      // newest at the bottom; drag up for older ones (with the slider on the right)
      int total = lines.size();
      sc.view = sc.h; sc.total = total * LH; sc.clamp(); sc.tick();
      if (!sc.drag && !sc.gliding()) pinned = sc.pos >= sc.maxPos() - 2;
      clipSet(15, sc.y, 436, sc.h);
      int first = (int)(sc.pos / LH);
      for (int i = first; i < total && i <= first + 9; i++) {
        L2& l = lines[i];
        uint16_t c = l.kind == 2 ? C_DIM : l.kind == 3 ? mix(C_FG, C_WHITE, 35) : C_FG;
        drawText(FT, 15, sc.y + i * LH - (int)sc.pos, l.s.c_str(), c, l.kind == 2 ? TX_FLAT : TX_GLOW);
      }
      clipReset();
      sc.drawBar();
      if (!total) drawTextC(FT, 240, 200, "NO MESSAGES YET. SAY HI!", C_DIM, TX_FLAT);
    }
    // the input line
    int y = rowY(12) - 4;
    rect(15, y, 450, 38, C_FG);
    drawText(FT, 24, y + 3, "> TYPE A MESSAGE", C_DIM, TX_FLAT);
    hit(15, y, 450, 38, [this] { kbOpen("ERROR-CHAT", "", 200, [this](const char* t) { say(t); }); });
    fkeys({{"ESC", "Back", [] { uiBack(); }},
           {"Q", quick ? "Chat" : "Quick", [this] { quick = !quick; }},
           {"#", "Channel", [] { kbOpen("CHANNEL CODE", cfg.chan, 8, [](const char* t) {
              char c[9] = ""; int k = 0;
              for (const char* p = t; *p && k < 8; p++) if (isalnum((unsigned char)*p)) c[k++] = toupper((unsigned char)*p);
              if (k >= 3) { strcpy(cfg.chan, c); cfgSave(); netChatReset(); toast("CHANNEL CHANGED"); }
              else toast("3-8 LETTERS OR NUMBERS");
            }); }}});
  }
  bool ev(const Ev& e) override {
    if (quick) return false;
    if (sc.ev(e)) { pinned = false; return true; }
    if (e.t == EV_KEY && e.key == K_PREV) { sc.vel = 0; sc.pos -= 3 * LH; sc.clamp(); pinned = false; uiDirty(); return true; }
    if (e.t == EV_KEY && e.key == K_NEXT) { sc.vel = 0; sc.pos += 3 * LH; sc.clamp(); pinned = sc.pos >= sc.maxPos() - 2; uiDirty(); return true; }
    if (e.t == EV_KEY && e.key == K_OK) { kbOpen("ERROR-CHAT", "", 200, [this](const char* t) { say(t); }); return true; }
    return false;
  }
};
Screen* makeChat() { return new Chat(); }

// ================================================================== LINK
struct Link : Screen {
  const char* name() override { return "LINK.EXE"; }
  void draw() override {
    drawText(FT, colX(0), TOP_Y, "C:\\ONLINE\\LINK>", C_FG);
    if (!cfg.link) {
      drawTextC(FT, 240, 160, "THE LINK IS OFF", C_FG);
      drawTextC(FT, 240, 196, "error.blue/cube can't reach", C_DIM, TX_FLAT);
      drawTextC(FT, 240, 226, "this cube now.", C_DIM, TX_FLAT);
      dosButton(90, 280, 300, 56, "TURN IT ON", [] { cfg.link = 1; cfgSave(); netLinkState(); });
      fkeysBack();
      return;
    }
    // the QR code: error.blue/cube?code=XXXXXX opens the control page with this cube
    char url[64]; snprintf(url, sizeof(url), "https://" CUBE_HOST "/cube/?code=%s", cfg.code);
    static uint8_t qr[cubeqr_BUFFER_LEN_FOR_VERSION(6)], tmp[cubeqr_BUFFER_LEN_FOR_VERSION(6)];
    if (cubeqr_encodeText(url, tmp, qr, cubeqr_Ecc_MEDIUM, 1, 6, cubeqr_Mask_AUTO, true)) {
      int n = cubeqr_getSize(qr), sc = 200 / (n + 4), sz = sc * (n + 4), x0 = 30, y0 = 100;
      fillRect(x0, y0, sz, sz, C_FG);
      for (int y = 0; y < n; y++) for (int x = 0; x < n; x++)
        if (cubeqr_getModule(qr, x, y)) fillRect(x0 + (x + 2) * sc, y0 + (y + 2) * sc, sc, sc, C_BG);
    }
    drawText(FT, 250, 100, "LINK CODE", C_DIM, TX_FLAT);
    drawText(FB, 250, 132, cfg.code, C_FG);
    drawText(FT, 250, 186, S.link ? "CONNECTED" : S.net == NET_UP ? "CONNECTING..." : "NO WIFI", S.link ? C_FG : C_WARN);
    char p[32]; snprintf(p, sizeof(p), "%d PHONE%s", S.panels, S.panels == 1 ? "" : "S");
    if (S.link) drawText(FT, 250, 218, p, C_DIM, TX_FLAT);
    drawTextC(FT, 240, 316, "Scan it, or open", C_FG);
    drawTextC(FT, 240, 348, "error.blue/cube", C_FG);
    if (!S.link && S.net == NET_UP && S.linkErr[0]) {      // why it doesn't connect
      char e[72]; snprintf(e, sizeof(e), "%s", S.linkErr); utf8Cut(e, 36);
      drawTextC(FT, 240, 384, e, C_WARN, TX_FLAT);
    }
    fkeys({{"ESC", "Back", [] { uiBack(); }},
           {"N", "New code", [] { ask("LINK", "Make a new code? Phones with the old code lose the cube.", {"NEW CODE", "KEEP"}, [](int i) { if (!i) netLinkNewCode(); }); }},
           {"X", "Off", [] { cfg.link = 0; cfgSave(); netLinkState(); }}});
    redrawIn(1000);
  }
};
Screen* makeLink() { return new Link(); }
