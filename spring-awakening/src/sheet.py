"""sheet.py — develop a raw multi-frame float file into one grid JPEG. usage: sheet.py raw W H out.jpg shot cols [maxw]"""
import sys, numpy as np
from PIL import Image
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from develop import develop
import grade
raw, w, h, out, shot, cols = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4], sys.argv[5], int(sys.argv[6])
maxw = int(sys.argv[7]) if len(sys.argv) > 7 else 1920
data = np.fromfile(raw, np.float32); n = data.size // (w * h * 3)
rows = (n + cols - 1) // cols
sheet = np.zeros((rows * h, cols * w, 3), np.uint8)
for i in range(n):
    img = develop(data[i * w * h * 3:(i + 1) * w * h * 3].reshape(h, w, 3), **grade.get(shot))
    sheet[(i // cols) * h:(i // cols + 1) * h, (i % cols) * w:(i % cols + 1) * w] = (img * 255 + 0.5).astype(np.uint8)
im = Image.fromarray(sheet)
if im.width > maxw: im = im.resize((maxw, int(im.height * maxw / im.width)), Image.LANCZOS)
im.save(out, quality=90)
