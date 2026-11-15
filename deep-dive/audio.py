#!/usr/bin/env python3
"""Soundtrack for "Deep Dive", synthesised from nothing but numpy/scipy (no samples).
   audio.py <timeline.txt> <stats.txt> <out.wav>
Layers: underwater bed (depth dependent), surge wash, regulator inhale hiss, exhale bubbles (Minnaert-tuned
blips + exhaust gurgle, driven by the same breath timeline that emits the bubbles in the picture), reef crackle
(snapping shrimp), a far whale-like call in the blue, and a very restrained pad."""
import sys, numpy as np
from scipy import signal
from scipy.io import wavfile

SR = 48000; LEN = 78.0; N = int(SR * LEN)
rng = np.random.default_rng(20261114)
t = np.arange(N) / SR

def lp(x, fc, order=2): return signal.sosfilt(signal.butter(order, fc, 'low', fs=SR, output='sos'), x)
def hp(x, fc, order=2): return signal.sosfilt(signal.butter(order, fc, 'high', fs=SR, output='sos'), x)
def bp(x, f1, f2, order=2): return signal.sosfilt(signal.butter(order, [f1, f2], 'band', fs=SR, output='sos'), x)
def sstep(a, b, x): u = np.clip((x - a) / (b - a), 0, 1); return u * u * (3 - 2 * u)
def noise(n=N): return rng.standard_normal(n)
def pan(x, p):  # p in -1..1, equal power
    a = (p + 1) * np.pi / 4; return np.stack([x * np.cos(a), x * np.sin(a)], 1)

# ---- inputs: breath timeline and camera depth
breaths = []
for line in open(sys.argv[1]):
    w = line.split()
    if w and w[0] == 'breath': breaths.append([float(v) for v in w[1:5]])
st = np.loadtxt(sys.argv[2])
depth = np.interp(t, st[:, 0] / 30.0, -st[:, 7])              # metres below the surface
deep = sstep(4.0, 20.0, depth)                                 # 0 shallow .. 1 deep

L = {k: np.zeros((N, 2)) for k in ('bed', 'inhale', 'exhale', 'crackle', 'call', 'pad', 'click')}

# ---- 1. the bed: a dark rumble, a little mid "water" and the slow wash of the surge
rum = np.stack([hp(lp(noise(), 120, 4), 32), hp(lp(noise(), 120, 4), 32)], 1); rum /= np.abs(rum).max()
slow = 0.75 + 0.25 * np.sin(2 * np.pi * 0.07 * t + 1.0)
L['bed'] += rum * (0.20 * slow * (0.8 + 0.5 * deep))[:, None]
mid = np.stack([bp(noise(), 250, 900), bp(noise(), 250, 900)], 1); mid /= np.abs(mid).max()
surge = np.abs(np.cos(t * 1.18) + 0.35 * 2.07 / 1.18 * np.cos(t * 2.07 + 1.3))    # |surge velocity|, same function as the picture
surge = lp(surge, 1.5)
L['bed'] += mid * (0.060 * (0.4 + 0.6 * surge / surge.max()) * (1 - 0.55 * deep))[:, None]
# lapping of the surface right overhead at the start
lap = bp(noise(), 400, 2500) * (0.5 + 0.5 * np.sin(2 * np.pi * 0.43 * t) ** 2) * (0.5 + 0.5 * lp(np.abs(noise()), 3))
lap /= np.abs(lap).max()
L['bed'] += pan(lap, 0.0) * (0.10 * (1 - sstep(1.5, 5.0, depth)))[:, None]

# ---- 2. regulator
def env_ads(n, a, r):
    e = np.ones(n); na, nr = int(a * SR), int(r * SR)
    e[:na] = np.linspace(0, 1, na) ** 0.7; e[-nr:] *= np.linspace(1, 0, nr) ** 1.3
    return e
