# mkfont.py: makes ErrorCube/fonts.h, the ERROR-DOS letters for the cube's 480x480 screen.
#
#   python3 esp32/ErrorCube/tools/mkfont.py
#
# Letters: VT323 (the font of error.blue's ERROR-DOS, SIL OFL 1.1, tools/fonts/), drawn at 1.5x, 2x and 6x its
# own dot grid. What VT323 doesn't have (Cyrillic, arrows, triangles...) comes from GNU Unifont 8x16
# (/usr/share/fonts/opentype/unifont/unifont.otf, SIL OFL 1.1), drawn as the same kind of round dots.
# Block characters (full, halves, light/medium/dark shade) are made here so they fill the whole cell.
#
# Every glyph is a box of bytes: high 4 bits = the letter itself, low 4 bits = its glow (a blur of the letter,
# like the CSS text-shadow on error.blue). The cube blends the colour with those.
import os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
VT = [os.path.join(HERE, 'fonts', f) for f in ('vt323-latin-400-normal.woff', 'vt323-latin-ext-400-normal.woff')]
UNI = '/usr/share/fonts/opentype/unifont/unifont.otf'
OUT = os.path.join(HERE, '..', 'src', 'core', 'fonts.h')

from fontTools.ttLib import TTFont
vt_cmaps = [set(TTFont(f).getBestCmap()) for f in VT]
uni_cmap = set(TTFont(UNI).getBestCmap())

def rng(a, b): return list(range(a, b + 1))
SYMBOLS = [0x2190, 0x2191, 0x2192, 0x2193, 0x2022, 0x2026, 0x20AC, 0x2122, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C,
           0x201D, 0x25B2, 0x25BC, 0x25C4, 0x25BA, 0x25A0, 0x25A1, 0x25CF, 0x25CB, 0x2713, 0x2717, 0x266A, 0x2665,
           0x263A, 0x2302, 0x21B5, 0x2261, 0x2212, 0x221A, 0x2248, 0x221E, 0x03C0, 0x00B0, 0x2318, 0x23FB, 0x2620]
BLOCKS = [0x2580, 0x2584, 0x2588, 0x258C, 0x2590, 0x2591, 0x2592, 0x2593]
CYR = rng(0x400, 0x45F) + [0x490, 0x491]
TEXT = rng(0x20, 0x7E) + rng(0xA0, 0xFF) + [c for c in rng(0x100, 0x17F) if c in vt_cmaps[1]] + CYR + SYMBOLS + BLOCKS
BIG = rng(0x20, 0x7E) + [0xB0, 0xB1, 0xD7, 0xF7] + CYR + [0x2190, 0x2191, 0x2192, 0x2193, 0x2212, 0x221A, 0x03C0, 0x25B2, 0x25BC, 0x25C4, 0x25BA, 0x2713] + BLOCKS
HUGE = [ord(c) for c in '0123456789:-. APM%'] + [0xB0]

#            name scale  cell w, h, baseline, glow sigma (px)
FONTS = [dict(n='T', s=1.5, cw=15, ch=32, base=25, sig=3.6, chars=TEXT),
         dict(n='B', s=2.0, cw=20, ch=42, base=33, sig=4.6, chars=BIG),
         dict(n='H', s=6.0, cw=60, ch=96, base=90, sig=10.0, chars=HUGE)]

def blur(a, sig):
    # gaussian blur of a float array (separable, numpy only)
    r = int(sig * 3 + 1)
    x = np.arange(-r, r + 1)
    k = np.exp(-x * x / (2 * sig * sig)); k /= k.sum()
    p = np.pad(a, r)
    t = np.apply_along_axis(lambda v: np.convolve(v, k, 'valid'), 1, p)
    return np.apply_along_axis(lambda v: np.convolve(v, k, 'valid'), 0, t)

def vt_glyph(cp, F, m):
    size = 25 * F['s']
    for path, cm in zip(VT, vt_cmaps):
        if cp in cm:
            font = ImageFont.truetype(path, size=round(size * 4) / 4)
            W, H = F['cw'] + 2 * m, F['ch'] + 2 * m
            ss = 4                                   # draw 4x bigger, then shrink: smooth round dots
            im = Image.new('L', (W * ss, H * ss), 0)
            f4 = ImageFont.truetype(path, size=size * ss)
            ImageDraw.Draw(im).text((m * ss, (m + F['base']) * ss), chr(cp), fill=255, font=f4, anchor='ls')
            im = im.resize((W, H), Image.LANCZOS)
            return np.asarray(im, dtype=np.float32) / 255.0
    return None

_uni = ImageFont.truetype(UNI, size=16)
def uni_bits(cp):
    im = Image.new('L', (32, 16), 0)
    ImageDraw.Draw(im).text((0, 14), chr(cp), fill=255, font=_uni, anchor='ls')
    a = np.asarray(im) > 127
    w = 16 if a[:, 8:].any() else 8
    return a[:, :w]

