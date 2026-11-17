#!/usr/bin/env python3
"""Evenly spaced frames of the finished film as one JPEG (one tile every 2 s, time stamped)."""
import sys, subprocess, json
from PIL import Image, ImageDraw
src, out = sys.argv[1], sys.argv[2]
dur = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", src], capture_output=True, text=True).stdout)
step = 2.0; times = [1.0 + i * step for i in range(int((dur - 1.0) / step) + 1)]; cols = 7; tw, th = 480, 270
rows = (len(times) + cols - 1) // cols; sheet = Image.new("RGB", (cols * tw, rows * th), (0, 0, 0)); d = ImageDraw.Draw(sheet)
for i, t in enumerate(times):
    raw = subprocess.run(["ffmpeg", "-v", "error", "-ss", f"{t:.3f}", "-i", src, "-frames:v", "1", "-vf", f"scale={tw}:{th}:flags=area", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"], capture_output=True).stdout
    if len(raw) != tw * th * 3: continue
    x, y = (i % cols) * tw, (i // cols) * th; sheet.paste(Image.frombytes("RGB", (tw, th), raw), (x, y))
    d.rectangle([x + 4, y + 4, x + 62, y + 20], fill=(0, 0, 0)); d.text((x + 8, y + 6), f"{int(t // 60)}:{t % 60:04.1f}", fill=(255, 255, 255))
sheet.save(out, quality=90); print(out, sheet.size, len(times), "tiles")
