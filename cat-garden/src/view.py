#!/usr/bin/env python3
# converts PPM stills to JPG (optionally tiles several into one sheet) for inspection
import sys
from PIL import Image
args = sys.argv[1:]
out = args[-1]; ins = args[:-1]
ims = [Image.open(p).convert('RGB') for p in ins]
if len(ims) == 1:
    ims[0].save(out, quality=90)
else:
    cols = 2 if len(ims) <= 4 else 3
    w, h = ims[0].size
    rows = (len(ims) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * w, rows * h))
    for i, im in enumerate(ims):
        sheet.paste(im.resize((w, h)), ((i % cols) * w, (i // cols) * h))
    sheet.save(out, quality=88)
