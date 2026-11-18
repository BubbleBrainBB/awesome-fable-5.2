#!/usr/bin/env python3
# assemble.py — 把显影好的 PNG 序列、合成的声音、字幕封装成成片，并生成总览图。
# ffmpeg 只做编码与混流。用法: assemble.py <work 目录>
import sys, os, subprocess, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from narration import SHOT_FRAMES, FPS
from PIL import Image
work = sys.argv[1] if len(sys.argv) > 1 else "work"
total = sum(SHOT_FRAMES)
missing = [f for f in range(total) if not os.path.exists(f"{work}/frames/f_{f:05d}.png")]
if missing: sys.exit(f"missing {len(missing)} frames, e.g. {missing[:5]}")
crf = os.environ.get("CRF", "21")
cmd = ["ffmpeg", "-v", "error", "-y", "-framerate", str(FPS), "-i", f"{work}/frames/f_%05d.png", "-i", f"{work}/audio.wav", "-i", "narration.srt",
       "-map", "0:v", "-map", "1:a", "-map", "2:s",
       "-c:v", "libx264", "-profile:v", "high", "-level", "4.1", "-pix_fmt", "yuv420p", "-preset", "slow", "-crf", crf,
       "-color_primaries", "bt709", "-color_trc", "bt709", "-colorspace", "bt709", "-color_range", "tv",
       "-vf", "scale=out_color_matrix=bt709:out_range=tv",
       "-c:a", "aac", "-b:a", "192k", "-ar", "48000", "-ac", "2",
       "-c:s", "mov_text", "-metadata:s:s:0", "language=eng", "-metadata:s:a:0", "language=eng",
       "-movflags", "+faststart", "-t", f"{total / FPS:.6f}", "bee_documentary.mp4"]   # 不用 -shortest：字幕轨比画面短，会把片尾截掉
subprocess.run(cmd, check=True)

# ---- 总览图：全片均匀抽 40 帧（8 列 × 5 行），每格左下角用自己画的七段数码标出时间（秒） ----
SEG = {"0": "abcdef", "1": "bc", "2": "abged", "3": "abgcd", "4": "fgbc", "5": "afgcd", "6": "afgedc", "7": "abc", "8": "abcdefg", "9": "abcdfg"}
def draw_digit(img, ch, x, y, s, col):
    t = max(1, s // 5)
    segs = {"a": (x, y, x + s, y + t), "b": (x + s - t, y, x + s, y + s), "c": (x + s - t, y + s, x + s, y + 2 * s), "d": (x, y + 2 * s - t, x + s, y + 2 * s),
            "e": (x, y + s, x + t, y + 2 * s), "f": (x, y, x + t, y + s), "g": (x, y + s - t // 2, x + s, y + s + t - t // 2)}
    if ch == ".":
        img[y + 2 * s - t:y + 2 * s, x:x + t] = col; return t + 2
    for k in SEG[ch]:
        x0, y0, x1, y1 = segs[k]; img[y0:y1, x0:x1] = col
    return s + max(2, s // 3)
cols, rows, tw, th = 8, 5, 480, 270
sheet = np.zeros((rows * th, cols * tw, 3), dtype=np.uint8)
n = cols * rows
for i in range(n):
    f = int((i + 0.5) * total / n)
    im = np.asarray(Image.open(f"{work}/frames/f_{f:05d}.png").convert("RGB").resize((tw, th), Image.LANCZOS)).copy()
    label = f"{f / FPS:.1f}"
    x = 8; y = th - 8 - 20
    im[y - 4:y + 24, 4:4 + 12 * len(label) + 8] = (im[y - 4:y + 24, 4:4 + 12 * len(label) + 8] * 0.35).astype(np.uint8)
    for ch in label: x += draw_digit(im, ch, x, y, 10, (240, 240, 240))
    sheet[(i // cols) * th:(i // cols + 1) * th, (i % cols) * tw:(i % cols + 1) * tw] = im
Image.fromarray(sheet).save("contact_sheet.jpg", quality=88)
print("assembled bee_documentary.mp4 and contact_sheet.jpg")
