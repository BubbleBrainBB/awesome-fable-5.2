#!/usr/bin/env python3
# audio.py — 全片声音：环境声（风、叶、鸟）、蜜蜂的翅声（常速的嗡鸣 / 慢动作里按拍翅节奏的扑动声）、
# 落花与足的细小接触声、配乐，全部用 numpy 合成；旁白（edge-tts 生成的 work/tts/line_XX.wav）叠在最前，
# 其他各层在旁白出现时闪避。最后按 ITU-R BS.1770 测响度，归一到 -16 LUFS，真峰值限制在 -1 dBTP 以内。
# 用法: audio.py <输出 wav>   （同时写出 narration.srt 与 work/audio_report.txt）
import sys, os, wave, numpy as np
from scipy.signal import lfilter, butter, sosfilt, fftconvolve, resample_poly
sys.path.insert(0, os.path.dirname(__file__))
from narration import LINES, SHOT_FRAMES, FPS

SR = 48000
ST = np.cumsum([0] + SHOT_FRAMES) / FPS          # 各镜头起点（秒）
DUR = float(ST[-1])
N = int(round(DUR * SR))
rng = np.random.default_rng(20261117)
t = np.arange(N) / SR

def bp(x, lo, hi, order=2):
    return sosfilt(butter(order, [lo, hi], btype="band", fs=SR, output="sos"), x)
def lp(x, f, order=2):
    return sosfilt(butter(order, f, btype="low", fs=SR, output="sos"), x)
def hp(x, f, order=2):
    return sosfilt(butter(order, f, btype="high", fs=SR, output="sos"), x)
def smoothstep(a, b, x):
    u = np.clip((x - a) / (b - a), 0, 1); return u * u * (3 - 2 * u)
def slow_noise(rate, seed, n=N):
    # 平滑的慢变随机曲线（约 0..1）
    r = np.random.default_rng(seed); k = int(n / SR * rate) + 4
    pts = r.random(k)
    x = np.arange(n) / SR * rate
    i = np.floor(x).astype(int); f = x - i; f = f * f * (3 - 2 * f)
    return pts[i] * (1 - f) + pts[i + 1] * f
def pan(x, p):
    # p ∈ [-1,1]，等功率声像
    a = (p + 1) * np.pi / 4
    return np.stack([x * np.cos(a), x * np.sin(a)], axis=1)
def place(buf, x, t0):
    i = int(round(t0 * SR))
    if i >= len(buf) or i + len(x) <= 0: return
    a = max(0, -i); b = min(len(x), len(buf) - i)
    buf[i + a:i + b] += x[a:b]

# 混响：用指数衰减的滤波噪声当脉冲响应
def reverb_ir(seconds, decay, seed, bright=5000):
    r = np.random.default_rng(seed); n = int(seconds * SR)
    ir = r.standard_normal((n, 2)) * np.exp(-np.arange(n) / (decay * SR))[:, None]
    ir[:, 0] = lp(ir[:, 0], bright); ir[:, 1] = lp(ir[:, 1], bright)
    ir[: int(0.004 * SR)] *= 0.2
    return ir / np.sqrt((ir ** 2).sum(axis=0, keepdims=True))
def reverb(st, ir, wet):
    out = np.stack([fftconvolve(st[:, c], ir[:, c])[: len(st)] for c in range(2)], axis=1)
    return st + wet * out

