#!/usr/bin/env python3
"""audio.py — synthesises the whole sound track from code (no samples, no libraries of sounds).
Reads the event timeline written by the renderer (footfalls, sniffs, creature distances ...) so sound and picture agree.
usage: audio.py work/timeline.json work/audio.wav"""
import json, sys, wave
import numpy as np
import scipy.signal as sg

SR = 48000
tl = json.load(open(sys.argv[1]))
OUT = sys.argv[2]
D = float(tl['duration']); N = int(D * SR)
rng = np.random.default_rng(20261116)
FPS = len(tl['cam']) / D
T = np.arange(N) / SR

def sos(kind, f, order=2):
    return sg.butter(order, np.array(f) / (SR / 2), btype=kind, output='sos')
def filt(x, kind, f, order=2):
    return sg.sosfilt(sos(kind, f, order), x)
def noise(n):
    return rng.standard_normal(int(n))
def env_exp(n, tau, attack=0.002):
    t = np.arange(int(n)) / SR
    return np.minimum(t / attack, 1.0) * np.exp(-t / tau)
def tone(f):
    return np.sin(2 * np.pi * np.cumsum(f) / SR)
def hann_env(n, a=0.15):
    n = int(n); e = np.ones(n); k = max(2, int(n * a))
    w = 0.5 - 0.5 * np.cos(np.pi * np.arange(k) / k)
    e[:k] = w; e[-k:] = w[::-1]
    return e
def track(name, col):  # per-frame track -> per-sample
    a = np.array(tl[name])[:, col]
    return np.interp(T, np.arange(len(a)) / FPS, a)