def dots(bits, F, m, sx, sy, base_row):
    # draw each set bit as a rounded dot sx x sy pixels, like VT323's own dots
    W, H = F['cw'] + 2 * m, F['ch'] + 2 * m
    ss = 4
    im = Image.new('L', (W * ss, H * ss), 0)
    d = ImageDraw.Draw(im)
    h, w = bits.shape
    ox = (F['cw'] - w * sx) / 2
    for y in range(h):
        for x in range(w):
            if bits[y, x]:
                x0 = (m + ox + x * sx) * ss; y0 = (m + F['base'] + (y - base_row) * sy) * ss
                r = min(sx, sy) * ss * 0.28
                d.rounded_rectangle([x0, y0, x0 + sx * ss - 1, y0 + sy * ss - 1], radius=r, fill=255)
    im = im.resize((W, H), Image.LANCZOS)
    return np.asarray(im, dtype=np.float32) / 255.0

def block(cp, F, m):
    W, H = F['cw'] + 2 * m, F['ch'] + 2 * m
    a = np.zeros((H, W), np.float32)
    cw, ch = F['cw'], F['ch']
    c = a[m:m + ch, m:m + cw]
    if cp == 0x2588: c[:] = 1
    elif cp == 0x2580: c[:ch // 2] = 1
    elif cp == 0x2584: c[ch // 2:] = 1
    elif cp == 0x258C: c[:, :(cw + 1) // 2] = 1
    elif cp == 0x2590: c[:, (cw + 1) // 2:] = 1
    else:
        d = max(2, round(F['s'] * 2))                    # shade dots on a grid of d x d
        n = {0x2591: 1, 0x2592: 2, 0x2593: 3}[cp]
        for y in range(0, ch, d):
            for x in range(0, cw, d):
                i, j = (y // d) & 1, (x // d) & 1
                on = (n == 3 and not (i == 0 and j == 0)) or (n == 2 and (i ^ j) == 0) or (n == 1 and i == 0 and j == 0)
                if on: c[y:y + d, x:x + d] = 1
    return a

def make(F):
    m = int(F['sig'] * 2.2 + 1)
    glyphs = []
    for cp in sorted(set(F['chars'])):
        if 0x2580 <= cp <= 0x259F: a = block(cp, F, m)
        else:
            a = vt_glyph(cp, F, m)
            if a is None:
                if cp not in uni_cmap: print('no glyph', hex(cp), file=sys.stderr); continue
                bits = uni_bits(cp)
                if bits.shape[1] == 16:                   # wide: shrink to the cell
                    sx = F['cw'] / 16.0
                else:
                    sx = F['cw'] / 8.0
                sy = F['ch'] / 16.0
                a = dots(bits, F, m, sx, sy, 14)
        g = blur(a, F['sig'])
        g = np.clip(g * 1.7, 0, 1)
        core = np.round(np.clip(a, 0, 1) * 15).astype(np.uint8)
        glow = np.round(g * 15).astype(np.uint8)
        nz = np.argwhere((core > 0) | (glow > 0))
        if len(nz) == 0:
            glyphs.append((cp, 0, 0, 0, 0, b'')); continue
        (y0, x0), (y1, x1) = nz.min(0), nz.max(0) + 1
        box = (core[y0:y1, x0:x1] << 4) | glow[y0:y1, x0:x1]
        glyphs.append((cp, int(x0 - m), int(y0 - m), int(x1 - x0), int(y1 - y0), box.astype(np.uint8).tobytes()))
    return glyphs

out = ['// fonts.h: made by tools/mkfont.py (VT323 + GNU Unifont, both SIL OFL 1.1). Do not edit by hand.',
       '// Each glyph: high nibble = letter, low nibble = glow. x/y = where its box starts, from the cell\'s top left.',
       '#pragma once', '#include <stdint.h>', '#include "font_types.h"', '']
total = 0
for F in FONTS:
    gl = make(F)
    data = bytearray(); recs = []
    for cp, x, y, w, h, b in gl:
        recs.append('{0x%04X,%d,%d,%d,%d,%d}' % (cp, x, y, w, h, len(data)))
        data += b
    total += len(data)
    n = F['n']
    out.append('static const uint8_t FONT_%s_DATA[%d] = {' % (n, max(1, len(data))))
    for i in range(0, len(data), 40):
        out.append(','.join(str(v) for v in data[i:i + 40]) + ',')
    out.append('};')
    out.append('static const CubeGlyph FONT_%s_GLYPHS[%d] = {' % (n, len(recs)))
    for i in range(0, len(recs), 8): out.append(','.join(recs[i:i + 8]) + ',')
    out.append('};')
    out.append('static const CubeFont FONT_%s = {%d, %d, %d, %d, FONT_%s_GLYPHS, FONT_%s_DATA};' % (n, F['cw'], F['ch'], F['base'], len(recs), n, n))
    out.append('')
    print('font %s: %d glyphs, %d bytes' % (n, len(recs), len(data)))
open(OUT, 'w').write('\n'.join(out) + '\n')
print('total', total, 'bytes ->', OUT)
