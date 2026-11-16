#!/usr/bin/env python3
# draws a spectrogram of the sound track (the only way the author can "look" at the audio)
import sys, wave, numpy as np, scipy.signal as sg
from PIL import Image, ImageDraw
w = wave.open(sys.argv[1]); sr = w.getframerate(); x = np.frombuffer(w.readframes(w.getnframes()), '<i2').reshape(-1, 2).mean(1) / 32768
f, t, S = sg.spectrogram(x, sr, nperseg=2048, noverlap=1024)
S = 10 * np.log10(S + 1e-12)
# log-frequency rows 40 Hz .. 12 kHz
rows = 360; fr = np.geomspace(40, 12000, rows)
idx = np.searchsorted(f, fr).clip(0, len(f) - 1)
img = S[idx][::-1]
cols = 1800; ci = np.linspace(0, img.shape[1] - 1, cols).astype(int)
img = img[:, ci]
v = np.clip((img + 95) / 60, 0, 1)
rgb = np.stack([v ** 0.8, v ** 1.6, 0.25 + 0.5 * v ** 3], 2)
im = Image.fromarray((rgb * 255).astype(np.uint8)); d = ImageDraw.Draw(im)
dur = len(x) / sr
for s in range(0, int(dur) + 1, 10):
    xx = int(s / dur * cols); d.line([xx, 0, xx, 8], fill=(255, 255, 255)); d.text((xx + 2, 0), '%ds' % s, fill=(255, 255, 255))
for hz in (100, 300, 1000, 3000, 8000):
    yy = rows - 1 - int(np.searchsorted(fr, hz)); d.text((2, yy - 5), '%d Hz' % hz, fill=(255, 255, 255))
im.save(sys.argv[2])
