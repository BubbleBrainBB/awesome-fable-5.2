#!/usr/bin/env python3
"""Spectrogram + level curve of the mix with shot boundaries, since nobody listens to it during the build."""
import sys, json, wave, numpy as np
from scipy import signal
from PIL import Image, ImageDraw
wav, edit, out = sys.argv[1], json.load(open(sys.argv[2])), sys.argv[3]
w = wave.open(wav); sr = w.getframerate(); x = np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, 2).astype(float) / 32768
m = x.mean(1); f, t, S = signal.spectrogram(m, sr, nperseg=4096, noverlap=3072)
keep = f < 12000; S = 10 * np.log10(S[keep] + 1e-12); f = f[keep]
# log-frequency rows
H, W = 420, 1600; rows = np.geomspace(40, 12000, H); img = np.zeros((H, W))
ti = np.linspace(0, S.shape[1] - 1, W).astype(int)
for r, fr in enumerate(rows): img[H - 1 - r] = S[np.argmin(np.abs(f - fr)), ti]
img = np.clip((img + 110) / 70, 0, 1)
rgb = np.stack([img ** 0.8, img ** 1.6, 0.25 + 0.5 * img ** 3], -1)
canvas = Image.new('RGB', (W, H + 160), (16, 16, 20)); canvas.paste(Image.fromarray((rgb * 255).astype(np.uint8)), (0, 0)); d = ImageDraw.Draw(canvas)
dur = len(m) / sr; win = int(sr * 0.4); rms = np.sqrt(np.convolve(m ** 2, np.ones(win) / win, 'same')); db = 20 * np.log10(rms + 1e-9)
pts = [(i, H + 150 - (np.clip(db[int(i / W * len(db))], -60, 0) + 60) / 60 * 140) for i in range(W)]; d.line(pts, fill=(120, 220, 140))
for s in edit['shots']:
    xx = int(s['start'] / dur * W); d.line([(xx, 0), (xx, H + 160)], fill=(255, 255, 255)); d.text((xx + 4, 4), s['name'], fill=(255, 255, 255))
for k in (-48, -36, -24, -12): yy = H + 150 - (k + 60) / 60 * 140; d.line([(0, yy), (W, yy)], fill=(60, 60, 70)); d.text((2, yy - 10), f"{k} dB", fill=(150, 150, 160))
canvas.save(out); print('duration', dur, 'peak', np.abs(x).max())
