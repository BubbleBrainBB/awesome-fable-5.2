# audio.py — 全部由代码合成的音轨（numpy/scipy）。输入：work/flight.csv（120 Hz 飞行数据）与事件表。
# 层次：风噪（随空速/俯冲）、振翅（与拍翅相位逐样本同步）、分区域环境声（云上/云中/湖面/河道/峡谷/瀑布/枝头）、
#       点水与鱼跃、落枝细响、极克制的铺底和声。最后做响度归一（自写 BS.1770 响度计）与真峰值限制。
import sys, numpy as np
from scipy import signal
SR = 48000
rng = np.random.default_rng(20261117)

def load(path):
    d = np.genfromtxt(path, delimiter=',', names=True)
    ev = {}; heads = []; rips = []
    for line in open(path + '.events'):
        p = line.split()
        if p[0] == 'head': heads.append(float(p[1]))
        elif p[0] == 'rip': rips.append((float(p[1]), float(p[2]), float(p[3])))
        else: ev[p[0]] = float(p[1])
    return d, ev, heads, rips

d, ev, heads, rips = load(sys.argv[1] if len(sys.argv) > 1 else 'work/flight.csv')
OUT = sys.argv[2] if len(sys.argv) > 2 else 'work/audio.wav'
T = ev['tEnd']; N = int(round(T * SR)); t = np.arange(N) / SR
def up(name):                      # 把 120 Hz 的飞行数据插值到音频采样率
    return np.interp(t, d['t'], d[name])
speed = up('speed'); vy = up('vy'); flapPh = up('flapPhase'); flapAmp = up('flapAmp'); fold = up('fold'); brake = up('brake')
gload = up('gload'); agl = up('agl'); roll = up('bodyRoll'); X = up('x'); Y = up('y'); Z = up('z')
def sm(a, b, x): u = np.clip((x - a) / (b - a), 0, 1); return u * u * (3 - 2 * u)
def win(t0, t1, r0=0.5, r1=0.5): return sm(t0 - r0, t0 + r0, t) * (1 - sm(t1 - r1, t1 + r1, t))
def noise(n=N): return rng.standard_normal(n)
def bp(x, lo, hi, order=2):
    sos = signal.butter(order, [lo, hi], btype='band', fs=SR, output='sos'); return signal.sosfilt(sos, x)
def lp(x, fc, order=2):
    sos = signal.butter(order, fc, btype='low', fs=SR, output='sos'); return signal.sosfilt(sos, x)
def hp(x, fc, order=2):
    sos = signal.butter(order, fc, btype='high', fs=SR, output='sos'); return signal.sosfilt(sos, x)
def slow(rate, seed, n=N):         # 慢变随机包络（0..1）
    r = np.random.default_rng(seed); m = int(n / SR * rate) + 4
    k = r.random(m); x = np.arange(n) / SR * rate; i = x.astype(int); f = x - i; f = f * f * (3 - 2 * f)
    return k[i] * (1 - f) + k[i + 1] * f
def pan(x, p):                     # p: -1 左 … +1 右（等功率）
    a = (p + 1) * np.pi / 4; return np.stack([x * np.cos(a), x * np.sin(a)], 1)
def place(buf, sig, t0, gain=1.0, p=0.0):
    i0 = int(t0 * SR)
    if i0 >= N or i0 + len(sig) <= 0: return
    a = max(i0, 0); b = min(i0 + len(sig), N); s = sig[a - i0:b - i0] * gain
    ang = (p + 1) * np.pi / 4; buf[a:b, 0] += s * np.cos(ang); buf[a:b, 1] += s * np.sin(ang)

mix = np.zeros((N, 2))
tLand = ev['tLand']; tFall = ev['tFall']; tTop = ev['tTop']; tLake = ev['tLake']; tCloudIn = ev['tCloudIn']; tCloudOut = ev['tCloudOut']
tMouth = ev['tMouth']; tGorge = ev['tGorge']; tRiver = ev['tRiver']; tApp = ev['tApproach']; tDive = ev['tDive']
flying = 1 - sm(tLand - 0.25, tLand + 0.05, t)

