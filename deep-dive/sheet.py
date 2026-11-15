#!/usr/bin/env python3
"""Contact sheet of frames: sheet.py <frames_dir> <out.jpg> [cols] [thumb_w] [every_nth]"""
import sys, glob, os
from PIL import Image, ImageDraw
d, out = sys.argv[1], sys.argv[2]
cols = int(sys.argv[3]) if len(sys.argv) > 3 else 4
tw = int(sys.argv[4]) if len(sys.argv) > 4 else 480
nth = int(sys.argv[5]) if len(sys.argv) > 5 else 1
fs = sorted(glob.glob(os.path.join(d, 'f_*.png')))[::nth]
th = tw * 9 // 16
rows = (len(fs) + cols - 1) // cols
sh = Image.new('RGB', (tw * cols, th * rows))
dr = ImageDraw.Draw(sh)
for i, f in enumerate(fs):
    im = Image.open(f).convert('RGB').resize((tw, th), Image.LANCZOS)
    x, y = (i % cols) * tw, (i // cols) * th
    sh.paste(im, (x, y))
    n = int(os.path.basename(f)[2:7])
    dr.text((x + 5, y + 4), f"{n}  {n/30:.1f}s", fill=(255, 255, 255))
sh.save(out, quality=88)
print(len(fs), 'frames ->', out)