for tin, din, tout, dout in breaths:
    # inhale: demand-valve hiss with a hose resonance, slight turbulence flutter, a soft valve tick
    i0 = int(tin * SR); n = int(din * SR)
    if i0 + n > 0 and i0 < N:
        x = noise(n)
        h = 0.60 * bp(x, 900, 8500, 1) + 0.35 * bp(x, 1900, 3300) + 0.22 * bp(x, 5200, 7800) + 0.45 * bp(x, 380, 760)
        fl = 1 + 0.18 * np.sin(2 * np.pi * (38 + 6 * rng.random()) * np.arange(n) / SR) + 0.25 * lp(noise(n), 25) / 0.05
        e = env_ads(n, 0.14, 0.38) * (0.62 + 0.38 * np.sin(np.pi * (np.arange(n) / n) ** 0.8))
        h = h * e * np.clip(fl, 0.3, 2.0) * 0.060
        tick = np.zeros(n); k = int(0.004 * SR); tick[:k] = noise(k) * np.exp(-np.arange(k) / (0.0008 * SR)); h += bp(tick, 1200, 6000) * 0.08
        a, b = max(i0, 0), min(i0 + n, N)
        L['inhale'][a:b] += pan(h[a - i0:b - i0], 0.0) * 0.9
    # exhale: exhaust gurgle + a swarm of bubbles ringing at their Minnaert frequency
    e0 = int(tout * SR); n = int((dout + 1.4) * SR)
    if e0 + n > 0 and e0 < N:
        tt = np.arange(n) / SR; u = np.clip(tt / dout, 0, 1)
        flow = sstep(0, 0.06, u) * (1 - 0.7 * u) * (1 - sstep(0.85, 1.0, u)) * (tt < dout)
        pulse = 0.55 + 0.45 * np.sin((tout + tt) * 31.0) * np.sin((tout + tt) * 12.7 + 1.0)        # same pulsing as the emitter
        gate = np.clip(lp(noise(n), 22) / 0.03, -1, 1) * 0.5 + 0.5
        g = (0.8 * lp(noise(n), 420, 4) / 0.12 + 0.35 * bp(noise(n), 500, 1400) / 0.2) * flow * pulse * (0.35 + 0.65 * gate) * 0.045
        buf = pan(g, 0.0)
        nb = int(62 * dout * 0.55 * 1.6)
        for _ in range(nb):
            # arrival times follow the flow; a tail of small stragglers after the breath
            while True:
                tb = rng.random() * (dout + 1.2)
                uu = min(tb / dout, 1.0)
                f = (min(uu / 0.06, 1) * (1 - 0.7 * uu)) if tb < dout else 0.10 * np.exp(-(tb - dout) / 0.5)
                if rng.random() < f: break
            r = np.clip(0.0034 * np.exp(1.5 * (rng.random() + rng.random() + rng.random() - 1.5)), 0.0016, 0.024)
            f0 = 3.26 / r * np.sqrt(1 + depth[min(max(e0, 0), N - 1)] / 10.0)                     # Minnaert, stiffer with pressure
            f0 = min(f0, 5200.0)
            dur = np.clip(14.0 / f0, 0.004, 0.09); m = int(dur * 5 * SR)
            if int(tb * SR) + m >= n: continue
            k = np.arange(m) / SR
            blip = np.sin(2 * np.pi * f0 * (k + 0.9 * k * k / dur * 0.08)) * np.exp(-k / dur) * np.minimum(k / 0.0006, 1)
            amp = 0.022 * (r / 0.004) ** 0.9 * (0.5 + rng.random()) * (0.45 if tb > dout else 1.0)
            j = int(tb * SR)
            buf[j:j + m] += pan(blip * amp, rng.uniform(-0.75, 0.75))
        a, b = max(e0, 0), min(e0 + n, N)
        L['exhale'][a:b] += buf[a - e0:b - e0]

# ---- 3. reef crackle: snapping shrimp, dense over the reef, thinning in the deep
ncl = int(LEN * 26)
ct = rng.random(ncl) * LEN
crk = np.zeros((N, 2))
for c in ct:
    i = int(c * SR)
    if i >= N - 400: continue
    dens = (1 - 0.8 * deep[i]) * sstep(2.0, 6.0, depth[i])
    if rng.random() > dens: continue
    m = int(rng.uniform(0.002, 0.005) * SR)
    k = noise(m) * np.exp(-np.arange(m) / (m / 4.0))
    crk[i:i + m] += pan(k, rng.uniform(-1, 1)) * rng.uniform(0.2, 1.0) ** 2
crk = np.stack([bp(crk[:, 0], 2200, 9000), bp(crk[:, 1], 2200, 9000)], 1)
L['crackle'] += crk

# ---- reverb impulse (decaying filtered noise) for the far sounds
def reverb(x, rt=3.2, fc=1800, wet=1.0):
    m = int(rt * SR); ir = noise(m) * np.exp(-np.arange(m) / (rt / 6.9 * SR)); ir = lp(ir, fc); ir /= np.sqrt((ir ** 2).sum())
    return signal.fftconvolve(x, ir)[:len(x)] * wet

# ---- 4. a far call in the blue (whale-like moans), before and while the manta appears
call = np.zeros(N)
for t0, f1, f2, dur, a in [(57.5, 190, 255, 2.6, 0.8), (61.2, 250, 170, 2.2, 0.7), (66.0, 160, 300, 3.2, 1.0), (71.5, 290, 210, 2.8, 0.8), (74.6, 180, 235, 2.4, 0.55)]:
    m = int(dur * SR); k = np.arange(m) / SR; u = k / dur
    f = f1 + (f2 - f1) * (u * u * (3 - 2 * u)) + 5 * np.sin(2 * np.pi * 4.7 * k) * u
    ph = 2 * np.pi * np.cumsum(f) / SR
    v = np.sin(ph) + 0.45 * np.sin(2 * ph + 0.3) + 0.22 * np.sin(3 * ph + 1.1) + 0.1 * np.sin(5 * ph)
    e = np.sin(np.pi * u) ** 1.5
    i = int(t0 * SR); call[i:i + m] += v * e * a