# ---------------- 1. 风噪：一组固定带通，各带增益随空速变化（避免时变滤波器的杂音） ----------------
bands = [(40, 110), (110, 260), (260, 600), (600, 1300), (1300, 2600), (2600, 5000), (5000, 9000)]
v = np.maximum(speed, 0.0)
inCloud = win(tCloudIn, tCloudOut, 0.6, 0.8)
windL = np.zeros(N); windR = np.zeros(N)
for k, (lo, hi) in enumerate(bands):
    fc = np.sqrt(lo * hi)
    edge = 260 + 95 * v                                   # 速度越高，高频越多
    g = (v / 20.0) ** 2.2 * (1.0 / (1.0 + (fc / edge) ** 2.2)) * (0.55 + 0.9 * (fc < 300))
    g *= 1 - 0.55 * inCloud * (fc > 1500)                 # 云里高频被闷住
    g *= 0.75 + 0.5 * slow(2.5 + k, 100 + k)              # 阵风的抖动
    g *= flying
    a = bp(noise(), lo, hi); b = bp(noise(), lo, hi)
    c = 0.55                                              # 左右部分相关
    windL += g * (a * np.sqrt(c) + b * np.sqrt(1 - c)); windR += g * (a * np.sqrt(c) - b * np.sqrt(1 - c))
balance = np.clip(roll * 0.35, -0.3, 0.3)                 # 压坡度时两耳风声略有差别
mix[:, 0] += 0.085 * windL * (1 + balance); mix[:, 1] += 0.085 * windR * (1 - balance)
# 俯冲时羽毛的啸声：窄带噪声，中心频率随速度升高
dive = fold * sm(24, 44, v)
ph = 2 * np.pi * np.cumsum(1500 + 38 * v) / SR
whistle = (bp(noise(), 1200, 4200, 2) * 0.5 + 0.5 * np.sin(ph + 2.0 * lp(noise(), 30))) * dive * (0.6 + 0.4 * slow(7, 7))
mix += pan(bp(whistle, 1500, 4000), 0.0) * 0.045
# 拉起时的过载：低频"压"一下
pull = sm(1.6, 3.2, gload) * flying
mix += pan(lp(noise(), 90) * pull, 0.0) * 0.5

# ---------------- 2. 振翅：下扑强、上扬弱，与拍翅相位同步 ----------------
ang = 2 * np.pi * flapPh
down = np.maximum(np.sin(ang), 0) ** 2; upst = np.maximum(-np.sin(ang), 0) ** 3
env = flapAmp * (down + 0.3 * upst)
effort = 1.0 + 1.6 * win(tFall, tTop, 0.3, 0.4) + 1.1 * brake + 0.5 * win(tMouth, tFall, 1, 1)
fl = (bp(noise(), 260, 1900) * 0.8 + bp(noise(), 1900, 5200) * 0.35 + lp(noise(), 240) * 0.9) * env * effort
fr = (bp(noise(), 260, 1900) * 0.8 + bp(noise(), 1900, 5200) * 0.35 + lp(noise(), 240) * 0.9) * env * effort
mix[:, 0] += 0.135 * fl; mix[:, 1] += 0.135 * fr
# 落枝瞬间收翅的一声
i = int(tLand * SR); n1 = int(0.18 * SR); e = np.exp(-np.arange(n1) / SR / 0.035) * (1 - np.exp(-np.arange(n1) / SR / 0.004))
place(mix, bp(noise(n1), 500, 4000) * e, tLand + 0.06, 0.10, 0.0)

# ---------------- 3. 环境声 ----------------
# 3a 高空的空旷：很低的风的"底"，带一点很远的、缓慢起伏的气流声
high = sm(150, 420, Y) * flying
amb = (lp(noise(), 70) * 1.0 + bp(noise(), 180, 520) * 0.10 * slow(0.3, 5)) * high
mix += pan(amb, 0.0) * 0.20
# 云中：近处潮湿的嘶嘶声 + 更重的抖动
cl = bp(noise(), 900, 3200) * inCloud * (0.5 + 0.5 * slow(9, 9))
mix += np.stack([cl, np.roll(cl, 331)], 1) * 0.022

# 鸟鸣合成：若干短音节，每个是带颤音/滑音的正弦加少量谐波
def note(f0, f1, dur, vib=0.0, vibf=30.0, harm=0.25, shape=3.0):
    n = int(dur * SR); tt = np.arange(n) / SR; u = tt / dur
    f = f0 * (f1 / f0) ** u * (1 + vib * np.sin(2 * np.pi * vibf * tt))
    ph = 2 * np.pi * np.cumsum(f) / SR
    e = np.sin(np.pi * u) ** (1.0 / shape)
    return (np.sin(ph) + harm * np.sin(2 * ph) + 0.3 * harm * np.sin(3 * ph)) * e