# ---------------------------------------------------------------- 环境声
def ambience():
    out = np.zeros((N, 2))
    gust = 0.35 + 0.65 * slow_noise(0.22, 11) * (0.6 + 0.4 * slow_noise(0.7, 12))
    for c in range(2):
        w = rng.standard_normal(N)
        low = bp(w, 120, 700) * 0.9                       # 远处的风
        f = rng.standard_normal(N)
        leaf = bp(f, 2200, 7500) * (0.25 + 0.75 * slow_noise(1.3, 20 + c) ** 2)   # 近处叶子的窸窣
        out[:, c] = low * gust * 0.55 + leaf * gust ** 1.5 * 0.16
    # 鸟：三种简单的鸣声，远近、声像各不相同
    birds = np.zeros((N, 2))
    r = np.random.default_rng(77)
    def chirp(f0, f1, d, amp):
        n = int(d * SR); tt = np.arange(n) / SR
        f = f0 + (f1 - f0) * (tt / d) ** 0.7
        ph = 2 * np.pi * np.cumsum(f) / SR
        return amp * np.sin(ph) * np.sin(np.pi * tt / d) ** 2
    tcur = 0.6
    while tcur < DUR - 1.0:
        kind = r.integers(0, 3); p = r.uniform(-0.9, 0.9); dist = r.uniform(0.25, 1.0)
        if kind == 0:      # 一串下行的短啾
            base = r.uniform(3800, 5200); n = r.integers(3, 6)
            for k in range(n): place(birds, pan(chirp(base * (1 - 0.03 * k), base * 0.72, 0.07, dist), p), tcur + 0.13 * k)
        elif kind == 1:    # 两个音来回的哨音
            a = r.uniform(2500, 3100); b = a * r.uniform(1.18, 1.3)
            for k in range(r.integers(2, 5)): place(birds, pan(chirp(a if k % 2 == 0 else b, (a if k % 2 == 0 else b) * 1.02, 0.16, dist * 0.8), p), tcur + 0.21 * k)
        else:              # 颤音
            n = int(0.55 * SR); tt = np.arange(n) / SR; f = r.uniform(4400, 5600)
            x = np.sin(2 * np.pi * f * tt + 1.5 * np.sin(2 * np.pi * 27 * tt)) * (0.5 + 0.5 * np.sin(2 * np.pi * 27 * tt)) * np.sin(np.pi * tt / 0.55) ** 2
            place(birds, pan(x * dist * 0.7, p), tcur)
        tcur += r.uniform(1.6, 4.5)
    birds = reverb(birds, reverb_ir(1.6, 0.35, 5, 4500), 0.9)
    birds[:, 0] = lp(birds[:, 0], 6500); birds[:, 1] = lp(birds[:, 1], 6500)
    out += birds * 0.035
    # 慢动作镜头里的环境声：压低、变闷（高速摄影的惯用处理）
    slow = np.zeros(N)
    for a, b in ((ST[1], ST[2]), (ST[2], ST[3]), (ST[8], ST[9])):
        slow = np.maximum(slow, smoothstep(a - 0.15, a + 0.25, t) * (1 - smoothstep(b - 0.25, b + 0.15, t)))
    dull = np.stack([lp(out[:, 0], 900), lp(out[:, 1], 900)], axis=1)
    out = out * (1 - slow[:, None]) + dull * slow[:, None] * 0.6
    return out

# ---------------------------------------------------------------- 蜜蜂
def buzz(n, f0=230.0, seed=1):
    # 常速的翅声：以拍翅频率为基频的谐波列 + 一点气流噪声
    r = np.random.default_rng(seed)
    tt = np.arange(n) / SR
    f = f0 * (1 + 0.025 * (slow_noise(6.0, seed + 1, n) - 0.5) + 0.012 * np.sin(2 * np.pi * 5.3 * tt))
    ph = 2 * np.pi * np.cumsum(f) / SR
    x = np.zeros(n)
    for k in range(1, 15):
        x += np.sin(k * ph + r.uniform(0, 6.28)) / k ** 1.15 * (1.0 + 0.8 * np.exp(-((k * f0 - 900) / 500.0) ** 2))
    x *= 0.6 + 0.4 * np.sin(ph) ** 2
    x += 0.15 * bp(r.standard_normal(n), 400, 3000) * (0.5 + 0.5 * np.sin(ph))
    return x
def slow_wings(n, beat_hz, seed):
    # 慢动作里的翅声：每半拍一次“呼”的气流声 + 很低的鼓动，按实际拍翅节奏排列
    r = np.random.default_rng(seed)
    x = np.zeros(n); half = 1.0 / (2 * beat_hz)
    d = int(min(half * 1.25, 0.22) * SR)
    env = np.sin(np.pi * np.arange(d) / d) ** 2
    noise = bp(r.standard_normal(n + d), 160, 900)
    k = 0
    while k * half * SR < n:
        i = int(k * half * SR)
        strong = 1.0 if k % 2 == 0 else 0.72                 # 下拍比上拍重
        seg = noise[i:i + d] * env * strong
        tt = np.arange(d) / SR
        thump = np.sin(2 * np.pi * (62 if k % 2 == 0 else 74) * tt) * np.exp(-tt / (half * 0.45)) * 0.55 * strong
        m = min(d, n - i)
        x[i:i + m] += (seg + thump)[:m]
        k += 1
    return x
