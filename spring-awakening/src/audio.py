"""audio.py — the whole sound track synthesised with numpy/scipy: wind, drips, meltwater, birds, rain, wings, music;
mixes in the narration (work/narr/*.wav), ducks everything else under it, masters to about -16 LUFS / -1.5 dBTP.
Writes work/mix.wav, narration.srt and work/audio_report.txt."""
import os, sys, wave, numpy as np
from scipy.signal import butter, sosfilt, sosfiltfilt, fftconvolve, lfilter, resample_poly
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import grade, variant
narration = variant

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SR = 48000
T = narration.shot_times()
DUR = sum(s for _, s, _ in grade.SHOTS)
N = int(round(DUR * SR))
rng = np.random.default_rng(31415)
tt = np.arange(N) / SR

def env(points):
    """piecewise-linear gain curve from (time, value) points"""
    p = sorted(points); return np.interp(tt, [a for a, _ in p], [b for _, b in p]).astype(np.float32)
def bp(x, lo, hi, order=2):
    return sosfilt(butter(order, [lo, hi], 'bandpass', fs=SR, output='sos'), x)
def lp(x, f, order=2):
    return sosfilt(butter(order, f, 'lowpass', fs=SR, output='sos'), x)
def hp(x, f, order=2):
    return sosfilt(butter(order, f, 'highpass', fs=SR, output='sos'), x)
def shaped_noise(n, f_lo, f_hi, tilt=-1.0):
    """noise with a power-law spectrum between two corner frequencies (shaped in the frequency domain)"""
    X = np.fft.rfft(rng.standard_normal(n)); f = np.fft.rfftfreq(n, 1 / SR)
    g = np.zeros_like(f); m = (f > f_lo) & (f < f_hi)
    g[m] = (f[m] / f_lo) ** (tilt / 2)
    g *= np.clip((f - f_lo) / (f_lo * 0.5 + 1), 0, 1) * np.clip((f_hi - f) / (f_hi * 0.2), 0, 1)
    y = np.fft.irfft(X * g, n); return (y / (np.std(y) + 1e-9)).astype(np.float32)
def slow(n, f):
    y = lp(rng.standard_normal(n), f, 2); return (y / (np.std(y) + 1e-9)).astype(np.float32)
def pan(x, p):
    """constant-power pan, p in [-1, 1] -> (n, 2)"""
    a = (p + 1) * np.pi / 4; return np.stack([x * np.cos(a), x * np.sin(a)], axis=1)
def place(buf, x, t0):
    i = int(round(t0 * SR))
    if i >= N or i + len(x) <= 0: return
    a = max(0, -i); b = min(len(x), N - i)
    buf[i + a:i + b] += x[a:b]

# ---------- reverb (own impulse response: decaying shaped noise) ----------
def make_ir(t60, bright):
    n = int(SR * t60 * 1.2); t = np.arange(n) / SR
    ir = np.stack([shaped_noise(n, 150, bright, -1.5), shaped_noise(n, 150, bright, -1.5)], 1) * np.exp(-6.9 * t / t60)[:, None]
    ir[:int(0.012 * SR)] *= np.linspace(0, 1, int(0.012 * SR))[:, None]
    return (ir / np.sqrt(np.sum(ir ** 2))).astype(np.float32)
IR_WOOD = make_ir(1.6, 7000); IR_HALL = make_ir(3.4, 5000)
def reverb(x, ir):
    return np.stack([fftconvolve(x[:, 0], ir[:, 0])[:N], fftconvolve(x[:, 1], ir[:, 1])[:N]], 1).astype(np.float32)

# ---------- ambience ----------
def wind():
    a = shaped_noise(N, 90, 900, -2.2) * (0.55 + 0.45 * np.clip(slow(N, 0.12) * 0.6 + 0.5, 0, 1))
    b = shaped_noise(N, 90, 900, -2.2) * (0.55 + 0.45 * np.clip(slow(N, 0.12) * 0.6 + 0.5, 0, 1))
    hiss = shaped_noise(N, 1500, 6000, -1.0) * np.clip(slow(N, 0.2) * 0.5 + 0.3, 0, 1) * 0.12
    g = env([(0, 1.0), (T['icicles'][0], 0.9), (T['drop'][0], 0.55), (T['thaw'][0], 0.45), (T['seedling'][0], 0.25), (T['rain'][0], 0.3), (T['spring'][0], 0.22), (DUR, 0.2)])
    return np.stack([a + hiss, b + hiss], 1) * g[:, None] * 0.085

