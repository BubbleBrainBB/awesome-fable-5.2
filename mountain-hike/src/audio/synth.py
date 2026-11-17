#!/usr/bin/env python3
"""Sound track of the hike, synthesised from nothing but oscillators, noise, envelopes, filters and a
generated reverb: ambience (forest air, wind, birds), footsteps timed to the renderer's gait, and a small
score (pad, felt-piano-like additive voice, bells). The optional narration mp3s (edge-tts) are mixed in.
usage: synth.py work/edit.json work/audio.wav [--novo]"""
import sys, json, os, subprocess
import numpy as np
from scipy import signal

SR = 48000
rng = np.random.default_rng(20261115)

def db(x): return 10 ** (x / 20)
def sec(t): return int(round(t * SR))

def smooth_noise(n, rate, seed, octaves=3):
    """slow random curve in -1..1 (value noise), 'rate' = features per second"""
    r = np.random.default_rng(seed); out = np.zeros(n); amp = 1.0; tot = 0
    for o in range(octaves):
        k = int(n / SR * rate * 2 ** o) + 4; pts = r.uniform(-1, 1, k)
        x = np.linspace(0, k - 3, n); i = x.astype(int); f = x - i; f = f * f * (3 - 2 * f)
        out += amp * (pts[i] * (1 - f) + pts[i + 1] * f); tot += amp; amp *= 0.5
    return out / tot

def band_noise(n, lo, hi, seed, slope=0.0):
    """noise with a smooth band-pass drawn directly in the spectrum; slope in dB/octave"""
    r = np.random.default_rng(seed); spec = np.fft.rfft(r.standard_normal(n)); f = np.fft.rfftfreq(n, 1 / SR) + 1e-9
    w = 1 / (1 + (lo / f) ** 4) / (1 + (f / hi) ** 4) * (f / 1000.0) ** (slope / 6.02)
    y = np.fft.irfft(spec * w, n); return y / (np.std(y) + 1e-12)

def stereo(n, lo, hi, seed, slope=0.0, width=0.7):
    a = band_noise(n, lo, hi, seed, slope); b = band_noise(n, lo, hi, seed + 1, slope)
    return np.stack([a + width * b, a - width * b], 1) / np.sqrt(1 + width * width)

def reverb(x, rt60=3.0, seed=1, lo=200, hi=6000, predelay=0.02):
    n = sec(rt60 * 1.3); t = np.arange(n) / SR; out = np.zeros((len(x) + n + sec(predelay), 2))
    for ch in range(2):
        ir = band_noise(n, lo, hi, seed + ch * 7) * np.exp(-6.9 * t / rt60); ir[:sec(0.004)] *= np.linspace(0, 1, sec(0.004)); ir /= np.sqrt(np.sum(ir ** 2))
        y = signal.fftconvolve(x[:, ch], ir); out[sec(predelay):sec(predelay) + len(y), ch] = y
    return out

def place(mix, x, t, gain=1.0, pan=0.0):
    i = sec(t)
    if i >= len(mix): return
    if x.ndim == 1: l = np.cos((pan + 1) * np.pi / 4); r = np.sin((pan + 1) * np.pi / 4); x = np.stack([x * l, x * r], 1) * 1.414
    n = min(len(x), len(mix) - i)
    if i < 0: x = x[-i:]; n = min(len(x), len(mix)); i = 0
    mix[i:i + n] += gain * x[:n]

def fade(n, a, b):
    e = np.ones(n); na, nb = min(sec(a), n), min(sec(b), n)
    if na > 0: e[:na] = 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, na))
    if nb > 0: e[-nb:] *= 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, nb))
    return e