def bee_layer():
    out = np.zeros((N, 2))
    # 镜头 1：常速飞入（5.3 s 起），由远及近
    a, b = ST[0] + 5.3, ST[1]
    n = int((b - a) * SR); tt = np.arange(n) / SR; u = tt / (b - a)
    x = buzz(n, 232, 3)
    near = u ** 2.2
    x = lp(x, 1200) * (1 - near) + x * near
    x *= (0.03 + 0.5 * near) * smoothstep(0, 0.3, tt) * (1 - smoothstep(b - a - 0.04, b - a, tt))
    place(out, pan(x, -0.5) * (1 - u[:, None] * 0.6) + pan(x, 0.0) * (u[:, None] * 0.6), a)
    # 镜头 2：60 倍慢动作，拍翅 230/60 ≈ 3.8 次/秒
    n = int((ST[2] - ST[1]) * SR); tt = np.arange(n) / SR
    x = slow_wings(n, 230 / 60.0, 5) * smoothstep(0, 0.3, tt) * (1 - smoothstep(n / SR - 0.2, n / SR, tt))
    place(out, pan(x * 0.42, 0.15) , ST[1])
    # 镜头 3：20 倍慢动作，拍翅 11.5 次/秒；触花后 0.6 秒内停下
    n = int((ST[3] - ST[2]) * SR); tt = np.arange(n) / SR
    wing_on = 1 - smoothstep(3.9, 5.0, tt)
    x = slow_wings(n, 230 / 20.0, 6) * smoothstep(0, 0.25, tt) * wing_on
    place(out, pan(x * 0.34, 0.1), ST[2])
    # 落花：一声很轻的闷响 + 花蕊被踩动的细碎声，然后花头回弹时的低频晃动
    tc = ST[2] + 0.146 * 20
    d = int(0.5 * SR); td = np.arange(d) / SR
    thud = np.sin(2 * np.pi * (85 * np.exp(-td * 6) + 45) * td) * np.exp(-td / 0.09) * 0.8 + bp(rng.standard_normal(d), 300, 1800) * np.exp(-td / 0.05) * 0.5
    place(out, pan(thud * 0.5, 0.0), tc)
    for k in range(9):
        dd = int(0.012 * SR); tk = np.arange(dd) / SR
        tick = bp(rng.standard_normal(dd), 2500, 7000) * np.exp(-tk / 0.003)
        place(out, pan(tick * 0.10 * rng.uniform(0.4, 1), rng.uniform(-0.3, 0.3)), tc + 0.02 + 0.9 * rng.random() ** 1.5)
    d = int(2.2 * SR); td = np.arange(d) / SR
    sway = np.sin(2 * np.pi * 0.45 * 2 * td) * np.exp(-td / 0.8) * bp(rng.standard_normal(d), 150, 500) * 0.5
    place(out, pan(sway * 0.35, 0.0), tc + 0.05)
    # 镜头 4、8：在花盘上走动时足尖的细小触碰声（实时）
    def steps(t0, t1, rate, amp, seed):
        r = np.random.default_rng(seed); tk = t0
        while tk < t1:
            dd = int(0.010 * SR); tq = np.arange(dd) / SR
            tick = bp(r.standard_normal(dd), 1800, 6500) * np.exp(-tq / 0.0025)
            place(out, pan(tick * amp * r.uniform(0.4, 1.0), r.uniform(-0.25, 0.25)), tk)
            tk += r.uniform(0.6, 1.4) / rate
    steps(ST[3] + 0.3, ST[3] + 3.6, 11, 0.06, 41)
    steps(ST[3] + 3.6, ST[4], 1.2, 0.035, 42)
    steps(ST[4], ST[7], 0.9, 0.03, 43)
    steps(ST[7] + 1.3, ST[7] + 3.8, 11, 0.055, 44)
    steps(ST[7] + 3.8, ST[8], 1.2, 0.03, 45)
    # 镜头 4、8：换位置时短促的振翅
    for t0, dur, amp in ((ST[3] + 1.9, 0.16, 0.10), (ST[7] + 2.2, 0.22, 0.12), (ST[5] + 5.1, 0.12, 0.05)):
        n = int(dur * SR); tt = np.arange(n) / SR
        place(out, pan(buzz(n, 215, int(t0 * 10)) * np.sin(np.pi * tt / dur) ** 2 * amp, 0.1), t0)
    # 镜头 7：中足刮胸侧（两次）、后足互搓（4.2 次/秒）
    for t0 in (ST[6] + 0.8, ST[6] + 1.75):
        n = int(0.8 * SR); tt = np.arange(n) / SR
        sw = bp(rng.standard_normal(n), 1500, 5000) * np.sin(np.pi * tt / 0.8) ** 3 * (0.6 + 0.4 * np.sin(2 * np.pi * 31 * tt))
        place(out, pan(sw * 0.035, -0.1), t0)
    a, b = ST[6] + 2.9, ST[6] + 6.0
    n = int((b - a) * SR); tt = np.arange(n) / SR
    rub = bp(rng.standard_normal(n), 1200, 4500) * np.abs(np.sin(2 * np.pi * 4.2 * tt)) ** 1.5 * smoothstep(0, 0.4, tt) * (1 - smoothstep(b - a - 0.4, b - a, tt))
    place(out, pan(rub * 0.04, 0.05), a)
    # 镜头 9：14 倍慢动作起飞，拍翅 16.4 次/秒；振翅渐强，飞离后渐远
    n = int((ST[9] - ST[8]) * SR); tt = np.arange(n) / SR
    env = smoothstep(0.35, 1.3, tt) * (1 - 0.75 * smoothstep(3.0, 5.6, tt)) * (1 - smoothstep(n / SR - 0.25, n / SR, tt))
    x = slow_wings(n, 230 / 14.0, 8) * env
    place(out, pan(x * 0.36, 0.0) * (1 - 0.5 * smoothstep(3.0, 5.6, tt))[:, None] + pan(x * 0.36, -0.6) * (0.5 * smoothstep(3.0, 5.6, tt))[:, None], ST[8])
    tr = ST[8] + 0.135 * 14
    d = int(1.8 * SR); td = np.arange(d) / SR
    sway = np.sin(2 * np.pi * 0.64 * 2 * td) * np.exp(-td / 0.7) * bp(rng.standard_normal(d), 150, 500)
    place(out, pan(sway * 0.3, 0.0), tr)
    return reverb(out, reverb_ir(0.5, 0.08, 9, 6000), 0.25)

