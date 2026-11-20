// Bluetooth keyboard + mouse: ERROR-OLED is the "computer", the keyboard/mouse connect to it.
// NimBLE-Arduino 2.x. Connecting and pairing happen in their own task (they wait a lot);
// key and mouse reports go through a queue to loop(), where the screens live.
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include "hid.h"
#include "hidparse.h"
#include "ui.h"

#define MAX_PEERS 2
static const NimBLEUUID UUID_HID((uint16_t)0x1812), UUID_MAP((uint16_t)0x2A4B), UUID_REPORT((uint16_t)0x2A4D),
    UUID_PROTO((uint16_t)0x2A4E), UUID_REPREF((uint16_t)0x2908), UUID_GAP((uint16_t)0x1800), UUID_NAME((uint16_t)0x2A00);

struct Peer {
  NimBLEClient* client = nullptr;
  char addr[18] = "";
  char name[30] = "";
  uint8_t type = 0;
  HidMap map;
  volatile bool up = false;
  volatile bool dropped = false;
  KeyTracker kt;
};
static Peer peers[MAX_PEERS];

struct Rep { uint8_t peer, rid, len; uint8_t d[32]; };
static QueueHandle_t repQ;

// devices we paired with (NimBLE keeps the keys; we keep names so we can find them again)
struct Known { char addr[18]; uint8_t atype; char name[30]; };
static Known known[3];
static int knownN = 0;
static Preferences kp;

static SemaphoreHandle_t mtx;
#define MAX_FOUND 24
static BtDev found[MAX_FOUND];
static uint8_t foundType[MAX_FOUND];        // address type, + 0x80 when it says it's a keyboard/mouse
static int foundN = 0;
static BtOpts opts = {10, 1, 1, -100, 1};
static uint32_t scanStartedAt = 0;
static volatile bool userScan = false, scanning = false;
static volatile uint32_t passkey = 0;
static char status[28] = "";
static uint32_t statusAt = 0;
static bool statusSticky = false;
static char wantAddr[18];
static uint8_t wantType = 0;
static volatile uint8_t want = 0;           // 1 = pair (you picked it), 2 = reconnect a known one
static volatile int8_t evUp = -1, evDown = -1;
static bool uaKeys = false;
static bool started = false;               // Bluetooth is on
static bool taskMade = false;
static volatile uint32_t lastUse = 0;      // last scan / pairing / connection: Bluetooth goes off again when unused
static char devName[24] = "ERROR-OLED";
static char blockAddr[18] = "";            // a paired one that just refused: leave it alone for a minute
static uint32_t blockUntil = 0;

static void setStatus(const char* s, bool sticky = false) {
  strncpy(status, s, sizeof(status) - 1);
  statusAt = millis();
  statusSticky = sticky;
}
static void lock() { xSemaphoreTake(mtx, portMAX_DELAY); }
static void unlock() { xSemaphoreGive(mtx); }

static void saveKnown() {
  kp.putBytes("known", known, sizeof(known));
  kp.putUChar("knownN", knownN);
}
static void remember(const char* addr, uint8_t atype, const char* name) {
  int i = 0;
  for (; i < knownN; i++) if (!strcmp(known[i].addr, addr)) break;
  if (i == knownN) {
    if (knownN == 3) { memmove(&known[0], &known[1], sizeof(Known) * 2); knownN = 2; }
    i = knownN++;
  }
  strncpy(known[i].addr, addr, sizeof(known[i].addr) - 1);
  known[i].atype = atype;
  strncpy(known[i].name, name, sizeof(known[i].name) - 1);
  saveKnown();
}
static bool isUp(const std::string& a) {
  for (auto& P : peers) if ((P.up || P.client) && a == P.addr) return true;
  return false;
}
static int freeSlot() {
  for (int i = 0; i < MAX_PEERS; i++) if (!peers[i].client) return i;
  return -1;
}