# ---------------------------------------------------------------- ambience
def wind(n, seed, strength):
    """gusting wind: low rumble on the microphone, mid whoosh, high hiss in grass and needles"""
    g = np.exp(1.1 * smooth_noise(n, 0.35, seed, 4)); g2 = np.exp(0.9 * smooth_noise(n, 0.9, seed + 3, 3))
    low = stereo(n, 45, 170, seed + 10, -3, 0.9) * (g ** 1.5)[:, None] * 0.30
    mid = stereo(n, 180, 1400, seed + 20, -4, 0.8) * (g * g2 ** 0.5)[:, None] * 0.5
    hi = stereo(n, 1800, 7500, seed + 30, -5, 0.9) * (g2 * (0.6 + 0.4 * np.exp(smooth_noise(n, 3.0, seed + 5, 2))))[:, None] * 0.22
    # a whistling resonance that drifts when it blows hard
    fc = 520 * np.exp(0.25 * smooth_noise(n, 0.5, seed + 40, 2)); ph = np.cumsum(2 * np.pi * fc / SR)
    wh = band_noise(n, 300, 1200, seed + 50) * 0 + np.sin(ph) * band_noise(n, 2, 14, seed + 51) * np.clip(g - 1.2, 0, 3) * 0.05
    return low + mid + hi + wh[:, None]

def forest_air(n, seed):
    """still forest: very quiet broadband air and a far stream somewhere below"""
    air = stereo(n, 80, 5000, seed, -6, 0.9) * 0.05
    far = stereo(n, 250, 1500, seed + 9, -3, 0.5) * (1 + 0.25 * smooth_noise(n, 2.0, seed + 2, 3))[:, None] * 0.035
    return air + far

def brook(n, seed):
    """running water over stones: several noise bands that each gurgle on their own, plus many tiny bubble pings"""
    r = np.random.default_rng(seed); out = np.zeros((n, 2))
    for k, (lo, hi, g, rate) in enumerate([(250, 700, 0.9, 5.0), (700, 1800, 1.0, 8.0), (1800, 4200, 0.7, 12.0), (4200, 9000, 0.35, 16.0)]):
        m = np.exp(0.45 * smooth_noise(n, rate, seed + 20 * k, 3)); out += stereo(n, lo, hi, seed + 100 + k * 3, -2, 0.9) * (g * m)[:, None]
    t = np.arange(sec(0.05)) / SR
    for i in range(int(n / SR * 70)):
        f = np.exp(r.uniform(np.log(500), np.log(3200))); d = r.uniform(0.008, 0.03); ping = np.sin(2 * np.pi * f * t * (1 + 2.5 * t)) * np.exp(-t / d) * r.uniform(0.3, 1.0)
        i0 = int(r.integers(0, max(1, n - len(t)))); p = r.uniform(-1, 1); out[i0:i0 + len(t), 0] += ping * (1 - p) * 0.9; out[i0:i0 + len(t), 1] += ping * (1 + p) * 0.9
    return out / (np.sqrt(np.mean(out ** 2)) + 1e-9)

def chirp(f0, f1, dur, harm=0.25, vib=0.0, vibf=30):
    n = sec(dur); t = np.arange(n) / SR; f = f0 * (f1 / f0) ** (t / dur) * (1 + vib * np.sin(2 * np.pi * vibf * t)); ph = np.cumsum(2 * np.pi * f / SR)
    env = np.sin(np.pi * np.clip(t / dur, 0, 1)) ** 0.7
    return (np.sin(ph) + harm * np.sin(2 * ph) + 0.4 * harm * np.sin(3 * ph)) * env

