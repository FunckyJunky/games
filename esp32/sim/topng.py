# PBM screenshots -> PNG that looks like the little OLED (6x bigger, pixel gaps, bezel)
import sys, glob, os
from PIL import Image, ImageDraw
S = 6
def oled(path, out):
    im = Image.open(path).convert('1')
    W, H = im.size
    pad = 3 * S
    img = Image.new('RGB', (W * S + 2 * pad, H * S + 2 * pad), (8, 10, 14))
    d = ImageDraw.Draw(img)
    d.rectangle([pad - 4, pad - 4, pad + W * S + 3, pad + H * S + 3], fill=(0, 0, 0))
    px = im.load()
    for y in range(H):
        for x in range(W):
            if not px[x, y]:   # PBM: 1 = black = lit OLED pixel
                x0, y0 = pad + x * S, pad + y * S
                d.rectangle([x0, y0, x0 + S - 1, y0 + S - 1], fill=(120, 190, 225))
                d.rectangle([x0 + 1, y0 + 1, x0 + S - 2, y0 + S - 2], fill=(165, 230, 255))
    img.save(out)
for p in sys.argv[1:]:
    oled(p, os.path.splitext(p)[0] + '.png')
