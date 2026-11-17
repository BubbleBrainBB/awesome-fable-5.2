#!/usr/bin/env python3
"""Debug view of a terrain .bin: hillshade + elevation tint + streams (+ optional trail)."""
import sys, numpy as np
from PIL import Image, ImageDraw

def load(path):
    with open(path, 'rb') as f:
        N = int(np.frombuffer(f.read(4), np.int32)[0]); dx = float(np.frombuffer(f.read(4), np.float32)[0])
        h = np.frombuffer(f.read(4 * N * N), np.float32).reshape(N, N)
        a = np.frombuffer(f.read(4 * N * N), np.float32).reshape(N, N)
    return N, dx, h, a

if __name__ == '__main__':
    path, out = sys.argv[1], sys.argv[2]
    size = int(sys.argv[3]) if len(sys.argv) > 3 else 1400
    N, dx, h, a = load(path)
    st = max(1, N // size)
    hs, as_ = h[::st, ::st], a[::st, ::st]
    gy, gx = np.gradient(hs, dx * st)
    n = np.stack([-gx, np.ones_like(gx), -gy], -1); n /= np.linalg.norm(n, axis=-1, keepdims=True)
    L = np.array([-0.5, 0.6, -0.6]); L /= np.linalg.norm(L)
    sh = np.clip((n * L).sum(-1), 0, 1) * 0.85 + 0.15
    t = np.clip((hs - 1000) / 2000, 0, 1)[..., None]
    col = (1 - t) * np.array([0.25, 0.45, 0.2]) + t * np.array([0.95, 0.93, 0.9])
    col = np.where((hs < 1450)[..., None], np.array([0.75, 0.8, 0.9]), col)
    img = col * sh[..., None]
    stream = np.clip((np.log10(as_) - 5.3) / 1.5, 0, 1)[..., None]
    img = img * (1 - stream) + stream * np.array([0.1, 0.3, 0.9])
    slope = np.sqrt(gx * gx + gy * gy)
    im = Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8))
    if len(sys.argv) > 4:
        tr = np.fromfile(sys.argv[4], np.float32).reshape(-1, 4)  # x,z,y,s
        d = ImageDraw.Draw(im)
        pts = [(p[0] / dx / st, p[1] / dx / st) for p in tr[::8]]
        d.line(pts, fill=(255, 40, 40), width=2)
    im.save(out)
    print('N', N, 'dx', dx, 'h range', h.min(), h.max(), 'slope pct>35deg', (slope > 0.7).mean(), '>50deg', (slope > 1.19).mean())
    iy, ix = np.unravel_index(np.argmax(h), h.shape); print('summit at x,z =', ix * dx, iy * dx)