# ---------------------------------------------------------------- 配乐：缓慢的铺底和声 + 稀疏的拨弦
def music():
    out = np.zeros((N + SR * 4, 2))
    A4 = 440.0
    def hz(m): return A4 * 2 ** ((m - 69) / 12)
    # 每个镜头一个和弦（D 大调）：D  Bm7  Gmaj7  A  D  F#m  G  Bm  A  D
    chords = [(50, [62, 66, 69, 76]), (47, [62, 66, 69, 74]), (43, [59, 62, 66, 74]), (45, [61, 64, 69, 76]),
              (50, [62, 66, 69, 73]), (42, [61, 66, 69, 73]), (43, [59, 62, 67, 74]), (47, [59, 62, 66, 73]),
              (45, [61, 64, 69, 73]), (50, [62, 66, 69, 78])]
    r = np.random.default_rng(303)
    for ci, (bass, notes) in enumerate(chords):
        a, b = ST[ci], ST[ci + 1]
        n = int((b - a + 2.5) * SR); tt = np.arange(n) / SR
        env = smoothstep(0, 2.2, tt) * (1 - smoothstep(b - a - 0.3, b - a + 2.4, tt))
        pad = np.zeros((n, 2))
        for m in notes + [bass + 12]:
            for det, pn in ((-0.07, -0.6), (0.06, 0.6), (0.0, 0.0)):
                f = hz(m + det)
                trem = 1 + 0.12 * np.sin(2 * np.pi * (0.13 + 0.07 * r.random()) * tt + r.uniform(0, 6.28))
                tone = (np.sin(2 * np.pi * f * tt + r.uniform(0, 6.28)) + 0.22 * np.sin(2 * np.pi * 2 * f * tt)) * trem
                pad += pan(tone, pn) * (0.5 if m == bass + 12 else 0.28)
        bassn = np.sin(2 * np.pi * hz(bass) * tt) * 0.9 + 0.25 * np.sin(2 * np.pi * 2 * hz(bass) * tt)
        pad += pan(bassn, 0.0) * 0.5
        place(out, pad * env[:, None] * 0.05, a)
        # 拨弦：从和弦音里稀疏地取，节拍 1 秒
        beat = 1.0; k = 0; tcur = a + 0.5
        motif = [0, 2, 1, 3, 2, 1, 3, 0, 2, 3]
        while tcur < b - 0.4:
            if r.random() < 0.72:
                m = notes[motif[(k + ci) % len(motif)]] + (12 if r.random() < 0.35 else 0)
                d = int(3.0 * SR); td = np.arange(d) / SR; f = hz(m)
                x = (np.sin(2 * np.pi * f * td) * np.exp(-td / 0.9) + 0.35 * np.sin(2 * np.pi * 2 * f * td) * np.exp(-td / 0.45)
                     + 0.12 * np.sin(2 * np.pi * 3.01 * f * td) * np.exp(-td / 0.25)) * (1 - np.exp(-td / 0.004))
                place(out, pan(x * 0.075 * r.uniform(0.6, 1.0), r.uniform(-0.45, 0.45)), tcur + r.uniform(-0.02, 0.02))
            tcur += beat * (1 if r.random() < 0.7 else 2); k += 1
    out = reverb(out, reverb_ir(3.2, 0.9, 21, 3800), 0.55)
    out = out[:N]
    out[:, 0] = lp(out[:, 0], 5000); out[:, 1] = lp(out[:, 1], 5000)
    return out