def phrase(kind, r):
    parts = []
    if kind == 'twitter':            # 燕子式的细碎啁啾
        for _ in range(r.integers(6, 13)):
            f0 = r.uniform(3200, 6800); parts += [note(f0, f0 * r.uniform(0.6, 1.5), r.uniform(0.02, 0.05), vib=0.05, vibf=90), np.zeros(int(r.uniform(0.01, 0.05) * SR))]
        parts += [note(4200, 2600, 0.14, vib=0.12, vibf=110, harm=0.5)]
    elif kind == 'two':              # 两音节的山雀式
        for _ in range(r.integers(2, 5)):
            parts += [note(4300, 4150, 0.11), np.zeros(int(0.04 * SR)), note(3350, 3250, 0.14), np.zeros(int(0.09 * SR))]
    elif kind == 'flute':            # 鸫类式的笛音短句
        for _ in range(r.integers(3, 6)):
            f0 = r.uniform(1700, 2900); parts += [note(f0, f0 * r.uniform(0.8, 1.25), r.uniform(0.12, 0.3), vib=0.02, vibf=12, harm=0.15), np.zeros(int(r.uniform(0.05, 0.16) * SR))]
    else:                            # 下滑的哨音
        parts += [note(5200, 2800, 0.35, harm=0.1), np.zeros(int(0.2 * SR)), note(5000, 2700, 0.33, harm=0.1)]
    return np.concatenate(parts)
def far(sig, dist):                  # 距离感：衰减 + 高频损失 + 一点漫反射
    g = 1.0 / (1.0 + dist / 12.0); x = lp(sig, max(1800.0, 9000.0 / (1 + dist / 60.0)))
    n = int(0.5 * SR); ir = np.zeros(n); ir[0] = 1; ir += 0.35 * rng.standard_normal(n) * np.exp(-np.arange(n) / SR / 0.12) * min(1.0, dist / 80.0)
    return signal.fftconvolve(x, ir)[:len(x) + n] * g
birds = np.zeros((N, 2)); r = np.random.default_rng(77)
# 云上：同伴的啁啾（近）
for tb in [1.4, 3.4, 5.9, 8.3, 9.6]: place(birds, far(phrase('twitter', r), r.uniform(12, 30)), tb + r.uniform(-0.2, 0.2), 0.5, r.uniform(-0.6, 0.6))
# 湖面与河道：林鸟（远近不一）
tb = tLake - 1.0
while tb < tGorge - 1.0:
    kind = r.choice(['two', 'flute', 'slide', 'flute']); dist = r.uniform(40, 260) if tb < tMouth else r.uniform(15, 120)
    place(birds, far(phrase(kind, r), dist), tb, 0.55, r.uniform(-0.9, 0.9)); tb += r.uniform(1.3, 3.2)
# 瀑布顶以后与枝头：稀疏的远处鸟鸣，间隔越来越长
tb = tTop + 1.5
while tb < T - 3.0:
    kind = r.choice(['flute', 'slide', 'two', 'twitter']); dist = r.uniform(80, 400)
    place(birds, far(phrase(kind, r), dist), tb, 0.6, r.uniform(-0.9, 0.9)); tb += r.uniform(2.2, 5.5) * (1 + 0.5 * sm(tLand, T, tb))
mix += birds * 0.16

# 3b 水声：流水 = 大量随机小气泡（短促上滑的正弦）+ 带通噪声
def babble(dur, rate, seed, f_lo=500, f_hi=3200):
    n = int(dur * SR); out = np.zeros(n + SR); r2 = np.random.default_rng(seed)
    cnt = int(dur * rate); ts = r2.random(cnt) * dur; fs = np.exp(r2.uniform(np.log(f_lo), np.log(f_hi), cnt)); am = r2.random(cnt) ** 2
    for t0, f0, a in zip(ts, fs, am):
        m = int(SR * r2.uniform(0.012, 0.04)); tt = np.arange(m) / SR
        s = np.sin(2 * np.pi * f0 * tt * (1 + 6.0 * tt)) * np.exp(-tt / (m / SR / 4.0)) * a
        i0 = int(t0 * SR); out[i0:i0 + m] += s
    return out[:n]
