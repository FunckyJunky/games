// pico.h: what PICO-CUBE's screens (pico.cpp) need from its network part (net/pnet.cpp)
#pragma once
#include <stdint.h>

// a cart on error.blue (public/pico8/carts.json)
struct PCart {
  char file[48];        // the file name in /pico8/carts/ (and on the cube, in C:\pico8)
  char name[28], author[28], lic[16], ver[8];
  char about[100];      // one line: what it is
  uint32_t size;
};
int pnetCarts(PCart* out, int max);      // the list: n, -1 = still loading, -2 = can't get it (no WiFi / error)
void pnetCartsRefresh();                 // get the list again
// the other firmware (ERROR-CUBE): look it up, then install it (S.upd / S.updMsg / S.updPct like an update)
void pnetSwitch();
extern volatile bool pnetSwitching;      // the job is the switch (not an update of PICO-CUBE)
