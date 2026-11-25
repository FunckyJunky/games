// jsapp.h: runs a web app (a JavaScript file from error.blue/cube/apps/) on the cube, with Duktape.
// The same file runs in the browser at error.blue/cube/apps/run.html. The API is in public/cube/apps/README.md.
#pragma once
#include <stdint.h>
#include <stddef.h>

bool jsStart(const char* id, const char* src, size_t n, char* err, size_t errLen);   // false = didn't start (err says why)
void jsStop();
bool jsRunning();
bool jsFrame(uint32_t dtMs, char* err, size_t errLen);   // update + draw into fb; false = the app crashed
void jsTouch(const char* type, int x, int y);
void jsSwipe(const char* dir);
bool jsKey(const char* k);                               // true = the app used it
void jsMsg(const char* json);                            // from the control page
void jsFetched(int id, int status, const char* body);
bool jsWantsExit();
int jsFps();
