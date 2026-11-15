#!/usr/bin/env python3
"""Waveform / spectrum level check of the soundtrack (nobody can listen here, so this is all the QC there is).
   audio_check.py <audio.wav> <timeline.txt> <out.png>
Draws a log-frequency spectrogram with the inhale (green) / exhale (orange) windows of the breath timeline on top,
and prints how the energy in the hiss band and the bubble band lines up with those windows."""
import sys, numpy as np
from scipy.io import wavfile
from scipy import signal
from PIL import Image, ImageDraw
sr, x = wavfile.read(sys.argv[1]); x = x.astype(np.float64) / 32768.0
mono = x.mean(1); T = len(mono) / sr
br = [[float(v) for v in l.split()[1:5]] for l in open(sys.argv[2]) if l.startswith('breath')]
f, tt, S = signal.spectrogram(mono, sr, nperseg=4096, noverlap=4096 - 960, window='hann')   # 20 ms hop
P = 10 * np.log10(S + 1e-14)
W, H = 1950, 560
fl = np.geomspace(30, 16000, H)
idx = np.searchsorted(f, fl).clip(0, len(f) - 1)
img = P[idx][::-1]
ti = np.linspace(0, P.shape[1] - 1, W).astype(int); img = img[:, ti]
lo, hi = np.percentile(img, 5), img.max()
g = np.clip((img - lo) / (hi - lo), 0, 1)
rgb = np.stack([g ** 0.6 * 255, g ** 1.2 * 255, 60 + g ** 2.5 * 195], 2).astype(np.uint8)
im = Image.new('RGB', (W, H + 60), (10, 10, 20)); im.paste(Image.fromarray(rgb), (0, 0)); d = ImageDraw.Draw(im)
for tin, din, tout, dout in br:
    d.rectangle([tin / T * W, H + 6, (tin + din) / T * W, H + 22], fill=(60, 200, 90))
    d.rectangle([tout / T * W, H + 26, (tout + dout) / T * W, H + 42], fill=(240, 150, 40))
for s in range(0, int(T) + 1, 5): d.text((s / T * W + 2, H + 46), f'{s}s', fill=(220, 220, 220))
for fr in (100, 1000, 10000):
    y = H - 1 - np.searchsorted(fl, fr); d.line([0, y, W, y], fill=(90, 90, 90)); d.text((4, y - 11), f'{fr} Hz', fill=(230, 230, 230))
im.save(sys.argv[3])
# band energies vs. breath windows
def band(a, b): return S[(f >= a) & (f < b)].sum(0)
hiss, bub = band(1500, 9000), band(150, 1400)
def inwin(t0, t1): return (tt >= t0) & (tt < t1)
mi = np.zeros(len(tt), bool); me = np.zeros(len(tt), bool)
for tin, din, tout, dout in br: mi |= inwin(tin + 0.1, tin + din - 0.1); me |= inwin(tout + 0.05, tout + dout - 0.2)
rest = ~(mi | me) & (tt > 2) & (tt < T - 3)
db = lambda v: 10 * np.log10(v + 1e-14)
print(f'hiss band 1.5-9 kHz : inhale {db(hiss[mi].mean()):.1f} dB, exhale {db(hiss[me].mean()):.1f} dB, between breaths {db(hiss[rest].mean()):.1f} dB')
print(f'bubble band .15-1.4k: inhale {db(bub[mi].mean()):.1f} dB, exhale {db(bub[me].mean()):.1f} dB, between breaths {db(bub[rest].mean()):.1f} dB')
# onset alignment: first time each exhale's bubble-band energy rises 6 dB over the preceding 0.3 s
offs = []
for tin, din, tout, dout in br:
    if tout < 1.5 or tout > T - 4: continue
    pre = bub[inwin(tout - 0.3, tout - 0.02)].mean(); k = np.where((tt >= tout - 0.2) & (bub > pre * 4))[0]
    if len(k): offs.append(tt[k[0]] - tout)
print('exhale onset (audio rise minus timeline), s:', np.round(offs, 3).tolist())
rms = np.sqrt((x ** 2).mean()); print(f'rms {20*np.log10(rms):.1f} dBFS, peak {20*np.log10(np.abs(x).max()):.1f} dBFS, L/R corr {np.corrcoef(x[:,0], x[:,1])[0,1]:.2f}, dc {x.mean():.5f}')