zf = np.interp(1440.0, X[::480][np.argsort(X[::480])], Z[::480][np.argsort(X[::480])]) if True else 0.0
# 到河道/瀑布的"接近度"随时间的包络
riverNear = win(tMouth - 1.0, tFall + 0.3, 2.0, 0.6) * (0.55 + 0.45 * sm(tRiver, tRiver + 3, t))
gorgeIn = win(tGorge, tFall + 1.5, 1.2, 1.2)
bab = babble(T, 260, 5) + 0.6 * babble(T, 120, 6, 250, 900)
riv = bab * 0.5 + bp(noise(), 350, 3200) * 0.22 * (0.7 + 0.3 * slow(3, 31)) + lp(noise(), 300) * 0.25
rivL = riv * riverNear; rivR = np.roll(riv, 977) * riverNear
mix[:, 0] += 0.11 * rivL * (1 + 0.5 * gorgeIn); mix[:, 1] += 0.11 * rivR * (1 + 0.5 * gorgeIn)
# 湖面：很轻的拍岸与水面的"沙沙"
lake = win(tLake - 1.5, tMouth + 0.5, 1.5, 1.5)
lap = bp(noise(), 250, 1400) * (0.3 + 0.7 * slow(0.8, 41) ** 2) * lake
mix += np.stack([lap, np.roll(lap, 1543)], 1) * 0.030
# 贴水飞行：水面把风声"压"出一点嘶声
skim = lake * sm(1.6, 0.4, agl)
mix += pan(bp(noise(), 2500, 7000) * skim * (0.6 + 0.4 * slow(6, 43)), 0.0) * 0.022
# 点水与鱼跃
def plip(size=1.0):
    n = int(0.35 * SR); tt = np.arange(n) / SR
    f = 520 / size * (1 + 2.2 * np.exp(-tt / 0.03))
    s = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-tt / 0.06) * 0.8
    s += hp(rng.standard_normal(n), 2500) * np.exp(-tt / 0.02) * 0.6 + bp(rng.standard_normal(n), 800, 4000) * np.exp(-tt / 0.09) * 0.25 * (tt > 0.02)
    return s
for (tr, xr, zr) in rips:
    xi = np.interp(tr, d['t'], d['x']); zi = np.interp(tr, d['t'], d['z']); dist = np.hypot(xr - xi, zr - zi)
    if abs(tr - ev['tDrink']) < 0.05: place(mix, plip(0.8), tr - 0.02, 0.50, 0.0)        # 自己点水：近、脆
    elif tr > tLake - 4: place(mix, far(plip(1.4), dist), tr, 0.38, np.clip((zr - zi) / 40.0, -0.8, 0.8))
# 3c 瀑布：宽带轰鸣，随距离与遮挡变化
fx, fy, fz = 1440.0 - 8.0, 20.0, np.interp(tFall, d['t'], d['z'])
dist = np.sqrt((X - fx) ** 2 + (np.maximum(Y - fy, 0) * 0.6) ** 2 + (Z - fz) ** 2)
occl = 0.25 + 0.75 * win(tGorge + 2.0, tTop - 0.4, 2.5, 0.5)          # 进峡谷后才直接听到；越过崖顶后被崖壁挡住
gfall = np.clip(14.0 / (dist + 9.0), 0, 1.3) ** 1.25 * occl
gfall *= 1 - 0.75 * sm(tLand - 2, tLand + 2, t) * 0.0
rumble = lp(noise(), 160) * 1.3 + bp(noise(), 160, 900) * 0.8; hiss = bp(noise(), 900, 7000) * 0.55
bright = win(tGorge + 3.0, tTop - 0.3, 2.0, 0.4)                      # 被遮挡时只剩低频
fallL = (rumble + hiss * (0.12 + 0.88 * bright)) * gfall
fallR = np.roll(rumble + hiss * (0.12 + 0.88 * bright), 2111) * gfall
mix[:, 0] += 0.55 * fallL; mix[:, 1] += 0.55 * fallR
# 峡谷混响：把河水与振翅送进一个指数衰减的噪声脉冲响应
n = int(1.4 * SR); irL = rng.standard_normal(n) * np.exp(-np.arange(n) / SR / 0.32); irR = rng.standard_normal(n) * np.exp(-np.arange(n) / SR / 0.32)
dry = (0.06 * (fl + fr) + 0.05 * (rivL + rivR)) * gorgeIn
wetL = signal.fftconvolve(dry, irL)[:N] * 0.012; wetR = signal.fftconvolve(dry, irR)[:N] * 0.012
mix[:, 0] += lp(wetL, 3500); mix[:, 1] += lp(wetR, 3500)
# 掠过近物的"呼"声：倒木与巨石
def whoosh(dur=0.22):
    n = int(dur * SR); u = np.arange(n) / n; e = np.sin(np.pi * u) ** 2
    return bp(rng.standard_normal(n), 300, 2600) * e