def bird(kind, r):
    """a few stylised songbird phrases (not imitations of recordings)"""
    out = np.zeros(sec(3.0)); t = 0.0
    if kind == 0:      # two-note 'tee-chu' repeated
        fa, fb = r.uniform(5200, 6200), r.uniform(3800, 4400)
        for k in range(r.integers(3, 6)):
            for f, d in ((fa, 0.09), (fb, 0.11)):
                c = chirp(f, f * 0.93, d); i = sec(t); out[i:i + len(c)] += c; t += d + 0.045
            t += 0.05
    elif kind == 1:    # accelerating descending trill ending in a flourish
        f = r.uniform(6000, 7000); d = 0.085
        for k in range(11):
            c = chirp(f, f * 0.8, d * 0.8); i = sec(t); out[i:i + len(c)] += c * (0.5 + 0.05 * k); t += d; d *= 0.93; f *= 0.955
        c = chirp(f * 0.9, f * 1.5, 0.16, vib=0.03); i = sec(t); out[i:i + len(c)] += c
    elif kind == 2:    # thin high 'seep' contact calls
        for k in range(r.integers(2, 4)):
            c = chirp(r.uniform(7600, 8400), 7200, 0.22, harm=0.05) * 0.6; i = sec(t); out[i:i + len(c)] += c; t += r.uniform(0.35, 0.6)
    else:              # distant raven-like croak: rough, low
        for k in range(r.integers(2, 4)):
            d = 0.28; n = sec(d); tt = np.arange(n) / SR; f = 430 * (1 - 0.25 * tt / d); ph = np.cumsum(2 * np.pi * f / SR)
            c = signal.sawtooth(ph) * (0.6 + 0.4 * signal.square(2 * np.pi * 55 * tt)) * np.sin(np.pi * tt / d) ** 0.5
            c = signal.lfilter(*signal.butter(2, [500 / (SR / 2), 2600 / (SR / 2)], 'band'), c); i = sec(t); out[i:i + n] += c * 1.5; t += 0.5
    return out[:sec(t + 0.3)]

# ---------------------------------------------------------------- footsteps
def footstep(surface, r, strength=1.0):
    n = sec(0.42); out = np.zeros(n); t = np.arange(n) / SR
    cfg = {"duff":   dict(K=26, f=(500, 2600), thump=0.9, dec=0.035, spread=0.05, gain=0.7),
           "gravel": dict(K=70, f=(1400, 7000), thump=0.6, dec=0.030, spread=0.06, gain=1.0),
           "rock":   dict(K=34, f=(1800, 6500), thump=0.8, dec=0.020, spread=0.035, gain=0.8),
           "snow":   dict(K=110, f=(500, 3200), thump=0.5, dec=0.060, spread=0.09, gain=0.9),
           "wood":   dict(K=10, f=(900, 2600), thump=0.0, dec=0.030, spread=0.02, gain=0.9)}[surface]
    for phase, amp in ((0.0, 1.0), (r.uniform(0.09, 0.13), 0.55)):          # heel, then ball of the foot
        th = np.sin(2 * np.pi * r.uniform(55, 85) * t) * np.exp(-t / cfg["dec"]) * cfg["thump"] * amp
        i0 = sec(phase); out[i0:] += th[:n - i0] * 0.5
        for k in range(int(cfg["K"] * amp)):
            t0 = phase + min(r.exponential(cfg["spread"]), 0.25); d = r.uniform(0.002, 0.012); m = sec(d)
            fc = np.exp(r.uniform(np.log(cfg["f"][0]), np.log(cfg["f"][1]))); g = np.sin(2 * np.pi * fc * np.arange(m) / SR + r.uniform(0, 6.28)) * np.hanning(m) * r.uniform(0.2, 1.0)
            g *= np.exp(-(t0 - phase) / (cfg["spread"] * 1.3)); i = sec(t0)
            if i + m < n: out[i:i + m] += g * 0.16
    if surface == "wood":      # planks: a hollow knock with a couple of board resonances
        for f0, a, dc in ((r.uniform(120, 150), 1.0, 0.07), (r.uniform(230, 290), 0.6, 0.05), (r.uniform(480, 620), 0.3, 0.03)):
            out += a * np.sin(2 * np.pi * f0 * t) * np.exp(-t / dc) * 0.5; i1 = sec(0.11); out[i1:] += 0.45 * a * (np.sin(2 * np.pi * f0 * 1.07 * t) * np.exp(-t / dc))[:n - i1] * 0.5
    if surface == "snow":      # the squeak of cold snow under the sole
        m = sec(0.16); tt = np.arange(m) / SR; f = r.uniform(900, 1300) * (1 + 0.25 * tt / 0.16)
        out[sec(0.03):sec(0.03) + m] += np.sin(np.cumsum(2 * np.pi * f / SR)) * band_noise(m, 20, 200, int(r.integers(1 << 30))) * np.hanning(m) * 0.05
    return out / (np.max(np.abs(out)) + 1e-9) * cfg["gain"] * strength

