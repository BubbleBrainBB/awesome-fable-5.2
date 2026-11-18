#!/usr/bin/env python3
# dev.py — 把渲染器输出的线性半浮点帧显影成 PNG/JPG（看图用；正式成片走 post.py）
import sys, numpy as np
from PIL import Image
sys.path.insert(0, __import__('os').path.dirname(__file__))
from post import develop
src, dst = sys.argv[1], sys.argv[2]
w = int(sys.argv[3]) if len(sys.argv) > 3 else 1920
a = np.fromfile(src, dtype=np.float16).astype(np.float32)
h = a.size // (w * 4)
a = a.reshape(h, w, 4)
img = develop(a, seed=1)
Image.fromarray(img).save(dst, quality=92)