def plink(f0, amp=1.0, deep=1.0):
    """a water drop: a bubble whose pitch rises as it shrinks, plus a tiny surface tick"""
    d = 0.09 * deep; n = int(d * SR); t = np.arange(n) / SR
    f = f0 * (1 + 1.4 * (1 - np.exp(-t / (0.02 * deep))))
    y = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t / (0.018 * deep)) * (1 - np.exp(-t / 0.0008))
    tick = bp(rng.standard_normal(n), 2500, 9000) * np.exp(-t / 0.002) * 0.25
    return ((y + tick) * amp).astype(np.float32)

def drips_and_water():
    buf = np.zeros((N, 2), np.float32)
    # single drips under the icicles, getting more frequent
    t = T['icicles'][0] + 0.9
    while t < T['thaw'][0] + 2.5:
        u = (t - T['icicles'][0]) / (T['thaw'][0] + 2.5 - T['icicles'][0])
        if not (T['drop'][0] - 0.2 < t < T['drop'][1] - 0.6):
            place(buf, pan(plink(rng.uniform(900, 2200), rng.uniform(0.25, 0.6)), rng.uniform(-0.7, 0.7)), t)
        t += rng.uniform(0.9, 1.6) * (1.25 - 0.9 * u)
    # the one drop of the slow-motion close-up, low and round, landing just before the cut
    place(buf, pan(plink(420, 0.9, deep=2.6), 0.05), T['drop'][1] - 0.42)
    # meltwater trickle: many small bubbles + a little moving noise
    g = env([(T['thaw'][0] - 0.2, 0), (T['thaw'][0] + 2.0, 0.8), (T['thaw'][1], 1.0), (T['crocus'][0] + 1, 0.7), (T['crocus'][1], 0.5), (T['seedling'][0] + 1.5, 0.22),
             (T['fern'][0], 0.14), (T['rain'][0], 0.10), (T['snail'][0], 0.2), (T['spring'][0], 0.22), (T['spring'][0] + 3, 0.5), (DUR, 0.45)])
    tr = np.zeros((N, 2), np.float32)
    t0 = T['thaw'][0] - 0.2; nb = int((DUR - t0) * 75)
    times = np.sort(rng.uniform(t0, DUR, nb))
    for tm in times:
        f0 = np.exp(rng.uniform(np.log(700), np.log(3800)))
        place(tr, pan(plink(f0, rng.uniform(0.05, 0.3) * (1400 / f0) ** 0.3, deep=rng.uniform(0.5, 1.1)), rng.uniform(-0.9, 0.9)), tm)
    wash = shaped_noise(N, 500, 5000, -1.2) * (0.6 + 0.4 * np.clip(slow(N, 3.0), -1, 1)) * 0.035
    tr += np.stack([wash, np.roll(wash, 977)], 1)
    buf += tr * g[:, None] * 0.55
    # after the rain: drips from the leaves
    t = T['rain'][1] - 1.0
    while t < T['snail'][1]:
        place(buf, pan(plink(rng.uniform(1100, 2600), rng.uniform(0.2, 0.5)), rng.uniform(-0.8, 0.8)), t); t += rng.uniform(0.25, 0.9)
    return buf