call = lp(call, 900)
cw = reverb(call, 4.0, 1200)
L['call'] += np.stack([cw + 0.25 * call, np.roll(cw, 331) + 0.25 * call], 1)

# ---- 5. pad: slow, consonant, almost not there
def pad_note(freq, t0, t1, amp):
    a, b = int(max(t0, 0) * SR), int(min(t1, LEN) * SR); k = np.arange(b - a) / SR
    e = sstep(0, 3.5, k) * (1 - sstep(t1 - t0 - 4.5, t1 - t0, k))
    x = np.zeros((b - a, 2))
    for d, p in [(-0.0025, -0.5), (0.0, 0.0), (0.0027, 0.5)]:
        ph = 2 * np.pi * freq * (1 + d) * k + rng.random() * 6.28
        v = np.sin(ph) + 0.30 * np.sin(2 * ph) + 0.12 * np.sin(3 * ph)
        x += pan(v * (0.8 + 0.2 * np.sin(2 * np.pi * 0.11 * k + p)), p)
    y = np.zeros((N, 2)); y[a:b] = x * (e * amp)[:, None]; return y
A = 110.0
def n_(semi): return A * 2 ** (semi / 12.0)
pad = np.zeros((N, 2))
chords = [  # (start, end, semitones above A2)
    (7.0, 22.0, [5, 12, 16, 19]),      # D  A  C#? -> D A (C# avoided): D3 A3 C#4.. use Dadd9 instead below
]
chords = [
    (7.0, 22.5, [5, 12, 17, 19]),      # D3 A3 D4 E4      (Dsus2)
    (20.0, 34.5, [1, 13, 17, 20]),     # Bb2 Bb3 D4 F4    (Bb)
    (32.5, 45.0, [-4, 8, 15, 20]),     # F2 F3 C4 F4      (F5)
    (43.0, 58.0, [-7, 5, 12, 17]),     # D2 D3 A3 D4      (D5, darker for the wall)
    (56.0, 67.5, [-11, 1, 8, 17]),     # Bb1 Bb2 F3 D4
    (65.0, 78.0, [-4, 8, 12, 15, 24]), # F2 F3 A3 C4 A4   (F, opens for the manta)
]
for t0, t1, notes in chords:
    for j, s in enumerate(notes): pad += pad_note(n_(s), t0, t1, 0.5 / (1 + 0.35 * j))
pad = np.stack([lp(pad[:, 0], 1400), lp(pad[:, 1], 1400)], 1)
pad = pad + 0.9 * np.stack([reverb(pad[:, 0], 3.5, 1500), reverb(pad[:, 1], 3.5, 1500)], 1) / 6.0
L['pad'] += pad

# ---- torch switch, a tiny dry click heard through the hand
i = int(45.62 * SR); m = int(0.012 * SR)
L['click'][i:i + m] += pan(bp(noise(m) * np.exp(-np.arange(m) / (0.0015 * SR)), 900, 5000), 0.3)

# ---- mix: every layer is set to a target level measured over the time it is actually sounding.
# The regulator is the loudest thing a diver hears; everything else sits well under it.
TARGET = {'bed': -33.0, 'inhale': -22.0, 'exhale': -17.5, 'crackle': -41.0, 'call': -35.0, 'pad': -34.0, 'click': -30.0}
out = np.zeros((N, 2))
for k, x in L.items():
    p = (x ** 2).sum(1); act = p > p.max() * 1e-4
    r = np.sqrt((x[act] ** 2).mean()) if act.any() else 1.0
    g = 10 ** (TARGET[k] / 20.0) / r
    out += x * g
    print(f'  layer {k:8s} active {act.mean()*100:5.1f}%  level while active {TARGET[k]:6.1f} dBFS  peak {20*np.log10(np.abs(x*g).max()+1e-9):6.1f} dBFS')
# master: everything under water is a little muffled; fades match the picture; gentle limiter
out = np.stack([lp(out[:, 0], 11000), lp(out[:, 1], 11000)], 1)
fade = sstep(0.0, 1.3, t) * (1 - sstep(LEN - 2.6, LEN - 0.1, t))
out *= fade[:, None]
out = np.tanh(out * 2.2) / 2.2
peak = np.abs(out).max()
if peak > 0.89: out *= 0.89 / peak
wavfile.write(sys.argv[3], SR, (out * 32767).astype(np.int16))
print(f'audio: {LEN}s, rms {20*np.log10(np.sqrt((out**2).mean())):.1f} dBFS, peak {20*np.log10(np.abs(out).max()):.1f} dBFS, {len(breaths)} breaths in timeline')
