"""contact.py — contact sheet of evenly spaced frames from the finished film, time codes drawn with own 7-segment digits."""
import os, sys, subprocess, numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import grade, variant
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, variant.CFG['mp4'])
COLS, ROWS, TW, TH = 8, 6, 480, 270
total = sum(grade.frames(s) for _, s, _ in grade.SHOTS)
SEG = {'0': 'abcdef', '1': 'bc', '2': 'abged', '3': 'abgcd', '4': 'fgbc', '5': 'afgcd', '6': 'afgedc', '7': 'abc', '8': 'abcdefg', '9': 'abfgcd'}
def digit(img, ch, x, y, s=3):
    """seven-segment digit, s = stroke width in pixels"""
    w, h = 4 * s, 7 * s
    seg = {'a': (0, 0, w, s), 'g': (0, 3 * s, w, s), 'd': (0, 6 * s, w, s), 'f': (0, 0, s, 4 * s), 'b': (w - s, 0, s, 4 * s), 'e': (0, 3 * s, s, 4 * s), 'c': (w - s, 3 * s, s, 4 * s)}
    if ch == ':': img[y + 2 * s:y + 3 * s, x + s:x + 2 * s] = 255; img[y + 5 * s:y + 6 * s, x + s:x + 2 * s] = 255; return 3 * s
    if ch == '.': img[y + 6 * s:y + 7 * s, x:x + s] = 255; return 2 * s
    for k in SEG[ch]:
        sx, sy, sw, sh = seg[k]; img[y + sy:y + sy + sh, x + sx:x + sx + sw] = 255
    return w + s
n = COLS * ROWS
idx = [int(round((i + 0.5) * total / n)) for i in range(n)]
sel = '+'.join(f'eq(n\\,{i})' for i in idx)
raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', SRC, '-vf', f"select='{sel}',scale={TW}:{TH}", '-vsync', '0', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True).stdout
fr = np.frombuffer(raw, np.uint8).reshape(-1, TH, TW, 3).copy()
sheet = np.zeros((ROWS * TH, COLS * TW, 3), np.uint8)
for k, f in enumerate(fr):
    t = idx[k] / grade.FPS; label = f'{int(t // 60)}:{int(t % 60):02d}.{int(t * 10) % 10}'
    box = f[TH - 30:TH - 4, 6:6 + 14 * len(label) + 8]; box[:] = (box * 0.35).astype(np.uint8)
    x = 10
    for ch in label: x += digit(f, ch, x, TH - 27, 3)
    sheet[(k // COLS) * TH:(k // COLS + 1) * TH, (k % COLS) * TW:(k % COLS + 1) * TW] = f
Image.fromarray(sheet).save(os.path.join(ROOT, variant.CFG['contact']), quality=88)
print(variant.CFG['contact'], len(fr), 'tiles')
