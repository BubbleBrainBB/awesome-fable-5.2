"""review.py — review sheet from the key frames written during rendering. usage: review.py out.jpg shot:frame ... [--crop x,y]"""
import sys, os
from PIL import Image
out = sys.argv[1]; items = [a for a in sys.argv[2:] if ':' in a]
cols = 3; tw, th = 640, 360
rows = (len(items) + cols - 1) // cols
sheet = Image.new('RGB', (cols * tw, rows * th))
for k, it in enumerate(items):
    parts = it.split(':'); shot, fr = parts[0], int(parts[1])
    im = Image.open(f'work/key/{shot}_{fr:04d}.jpg')
    if len(parts) > 2:  # full-resolution crop centred at x,y
        x, y = [int(v) for v in parts[2].split(',')]; im = im.crop((x - 320, y - 180, x + 320, y + 180))
    else: im = im.resize((tw, th), Image.LANCZOS)
    sheet.paste(im, ((k % cols) * tw, (k // cols) * th))
sheet.save(out, quality=90)
