#!/usr/bin/env python3
"""Turn the luminance statistics of the low-res pre-pass into a per-frame exposure curve, the way a camera's
auto-exposure would: follow the scene brightness slowly and only part of the way.
   auto_exposure.py work/stats.txt work/exposure.txt"""
import sys, numpy as np
from scipy.ndimage import gaussian_filter1d
st = np.loadtxt(sys.argv[1])
fr = st[:, 0].astype(int); ll = st[:, 1]
n = fr.max() + 1
full = np.interp(np.arange(n), fr, ll)
fps = 30.0
sm = gaussian_filter1d(full, sigma=0.9 * fps, mode='nearest')
# key: a log-average scene luminance of L_REF is exposed with E_REF; brighter/darker scenes are compensated by 78 %
L_REF, E_REF, COMP = float(np.log(0.20)), 1.55, 0.78
ex = E_REF * np.exp(-COMP * (sm - L_REF))
ex = np.clip(ex, 0.25, 9.0)
np.savetxt(sys.argv[2], ex, fmt='%.5f')
print('frames', n, 'logLum range', full.min().round(2), full.max().round(2), 'exposure range', ex.min().round(2), ex.max().round(2))