def smooth(x, sec):
    k = max(1, int(sec * SR)); c = np.cumsum(np.insert(x, 0, 0.0))
    y = (c[k:] - c[:-k]) / k
    return np.concatenate([np.full(k // 2, y[0]), y, np.full(len(x) - len(y) - k // 2, y[-1])])

class Bus:
    def __init__(self): self.l = np.zeros(N); self.r = np.zeros(N)
    def add(self, s, t0, pan=0.0, gain=1.0):
        i = int(t0 * SR)
        if i >= N or i + len(s) <= 0: return
        if i < 0: s = s[-i:]; i = 0
        s = s[:N - i]
        p = (np.clip(pan, -1, 1) + 1) * np.pi / 4
        self.l[i:i + len(s)] += s * gain * np.cos(p); self.r[i:i + len(s)] += s * gain * np.sin(p)
    def addpan(self, s, pan):  # full-length signal with time-varying pan
        p = (np.clip(pan, -1, 1) + 1) * np.pi / 4
        self.l += s * np.cos(p); self.r += s * np.sin(p)

outdoor, indoor, close, closeIn = Bus(), Bus(), Bus(), Bus()   # closeIn: close sounds that also get the glasshouse reverb
cam_speed, inside_raw, pondD, turn = track('cam', 0), track('cam', 1), track('cam', 2), track('cam', 3)
inside = smooth(inside_raw, 0.8)

# ------------------------------------------------------------------ ambience: wind, leaves, far rumble
wind = 0.62 + 0.30 * np.sin(T * 0.31 + 1.0) * np.sin(T * 0.113 + 0.4) + 0.12 * np.sin(T * 0.9)
gust = smooth(np.clip(wind, 0, 2), 0.5)
for ch in (outdoor.l, outdoor.r):
    w = filt(noise(N), 'low', 420) * (0.25 + 0.75 * gust ** 2)
    slow = 0.6 + 0.4 * smooth(np.abs(filt(noise(N), 'low', 3)), 0.3) * 40
    rustle = filt(noise(N), 'band', [1800, 7500]) * (gust ** 3) * np.clip(slow, 0, 2)
    ch += 0.020 * w + 0.0045 * rustle
    ch += 0.010 * filt(np.cumsum(noise(N)) * 0.02, 'band', [35, 130])

# ------------------------------------------------------------------ birds
def glide(f0, f1, dur, curve=1.0):
    n = int(dur * SR); x = np.linspace(0, 1, n) ** curve
    return f0 + (f1 - f0) * x
def note(f0, f1, dur, vib=0.0, vibf=28, h2=0.12, a=0.2):
    f = glide(f0, f1, dur); n = len(f)
    if vib: f = f * (1 + vib * np.sin(2 * np.pi * vibf * np.arange(n) / SR))
    ph = 2 * np.pi * np.cumsum(f) / SR
    return (np.sin(ph) + h2 * np.sin(2 * ph)) * hann_env(n, a)
def seq(notes, gaps):
    out = []
    for nt, g in zip(notes, gaps):
        out.append(nt); out.append(np.zeros(int(g * SR)))
    return np.concatenate(out)
def bird(kind):
    r = rng
    if kind == 'tit':
        f1, f2 = r.uniform(4000, 4600), r.uniform(3300, 3700); k = r.integers(6, 11)
        return seq([note(f1 if i % 2 == 0 else f2, (f1 if i % 2 == 0 else f2) * 0.96, 0.11) for i in range(k)], [0.05] * k)
    if kind == 'chiff':
        k = r.integers(8, 15)
        return seq([note(r.uniform(5200, 5600) if i % 2 == 0 else r.uniform(4000, 4400), 3600 if i % 2 == 0 else 3300, 0.085, a=0.3) for i in range(k)], [0.075] * k)
    if kind == 'blackbird':
        k = r.integers(5, 9); ns = []; gs = []
        for i in range(k):
            f = r.uniform(1700, 3300); ns.append(note(f, f * r.uniform(0.8, 1.25), r.uniform(0.12, 0.3), vib=0.012 * r.random(), vibf=r.uniform(18, 35), h2=0.2)); gs.append(r.uniform(0.02, 0.09))
        for i in range(r.integers(2, 5)):
            f = r.uniform(5000, 7500); ns.append(0.5 * note(f, f * r.uniform(0.7, 1.3), r.uniform(0.04, 0.08))); gs.append(0.02)
        return seq(ns, gs)
    if kind == 'robin':
        k = r.integers(12, 22); ns = []; gs = []
        for i in range(k):
            f = r.uniform(2600, 7600); ns.append(note(f, f * r.uniform(0.75, 1.3), r.uniform(0.04, 0.11), vib=0.02 * r.random(), vibf=40)); gs.append(r.uniform(0.01, 0.07))
        return seq(ns, gs)
    if kind == 'sparrow':
        k = r.integers(3, 9); ns = []; gs = []
        for i in range(k):
            n = int(0.07 * SR); x = np.linspace(0, 1, n)
            f = 3600 + 2200 * np.sin(np.pi * x) ** 0.7 + r.uniform(-300, 300)
            ns.append(tone(f) * hann_env(n, 0.25) * (0.6 + 0.4 * np.sign(np.sin(2 * np.pi * 150 * x * 0.07)))); gs.append(r.uniform(0.18, 0.4))
        return seq(ns, gs)
    if kind == 'pigeon':
        durs = [0.28, 0.5, 0.3, 0.26, 0.3]; f0 = r.uniform(440, 500)
        ns = [note(f0 * (1.06 if i == 1 else 1.0), f0 * 0.93, d, vib=0.01, vibf=9, h2=0.25, a=0.3) for i, d in enumerate(durs)]
        return seq(ns, [0.1, 0.12, 0.3, 0.1, 0.1])
    if kind == 'wren':
        k = r.integers(22, 34); f = r.uniform(4200, 5800)
        return seq([note(f * r.uniform(0.95, 1.05), f * 0.9, 0.035, a=0.3) for i in range(k)], [0.018] * k)
    return np.zeros(10)
def far(sig, dist):  # distance: quieter, duller, slight slap echoes from walls/trees
    s = filt(sig, 'low', max(2500, 9000 - 5500 * dist), 1) if dist > 0.3 else sig
    out = np.concatenate([s, np.zeros(int(0.55 * SR))])
    for d, g in ((0.11, 0.22), (0.19, 0.13), (0.31, 0.07)):
        k = int(d * SR * (0.6 + dist)); out[k:k + len(s)] += s * g * dist
    return out
plan = [('tit', 8), ('chiff', 7), ('blackbird', 9), ('robin', 8), ('sparrow', 12), ('pigeon', 4), ('wren', 4)]
for kind, cnt in plan:
    home = rng.uniform(-0.8, 0.8)
    for i in range(cnt):
        t0 = rng.uniform(-1, D - 2); dist = rng.uniform(0.15, 1.0)
        s = far(bird(kind), dist)
        g = {'pigeon': 0.06, 'blackbird': 0.05, 'sparrow': 0.035}.get(kind, 0.04) * (1.15 - dist) ** 1.5
        outdoor.add(s, t0, np.clip(home + rng.uniform(-0.35, 0.35), -1, 1), g)

# ------------------------------------------------------------------ the cat's own footsteps
def clicks(n, count, hp=2000):
    x = np.zeros(int(n))
    for _ in range(count):
        i = rng.integers(0, max(1, int(n) - 300)); k = rng.integers(60, 260)
        x[i:i + k] += noise(k) * env_exp(k, k / SR / 4, 0.0003) * rng.uniform(0.3, 1.0)
    return filt(x, 'high', hp)
def step_sound(surf, strength):
    if surf == 0:   # dry leaves
        n = int(0.11 * SR); s = clicks(n, 14, 2200) * 0.9 + filt(noise(n), 'band', [1200, 5500]) * env_exp(n, 0.03, 0.006) * 0.5; g = 0.10
    elif surf == 1: # grass
        n = int(0.08 * SR); s = filt(noise(n), 'band', [1500, 6500]) * env_exp(n, 0.022, 0.012); g = 0.028
    elif surf == 2: # soil / mulch
        n = int(0.07 * SR); s = filt(noise(n), 'low', 520) * env_exp(n, 0.016, 0.004) * 1.5 + clicks(n, 4, 1500) * 0.4; g = 0.05
    elif surf == 3: # gravel
        n = int(0.14 * SR); s = clicks(n, 22, 900) + filt(noise(n), 'band', [800, 5200]) * env_exp(n, 0.035, 0.004) * 0.6; g = 0.11
    else:           # stone
        n = int(0.05 * SR); s = filt(noise(n), 'low', 380) * env_exp(n, 0.012, 0.003) * 2.0
        if rng.random() < 0.35: s[200:260] += noise(60) * 0.8
        g = 0.028
    return s * g * strength * rng.uniform(0.75, 1.25)
for i, (t, strength, surf) in enumerate(tl['steps']):
    insideNow = inside_raw[min(N - 1, int(t * SR))] > 0.5
    (closeIn if insideNow else close).add(step_sound(int(surf), strength), t, 0.18 * (1 if i % 2 else -1), 1.0)

# movement rumble on the "microphone" (body noise, air)
rum = filt(noise(N), 'band', [25, 110]) * smooth(np.clip(cam_speed * 0.5 + turn * 0.05, 0, 2.5), 0.08)
close.l += 0.016 * rum; close.r += 0.016 * filt(noise(N), 'band', [25, 110]) * smooth(np.clip(cam_speed * 0.5 + turn * 0.05, 0, 2.5), 0.08)
swish = filt(noise(N), 'band', [2500, 9000]) * smooth(np.clip(turn - 2.0, 0, 8) * 0.03, 0.05)
close.l += 0.02 * swish; close.r += 0.02 * swish

# ------------------------------------------------------------------ sniffing
for a, b in tl['sniffs']:
    t = a; k = 0
    while t < b:
        n = int(0.085 * SR)
        s = filt(noise(n), 'band', [1800, 6500] if k % 2 == 0 else [1200, 4200]) * np.sin(np.pi * np.arange(n) / n) ** (0.7 if k % 2 == 0 else 1.5)
        close.add(s, t, rng.uniform(-0.1, 0.1), 0.06 * (1.0 if k % 2 == 0 else 0.55))
        t += 1.0 / 6.5 * rng.uniform(0.9, 1.1); k += 1

# ------------------------------------------------------------------ insects (distance + pan tracks from the renderer)
def insect(name, f0, harm, amp0, dref, power, lp, jit=0.04, am=0.0, amf=30.0, gate=None, hp=0):
    d = track(name, 0); pan = track(name, 1)
    on = (d > 0).astype(float)
    if gate is not None: on = on * gate
    if on.max() <= 0: return
    amp = amp0 * np.minimum(1.0, dref / np.maximum(d, 0.03)) ** power * smooth(on, 0.15)
    f = f0 * (1 + jit * smooth(filt(noise(N), 'low', 6) * 25, 0.05))
    dd = np.gradient(np.where(d > 0, d, np.nan)); dd = np.nan_to_num(dd) * SR
    f = f * (1 - np.clip(smooth(dd, 0.05), -6, 6) / 343.0 * 4.0)   # exaggerated doppler
    ph = 2 * np.pi * np.cumsum(f) / SR
    s = sum(np.sin(k * ph + k) / k ** harm for k in range(1, 11))
    if am: s = s * (1 - am + am * (0.5 + 0.5 * np.sign(np.sin(2 * np.pi * amf * T))))
    s = filt(s, 'low', lp)
    if hp: s = filt(s, 'high', hp)
    close.addpan(s * amp, pan * 0.9)
insect('bee', 188.0, 0.85, 0.085, 0.14, 1.7, 3200, jit=0.05)
insect('lady', 112.0, 0.4, 0.016, 0.10, 1.6, 2400, jit=0.08, hp=450)
perch = ((T > 50.2) & (T < 53.3))
drag_gate = smooth(1.0 - perch.astype(float), 0.1)
d = track('dragon', 0); pan = track('dragon', 1)
rat = filt(noise(N), 'band', [900, 4200]) * (0.5 + 0.5 * np.sign(np.sin(2 * np.pi * 34 * T))) * 0.02 * np.minimum(1.0, 0.4 / np.maximum(d, 0.05)) ** 1.5 * smooth((d > 0).astype(float), 0.2) * drag_gate
close.addpan(rat, pan * 0.9)

# ------------------------------------------------------------------ the person on the gravel path
for t, dist, pan in tl['human']:
    n = int(0.24 * SR)
    s = clicks(n, 40, 500) * 1.2 + filt(noise(n), 'band', [500, 5500]) * env_exp(n, 0.06, 0.004)
    s[:int(0.07 * SR)] += filt(noise(int(0.07 * SR)), 'low', 160) * env_exp(int(0.07 * SR), 0.02, 0.003) * 4.0
    g = 0.22 * min(1.0, 1.7 / max(dist, 0.5)) ** 1.3
    outdoor.add(far(s, min(dist / 8.0, 1.0)), t, pan * 0.8, g)
# (the person and the cloth swish were removed in the revision; tl['human'] is now empty)
# the cat sits at the path edge and looks around (34.8-39.8 s): two nearby calls give the head turns something to answer
outdoor.add(far(bird('sparrow'), 0.12), 34.40, 0.7, 0.042)     # from the right, down the path -> first look
outdoor.add(far(bird('blackbird'), 0.18), 35.35, 0.1, 0.042)    # from the trees beyond the bench -> look up
outdoor.add(far(bird('sparrow'), 0.3), 37.9, -0.5, 0.03)

# ------------------------------------------------------------------ pond: water, frogs, drinking, the koi's splash
near = np.clip((6.0 - pondD) / 3.5, 0, 1) * (1 - inside)
lap = filt(noise(N), 'band', [300, 1600]) * (0.3 + 0.7 * smooth(np.abs(filt(noise(N), 'low', 2.5)) * 30, 0.2))
outdoor.l += 0.006 * lap * near; outdoor.r += 0.006 * np.roll(lap, 911) * near
def plip(f0, f1, dur, g):
    n = int(dur * SR); return tone(glide(f0, f1, dur, 0.6)) * env_exp(n, dur / 3, 0.002) * g
for t in np.arange(40, 66, 0.9):
    if rng.random() < 0.6: outdoor.add(plip(rng.uniform(500, 900), rng.uniform(1000, 1700), 0.05, 0.02), t + rng.uniform(0, 0.8), rng.uniform(-0.7, 0.7), 1.0)
def croak():
    out = np.zeros(int(0.45 * SR)); f = rng.uniform(560, 700)
    for k in range(10):
        n = int(0.03 * SR); i = int(k * SR / 26)
        out[i:i + n] += (np.sin(2 * np.pi * f * np.arange(n) / SR) + 0.6 * np.sin(2 * np.pi * 2.02 * f * np.arange(n) / SR)) * env_exp(n, 0.008, 0.001)
    return out
for t, p in ((45.6, 0.6), (46.3, 0.6), (50.9, -0.4), (57.2, 0.7), (62.1, 0.2), (62.9, 0.2)):
    outdoor.add(far(croak(), 0.5), t, p, 0.035)
for t in tl['laps']:
    n = int(0.075 * SR)
    s = filt(noise(n), 'band', [500, 2600]) * env_exp(n, 0.018, 0.003)
    s[:int(0.045 * SR)] += plip(rng.uniform(850, 1050), rng.uniform(450, 600), 0.045, 0.7)
    close.add(s, t, rng.uniform(-0.05, 0.05), 0.07)
ts = tl['splash']
n = int(0.7 * SR)
sp = filt(noise(n), 'band', [200, 7000]) * env_exp(n, 0.13, 0.004) + filt(noise(n), 'low', 300) * env_exp(n, 0.06, 0.003) * 2
for k in range(7):
    i = int(rng.uniform(0.05, 0.5) * SR); b = plip(rng.uniform(350, 700), rng.uniform(800, 1500), 0.06, 0.6); sp[i:i + len(b)] += b[:max(0, n - i)]
close.add(sp, ts, 0.1, 0.11)

# ------------------------------------------------------------------ glasshouse interior: hum, mist hiss, drips
hum = 0.5 * np.sin(2 * np.pi * 100 * T) + 0.25 * np.sin(2 * np.pi * 200 * T + 1) + 0.3 * np.sin(2 * np.pi * 50 * T) + filt(noise(N), 'band', [140, 650]) * 0.9
indoor.l += 0.011 * hum * inside; indoor.r += 0.011 * np.roll(hum, 333) * inside
hiss = filt(noise(N), 'high', 5000) * inside
indoor.l += 0.0022 * hiss; indoor.r += 0.0022 * filt(noise(N), 'high', 5000) * inside
t = 66.0
while t < 92.5:
    f = rng.uniform(1700, 3300); n = int(0.09 * SR)
    s = tone(glide(f, f * 0.88, 0.09)) * env_exp(n, 0.02, 0.0008) + 0.5 * tone(glide(420, 300, 0.09)) * env_exp(n, 0.03, 0.002)
    if inside_raw[min(N - 1, int(t * SR))] > 0.5: indoor.add(s, t, rng.uniform(-0.9, 0.9), rng.uniform(0.02, 0.06))
    t += rng.uniform(0.5, 2.3)

# ------------------------------------------------------------------ foliage brushing past the head
def brush(dur, lo, hi, crackle):
    n = int(dur * SR); x = filt(noise(n), 'band', [lo, hi]) * np.sin(np.pi * np.arange(n) / n) ** 1.5
    g = smooth((rng.random(n) < crackle).astype(float), 0.004) / max(crackle, 1e-3)
    return x * np.clip(g, 0, 3)
for t, dur, lo, hi, g in ((3.4, 0.5, 900, 6000, 0.05), (4.1, 0.45, 900, 6500, 0.09), (4.5, 0.4, 1000, 7000, 0.10), (4.9, 0.5, 1200, 7000, 0.08), (5.4, 0.4, 1500, 7500, 0.06), (5.9, 0.35, 1500, 7500, 0.04),
                          (23.5, 0.4, 300, 3000, 0.06), (23.95, 0.5, 300, 3500, 0.08), (24.5, 0.4, 350, 3000, 0.05), (61.8, 0.3, 1200, 6000, 0.03)):
    close.add(brush(dur, lo, hi, 0.25), t, rng.uniform(-0.5, 0.5), g)

# ------------------------------------------------------------------ cactus prick: a startled chirp, then a head shake
tp = tl['prick']
n = int(0.20 * SR); f = glide(540, 800, 0.20, 0.7); ph = 2 * np.pi * np.cumsum(f) / SR
v = sum(np.sin(k * ph) / k for k in range(1, 14))
v = (filt(v, 'band', [800, 1500]) + 0.6 * filt(v, 'band', [2200, 3200])) * hann_env(n, 0.25) * (0.6 + 0.4 * np.sin(2 * np.pi * 31 * np.arange(n) / SR))
close.add(v, tp + 0.03, 0.0, 0.10)
for k in range(8):
    n = int(0.05 * SR); close.add(filt(noise(n), 'low', 700) * np.sin(np.pi * np.arange(n) / n) ** 2, tp + 0.32 + k / 8.5, 0.5 * (1 if k % 2 else -1), 0.05 * np.sin(np.pi * (k + 0.5) / 8))

# ------------------------------------------------------------------ pounce, jump
def thud(g, lp=220, dur=0.09):
    n = int(dur * SR); return filt(noise(n), 'low', lp) * env_exp(n, dur / 4, 0.003) * g
def scrape(g, dur=0.07):
    n = int(dur * SR); return filt(noise(n), 'band', [2500, 8000]) * env_exp(n, dur / 3, 0.004) * g
closeIn.add(scrape(0.05, 0.09), tl['pounce'] - 0.75, 0.0, 1.0)
close.add(thud(0.22) , tl['pounce'], 0.0, 1.0); close.add(scrape(0.035), tl['pounce'] + 0.01, 0.1, 1.0)
close.add(scrape(0.04, 0.1), tl['land'] - 0.54, 0.0, 1.0)
close.add(thud(0.2, 260), tl['land'], 0.0, 1.0); close.add(scrape(0.03), tl['land'] + 0.015, -0.1, 1.0)

# ------------------------------------------------------------------ purr at the end
t0p = 100.8
breath = (T - t0p) / 1.55; phase = breath - np.floor(breath)
inh = phase < 0.47
rate = np.where(inh, 26.5, 22.5) * (1 + 0.03 * np.sin(T * 1.3))
pp = np.cumsum(rate) / SR
pulse = np.diff(np.floor(pp), prepend=0) > 0
imp = np.zeros(N); imp[pulse] = 1.0
k = int(0.045 * SR); tt = np.arange(k) / SR
ker = (np.sin(2 * np.pi * 105 * tt) + 0.7 * np.sin(2 * np.pi * 212 * tt + 0.5) + 0.35 * np.sin(2 * np.pi * 330 * tt + 1.1)) * np.exp(-tt / 0.012)
purr = sg.fftconvolve(imp, ker)[:N]
gap = np.minimum(np.minimum(phase, np.abs(phase - 0.47)), 1 - phase)
purr *= np.where(inh, 1.0, 0.7) * np.clip(gap / 0.04, 0.15, 1.0)
purr += filt(noise(N), 'low', 900) * 0.25 * np.where(inh, 1.0, 0.6)
purr = filt(purr, 'low', 1100) * np.interp(T, [0, t0p, 104.5, D], [0, 0, 1, 1])
close.l += 0.060 * purr; close.r += 0.060 * purr

# ------------------------------------------------------------------ mix: outdoor sound is muffled inside the glasshouse; interior events get a short bright reverb
def reverb(x, rt=0.9):
    n = int(rt * 1.3 * SR); ir = noise(n) * np.exp(-np.arange(n) / SR * 6.9 / rt)
    ir = filt(ir, 'high', 350); ir[:int(0.004 * SR)] = 0
    for d_, g_ in ((0.011, 0.5), (0.019, 0.4), (0.031, 0.3)): ir[int(d_ * SR)] += g_ * 8
    return sg.fftconvolve(x, ir / 28.0)[:len(x)]
mixL = np.zeros(N); mixR = np.zeros(N)
for src, dst in ((outdoor.l, 'l'), (outdoor.r, 'r')):
    muff = filt(src, 'low', 1100) * 0.42
    y = src * (1 - inside) + muff * inside
    if dst == 'l': mixL += y
    else: mixR += y
mixL += indoor.l + 0.5 * reverb(indoor.l) + close.l + closeIn.l + 0.6 * reverb(closeIn.l)
mixR += indoor.r + 0.5 * reverb(indoor.r) + close.r + closeIn.r + 0.6 * reverb(closeIn.r)
mix = np.stack([mixL, mixR], 1)
mix = sg.sosfilt(sos('high', 28), mix, axis=0)
fade = np.interp(T, [0, 0.6, D - 1.3, D - 0.05], [0, 1, 1, 0])
mix *= fade[:, None]
# loudness: normalise RMS of the loudest 10-second windows, then gentle soft clip
gain = float(sys.argv[3]) if len(sys.argv) > 3 else 5.8
mix *= gain
mix = np.tanh(mix * 1.1) / 1.1
mix = sg.sosfilt(sos('low', 18000, 4), mix, axis=0)
pk = np.abs(mix).max()
if pk > 0.78: mix *= 0.78 / pk
w = wave.open(OUT, 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
w.writeframes((np.clip(mix, -1, 1) * 32767).astype('<i2').tobytes()); w.close()
print('wrote %s: %.2f s, peak %.3f, rms %.4f' % (OUT, N / SR, np.abs(mix).max(), np.sqrt((mix ** 2).mean())))
