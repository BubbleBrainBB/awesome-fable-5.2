# audio_check.py — 没法"听"，就用数字检查：分段响度、频谱图、振翅同步的互相关、事件时刻的瞬态
import sys, numpy as np, wave
from scipy import signal
from PIL import Image, ImageDraw
wav = sys.argv[1]; csv = sys.argv[2]; out = sys.argv[3]
w = wave.open(wav); SR = w.getframerate(); n = w.getnframes(); x = np.frombuffer(w.readframes(n), '<i2').reshape(-1, 2) / 32768.0
d = np.genfromtxt(csv, delimiter=',', names=True); ev = {}
for line in open(csv + '.events'):
    p = line.split()
    if p[0] not in ('head', 'rip'): ev[p[0]] = float(p[1])
mom = np.load(wav + '.mom.npy'); tm = np.arange(len(mom)) * 0.1 + 0.2
rep = []
rep.append('file %s: %d samples, %.3f s, %d Hz, stereo; peak %.3f; samples at full scale: %d' % (wav, n, n / SR, SR, np.abs(x).max(), int((np.abs(x) >= 0.9999).sum())))
segs = [('云上', 0, ev['tDive']), ('穿云', ev['tCloudIn'], ev['tCloudOut']), ('俯冲', ev['tCloudOut'], ev['tLake']), ('湖面', ev['tLake'], ev['tMouth']), ('河道', ev['tMouth'], ev['tGorge']),
        ('峡谷', ev['tGorge'], ev['tFall']), ('瀑布拉起', ev['tFall'], ev['tTop']), ('盘旋', ev['tTop'], ev['tApproach']), ('进近', ev['tApproach'], ev['tLand']), ('枝头前半', ev['tLand'], ev['tLand'] + 5.5), ('枝头静止', ev['tLand'] + 5.5, ev['tEnd'] - 2)]
rep.append('分段的瞬时响度（400 ms 窗，LUFS）：均值 / 最大')
for name, a, b in segs:
    m = (tm >= a) & (tm < b); rep.append('  %-8s %6.1f – %6.1f s : %6.1f / %6.1f' % (name, a, b, 10 * np.log10(np.mean(10 ** (mom[m] / 10))), mom[m].max()))
# 频带能量随时间（确认各段的声音特征不同）
bands = [(30, 120), (120, 500), (500, 2000), (2000, 8000)]
mono = x.mean(1); rep.append('各段各频带能量（dB，相对全片该频带均值）: 30-120 / 120-500 / 500-2k / 2k-8k Hz')
be = []
for lo, hi in bands:
    y = signal.sosfilt(signal.butter(4, [lo, hi], btype='band', fs=SR, output='sos'), mono); be.append(y ** 2)
for name, a, b in segs:
    i0, i1 = int(a * SR), int(b * SR); rep.append('  %-8s ' % name + ' '.join('%6.1f' % (10 * np.log10(e[i0:i1].mean() / e.mean())) for e in be))
# 振翅同步：500-5000 Hz 包络 与 期望的下扑包络 的互相关
t = np.arange(n) / SR; ph = np.interp(t, d['t'], d['flapPhase']); am = np.interp(t, d['t'], d['flapAmp'])
expct = am * np.maximum(np.sin(2 * np.pi * ph), 0) ** 2
env = np.abs(signal.sosfilt(signal.butter(4, [300, 5000], btype='band', fs=SR, output='sos'), mono))
env = signal.sosfiltfilt(signal.butter(2, 40, fs=SR, output='sos'), env)
for name, a, b in [('湖面', ev['tLake'] + 1, ev['tMouth']), ('河道', ev['tMouth'], ev['tGorge']), ('瀑布拉起', ev['tFall'], ev['tTop']), ('落枝前刹车', ev['tLand'] - 1.2, ev['tLand'])]:
    i0, i1 = int(a * SR), int(b * SR); sosb = signal.butter(2, [4, 25], btype='band', fs=SR, output='sos')
    e1 = signal.sosfiltfilt(sosb, env[i0:i1]); e2 = signal.sosfiltfilt(sosb, expct[i0:i1])
    L = int(0.08 * SR); cc = [np.dot(e1[L + k:len(e1) - L + k], e2[L:len(e2) - L]) for k in range(-L, L + 1, 24)]
    k = np.argmax(cc); lag = (-L + k * 24) / SR * 1000; r = cc[k] / (np.linalg.norm(e1[L:-L]) * np.linalg.norm(e2[L:-L]) + 1e-12)
    rep.append('振翅同步 %-8s: 4–25 Hz 包络互相关峰值在 %+.1f ms，相关系数 %.2f' % (name, lag, r))
# 事件瞬态：落枝、点水
def onset(t0, lo=1500):
    y = signal.sosfilt(signal.butter(4, lo, btype='high', fs=SR, output='sos'), mono); i0 = int((t0 - 0.3) * SR); seg = np.abs(y[i0:i0 + int(0.6 * SR)])
    e = signal.sosfiltfilt(signal.butter(2, 200, fs=SR, output='sos'), seg); base = np.median(e[:int(0.2 * SR)]) + 1e-9
    k = np.argmax(e); return (i0 + k) / SR, e[k] / base
for name, t0 in [('落枝', ev['tLand']), ('点水', ev['tDrink'])]:
    tt, ratio = onset(t0); rep.append('%s：画面事件在 %.3f s，音轨高频瞬态峰在 %.3f s（差 %+.0f ms），峰值/底噪 = %.1f' % (name, t0, tt, (tt - t0) * 1000, ratio))
# 左右声道相关度
rep.append('左右声道相关系数：%.2f' % np.corrcoef(x[:, 0], x[:, 1])[0, 1])
open(out + '.txt', 'w').write('\n'.join(rep) + '\n'); print('\n'.join(rep))
# 频谱图
f, tt, S = signal.spectrogram(mono, SR, nperseg=2048, noverlap=1024); S = 10 * np.log10(S + 1e-12)
fi = np.unique(np.geomspace(2, len(f) - 1, 400).astype(int)); img = S[fi][::-1]; img = np.clip((img + 110) / 70, 0, 1)
W = 1800; idx = np.linspace(0, img.shape[1] - 1, W).astype(int); img = img[:, idx]
rgb = np.stack([img ** 0.6, img ** 1.3, 0.25 + 0.5 * img ** 3], -1); im = Image.fromarray((rgb * 255).astype(np.uint8)).resize((W, 500))
canvas = Image.new('RGB', (W, 620), (10, 10, 14)); canvas.paste(im, (0, 0)); dr = ImageDraw.Draw(canvas)
for k, v in ev.items():
    xx = int(v / (n / SR) * W); dr.line([(xx, 0), (xx, 510)], fill=(255, 255, 255), width=1); dr.text((xx + 2, 512 + 12 * (list(ev).index(k) % 3)), k[1:], fill=(255, 255, 160))
lm = np.interp(np.arange(W) / W * (n / SR), tm, mom); pts = [(i, 615 - int(np.clip((lm[i] + 50) / 40, 0, 1) * 60)) for i in range(W)]; dr.line(pts, fill=(120, 255, 120), width=1)
canvas.save(out + '.png'); print('saved', out + '.png')
