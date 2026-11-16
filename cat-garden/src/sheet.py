#!/usr/bin/env python3
# contact sheet from a directory of f_NNNNN.ppm frames (or a list of image files); labels each tile with its time
import sys, glob, os
from PIL import Image, ImageDraw
src, out = sys.argv[1], sys.argv[2]
cols = int(sys.argv[3]) if len(sys.argv) > 3 else 6
tw = int(sys.argv[4]) if len(sys.argv) > 4 else 320
fps = float(sys.argv[5]) if len(sys.argv) > 5 else 30.0
files = sorted(glob.glob(os.path.join(src, 'f_*.ppm')) + glob.glob(os.path.join(src, 'f_*.png')) + glob.glob(os.path.join(src, 'f_*.jpg')))
ims = [Image.open(f).convert('RGB') for f in files]
w, h = ims[0].size; th = tw * h // w
rows = (len(ims) + cols - 1) // cols
sheet = Image.new('RGB', (cols * tw, rows * th), (0, 0, 0))
d = ImageDraw.Draw(sheet)
for i, (f, im) in enumerate(zip(files, ims)):
    x, y = (i % cols) * tw, (i // cols) * th
    sheet.paste(im.resize((tw, th), Image.LANCZOS), (x, y))
    n = int(os.path.basename(f)[2:7]); lab = '%.1fs' % (n / fps)
    d.rectangle([x + 2, y + 2, x + 8 + 6 * len(lab), y + 14], fill=(0, 0, 0))
    d.text((x + 5, y + 3), lab, fill=(255, 255, 255))
sheet.save(out, quality=88)