// ------------------------------------------------------------------ scanning
class ScanCB : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    std::string a = d->getAddress().toString();
    std::string nm = d->haveName() ? d->getName() : "";
    // one of ours is back: reconnect
    bool blocked = a == blockAddr && (int32_t)(millis() - blockUntil) < 0;
    if (opts.autoReconnect && !want && !blocked && !isUp(a) && freeSlot() >= 0) {
      bool ours = false;
      for (int i = 0; i < knownN && !ours; i++) ours = a == known[i].addr || (!nm.empty() && nm == known[i].name);
      if (ours && d->isConnectable()) {
        strncpy(wantAddr, a.c_str(), sizeof(wantAddr) - 1);
        wantType = d->getAddress().getType();
        want = 2;
      }
    }
    if (!userScan) return;
    uint16_t app = d->haveAppearance() ? d->getAppearance() : 0;
    bool hid = d->isAdvertisingService(UUID_HID) || (app >= 0x03C0 && app <= 0x03CF);
    // what to list (setup page): keyboards/mice only, + anything with a name, or everything
    if (!hid && opts.show == 0) return;
    if (!hid && opts.show == 1 && (nm.empty() || !d->isConnectable())) return;
    if (d->getRSSI() < opts.minRssi) return;
    lock();
    int i = 0;
    for (; i < foundN; i++) if (a == found[i].addr) break;
    bool fresh = i == foundN;
    if (fresh && foundN == MAX_FOUND && hid) {                 // full: a keyboard pushes out a non-keyboard
      for (int k = MAX_FOUND - 1; k >= 0; k--) if (!(foundType[k] & 0x80)) { i = k; break; }
    }
    if (i == foundN && foundN < MAX_FOUND) foundN++;
    if (fresh && i < MAX_FOUND) { memset(&found[i], 0, sizeof(found[i])); foundType[i] = 0; }
    if (i < MAX_FOUND) {
      BtDev& f = found[i];
      strncpy(f.addr, a.c_str(), sizeof(f.addr) - 1);
      if (!nm.empty()) strncpy(f.name, nm.c_str(), sizeof(f.name) - 1);
      f.rssi = d->getRSSI();
      if (app == 0x03C1) f.type = BT_KEYBOARD; else if (app == 0x03C2) f.type = BT_MOUSE;
      f.bonded = NimBLEDevice::isBonded(d->getAddress());
      if (hid && f.type == BT_OTHER) f.type = BT_KEYBOARD;      // HID without saying which: probably a keyboard
      if (hid) foundType[i] |= 0x80;
      foundType[i] = (foundType[i] & 0x80) | d->getAddress().getType();
      f.hid = (foundType[i] & 0x80) ? 1 : 0;
      f.atype = d->getAddress().getType();
    }
    unlock();
  }
  void onScanEnd(const NimBLEScanResults&, int) override { scanning = false; userScan = false; }
};
static ScanCB scanCB;

static void startScan(uint32_t ms, bool user) {
  NimBLEScan* s = NimBLEDevice::getScan();
  if (s->isScanning()) s->stop();
  userScan = user;
  s->setActiveScan(user && opts.active);   // active: also ask for the names
  s->setInterval(100);
  s->setWindow(user ? 90 : 30);            // in the background: listen only a little (WiFi needs the radio too)
  s->clearResults();
  scanStartedAt = millis();
  scanning = s->start(ms, false, true);
}

// ------------------------------------------------------------------ connections
class ClientCB : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient* c, int reason) override {
    for (int i = 0; i < MAX_PEERS; i++)
      if (peers[i].client == c) { if (peers[i].up) evDown = i; peers[i].up = false; peers[i].dropped = true; }
    passkey = 0;
  }
  uint32_t onPassKeyDisplay(NimBLEConnInfo& info) override {
    passkey = 100000 + esp_random() % 900000;
    setStatus("TYPE THE CODE", true);
    return passkey;
  }
  void onConfirmPasskey(NimBLEConnInfo& info, uint32_t pin) override { NimBLEDevice::injectConfirmPasskey(info, true); }
  void onPassKeyEntry(NimBLEConnInfo& info) override { NimBLEDevice::injectPassKey(info, 0); }
  void onAuthenticationComplete(NimBLEConnInfo& info) override { passkey = 0; }
};
static ClientCB clientCB;

static void fail(NimBLEClient* c, const char* why) {
  passkey = 0;
  if (c) { strncpy(blockAddr, c->getPeerAddress().toString().c_str(), sizeof(blockAddr) - 1); blockUntil = millis() + 60000; }
  setStatus(why);
  if (c) { if (c->isConnected()) c->disconnect(); NimBLEDevice::deleteClient(c); }
}

