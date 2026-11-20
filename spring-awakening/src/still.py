"""still.py — develop raw float frames into JPEG/PNG stills (for look development and checks)."""
import sys, numpy as np
from PIL import Image
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from develop import develop
import grade

def main():
    raw, w, h, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    shot = sys.argv[5] if len(sys.argv) > 5 else None
    maxw = int(sys.argv[6]) if len(sys.argv) > 6 else 0
    data = np.fromfile(raw, np.float32)
    n = data.size // (w * h * 3)
    for i in range(n):
        lin = data[i * w * h * 3:(i + 1) * w * h * 3].reshape(h, w, 3)
        img = develop(lin, **grade.get(shot))
        im = Image.fromarray((img * 255 + 0.5).astype(np.uint8))
        if maxw and w > maxw:
            im = im.resize((maxw, int(h * maxw / w)), Image.LANCZOS)
        o = out if n == 1 else out.replace('.', '_%03d.' % i, 1) if out.count('.') == 1 else out
        im.save(o, quality=92)
main()
