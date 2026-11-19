# puts PNG screenshots side by side with their names: sheet.py OUT.png a.png b.png ...
import sys, os
from PIL import Image, ImageDraw
out, files = sys.argv[1], sys.argv[2:]
ims = [Image.open(f) for f in files]
cols = 3 if len(ims) > 4 else len(ims)
w = max(i.width for i in ims); h = max(i.height for i in ims) + 18
rows = (len(ims) + cols - 1) // cols
sheet = Image.new('RGB', (cols * (w + 10) + 10, rows * (h + 10) + 10), (30, 32, 38))
d = ImageDraw.Draw(sheet)
for k, (f, im) in enumerate(zip(files, ims)):
    x = 10 + (k % cols) * (w + 10); y = 10 + (k // cols) * (h + 10)
    sheet.paste(im, (x, y + 18))
    d.text((x, y + 2), os.path.splitext(os.path.basename(f))[0], fill=(200, 200, 200))
sheet.save(out)
