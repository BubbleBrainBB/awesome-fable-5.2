#!/usr/bin/env python3
# post.py — 显影：景深掩膜平滑、轻微的镜头柔化、高光辉光、曝光、胶片曲线、颗粒、8 位抖动。
# 全部是 numpy/scipy 的数组运算，没有现成滤镜或 LUT。
# 用法: post.py shot <线性帧目录> <输出PNG目录> <镜头号>      把一个镜头的线性帧显影成 PNG（多进程）
import sys, os, numpy as np
from scipy.ndimage import gaussian_filter

EXPOSURE = 0.42
W, H = int(os.environ.get("POSTW", "1920")), int(os.environ.get("POSTH", "1080"))    # QUICK 模式下由 render.sh 传入

def tone(x):
    # 带肩部的胶片式 S 曲线（高光柔和滚降），再转 sRGB
    x = np.maximum(x, 0)
    a, b, c, d, e = 2.51, 0.03, 2.43, 0.59, 0.14
    y = (x * (a * x + b)) / (x * (c * x + d) + e)
    y = np.clip(y, 0, 1)
    return np.where(y <= 0.0031308, 12.92 * y, 1.055 * np.power(y, 1 / 2.4) - 0.055)

def develop(a, seed=0, exposure=None, grain=0.011, sat=1.0, gain=1.0, soften=0.55, denoise=True):
    rgb = a[..., :3].astype(np.float32)
    coc = a[..., 3].astype(np.float32)
    h, w = coc.shape
    sc = w / 1920.0
    exposure = EXPOSURE if exposure is None else exposure
    rgb = rgb * (exposure * gain)
    if denoise:
        # 景深掩膜平滑：只在“这个像素看到的一切都严重离焦”的地方（弥散圆大）做小半径高斯，压掉景深采样噪声。
        # 焦内的毛发、花蕊不受影响。这是降噪，README 里写明。
        cocp = np.minimum(coc, 400.0) / sc          # 换算到 1080p 下的像素
        cocp = np.minimum(cocp, gaussian_filter(cocp, 1.5 * sc) * 1.5)     # 掩膜边缘向内收，保护焦内细节的边
        m1 = np.clip((cocp - 9.0) / 10.0, 0, 1)[..., None]
        m2 = np.clip((cocp - 26.0) / 20.0, 0, 1)[..., None]
        g1 = gaussian_filter(rgb, (1.1 * sc, 1.1 * sc, 0))
        g2 = gaussian_filter(rgb, (2.4 * sc, 2.4 * sc, 0))
        rgb = rgb * (1 - m1) + (g1 * (1 - m2) + g2 * m2) * m1
    if soften > 0:
        # 镜头不是无限锐的（衍射、像差）：全画面极轻的柔化
        rgb = gaussian_filter(rgb, (soften * sc, soften * sc, 0))
    # 高光辉光（镜头内散射）：很弱的两级高斯
    hi = np.maximum(rgb - 0.9, 0)
    small = hi[::4, ::4]
    g1 = gaussian_filter(small, (5 * sc, 5 * sc, 0)); g2 = gaussian_filter(small, (18 * sc, 18 * sc, 0))
    glow = 0.10 * g1 + 0.07 * g2
    glow = np.repeat(np.repeat(glow, 4, axis=0), 4, axis=1)[:h, :w]
    glow = gaussian_filter(glow, (2, 2, 0))
    rgb = rgb + glow
    if sat != 1.0:
        l = (rgb * np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)).sum(axis=2, keepdims=True)
        rgb = l + (rgb - l) * sat
    y = tone(rgb)
    rng = np.random.default_rng(seed)
    # 颗粒：略带空间相关的亮度噪声，中间调最明显
    n = rng.standard_normal((h, w)).astype(np.float32)
    n = 0.75 * n + 0.25 * gaussian_filter(n, 0.9) * 2.2
    l = y.mean(axis=2)
    amp = grain * (0.35 + 0.65 * np.sqrt(np.clip(4 * l * (1 - l), 0, 1)))
    y = y + (n * amp)[..., None]
    y = y + (rng.random((h, w, 3)).astype(np.float32) - 0.5) / 255.0
    return (np.clip(y, 0, 1) * 255 + 0.5).astype(np.uint8)

def _one(args):
    src, dst, seed, gain = args
    from PIL import Image
    a = np.fromfile(src, dtype=np.float16).astype(np.float32).reshape(H, W, 4)
    img = develop(a, seed=seed, gain=gain)
    tmp = dst + ".tmp.png"
    Image.fromarray(img).save(tmp, compress_level=3)
    os.replace(tmp, dst)
    return dst

def fade_gain(gf, total):
    # 片头从黑渐入 0.9 秒，片尾渐出 1.6 秒（在线性光里乘增益）
    t = gf / 24.0; T = total / 24.0
    g = 1.0
    if t < 0.9: g *= (t / 0.9) ** 1.6
    if t > T - 1.6: g *= max(0.0, (T - t) / 1.6) ** 1.6
    return g

if __name__ == "__main__":
    if sys.argv[1] == "shot":
        sys.path.insert(0, os.path.dirname(__file__))
        from narration import SHOT_FRAMES
        src, dst, shot = sys.argv[2], sys.argv[3], int(sys.argv[4])
        only = [int(x) for x in sys.argv[5].split(",")] if len(sys.argv) > 5 else None
        os.makedirs(dst, exist_ok=True)
        base = sum(SHOT_FRAMES[:shot - 1]); total = sum(SHOT_FRAMES); n = SHOT_FRAMES[shot - 1]
        jobs = []
        for f in (only if only else range(n)):
            gfi = base + f
            jobs.append((f"{src}/f_{f:05d}.bin", f"{dst}/f_{gfi:05d}.png", 20261117 + gfi, fade_gain(gfi, total)))
        from multiprocessing import Pool
        with Pool(int(os.environ.get("POSTJOBS", "6"))) as p:
            for i, r in enumerate(p.imap_unordered(_one, jobs)):
                pass
        print("developed", len(jobs), "frames of shot", shot)