static void connectTo(const char* addr, uint8_t atype, bool byUser) {
  int slot = freeSlot();
  if (slot < 0) { if (byUser) setStatus("2 ALREADY ON"); return; }
  NimBLEAddress A(std::string(addr), atype);
  setStatus("CONNECTING...", true);
  NimBLEClient* c = NimBLEDevice::createClient();
  if (!c) { setStatus("NO MEMORY"); return; }
  c->setClientCallbacks(&clientCB, false);
  c->setConnectTimeout(byUser ? 10000 : 6000);
  if (!c->connect(A, true, false, true)) { fail(c, byUser ? "NO ANSWER" : ""); return; }
  setStatus("PAIRING...", true);
  if (!c->secureConnection()) { fail(c, "PAIRING FAILED"); return; }
  passkey = 0;
  setStatus("READING...", true);
  NimBLERemoteService* s = c->getService(UUID_HID);
  if (!s) { fail(c, "NOT A KEYBOARD"); return; }
  Peer& P = peers[slot];
  P.map = HidMap();
  if (NimBLERemoteCharacteristic* m = s->getCharacteristic(UUID_MAP)) {
    NimBLEAttValue v = m->readValue();
    hidParseMap(v.data(), v.size(), P.map);
  }
  if (NimBLERemoteCharacteristic* pm = s->getCharacteristic(UUID_PROTO)) {     // "report" mode, not "boot"
    uint8_t one = 1;
    if (pm->canWriteNoResponse()) pm->writeValue(&one, 1, false);
  }
  P.client = c;
  P.dropped = false;
  strncpy(P.addr, addr, sizeof(P.addr) - 1);
  int subs = 0;
  for (NimBLERemoteCharacteristic* ch : s->getCharacteristics(true)) {
    if (ch->getUUID() != UUID_REPORT || !ch->canNotify()) continue;
    uint8_t rid = 0, rtype = 1;
    if (NimBLERemoteDescriptor* rr = ch->getDescriptor(UUID_REPREF)) {
      NimBLEAttValue v = rr->readValue();
      if (v.size() >= 2) { rid = v.data()[0]; rtype = v.data()[1]; }
    }
    if (rtype != 1) continue;                 // only input reports
    uint8_t pi = slot;
    bool ok = ch->subscribe(true, [pi, rid](NimBLERemoteCharacteristic*, uint8_t* d, size_t n, bool) {
      Rep r;
      r.peer = pi; r.rid = rid; r.len = n > sizeof(r.d) ? sizeof(r.d) : n;
      memcpy(r.d, d, r.len);
      xQueueSend(repQ, &r, 0);
    });
    if (ok) subs++;
  }
  if (!subs) { P.client = nullptr; fail(c, "NO KEYS FOUND"); return; }
  // its name: from the scan, our list, or ask it
  P.name[0] = 0;
  lock();
  for (int i = 0; i < foundN; i++) if (!strcmp(found[i].addr, addr)) { strncpy(P.name, found[i].name, sizeof(P.name) - 1); P.type = found[i].type; }
  unlock();
  for (int i = 0; i < knownN && !P.name[0]; i++) if (!strcmp(known[i].addr, addr)) strncpy(P.name, known[i].name, sizeof(P.name) - 1);
  if (!P.name[0]) {
    NimBLEAttValue v = c->getValue(UUID_GAP, UUID_NAME);
    size_t n = v.size() < sizeof(P.name) - 1 ? v.size() : sizeof(P.name) - 1;
    memcpy(P.name, v.data(), n); P.name[n] = 0;
  }
  if (!P.name[0]) strcpy(P.name, "HID");
  uint8_t t = (P.map.keyboard ? BT_KEYBOARD : 0) | (P.map.mouse ? BT_MOUSE : 0);
  if (t) P.type = t;
  if (!P.type) P.type = BT_KEYBOARD;
  P.kt.releaseAll();
  P.kt.ukrainian = uaKeys;
  P.up = true;
  remember(addr, atype, P.name);
  setStatus("");
  evUp = slot;
}

// Bluetooth off again: gives its ~50 KB back (HTTPS needs it). Only when nothing is paired or connected.
static void stopBle() {
  if (!started) return;
  if (NimBLEDevice::getScan()->isScanning()) NimBLEDevice::getScan()->stop();
  for (auto& P : peers) { P.client = nullptr; P.up = false; P.dropped = false; }
  NimBLEDevice::deinit(true);
  started = false;
  scanning = false;
  userScan = false;
}