# ---------------------------------------------------------------- instruments
def midi(m): return 440.0 * 2 ** ((m - 69) / 12)

def pad_chord(notes, dur, bright=0.5, seed=0):
    """slow string/voice pad: detuned saw stacks through a moving low-pass"""
    r = np.random.default_rng(seed + 100); n = sec(dur); t = np.arange(n) / SR; L = np.zeros(n); R = np.zeros(n)
    for m in notes:
        f = midi(m)
        for d in (-7, -3, 0, 4, 8):
            ff = f * 2 ** (d / 1200) * (1 + 0.0015 * np.sin(2 * np.pi * r.uniform(0.1, 0.3) * t + r.uniform(0, 6))); ph = np.cumsum(2 * np.pi * ff / SR) + r.uniform(0, 6)
            # band-limited saw from its first partials
            s = sum(np.sin(k * ph) / k for k in range(1, max(2, min(14, int(9000 / f)))))
            p = r.uniform(-0.8, 0.8); L += s * (1 - p) * 0.5; R += s * (1 + p) * 0.5
        sub = np.sin(2 * np.pi * f / 2 * t) * 0.6; L += sub; R += sub
    x = np.stack([L, R], 1) / (len(notes) * 4)
    fc = 350 + 2200 * bright * (0.6 + 0.4 * np.sin(np.pi * t / dur)); y = np.zeros_like(x); z = np.zeros(2); z2 = np.zeros(2)
    # two cascaded one-pole low-passes with a time-varying cut-off (block-wise for speed)
    B = 256
    for i in range(0, n, B):
        a = 1 - np.exp(-2 * np.pi * fc[i] / SR); blk = x[i:i + B]
        for ch in range(2):
            o, zi = signal.lfilter([a], [1, a - 1], blk[:, ch], zi=[z[ch] * (1 - a)]); z[ch] = o[-1]
            o2, zi2 = signal.lfilter([a], [1, a - 1], o, zi=[z2[ch] * (1 - a)]); z2[ch] = o2[-1]; y[i:i + B, ch] = o2
    return y

def felt(m, dur, vel=0.6, seed=0):
    """soft piano-like tone: slightly inharmonic partials, each with its own decay, plus a hammer thump"""
    r = np.random.default_rng(seed); n = sec(dur); t = np.arange(n) / SR; f0 = midi(m); out = np.zeros(n)
    for k in range(1, 14):
        f = f0 * k * np.sqrt(1 + 0.00035 * k * k)
        if f > 9000: break
        a = (1.0 / k ** (2.1 - 0.9 * vel)) * (0.6 if k % 7 == 0 else 1.0)
        tau = (2.6 / (1 + 0.22 * k)) * (220 / f0) ** 0.35
        out += a * np.sin(2 * np.pi * f * t + r.uniform(0, 6.28)) * np.exp(-t / tau)
        out += 0.35 * a * np.sin(2 * np.pi * f * 1.0015 * t + r.uniform(0, 6.28)) * np.exp(-t / (tau * 1.6))      # second string, slow beat
    att = np.minimum(1, t / 0.006); thump = band_noise(sec(0.03), 150, 1800, int(r.integers(1 << 30))) * np.hanning(sec(0.03)) * 0.25 * vel
    out *= att; out[:len(thump)] += thump
    return out * vel * fade(n, 0, min(0.3, dur * 0.3))

