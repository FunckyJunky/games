// linetext.h: words as 3D line art. A stroke font (A-Z, 0-9, . , ! ? ' - : + / ( ) and a note for ♪);
// accented letters fold to their letter (é -> E, ß -> SS). Each letter is drawn twice (front and back, the depth
// apart) with lines joining the two at its corners: an extruded, glowing wireframe word. Plain C++.
#pragma once
#include "r3d.h"

float ltWidth(const char* utf8);      // in glyph units: a letter is 4 x 6, 5.5 apart, a space 4
// the text centred on o; right / up: one glyph unit along the letters; depth: front minus back.
// reveal 0..1: how much is drawn yet (stroke by stroke). alpha 0..32. -> lines drawn
int ltDraw(const char* utf8, Vec o, Vec right, Vec up, Vec depth, Col front, Col back, float reveal, int alpha);
