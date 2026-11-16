#!/usr/bin/env python3
# auto-exposure curve from the metering pre-pass: partial compensation + causal smoothing (camera-like lag)
import sys, numpy as np
meter, out, nframes, fps = sys.argv[1], sys.argv[2], int(sys.argv[3]), float(sys.argv[4])
d = np.loadtxt(meter)
f, L = d[:, 0], np.maximum(d[:, 1], 1e-3)
Lf = np.interp(np.arange(nframes), f, L)
target = 0.30
gain = np.clip((target / Lf) ** 0.62, 0.55, 2.4)
tau = 0.45 * fps
g = np.empty(nframes); s = gain[0]
for i in range(nframes):
    s += (gain[i] - s) * (1 - np.exp(-1 / tau)); g[i] = s
np.savetxt(out, g, fmt='%.4f')
print('exposure gain: min %.2f max %.2f median %.2f; metered median %.3f' % (g.min(), g.max(), np.median(g), np.median(Lf)))