def rain():
    g = env([(T['rain'][0] - 0.15, 0), (T['rain'][0] + 0.5, 0.85), (T['rain'][0] + 2.5, 1.0), (T['rain'][1], 0.9), (T['snail'][0] + 2.8, 0.12), (T['snail'][0] + 4.5, 0.0)])
    L = shaped_noise(N, 350, 11000, -0.9) * (0.8 + 0.2 * np.clip(slow(N, 0.5), -1, 1))
    R = shaped_noise(N, 350, 11000, -0.9) * (0.8 + 0.2 * np.clip(slow(N, 0.5), -1, 1))
    low = shaped_noise(N, 60, 300, -2.0) * 0.5
    buf = np.stack([L + low, R + low], 1) * 0.11
    taps = np.zeros((N, 2), np.float32)
    t0, t1 = T['rain'][0], T['snail'][0] + 3.5
    for tm in np.sort(rng.uniform(t0, t1, int((t1 - t0) * 34))):
        n = int(0.02 * SR); t = np.arange(n) / SR
        fc = rng.uniform(1200, 5200)
        x = bp(rng.standard_normal(n), fc * 0.7, min(fc * 1.5, 11000)) * np.exp(-t / rng.uniform(0.002, 0.006)) * rng.uniform(0.1, 0.6)
        place(taps, pan(x.astype(np.float32), rng.uniform(-1, 1)), tm)
    return (buf + taps * 0.35) * g[:, None]

# ---------- birds ----------
def tone(freqs, amp_env, harm=(1.0, 0.12, 0.04)):
    ph = 2 * np.pi * np.cumsum(freqs) / SR
    return sum(h * np.sin((k + 1) * ph) for k, h in enumerate(harm)) * amp_env
def note(f0, f1, d, vib=0.0, vibf=28.0, attack=0.008, curve=1.0):
    n = int(d * SR); t = np.arange(n) / SR; u = t / d
    f = f0 + (f1 - f0) * u ** curve
    if vib: f = f * (1 + vib * np.sin(2 * np.pi * vibf * t))
    a = np.minimum(1, t / attack) * np.minimum(1, (d - t) / (d * 0.35)) ** 1.5
    return tone(f, a).astype(np.float32)
def seq(parts):
    """parts: list of (gap_before, signal)"""
    n = sum(int(g * SR) + len(s) for g, s in parts); out = np.zeros(n, np.float32); i = 0
    for g, s in parts:
        i += int(g * SR); out[i:i + len(s)] += s; i += len(s)
    return out
def tit():
    hi, lo = rng.uniform(4200, 4700), rng.uniform(3200, 3600); k = rng.integers(3, 6)
    return seq([(0.05, note(hi, hi * 0.97, 0.11)), (0.03, note(lo, lo * 0.98, 0.13))] * k)
def chiffchaff():
    parts = []
    for i in range(rng.integers(6, 10)):
        f = rng.choice([4700, 4000, 4350]) * rng.uniform(0.98, 1.02); parts.append((0.17, note(f * 1.08, f, 0.085, curve=0.5)))
    return seq(parts)
def blackbird():
    parts = []
    for i in range(rng.integers(4, 7)):
        f0 = rng.uniform(1700, 2900); f1 = f0 * rng.uniform(0.82, 1.25); d = rng.uniform(0.14, 0.38)
        parts.append((rng.uniform(0.03, 0.12), note(f0, f1, d, vib=rng.uniform(0, 0.012), vibf=rng.uniform(5, 9), attack=0.02)))
    parts.append((0.05, note(5200, 6400, 0.12, vib=0.04, vibf=40) * 0.35))   # the squeaky ending
    return seq(parts)
def robin():
    parts = []
    f = rng.uniform(5200, 6600)
    for i in range(rng.integers(9, 15)):
        f *= rng.uniform(0.9, 1.06); parts.append((rng.uniform(0.015, 0.07), note(f, f * rng.uniform(0.8, 1.2), rng.uniform(0.04, 0.11), vib=0.02, vibf=45)))
    return seq(parts) * 0.8
def cuckoo():
    a = note(667, 655, 0.20, attack=0.03); b = note(540, 528, 0.34, attack=0.03)
    return lp(seq([(0, a), (0.07, b)]) * 1.0, 2500).astype(np.float32)
def drum():
    parts = []; k = rng.integers(13, 18)
    for i in range(k):
        n = int(0.03 * SR); t = np.arange(n) / SR
        x = (np.sin(2 * np.pi * 850 * t) * np.exp(-t / 0.006) + bp(rng.standard_normal(n), 600, 2500) * np.exp(-t / 0.003) * 0.6) * (1 - 0.75 * i / k)
        parts.append((0.052 - 0.014 * i / k, x.astype(np.float32)))
    return seq(parts)