for xobj, g in [(1168.0, 0.20), (1215.0, 0.12)]:
    tobj = np.interp(xobj, d['x'][(d['t'] > tMouth) & (d['t'] < tFall)], d['t'][(d['t'] > tMouth) & (d['t'] < tFall)])
    place(mix, whoosh(), tobj - 0.11, g, 0.0)
# 河道两岸树木掠过：给风声的高频加一点忽强忽弱
trees = win(tRiver, tGorge, 1.0, 1.0)
mod = (0.5 + 0.5 * np.sin(2 * np.pi * np.cumsum(speed / 6.5) / SR + 1.7 * lp(noise(), 3))) ** 3
tw = bp(noise(), 1200, 5000) * trees * mod
mix += np.stack([tw, np.roll(tw, 487)], 1) * 0.020

# 3d 枝头：傍晚的安静——叶子在微风里的沙沙声、秋虫、远处瀑布的低鸣
perch = sm(tLand - 0.5, tLand + 1.5, t)
gust = 0.25 + 0.75 * slow(0.35, 51) ** 2
leaves = bp(noise(), 2200, 9000) * gust * (0.6 + 0.4 * slow(11, 52)) * perch
mix += np.stack([leaves, np.roll(bp(noise(), 2200, 9000) * gust * perch, 0)], 1) * 0.020
breeze = lp(noise(), 220) * (0.4 + 0.6 * slow(0.25, 53)) * perch
mix += pan(breeze, 0.0) * 0.06
def crickets(seed, f0, rate, gap):
    r3 = np.random.default_rng(seed); out = np.zeros(N); tt = tTop
    while tt < T:
        dur = r3.uniform(0.25, 0.6); n2 = int(dur * SR); u = np.arange(n2) / SR
        pulse = (np.sin(2 * np.pi * rate * u) > 0.2).astype(float); pulse = lp(pulse, 400)
        s = np.sin(2 * np.pi * f0 * u * (1 + 0.004 * np.sin(2 * np.pi * 7 * u))) * pulse * np.sin(np.pi * np.arange(n2) / n2) ** 0.3
        i0 = int(tt * SR)
        if i0 + n2 < N: out[i0:i0 + n2] += s
        tt += dur + r3.uniform(gap * 0.6, gap * 1.6)
    return out
ck = sm(tTop + 1, tTop + 6, t) * (0.35 + 0.65 * perch)
mix += pan(crickets(61, 4600, 62, 0.5) * ck, -0.6) * 0.010 + pan(crickets(62, 5100, 55, 0.8) * ck, 0.5) * 0.008 + pan(crickets(63, 4250, 70, 1.1) * ck, 0.1) * 0.006
# 落枝：脚抓住树枝的"嗒"、枝条一沉的吱呀、叶子被带动的一阵响
tt = np.arange(int(1.6 * SR)) / SR
creak_f = 210 + 60 * np.exp(-tt * 3.0) * np.cos(2 * np.pi * 2.3 * tt)
creak = np.sin(2 * np.pi * np.cumsum(creak_f) / SR) * np.exp(-tt * 3.2) * (0.5 + 0.5 * np.abs(np.sin(2 * np.pi * 2.3 * tt)))
creak = bp(creak + 0.3 * rng.standard_normal(len(tt)) * np.exp(-tt * 6), 150, 1200)
place(mix, creak, tLand, 0.05, -0.1)
tick = hp(rng.standard_normal(int(0.03 * SR)), 1800) * np.exp(-np.arange(int(0.03 * SR)) / SR / 0.004)
place(mix, tick, tLand - 0.005, 0.16, 0.0); place(mix, tick, tLand + 0.07, 0.08, 0.1)
rust = bp(rng.standard_normal(len(tt)), 2500, 9000) * np.exp(-tt * 2.6) * (0.4 + 0.6 * np.abs(np.sin(2 * np.pi * 2.3 * tt + 0.5)))
place(mix, rust, tLand + 0.02, 0.10, 0.2)
# 停栖后小幅挪动/转头时羽毛极轻的摩擦
for th in heads[1:]:
    n2 = int(0.09 * SR); e = np.sin(np.pi * np.arange(n2) / n2) ** 2
    place(mix, bp(rng.standard_normal(n2), 1500, 6000) * e, th, 0.012, 0.0)