static void hidTask(void*) {
  uint32_t lastBg = 0;
  for (;;) {
    if (!started) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
    // Bluetooth takes ~70 KB: with it on there's no room left for HTTPS (WHO and CHAT stop working).
    // So it goes off again when nothing is connected: 45 s after a scan, or 60 s after start-up when
    // a paired keyboard doesn't show up (open BT.EXE to look for it again). Background scans for
    // paired devices don't count as use.
    bool busy = (scanning && userScan) || want || passkey;
    for (auto& P : peers) if (P.client) busy = true;
    if (busy) lastUse = millis();
    else if (millis() - lastUse > (knownN ? 60000u : 45000u)) { stopBle(); continue; }
    for (auto& P : peers)
      if (P.dropped) {
        NimBLEClient* c = P.client;
        P.client = nullptr; P.dropped = false; P.up = false;
        if (c) NimBLEDevice::deleteClient(c);
      }
    if (want) {
      if (NimBLEDevice::getScan()->isScanning()) NimBLEDevice::getScan()->stop();
      char a[18]; strcpy(a, wantAddr);
      bool byUser = want == 1;
      uint8_t t = wantType;
      connectTo(a, t, byUser);
      want = 0;
    } else if (!scanning && freeSlot() >= 0 && millis() - lastBg > 12000) {
      // paired keyboard/mouse not here? listen for it now and then
      int up = 0;
      for (auto& P : peers) if (P.up) up++;
      if (opts.autoReconnect && knownN > up) { lastBg = millis(); startScan(4000, false); }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ------------------------------------------------------------------ to the screens
static void emitKey(const KeyEv& k) {
  if (k.code == K_LANG) {
    uaKeys = !uaKeys;
    for (auto& P : peers) P.kt.ukrainian = uaKeys;
    uiSetLang(uaKeys);
    return;
  }
  uiKey(k);
}

void hidLoop(uint32_t now, uint8_t layout) {
  if (!started) return;
  Rep r;
  while (xQueueReceive(repQ, &r, 0) == pdTRUE) {
    if (r.peer >= MAX_PEERS) continue;
    Peer& P = peers[r.peer];
    if (!P.up) continue;
    HidMap tmp;
    const HidMap* m = &P.map;
    const uint8_t* d = r.d;
    size_t len = r.len;
    uint8_t rid = r.rid;
    if (!m->n) {                                  // no report map: guess the classic layouts
      if (len >= 8) hidBootKeyboardMap(tmp, 0); else hidBootMouseMap(tmp, 0);
      m = &tmp; rid = 0;
    } else if (m->hasIds && rid == 0 && len > 1) { rid = d[0]; d++; len--; }   // id inside the data
    else if (!m->hasIds) rid = 0;
    HidInput in;
    hidDecode(*m, rid, d, len, in);
    P.kt.layout = layout;
    if (in.kb) P.kt.report(in, now, emitKey);
    if (in.mouse) uiMouse(in.dx, in.dy, in.wheel, in.buttons);
  }
  for (auto& P : peers) if (P.up) { P.kt.layout = layout; P.kt.tick(now, emitKey); }
  if (evUp >= 0) {
    Peer& P = peers[evUp];
    char t[40];
    snprintf(t, sizeof(t), "%s ON: %s", P.type == BT_MOUSE ? "MOUSE" : "KEYBOARD", P.name);
    uiToast(t);
    evUp = -1;
  }
  if (evDown >= 0) {
    peers[evDown].kt.releaseAll();
    uiToast(peers[evDown].type == BT_MOUSE ? "MOUSE OFF" : "KEYBOARD OFF");
    evDown = -1;
  }
}

// Bluetooth only starts when it's needed (a keyboard/mouse was paired before, or you open BT.EXE):
// it takes a big piece of memory that HTTPS needs too.
static void start() {
  if (started) return;
  started = true;
  lastUse = millis();
  NimBLEDevice::init(devName);
  NimBLEDevice::setSecurityAuth(true, true, true);           // bond, protect against snooping, secure connections
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);    // we show a code, the keyboard types it
  if (knownN == 0 && NimBLEDevice::getNumBonds() > 0) NimBLEDevice::deleteAllBonds();   // left over from other sketches
  NimBLEScan* sc = NimBLEDevice::getScan();
  sc->setScanCallbacks(&scanCB, false);
  sc->setMaxResults(0);                                      // don't keep every device it hears: saves memory
  if (!taskMade) { taskMade = true; xTaskCreate(hidTask, "hid", 5120, nullptr, 2, nullptr); }
}

void hidBegin(const char* deviceName) {
  repQ = xQueueCreate(16, sizeof(Rep));
  mtx = xSemaphoreCreateMutex();
  strncpy(devName, deviceName, sizeof(devName) - 1);
  kp.begin("errorbt", false);
  knownN = kp.getUChar("knownN", 0);
  if (knownN > 3 || kp.getBytes("known", known, sizeof(known)) != sizeof(known)) { knownN = 0; memset(known, 0, sizeof(known)); }
  BtOpts o;
  if (kp.getBytes("opts", &o, sizeof(o)) == sizeof(o)) hidSetOpts(o);
  if (knownN > 0) start();
}

void hidScan() {
  start();
  lock(); foundN = 0; memset(found, 0, sizeof(found)); memset(foundType, 0, sizeof(foundType)); unlock();
  startScan(opts.secs * 1000u, true);
  setStatus("");
}
const BtOpts& hidOpts() { return opts; }
void hidSetOpts(const BtOpts& o) {
  opts = o;
  if (opts.secs < 3) opts.secs = 3;
  if (opts.secs > 60) opts.secs = 60;
  if (opts.show > 2) opts.show = 2;
  opts.active = opts.active ? 1 : 0;
  opts.autoReconnect = opts.autoReconnect ? 1 : 0;
  kp.putBytes("opts", &opts, sizeof(opts));
}
bool hidStarted() { return started; }
uint32_t hidScanStartedMs() { return scanStartedAt; }
void hidPairAddr(const char* addr, uint8_t atype) {
  start();
  strncpy(wantAddr, addr, sizeof(wantAddr) - 1);
  wantType = atype & 1;
  want = 1;
}
bool hidScanning() { return scanning && userScan; }
int hidResults(BtDev* out, int max) {
  lock();
  int n = 0;
  for (int pass = 0; pass < 2; pass++)                       // keyboards/mice first, then the rest
    for (int i = 0; i < foundN && n < max; i++) {
      if (((foundType[i] & 0x80) != 0) != (pass == 0)) continue;
      bool on = false;
      for (auto& P : peers) if (P.up && !strcmp(P.addr, found[i].addr)) on = true;
      if (on) continue;
      out[n] = found[i];
      out[n].hid = (foundType[i] & 0x80) ? 1 : 0;
      out[n].atype = foundType[i] & 0x7F;
      n++;
    }
  unlock();
  return n;
}
void hidPair(const char* addr) {
  start();
  lock();
  wantType = 1;
  for (int i = 0; i < foundN; i++) if (!strcmp(found[i].addr, addr)) wantType = foundType[i] & 0x7F;
  unlock();
  strncpy(wantAddr, addr, sizeof(wantAddr) - 1);
  want = 1;
}
void hidForget() {
  if (started) {
    for (auto& P : peers) if (P.client && P.client->isConnected()) P.client->disconnect();
    NimBLEDevice::deleteAllBonds();
  }
  knownN = 0;
  memset(known, 0, sizeof(known));
  saveKnown();
}
int hidConnected(BtDev* out, int max) {
  int n = 0;
  for (auto& P : peers)
    if (P.up && n < max) {
      BtDev& d = out[n++];
      memset(&d, 0, sizeof(d));
      strncpy(d.name, P.name, sizeof(d.name) - 1);
      strncpy(d.addr, P.addr, sizeof(d.addr) - 1);
      d.type = P.type;
      d.bonded = 1;
    }
  return n;
}
uint32_t hidPasskey() { return passkey; }
const char* hidStatus() {
  if (status[0] && !statusSticky && millis() - statusAt > 6000) status[0] = 0;
  return status;
}
int hidBonds() { return knownN; }
int hidKnown(BtDev* out, int max) {
  int n = 0;
  for (int i = 0; i < knownN && n < max; i++) {
    BtDev& d = out[n++];
    memset(&d, 0, sizeof(d));
    strncpy(d.name, known[i].name, sizeof(d.name) - 1);
    strncpy(d.addr, known[i].addr, sizeof(d.addr) - 1);
    d.atype = known[i].atype;
    d.bonded = 1;
  }
  return n;
}