def bird(buf_dry, buf_wet, t0, sig, p, dist, amp=1.0):
    """dist 0 (near) .. 1 (far): farther birds are duller, quieter and wetter"""
    x = lp(sig, 9000 - 5500 * dist) * amp * (0.9 - 0.65 * dist)
    place(buf_dry, pan(x.astype(np.float32), p), t0)
    place(buf_wet, pan(x.astype(np.float32), p * 0.6) * (0.35 + 0.9 * dist), t0)

def birds():
    dry = np.zeros((N, 2), np.float32); wet = np.zeros((N, 2), np.float32)
    def scatter(name, makers, count, amp=1.0, t_in=0.3, t_out=0.3, far=(0.2, 0.9)):
        a, b = T[name]
        for tm in np.sort(rng.uniform(a + t_in, b - t_out, count)):
            mk = makers[rng.integers(len(makers))]
            bird(dry, wet, tm, mk(), rng.uniform(-0.9, 0.9), rng.uniform(*far), amp)
    bird(dry, wet, T['icicles'][0] + 4.3, tit(), 0.5, 0.85, 0.6)           # the first voice, far away
    bird(dry, wet, T['thaw'][0] + 4.8, tit(), -0.6, 0.7, 0.7)
    scatter('crocus', [tit, chiffchaff], 3, 0.8)
    scatter('seedling', [tit, chiffchaff, robin], 5, 0.85)
    scatter('fern', [tit, blackbird, robin, chiffchaff], 5, 0.9)
    bird(dry, wet, T['fern'][0] + 3.6, drum(), -0.4, 0.75, 0.9)
    scatter('bud', [tit, blackbird, robin, chiffchaff, blackbird], 8, 0.95)
    bird(dry, wet, T['bud'][0] + 2.2, cuckoo(), 0.3, 0.95, 1.6); bird(dry, wet, T['bud'][0] + 4.4, cuckoo(), 0.3, 0.95, 1.6)
    scatter('ladybird', [tit, blackbird, robin, chiffchaff], 7, 0.85)
    scatter('takeoff', [blackbird], 1, 0.35, far=(0.8, 0.95))
    scatter('snail', [robin, blackbird, tit], 3, 0.7, t_in=2.5)
    scatter('spring', [tit, blackbird, robin, chiffchaff, blackbird, robin], 13, 1.0, t_out=1.5)
    for k in range(3): bird(dry, wet, T['spring'][0] + 1.5 + 2.3 * k, cuckoo(), -0.35, 0.95, 1.6)
    bird(dry, wet, T['spring'][0] + 6.2, drum(), 0.6, 0.8, 0.8)
    return (dry + reverb(wet, IR_WOOD) * 0.8) * 0.16

def wings():
    """slow-motion take-off: a soft tick as the wing cases open, then a low fluttering of air"""
    buf = np.zeros((N, 2), np.float32); t0 = T['takeoff'][0]
    n = int(0.03 * SR); t = np.arange(n) / SR
    place(buf, pan((bp(rng.standard_normal(n), 900, 4000) * np.exp(-t / 0.004) * 0.25).astype(np.float32), -0.3), t0 + 0.5)
    d = T['takeoff'][1] - (t0 + 1.45); n = int(d * SR); t = np.arange(n) / SR
    beat = 0.5 + 0.5 * np.sin(2 * np.pi * 6.8 * t - 1.2); beat = beat ** 2.5   # one puff per half-stroke of the 3.4 Hz slow-motion wingbeat
    air = shaped_noise(n, 70, 700, -1.6) * beat + shaped_noise(n, 900, 3500, -1.0) * beat * 0.12
    g = np.interp(t, [0, 0.6, 1.3, 2.0, d], [0, 0.6, 1.0, 0.7, 0.0])
    p = np.interp(t, [0, 0.8, d], [-0.35, -0.2, 0.75])
    a = (p + 1) * np.pi / 4
    place(buf, np.stack([air * g * np.cos(a), air * g * np.sin(a)], 1).astype(np.float32) * 0.3, t0 + 1.45)
    return buf