# ---------------- 4. 极克制的铺底和声（只在片头云上与瀑布顶之后） ----------------
def pad(freqs, t0, t1, rise, fall):
    out = np.zeros(N); e = sm(t0, t0 + rise, t) * (1 - sm(t1 - fall, t1, t))
    for k, f in enumerate(freqs):
        dt = 1 + 0.0015 * np.sin(2 * np.pi * (0.07 + 0.013 * k) * t + k)
        ph = 2 * np.pi * np.cumsum(f * dt) / SR
        out += (np.sin(ph) + 0.25 * np.sin(2 * ph + k) + 0.08 * np.sin(3 * ph)) * (0.7 + 0.3 * np.sin(2 * np.pi * (0.05 + 0.011 * k) * t + 1.3 * k))
    return lp(out, 1400) * e / len(freqs)
D2, A2, D3, Fs3, A3, B3, E4, Fs4 = 73.42, 110.0, 146.83, 185.0, 220.0, 246.94, 329.63, 369.99
music = pad([D3, A3, E4], 0.3, tDive + 1.5, 3.0, 3.0) * 0.6
music += pad([D2, A2, Fs3, A3, E4], tTop - 0.6, T - 0.2, 5.0, 5.0)
music += pad([B3, Fs4], tLand + 4.0, T - 0.2, 4.0, 5.0) * 0.5
mix += np.stack([music, np.roll(music, 601)], 1) * 0.030

# ---------------- 5. 母带：片头片尾淡入淡出、动态收拢、响度归一、真峰值限制 ----------------
fade = sm(0.0, 0.8, t) * (1 - sm(T - 1.8, T - 0.05, t)); mix *= fade[:, None]
mix = hp(mix.T, 28).T
# 慢速"手推音量"：把很响与很轻的段落收近一些（约压掉三分之一的动态）
pw = lp(np.mean(mix ** 2, 1), 0.35) ; pw = np.maximum(pw, 1e-9); rms = np.sqrt(pw)
ref = np.exp(np.mean(np.log(rms[int(2 * SR):int((T - 2) * SR)])))
mix *= ((ref / rms) ** 0.38)[:, None]

def lufs(x):
    b1 = [1.53512485958697, -2.69169618940638, 1.19839281085285]; a1 = [1.0, -1.69065929318241, 0.73248077421585]
    b2 = [1.0, -2.0, 1.0]; a2 = [1.0, -1.99004745483398, 0.99007225036621]
    y = signal.lfilter(b2, a2, signal.lfilter(b1, a1, x, axis=0), axis=0)
    blk = int(0.4 * SR); hop = int(0.1 * SR); z = []
    for i in range(0, len(y) - blk, hop): z.append(np.sum(np.mean(y[i:i + blk] ** 2, 0)))
    z = np.array(z); l = -0.691 + 10 * np.log10(np.maximum(z, 1e-12))
    g1 = z[l > -70]; rel = -0.691 + 10 * np.log10(np.mean(g1)) - 10
    g2 = z[l > rel]; return -0.691 + 10 * np.log10(np.mean(g2)), l
def true_peak(x): return np.max(np.abs(signal.resample_poly(x, 4, 1, axis=0)))
def limiter(x, ceil):
    # 前视 2 ms 的峰值限制：按需压低增益，再平滑
    pk = np.max(np.abs(x), 1); la = int(0.002 * SR)
    from scipy.ndimage import maximum_filter1d
    pkm = maximum_filter1d(pk, 2 * la + 1); g = np.minimum(1.0, ceil / np.maximum(pkm, 1e-9))
    g = signal.filtfilt(np.ones(la) / la, [1.0], g); return x * g[:, None]
TARGET = -16.0; CEIL = 10 ** (-1.6 / 20)
for it in range(4):
    L, _ = lufs(mix); mix *= 10 ** ((TARGET - L) / 20); mix = limiter(mix, CEIL * 0.97)
L, mom = lufs(mix); tp = true_peak(mix)
if tp > CEIL: mix *= CEIL / tp; L, mom = lufs(mix); tp = true_peak(mix)
print('integrated loudness %.2f LUFS, true peak %.2f dBTP, clipped samples %d' % (L, 20 * np.log10(tp), int(np.sum(np.abs(mix) >= 1.0))))
import wave
w = wave.open(OUT, 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
dith = (rng.random(mix.shape) - rng.random(mix.shape)) / 32768.0
w.writeframes((np.clip(mix + dith, -1, 1) * 32767.0).astype('<i2').tobytes()); w.close()
np.save(OUT + '.mom.npy', mom)
print('wrote', OUT, 'duration %.3f s' % (N / SR))
