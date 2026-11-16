#!/usr/bin/env python3
# numeric check of the sound track: loudness, peaks, clipping, per-5s level profile, and step/picture alignment
import sys, json, subprocess, wave, re
import numpy as np
wavf, tlf = sys.argv[1], sys.argv[2]
w = wave.open(wavf); sr = w.getframerate(); n = w.getnframes()
x = np.frombuffer(w.readframes(n), '<i2').reshape(-1, 2) / 32768.0
out = subprocess.run(['ffmpeg', '-hide_banner', '-nostats', '-i', wavf, '-af', 'ebur128=peak=true', '-f', 'null', '-'], capture_output=True, text=True).stderr
summ = out[out.rfind('Summary:'):]
I = re.search(r'I:\s+(-?[\d.]+) LUFS', summ); LRA = re.search(r'LRA:\s+(-?[\d.]+) LU', summ); PK = re.search(r'Peak:\s+(-?[\d.]+) dBFS', summ)
print('duration %.3f s, %d Hz stereo' % (n / sr, sr))
print('integrated loudness %s LUFS, range %s LU, true peak %s dBFS' % (I.group(1), LRA.group(1), PK.group(1)))
print('sample peak %.3f (%.1f dBFS), samples >= 0.999: %d' % (np.abs(x).max(), 20 * np.log10(np.abs(x).max()), int((np.abs(x) >= 0.999).sum())))
print('L/R correlation %.2f' % np.corrcoef(x[:, 0], x[:, 1])[0, 1])
m = x.mean(1)
print('level per 5 s (dBFS rms):')
print(' '.join('%.0f' % (20 * np.log10(np.sqrt((m[i:i + 5 * sr] ** 2).mean()) + 1e-9)) for i in range(0, len(m), 5 * sr)))
tl = json.load(open(tlf))
# alignment: energy in the 20 ms after each cat footstep on gravel/leaves vs. the 20 ms before
r = []
for t, s, surf in tl['steps']:
    if int(surf) in (0, 3) and 1 < t < n / sr - 1:
        i = int(t * sr); a = (m[i:i + int(0.03 * sr)] ** 2).mean(); b = (m[i - int(0.03 * sr):i] ** 2).mean()
        r.append(10 * np.log10((a + 1e-12) / (b + 1e-12)))
print('footsteps on gravel/leaves: %d events, median onset rise %.1f dB (positive = sound starts at the step time)' % (len(r), np.median(r)))
for name in ('splash', 'land', 'pounce', 'prick'):
    t = tl[name]; i = int(t * sr)
    a = (m[i:i + int(0.1 * sr)] ** 2).mean(); b = (m[i - int(0.3 * sr):i - int(0.1 * sr)] ** 2).mean()
    print('%s at %.2f s: +%.1f dB over the preceding 0.2 s' % (name, t, 10 * np.log10((a + 1e-12) / (b + 1e-12))))