# ---------- music ----------
def midi(m): return 440.0 * 2 ** ((m - 69) / 12)
def pad_note(m, t0, t1, amp=1.0):
    """soft string-like pad: three slightly detuned harmonic stacks, slow attack and release"""
    a = max(0, t0 - 1.2); b = min(DUR, t1 + 3.0); n = int((b - a) * SR); t = np.arange(n) / SR
    f = midi(m); y = np.zeros((n, 2), np.float32)
    for k, (det, pn) in enumerate([(-0.0035, -0.55), (0.0, 0.0), (0.0041, 0.55)]):
        ph = 2 * np.pi * f * (1 + det) * t + rng.uniform(0, 6.28) + 0.5 * np.sin(2 * np.pi * (0.11 + 0.03 * k) * t)
        s = sum(np.sin(h * ph) / h ** 1.9 for h in range(1, 7))
        y += pan(s.astype(np.float32), pn)
    e = np.clip((t + a - (t0 - 1.2)) / 2.6, 0, 1) ** 1.5 * np.clip((b - (t + a)) / 3.2, 0, 1) ** 1.5
    e *= 0.85 + 0.15 * np.sin(2 * np.pi * 0.13 * t + m)
    out = np.zeros((N, 2), np.float32); i = int(a * SR); out[i:i + n] = y * (e * amp)[:, None]; return out
def bell(m, amp=1.0, d=5.0):
    """felt-piano-like tone: quick soft attack, slow decay, upper partials dying faster"""
    n = int(d * SR); t = np.arange(n) / SR; f = midi(m); y = np.zeros(n, np.float32)
    for h, w in [(1, 1.0), (2, 0.42), (3, 0.16), (4, 0.07), (5, 0.03)]:
        y += w * np.sin(2 * np.pi * f * h * (1 + 0.0004 * h * h) * t) * np.exp(-t * (0.7 + 0.55 * h))
    y *= 1 - np.exp(-t / 0.012)
    return (lp(y, 3200) * amp).astype(np.float32)

def music():
    pads = np.zeros((N, 2), np.float32); keys = np.zeros((N, 2), np.float32)
    S = lambda n: T[n][0]
    chords = [  # (start, end, midi notes, level)
        (0.0, S('icicles'), [38, 45], 0.75), (0.0, S('drop') + 2, [69], 0.14),                      # D-A, a cold high A
        (S('icicles'), S('thaw'), [38, 45, 52], 0.75),                                               # Dsus2
        (S('thaw'), S('crocus'), [43, 50, 55, 59], 0.8),                                             # G
        (S('crocus'), S('seedling'), [42, 50, 57, 62], 0.85),                                        # D/F#
        (S('seedling'), S('seedling') + 4.8, [43, 50, 59, 62], 0.9), (S('seedling') + 4.8, S('fern'), [45, 52, 57, 61], 0.9),   # G -> A
        (S('fern'), S('bud'), [47, 54, 59, 62], 0.9),                                                # Bm
        (S('bud'), S('ladybird'), [43, 55, 59, 62, 66], 1.0),                                        # Gmaj7
        (S('ladybird'), S('takeoff'), [38, 50, 57, 62, 66], 1.0),                                    # D
        (S('takeoff'), S('rain'), [45, 52, 57, 64], 0.8),                                            # Asus
        (S('rain'), S('snail'), [40, 52, 55, 59], 0.7),                                              # Em
        (S('snail'), S('spring'), [43, 50, 55, 59, 62], 0.85),                                       # G
        (S('spring'), DUR - 1.0, [38, 45, 50, 57, 62, 66, 69], 1.05), (S('spring') + 5, DUR - 0.5, [76], 0.3),   # D, opening out
    ]
    for a, b, ns, lv in chords:
        for m in ns: pads += pad_note(m, a, b, lv * (0.8 if m < 45 else 0.55))
    motif = [  # sparse notes, placed in the gaps of the narration where possible
        (S('thaw') + 0.3, 74, 0.5), (S('thaw') + 6.2, 69, 0.4),
        (S('crocus') + 0.4, 78, 0.5), (S('crocus') + 5.6, 76, 0.45), (S('crocus') + 6.6, 74, 0.45),
        (S('seedling') + 0.1, 74, 0.4), (S('seedling') + 4.9, 76, 0.4), (S('seedling') + 8.8, 78, 0.5),
        (S('fern') + 0.5, 71, 0.5), (S('fern') + 6.3, 74, 0.5), (S('fern') + 6.9, 78, 0.5),
        (S('bud') + 0.3, 67, 0.5), (S('bud') + 0.7, 71, 0.45), (S('bud') + 7.0, 79, 0.5),
        (S('ladybird') + 0.3, 81, 0.5), (S('ladybird') + 0.8, 78, 0.5), (S('ladybird') + 7.3, 74, 0.5),
        (S('takeoff') + 1.6, 76, 0.4), (S('takeoff') + 2.3, 81, 0.45), (S('takeoff') + 3.0, 85, 0.45),
        (S('rain') + 0.4, 76, 0.35), (S('rain') + 6.3, 71, 0.35),
        (S('snail') + 0.6, 74, 0.45), (S('snail') + 5.4, 71, 0.45), (S('snail') + 6.2, 74, 0.45),
        (S('spring') + 0.2, 74, 0.6), (S('spring') + 0.8, 78, 0.55), (S('spring') + 1.4, 81, 0.55), (S('spring') + 2.0, 86, 0.55),
        (S('spring') + 8.6, 78, 0.5), (S('spring') + 9.0, 74, 0.5),
    ]
    for t0, m, a in motif:
        b = bell(m, a); place(keys, pan(b, rng.uniform(-0.35, 0.35)), t0); place(keys, pan(bell(m - 12, a * 0.35), 0.0), t0)
    pads = np.stack([lp(pads[:, 0], 2600), lp(pads[:, 1], 2600)], 1).astype(np.float32)
    dry = pads * 0.035 + keys * 0.12
    return dry * 0.75 + reverb(dry, IR_HALL) * 0.55