def bell(m, dur, seed=0):
    n = sec(dur); t = np.arange(n) / SR; f = midi(m)
    y = np.sin(2 * np.pi * f * t + 2.2 * np.exp(-t / 0.9) * np.sin(2 * np.pi * f * 3.51 * t)) * np.exp(-t / (dur * 0.35))
    return y * np.minimum(1, t / 0.004)

# ---------------------------------------------------------------- the score
# one entry per shot: pad chords [(t_rel, dur, notes, bright)], felt notes [(t_rel, midi, vel)], bells [(t_rel, midi)]
SCORE = {
    "forest":   dict(pad=[(0, 13.5, [35, 47, 54, 61, 62], 0.18)], felt=[(6.0, 66, 0.35), (8.4, 62, 0.3)]),
    "stream":   dict(pad=[(0, 12.5, [43, 50, 54, 59, 62], 0.25)], felt=[(2.0, 71, 0.35), (3.2, 69, 0.3), (4.4, 66, 0.3), (8.0, 62, 0.3)]),
    "shafts":   dict(pad=[(0, 12.5, [40, 47, 55, 62, 66], 0.28)], felt=[(4.6, 71, 0.4), (5.8, 74, 0.42), (7.0, 78, 0.45), (9.4, 76, 0.35)]),
    "treeline": dict(pad=[(0, 6.5, [38, 50, 57, 64, 66], 0.50), (5.5, 7.0, [37, 49, 57, 64, 69], 0.55)],
                     felt=[(0.3, 66, 0.5), (1.2, 69, 0.5), (2.1, 71, 0.55), (3.9, 69, 0.45), (5.7, 73, 0.5), (6.6, 76, 0.55), (7.5, 69, 0.45), (9.3, 64, 0.4)]),
    "meadow":   dict(pad=[(0, 5.5, [35, 47, 57, 62, 66], 0.50), (4.5, 6.0, [43, 55, 59, 62, 67], 0.55)],
                     felt=[(0.4, 74, 0.5), (1.3, 71, 0.45), (2.2, 69, 0.45), (4.6, 67, 0.45), (5.5, 71, 0.5), (6.4, 74, 0.5)]),
    "saddle":   dict(pad=[(0, 6.5, [38, 50, 57, 62, 66], 0.60), (5.5, 7.0, [33, 45, 57, 61, 64], 0.60)],
                     felt=[(0.3, 78, 0.55), (1.2, 76, 0.5), (2.1, 74, 0.5), (3.0, 69, 0.45), (5.6, 73, 0.5), (6.5, 76, 0.55), (7.4, 81, 0.55), (9.2, 76, 0.4)]),
    "ridge":    dict(pad=[(0, 6.0, [28, 40, 52, 59, 62], 0.35), (5.0, 6.5, [31, 43, 50, 57, 62], 0.42)],
                     felt=[(1.0, 64, 0.4), (3.4, 67, 0.4), (5.2, 69, 0.45), (7.6, 74, 0.45)]),
    "summit":   dict(pad=[(0, 5.5, [31, 43, 50, 59, 62], 0.50), (4.5, 5.8, [33, 45, 52, 57, 61, 64], 0.60), (9.4, 12.5, [26, 38, 50, 57, 62, 66, 69], 0.85)],
                     felt=[(0.4, 67, 0.5), (1.3, 71, 0.5), (2.2, 74, 0.55), (4.6, 73, 0.5), (5.5, 76, 0.55), (6.4, 81, 0.6), (7.6, 78, 0.5),
                           (9.5, 74, 0.7), (9.5, 62, 0.6), (10.4, 78, 0.6), (11.3, 81, 0.65), (12.2, 86, 0.6), (13.6, 85, 0.5), (14.5, 81, 0.5), (15.4, 78, 0.5), (16.8, 74, 0.5), (16.8, 66, 0.4)],
                     bell=[(9.5, 86), (10.4, 90), (12.2, 93), (14.5, 90), (16.8, 98)]),
}
AMB = {  # ambience recipe per shot: kind, wind strength, footstep surface, bird kinds and how many
    "forest": ("forest", -41, "duff", [0, 1, 2, 0], 4), "stream": ("forest", -40, "duff", [2, 0], 2), "shafts": ("forest", -42, "duff", [1, 2, 0], 3),
    "treeline": ("open", -37, "gravel", [2, 0], 2), "meadow": ("open", -34, "gravel", [2], 1), "saddle": ("open", -33, "gravel", [3], 1),
    "ridge": ("open", -30, "snow", [3], 1), "summit": ("open", -30, "snow", [], 0),
}

