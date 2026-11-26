// p8host.cpp: FAKE-08's Host for the cube (based on its libretro host and hostCommonFunctions.cpp, MIT).
// Input comes from p8run.cpp; pictures and sound are taken by p8run.cpp straight from the Vm and Audio.
// Cart data (cartdata/dset) is kept in C:\pico8\cdata.
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string>
#include <vector>
#include "core/host.h"
#include "core/hostVmShared.h"
#include "core/filehelpers.h"

uint8_t p8InDown = 0, p8InHeld = 0;      // set by p8run.cpp before every step
static std::string cartDir;

#ifdef ARDUINO
Host::Host(int windowWidth, int windowHeight) { _logFilePrefix = "/c/pico8/"; }
#else
std::string simRealPath(const char* p);
Host::Host(int windowWidth, int windowHeight) { _logFilePrefix = simRealPath("/c/pico8/"); }
#endif
void Host::oneTimeSetup(Audio* audio) {}
void Host::oneTimeCleanup() {}
void Host::unpackCarts() {}
void Host::setTargetFps(int targetFps) {}
void Host::changeStretch() {}
void Host::forceStretch(StretchOption newStretch) {}
InputState_t Host::scanInput() {
  InputState_t r{p8InDown, p8InHeld, 0, 0, 0, false, ""};
  p8InDown = 0;                          // a press counts once, when the cart reads it
  return r;
}
bool Host::shouldQuit() { return false; }
void Host::waitForTargetFps() {}
void Host::drawFrame(uint8_t* picoFb, uint8_t* screenPaletteMap, uint8_t drawMode) {}
bool Host::shouldFillAudioBuff() { return false; }
void* Host::getAudioBufferPointer() { return nullptr; }
size_t Host::getAudioBufferSize() { return 0; }
void Host::playFilledAudioBuffer() {}
bool Host::shouldRunMainLoop() { return true; }
double Host::deltaTMs() { return 1000.0 / 60; }

std::vector<std::string> Host::listcarts() {
  std::vector<std::string> carts;
  DIR* d = opendir(cartDir.c_str());
  if (!d) return carts;
  while (struct dirent* e = readdir(d)) {
    std::string n = e->d_name;
    if (n.size() > 3 && (n.compare(n.size() - 3, 3, ".p8") == 0 || (n.size() > 7 && n.compare(n.size() - 7, 7, ".p8.png") == 0)))
      carts.push_back(cartDir + "/" + n);
  }
  closedir(d);
  return carts;
}
std::vector<std::string> Host::listdirs() { return std::vector<std::string>(); }
std::string Host::customBiosLua() { return ""; }
std::string Host::getCartDirectory() { return cartDir; }
void Host::setCartDirectory(std::string d) { cartDir = d; }
void Host::overrideLogFilePrefix(const char* newPrefix) { _logFilePrefix = newPrefix; }
const char* Host::logFilePrefix() { return _logFilePrefix.c_str(); }

void Host::setUpPaletteColors() {
  static const Color P[32] = {COLOR_00, COLOR_01, COLOR_02, COLOR_03, COLOR_04, COLOR_05, COLOR_06, COLOR_07,
                              COLOR_08, COLOR_09, COLOR_10, COLOR_11, COLOR_12, COLOR_13, COLOR_14, COLOR_15,
                              COLOR_128, COLOR_129, COLOR_130, COLOR_131, COLOR_132, COLOR_133, COLOR_134, COLOR_135,
                              COLOR_136, COLOR_137, COLOR_138, COLOR_139, COLOR_140, COLOR_141, COLOR_142, COLOR_143};
  for (int i = 0; i < 144; i++) _paletteColors[i] = {0, 0, 0, 0};
  for (int i = 0; i < 16; i++) { _paletteColors[i] = P[i]; _paletteColors[128 + i] = P[16 + i]; }
}
Color* Host::GetPaletteColors() { return _paletteColors; }

// cart data: C:\pico8\cdata\KEY.p8d.txt
static void mkdirs(const std::string& p) { mkdir(p.substr(0, p.rfind('/', p.size() - 2)).c_str(), 0777); mkdir(p.c_str(), 0777); }
std::string Host::getCartDataFile(std::string key) { return _logFilePrefix + "cdata/" + key + ".p8d.txt"; }
std::string Host::getCartDataFileContents(std::string key) { return get_file_contents(getCartDataFile(key)); }
void Host::saveCartData(std::string key, std::string contents) {
  mkdirs(_logFilePrefix + "cdata");
  FILE* f = fopen(getCartDataFile(key).c_str(), "w");
  if (f) { fputs(contents.c_str(), f); fclose(f); }
}
size_t Host::getFileContents(std::string fileName, char* buffer) {
  FILE* f = fopen((_logFilePrefix + "cdata/" + fileName).c_str(), "rb");
  if (!f) return 0;
  size_t len = 0;
  if (fread(&len, sizeof(len), 1, f) != 1) len = 0;
  len = fread(buffer, 1, len, f);
  fclose(f);
  return len;
}
void Host::writeBufferToFile(std::string fileName, char* buffer, size_t length) {
  mkdirs(_logFilePrefix + "cdata");
  FILE* f = fopen((_logFilePrefix + "cdata/" + fileName).c_str(), "wb");
  if (f) { fwrite(&length, sizeof(length), 1, f); fwrite(buffer, 1, length, f); fclose(f); }
}

// settings FAKE-08's own menus ask for
int Host::getSetting(std::string s) {
  if (s == "p8_bgcolor") return 0;
  if (s == "p8_textcolor") return 7;
  if (s == "stretch") return (int)stretch;
  return 0;
}
void Host::setSetting(std::string s, int v) {}