# ---------- narration ----------
def load_narration():
    buf = np.zeros(N, np.float32); cues = []
    d = os.path.join(ROOT, 'work', variant.CFG['narr_dir'])
    for i, (shot, off, text) in enumerate(narration.LINES):
        p = os.path.join(d, f'{i:02d}.wav')
        if not os.path.exists(p): print('missing narration', p, file=sys.stderr); continue
        with wave.open(p) as w: x = np.frombuffer(w.readframes(w.getnframes()), '<i2').astype(np.float32) / 32768.0
        a = np.abs(x); thr = 0.02 * a.max(); idx = np.where(a > thr)[0]
        x = x[max(0, idx[0] - 480):idx[-1] + 2400]
        x = hp(x, 70).astype(np.float32)
        x *= 0.09 / (np.sqrt(np.mean(x[np.abs(x) > thr] ** 2)) + 1e-9)          # same speech level for every line
        t0 = T[shot][0] + off
        place_m = int(round(t0 * SR)); buf[place_m:place_m + len(x)] += x[:max(0, N - place_m)]
        cues.append((t0, t0 + len(x) / SR - 0.04, text, shot))
    return buf, cues

# ---------- loudness (ITU-R BS.1770 K-weighting, gated) and true peak ----------
def lufs(x):
    y = lfilter([1.53512485958697, -2.69169618940638, 1.19839281085285], [1, -1.69065929318241, 0.73248077421585], x, axis=0)
    y = lfilter([1.0, -2.0, 1.0], [1, -1.99004745483398, 0.99007225036621], y, axis=0)
    blk, hop = int(0.4 * SR), int(0.1 * SR)
    ms = np.array([np.sum(np.mean(y[i:i + blk] ** 2, axis=0)) for i in range(0, len(y) - blk, hop)])
    l = -0.691 + 10 * np.log10(ms + 1e-12)
    ms1 = ms[l > -70]; rel = -0.691 + 10 * np.log10(np.mean(ms1)) - 10
    return -0.691 + 10 * np.log10(np.mean(ms1[(-0.691 + 10 * np.log10(ms1)) > rel]))
def true_peak(x):
    return 20 * np.log10(np.max(np.abs(resample_poly(x, 4, 1, axis=0))) + 1e-12)