# ---------------------------------------------------------------- 旁白
def narration():
    out = np.zeros(N); cues = []
    for i, (shot, t0, text) in enumerate(LINES):
        path = f"work/tts/line_{i:02d}.wav"
        w = wave.open(path); a = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float64) / 32768
        assert w.getframerate() == SR and w.getnchannels() == 1
        idx = np.where(np.abs(a) > 0.01)[0]
        a = a[max(0, idx[0] - int(0.03 * SR)): idx[-1] + int(0.12 * SR)]
        act = a[np.abs(a) > 0.02]
        a = a * (0.10 / np.sqrt((act ** 2).mean()))        # 各句响度拉齐
        a = hp(a, 70)
        place(out, a, t0)
        cues.append((t0 + 0.03, t0 + len(a) / SR - 0.10, text, shot))
    return out, cues

# ---------------------------------------------------------------- 响度（ITU-R BS.1770-4）
def k_weight(x):
    # 48 kHz 下的两级 K 加权双二阶滤波器（标准给出的系数）
    b1 = [1.53512485958697, -2.69169618940638, 1.19839281085285]; a1 = [1.0, -1.69065929318241, 0.73248077421585]
    b2 = [1.0, -2.0, 1.0]; a2 = [1.0, -1.99004745483398, 0.99007225036621]
    return lfilter(b2, a2, lfilter(b1, a1, x, axis=0), axis=0)
def lufs(st):
    y = k_weight(st)
    blk = int(0.4 * SR); hop = int(0.1 * SR)
    ms = np.array([(y[i:i + blk] ** 2).mean(axis=0).sum() for i in range(0, len(y) - blk, hop)])
    l = -0.691 + 10 * np.log10(np.maximum(ms, 1e-12))
    g = ms[l > -70]
    rel = -0.691 + 10 * np.log10(g.mean()) - 10
    g2 = ms[(l > -70) & (l > rel)]
    return -0.691 + 10 * np.log10(g2.mean())
def true_peak_db(st):
    up = resample_poly(st, 4, 1, axis=0)
    return 20 * np.log10(np.abs(up).max() + 1e-12)
def limit(st, ceil_db=-1.5):
    # 前视软限幅：按峰值包络算增益，平滑后施加
    c = 10 ** (ceil_db / 20)
    pk = np.abs(st).max(axis=1)
    la = int(0.004 * SR)
    from scipy.ndimage import maximum_filter1d, uniform_filter1d
    env = maximum_filter1d(pk, 2 * la + 1)
    g = np.minimum(1.0, c / np.maximum(env, 1e-9))
    g = uniform_filter1d(g, la)
    return st * g[:, None]