def main():
    edit = json.load(open(sys.argv[1])); out = sys.argv[2]; novo = "--novo" in sys.argv
    shots = edit["shots"]; total = shots[-1]["start"] + shots[-1]["dur"]; N = sec(total + 0.5)
    amb = np.zeros((N, 2)); steps = np.zeros((N, 2)); dry = np.zeros((N + sec(16), 2)); birds = np.zeros((N + sec(4), 2))
    for si, sh in enumerate(shots):
        name, t0, dur = sh["name"], sh["start"], sh["dur"]; kind, wstr, surf, bk, nb = AMB[name]; n = sec(dur + 0.6); r = np.random.default_rng(1000 + si)
        a = wind(n, 500 + 13 * si, 1.0) * (0.25 if kind == "forest" else 1.0) + (forest_air(n, 700 + si) * 6.0 if kind == "forest" else 0)
        a *= db(wstr) / (np.sqrt(np.mean(a ** 2)) + 1e-9)
        place(amb, a * fade(n, 0.3, 0.3)[:, None], t0 - 0.3)                       # 0.3 s audio cross-fade under the picture cut
        if name == "stream":       # the brook swells as we reach the bridge and stand over it
            tt = np.arange(n) / SR; prox = db(-36 + 11 * np.exp(-((tt - 7.2) / 4.2) ** 2)); b = brook(n, 900 + si) * prox[:, None]; place(amb, b * fade(n, 0.5, 0.5)[:, None], t0 - 0.3)
        for k in range(nb):
            b = bird(bk[k % len(bk)], r); tb = t0 + r.uniform(0.5, max(1.0, dur - 3.0)); dist = r.uniform(0.25, 1.0)
            b = signal.lfilter(*signal.butter(1, min(0.9, (3000 + 9000 * dist) / (SR / 2))), b); b /= np.max(np.abs(b)) + 1e-9; place(birds, b, tb, db(-34) * (0.35 + 0.65 * dist), r.uniform(-0.9, 0.9))
        sp = sh.get("steps")
        if sp and os.path.exists(sp):
            for i, line in enumerate(open(sp)):
                col = [float(x) for x in line.split()]; ts, v, g = col[:3]; deck = len(col) > 3 and col[3] > 0.5
                if v < 0.25: continue
                f = footstep("wood" if deck else surf, r, strength=min(1.0, 0.55 + 0.5 * v)); place(steps, f, t0 + ts - 0.02, db(-21), 0.18 * (1 if i % 2 else -1))
        sc = SCORE[name]
        for (tr, d, notes, br) in sc.get("pad", []):
            p = pad_chord(notes, d + 1.5, br, seed=si * 10 + int(tr)); p *= fade(len(p), 1.8, 2.6)[:, None]; place(dry, p, t0 + tr - 0.6, 0.20)
        for j, (tr, m, vel) in enumerate(sc.get("felt", [])):
            f = felt(m, 5.0, vel, seed=si * 100 + j); place(dry, f, t0 + tr, 0.30, pan=(m - 72) / 40)
        for j, (tr, m) in enumerate(sc.get("bell", [])):
            place(dry, bell(m, 4.0), t0 + tr + 0.01, 0.022, pan=0.3 * (-1) ** j)
    music = dry * 0.72; wet = reverb(dry, 4.2, seed=3); music[:len(wet)] += 0.5 * wet[:len(music)]
    bw = reverb(birds, 1.8, seed=9, lo=800, hi=9000); birds = birds * 0.6; birds[:] += 0.7 * bw[:len(birds)]
    music = music[:N]; birds = birds[:N]
    music *= db(-23.0) / (np.sqrt(np.mean(music ** 2)) + 1e-9)
    # narration
    vo = np.zeros((N, 2)); duck = np.ones(N)
    vodir = edit.get("vo")
    if vodir and not novo and os.path.exists(os.path.join(vodir, "lines.json")):
        start = {s["name"]: s["start"] for s in shots}
        for ln in json.load(open(os.path.join(vodir, "lines.json"))):
            if ln["shot"] not in start: continue
            raw = subprocess.run(["ffmpeg", "-v", "error", "-i", os.path.join(vodir, ln["key"] + ".mp3"), "-f", "f32le", "-ac", "1", "-ar", str(SR), "-"], capture_output=True).stdout
            x = np.frombuffer(raw, np.float32).astype(float); x = x / (np.sqrt(np.mean(x ** 2)) + 1e-9) * db(-23.5)
            x = signal.lfilter(*signal.butter(2, 90 / (SR / 2), 'high'), x); t = start[ln["shot"]] + ln["t"]; place(vo, x, t, 1.0)
            i0, i1 = max(0, sec(t - 0.4)), min(N, sec(t + len(x) / SR + 0.5)); duck[i0:i1] = db(-5.0)
        k = sec(0.35); duck = np.convolve(duck, np.hanning(k) / np.hanning(k).sum(), 'same')
        vr = reverb(vo, 0.7, seed=21, lo=300, hi=5000, predelay=0.012); vo += 0.10 * vr[:N]
    mix = amb + steps + birds + music * duck[:, None] + vo
    # film fades (picture fades in over 0.8 s and out over 1.6 s)
    env = np.ones(N); env[:sec(0.8)] = np.linspace(0, 1, sec(0.8)) ** 2; e = sec(2.2); tail = N - sec(0.5); env[tail - e:tail] = np.linspace(1, 0, e) ** 1.5; env[tail:] = 0
    mix *= env[:, None]
    # master: fixed stem levels above, then one loudness pass to about -17 LUFS and a soft limiter at -1.5 dBFS
    mix = signal.lfilter(*signal.butter(2, 28 / (SR / 2), 'high'), mix, axis=0)[:sec(total)]
    import wave
    def write(x):
        peak = db(-1.5); x = np.where(np.abs(x) > 0.6 * peak, np.sign(x) * (0.6 * peak + (peak * 0.4) * np.tanh((np.abs(x) - 0.6 * peak) / (peak * 0.4))), x)
        w = wave.open(out, 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR); w.writeframes((np.clip(x, -1, 1) * 32767).astype('<i2').tobytes()); w.close(); return x
    def lufs():
        e = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", out, "-af", "ebur128", "-f", "null", "-"], capture_output=True, text=True).stderr
        return float([l for l in e.splitlines() if l.strip().startswith("I:")][-1].split()[1])
    write(mix); l0 = lufs(); target = db(-17.0 - l0); mix = write(mix * target); l1 = lufs()
    stems = {"ambience": amb, "steps": steps, "birds": birds, "music": music, "voice": vo}
    rep = {k: float(20 * np.log10(np.sqrt(np.mean(v[:sec(total)] ** 2)) * target + 1e-12)) for k, v in stems.items()}
    rep["mix_rms_dbfs"] = float(20 * np.log10(np.sqrt(np.mean(mix ** 2)))); rep["mix_peak_dbfs"] = float(20 * np.log10(np.max(np.abs(mix)))); rep["seconds"] = total; rep["lufs_integrated"] = l1; rep["clipped_samples"] = int(np.sum(np.abs(mix) >= 0.999))
    json.dump(rep, open(os.path.splitext(out)[0] + "_levels.json", "w"), indent=1); print(json.dumps(rep, indent=1))

if __name__ == "__main__":
    main()