def limit(x, ceil_db):
    """look-ahead peak limiter: smoothed gain reduction, no clipping"""
    c = 10 ** (ceil_db / 20); pk = np.max(np.abs(x), axis=1)
    need = np.minimum(1.0, c / np.maximum(pk, 1e-9))
    la = int(0.004 * SR)
    from scipy.ndimage import minimum_filter1d, uniform_filter1d
    g = minimum_filter1d(need, 2 * la + 1); g = uniform_filter1d(g, la)
    return x * g[:, None]

def main():
    amb = wind() + drips_and_water() + rain() + wings()
    brd = birds(); mus = music()
    nar, cues = load_narration()
    # duck everything else while the narrator speaks
    presence = np.zeros(N, np.float32)
    for a, b, _, _ in cues: presence[int(max(0, a - 0.25) * SR):int(min(DUR, b + 0.35) * SR)] = 1
    presence = sosfiltfilt(butter(2, 2.5, 'lowpass', fs=SR, output='sos'), presence)
    duck = (1 - 0.42 * np.clip(presence, 0, 1))[:, None]
    swell = env([(0, 0.55), (T['thaw'][0], 0.7), (T['seedling'][0], 0.85), (T['bud'][0], 1.0), (T['takeoff'][0], 0.8), (T['rain'][0], 0.75), (T['spring'][0], 1.0), (T['spring'][0] + 2.5, 1.25), (DUR, 1.1)])[:, None]
    bed = (amb * 0.62 + brd * 0.95 + mus * 0.33 * swell) * duck
    mix = bed + np.stack([nar, nar], 1) * 1.0
    fade = env([(0, 0), (1.3, 1), (DUR - 2.2, 1), (DUR - 0.05, 0), (DUR, 0)])
    mix = mix * fade[:, None]
    mix = mix * 10 ** ((-16.0 - lufs(mix)) / 20)
    mix = limit(mix, -2.6)
    mix = mix * 10 ** ((-16.0 - lufs(mix)) / 20)
    mix = limit(mix, -2.6)
    rep = []
    rep.append(f'duration {DUR:.3f} s, integrated loudness {lufs(mix):.2f} LUFS, true peak {true_peak(mix):.2f} dBTP, sample peak {20 * np.log10(np.max(np.abs(mix))):.2f} dBFS')
    for nm, s in (('ambience', amb * 0.62), ('birds', brd * 0.95), ('music', mus * 0.33), ('narration', np.stack([nar, nar], 1))):
        rep.append(f'  stem {nm:10s} rms {20 * np.log10(np.sqrt(np.mean(s ** 2)) + 1e-12):6.1f} dBFS (before ducking/mastering)')
    for name, (a, b) in T.items():
        seg = mix[int(a * SR):int(b * SR)]; rep.append(f'  shot {name:9s} {a:6.2f}-{b:6.2f} s  rms {20 * np.log10(np.sqrt(np.mean(seg ** 2)) + 1e-12):6.1f} dBFS')
    for a, b, text, shot in cues:
        ok = T[shot][0] <= a and b <= T[shot][1]
        rep.append(f'  line {a:6.2f}-{b:6.2f} s in {shot:9s} [{T[shot][0]:.2f}-{T[shot][1]:.2f}] {"OK" if ok else "OUTSIDE"}  {text}')
    os.makedirs(os.path.join(ROOT, 'work'), exist_ok=True)
    with wave.open(os.path.join(ROOT, 'work', variant.CFG['mix']), 'wb') as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
        d = (rng.random(mix.shape) - rng.random(mix.shape)) / 32768.0           # TPDF dither
        w.writeframes((np.clip(mix + d, -1, 1) * 32767).astype('<i2').tobytes())
    def ts(x):
        h = int(x // 3600); m = int(x % 3600 // 60); s = x % 60; return f'{h:02d}:{m:02d}:{int(s):02d},{int(round((s - int(s)) * 1000)) % 1000:03d}'
    with open(os.path.join(ROOT, variant.CFG['srt']), 'w') as f:
        for i, (a, b, text, _) in enumerate(cues): f.write(f'{i + 1}\n{ts(a)} --> {ts(b)}\n{text}\n\n')
    open(os.path.join(ROOT, 'work', variant.CFG['report']), 'w').write('\n'.join(rep) + '\n')
    print('\n'.join(rep))

if __name__ == '__main__':
    main()
