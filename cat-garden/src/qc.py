#!/usr/bin/env python3
"""qc.py — bad-frame checks on the finished film.
A) decode every frame: black / frozen frames and single-frame block glitches (a block that differs from both
   neighbours while the neighbours agree — the signature of a damaged GPU frame).
B) compare against an independent low-resolution re-render (work/verify/f_NNNNN.ppm, every 10th frame).
usage: qc.py cat_garden.mp4 work/verify report.txt"""
import sys, os, glob, subprocess
import numpy as np
from PIL import Image
video, vdir, rep = sys.argv[1], sys.argv[2], sys.argv[3]
W, H = 320, 180
p = subprocess.Popen(['ffmpeg', '-v', 'error', '-i', video, '-vf', 'scale=%d:%d:flags=area' % (W, H), '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'], stdout=subprocess.PIPE)
raw = p.stdout.read(); p.wait()
F = np.frombuffer(raw, np.uint8).reshape(-1, H, W, 3).astype(np.float32)
n = len(F)
lum = F.mean(3)
bw, bh = 20, 20
blocks = lum.reshape(n, H // bh, bh, W // bw, bw).mean((2, 4))          # n x 9 x 16
out = []
out.append('frames decoded: %d (%.3f s at 30 fps)' % (n, n / 30))
mean = lum.mean((1, 2))
# eyelids: the slow blink (105.9-107.5 s) and the final eye-closing (from 107.9 s) are black on purpose
eyelid = lambda i: 105.9 <= i / 30 <= 107.5 or i / 30 >= 107.9
black = [i for i in range(n) if mean[i] < 4 and i > 25 and not eyelid(i)]
out.append('black frames outside the fade-in and the eyelid windows: %d %s' % (len(black), black[:20]))
mad = np.abs(lum[1:] - lum[:-1]).mean((1, 2))
frozen = [i + 1 for i in range(n - 1) if mad[i] < 0.02 and mean[i] > 6]
out.append('frozen frames (identical to the previous one): %d %s' % (len(frozen), frozen[:20]))
# lost-tile signature: a block that goes dark for exactly one frame while its two neighbours in time are bright and agree
bp, bc, bn = blocks[:-2], blocks[1:-1], blocks[2:]
lost = (np.minimum(bp, bn) > 24) & (bc < 0.45 * np.minimum(bp, bn)) & (np.abs(bp - bn) < 0.15 * np.maximum(bp, bn))
gl = [(i + 1, int(lost[i].sum())) for i in range(n - 2) if lost[i].any() and not eyelid(i + 1)]
out.append('lost-tile candidates (block dark for one frame, neighbours bright and equal): %d %s' % (len(gl), gl[:20]))
# general single-frame outlier score, informational only: fast near-lens motion (grass brushing past the lens, quick head turns)
# scores high here by design, so it is listed for manual inspection instead of being used as a re-render trigger
d_prev = np.abs(bc - bp); d_next = np.abs(bc - bn); d_nb = np.abs(bn - bp)
glitch = (np.minimum(d_prev, d_next) - d_nb).max((1, 2))
top = sorted(((float(glitch[i]), i + 1) for i in range(n - 2)), reverse=True)[:8]
out.append('single-frame outlier score (informational): %d frames > 14; top: %s' % (int((glitch > 14).sum()), ['%d:%.0f' % (f, g) for g, f in top]))
out.append('largest frame-to-frame mean change: %.1f at frame %d' % (mad.max(), int(mad.argmax()) + 1))
# B) independent re-render
files = sorted(glob.glob(os.path.join(vdir, 'f_*.ppm')))
worst = []; flagged = []
for f in files:
    i = int(os.path.basename(f)[2:7])
    if i >= n: continue
    v = np.asarray(Image.open(f).convert('RGB').resize((W, H), Image.BOX), np.float32).mean(2)
    vb = v.reshape(H // bh, bh, W // bw, bw).mean((1, 3))
    d = np.abs(vb - blocks[i]).max()
    worst.append((d, i))
    if d > 18: flagged.append((i, float(d)))
if worst:
    worst.sort(reverse=True)
    out.append('independent re-render: %d frames compared, worst block difference %.1f/255 (frame %d), median %.1f; flagged (> 18): %d %s' % (len(worst), worst[0][0], worst[0][1], float(np.median([w[0] for w in worst])), len(flagged), flagged[:20]))
else: out.append('independent re-render: no verification frames found')
bad = sorted(set(black + frozen + [g[0] for g in gl] + [f[0] for f in flagged]))
out.append('segments holding candidates to inspect by eye (a candidate is not a verdict; see README section 6): %s' % sorted(set(b // 60 for b in bad)))
open(rep, 'w').write('\n'.join(out) + '\n')
print('\n'.join(out))