def main():
    outp = sys.argv[1] if len(sys.argv) > 1 else "work/audio.wav"
    amb = ambience(); bee = bee_layer(); mus = music(); voc, cues = narration()
    # 闪避：旁白的包络（提前 0.12 秒起、0.5 秒释放）
    env = np.sqrt(np.convolve(voc ** 2, np.ones(int(0.05 * SR)) / int(0.05 * SR), "same"))
    gate = (env > 0.01).astype(np.float64)
    from scipy.ndimage import maximum_filter1d, uniform_filter1d
    gate = maximum_filter1d(gate, int(0.35 * SR))
    gate = np.roll(gate, -int(0.10 * SR))
    gate = uniform_filter1d(gate, int(0.30 * SR))
    duck_bed = 1 - 0.55 * gate       # 环境与配乐压 7 dB
    duck_fx = 1 - 0.52 * gate        # 蜜蜂的声音压 6 dB
    fade = smoothstep(0, 0.9, t) * (1 - smoothstep(DUR - 1.7, DUR - 0.05, t))
    GA, GM, GB = 0.42, 0.40, 0.46          # 环境、配乐、蜜蜂三层相对旁白的电平
    bed = (amb * GA + mus * GM) * duck_bed[:, None] + bee * GB * duck_fx[:, None]
    mix = bed.copy()
    mix = mix * fade[:, None]
    mix += np.stack([voc, voc], axis=1) * 1.0
    mix = hp(mix[:, 0], 30)[:, None] * [1, 0] + hp(mix[:, 1], 30)[:, None] * [0, 1]
    # 归一到 -16 LUFS，再限幅（混音留到约 -2.2 dBTP，编码后仍在 -1 dBTP 以内；两者互相影响，迭代几次）
    for it in range(4):
        mix *= 10 ** ((-16.0 - lufs(mix)) / 20)
        if true_peak_db(mix) > -2.2: mix = limit(mix, -2.8)      # 给 AAC 编码后的峰值过冲留余量（实测编码会抬高约 0.5 dB）
    L = lufs(mix); TP = true_peak_db(mix)
    def lv(x): return lufs(x) if np.abs(x).max() > 0 else -99
    scale = 1.0
    rep = [f"integrated loudness (own BS.1770 meter): {L:.2f} LUFS", f"true peak (4x oversampled): {TP:.2f} dBTP", f"duration: {len(mix) / SR:.3f} s, {SR} Hz stereo"]
    vo2 = np.stack([voc, voc], axis=1)
    rep.append(f"layer loudness before final gain: narration {lv(vo2):.1f} LUFS, ambience {lv(amb * GA):.1f}, music {lv(mus * GM):.1f}, bee/foley {lv(bee * GB):.1f}")
    # 旁白说话时与不说话时，垫底各层（闪避之后）的短时电平
    sp = gate > 0.5
    def rms_db(x): return 10 * np.log10((x ** 2).mean() + 1e-12)
    bedf = bed * fade[:, None]
    rep.append(f"during speech: narration {rms_db(vo2[sp]):.1f} dB RMS, everything else {rms_db(bedf[sp]):.1f} dB RMS  -> voice is {rms_db(vo2[sp]) - rms_db(bedf[sp]):.1f} dB above the bed")
    rep.append(f"between lines: everything else {rms_db(bedf[~sp]):.1f} dB RMS")
    for i, (a, b, text, shot) in enumerate(cues):
        i0, i1 = int(a * SR), int(b * SR)
        rep.append(f"  line {i + 1} (shot {shot}): voice {rms_db(vo2[i0:i1]):.1f} dB, bed {rms_db(bedf[i0:i1]):.1f} dB, margin {rms_db(vo2[i0:i1]) - rms_db(bedf[i0:i1]):.1f} dB")
    pcm = np.clip(mix, -1, 1)
    d = (np.random.default_rng(5).random(pcm.shape) - 0.5) / 32768          # 抖动
    pcm = np.round((pcm + d) * 32767).astype(np.int16)
    w = wave.open(outp, "wb"); w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes()); w.close()
    # 字幕
    def tc(x):
        h = int(x // 3600); m = int(x % 3600 // 60); s = x % 60
        return f"{h:02d}:{m:02d}:{int(s):02d},{int(round((s - int(s)) * 1000)):03d}".replace(",1000", ",999")
    with open("narration.srt", "w") as f:
        for i, (a, b, text, shot) in enumerate(cues):
            a2 = max(a - 0.05, ST[shot - 1] + 0.04); b2 = min(b + 0.30, ST[shot] - 0.04)
            f.write(f"{i + 1}\n{tc(a2)} --> {tc(b2)}\n{text}\n\n")
            ok = a >= ST[shot - 1] and b <= ST[shot]
            rep.append(f"line {i + 1}: voice {a:6.2f}-{b:6.2f} s  shot {shot} [{ST[shot - 1]:6.2f},{ST[shot]:6.2f}]  {'inside shot' if ok else 'CROSSES CUT'}  margin to next cut {ST[shot] - b:.2f} s")
    os.makedirs("work", exist_ok=True)
    open("work/audio_report.txt", "w").write("\n".join(rep) + "\n")
    print("\n".join(rep))

if __name__ == "__main__":
    main()
