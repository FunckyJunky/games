#!/usr/bin/env python3
# Makes esp32/ErrorCube/src/p8/carts.h from esp32/ErrorCube/carts/*.p8 (the carts that come with the cube).
import os, sys
G = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
d = os.path.join(G, 'esp32/ErrorCube/carts')
files = sorted(f for f in os.listdir(d) if f.endswith('.p8'))
out = ['// carts.h: the PICO-8 carts that come with the cube (made from esp32/ErrorCube/carts/*.p8 by tools/cube-carts.py).',
       '#pragma once', '#include <stddef.h>', 'struct P8Cart { const char* name; const char* data; size_t size; };']
names = []
for i, f in enumerate(files):
    b = open(os.path.join(d, f), 'rb').read()
    lines = b.split(b'\n')
    out.append(f'static const char P8CART{i}[] =')
    for j, l in enumerate(lines):
        esc = l.decode('utf-8').replace('\\', '\\\\').replace('"', '\\"')
        out.append(f'  "{esc}' + ('\\n' if j < len(lines) - 1 else '') + '"')
    out[-1] += ';'
    names.append(f'  {{"{f}", P8CART{i}, {len(b)}}},')
out += ['static const P8Cart P8CARTS[] = {'] + names + ['};', 'static const int N_P8CARTS = sizeof(P8CARTS) / sizeof(P8CARTS[0]);']
open(os.path.join(G, 'esp32/ErrorCube/src/p8/carts.h'), 'w').write('\n'.join(out) + '\n')
print('carts.h:', ', '.join(files))
