"""narr_zh_cut.py <take> [write] — cut one full take into the 12 lines.
The local whisper transcript (word times) is aligned to the script to find which speech segments belong to which line;
cuts are made in the silences; pauses inside a line longer than 0.5 s are shortened to 0.5 s. Prints fit against each shot."""
import sys, os, json, re, wave, difflib, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import narr_zh_split as S, narration_zh as NZ
D = S.D; SR = 48000
name = sys.argv[1]; write = len(sys.argv) > 2
strip = lambda s: re.sub(r"[\s，。、！？；：,.!?;:「」《》“”\"']", "", s)
x = S.read(os.path.join(D, f'{name}.wav')); segs = S.segments(x, gap=0.30)
d = json.load(open(os.path.join(D, '_whisper', f'{name}.json')))
heard = []                                              # (char, time)
for s in d['segments']:
    for w in s.get('words', []):
        t = strip(w['word'])
        for i, c in enumerate(t): heard.append((c, w['start'] + (w['end'] - w['start']) * (i + 0.5) / max(len(t), 1)))
ref = ''.join(strip(l[3]) for l in NZ.LINES); hs = ''.join(c for c, _ in heard)
sm = difflib.SequenceMatcher(None, ref, hs, autojunk=False)
print(f'{name}: script {len(ref)} chars, heard {len(hs)} chars, match ratio {sm.ratio():.3f}')
for tag, i1, i2, j1, j2 in sm.get_opcodes():
    if tag != 'equal': print(f'   {tag}: script "{ref[i1:i2]}" -> heard "{hs[j1:j2]}"')
r2h = {}
for tag, i1, i2, j1, j2 in sm.get_opcodes():
    for k in range(i2 - i1): r2h[i1 + k] = min(j1 + (k if tag == 'equal' else min(k, max(j2 - j1 - 1, 0))), len(heard) - 1)
# boundary between line k and k+1: the silence between the last heard char of k and the first of k+1
pos = 0; bounds = [0.0]
for l in NZ.LINES[:-1]:
    pos += len(strip(l[3])); ta = heard[r2h[pos - 1]][1]; tb = heard[r2h[pos]][1]
    gaps = [(segs[i][1], segs[i + 1][0]) for i in range(len(segs) - 1) if segs[i][1] > ta - 0.6 and segs[i + 1][0] < tb + 0.6]
    if not gaps: gaps = [(ta, tb)]
    g = max(gaps, key=lambda q: q[1] - q[0]); bounds.append(0.5 * (g[0] + g[1]))
bounds.append(len(x) / SR)
T = NZ.shot_times(); ok = True; f = S.f0(x)
print(f'f0 median {np.median(f):.0f} Hz')
for k, l in enumerate(NZ.LINES):
    mine = [s for s in segs if s[0] >= bounds[k] - 1e-3 and s[1] <= bounds[k + 1] + 1e-3]
    if not mine: print('  line', k, 'NO AUDIO'); ok = False; continue
    parts = []; 
    for i, (a, b) in enumerate(mine):
        a0 = max(a - 0.03, bounds[k]); b0 = min(b + 0.08, bounds[k + 1])
        seg = x[int(a0 * SR):int(b0 * SR)].copy(); n = min(len(seg) // 2, 480); seg[:n] *= np.linspace(0, 1, n); seg[-n:] *= np.linspace(1, 0, n)
        if i > 0: parts.append(np.zeros(int(min(a - mine[i - 1][1], 0.5) * SR), np.float32))
        parts.append(seg)
    y = np.concatenate(parts); dur = len(y) / SR
    shot, off = l[0], l[1]; room = T[shot][1] - T[shot][0] - off - (1.9 if shot == 'spring' else 0.5)
    fl = S.f0(y); fit = dur <= room; ok &= fit
    print(f'  {k:2d} {shot:9s} {dur:5.2f} s (raw {mine[-1][1] - mine[0][0]:5.2f}) room {room:4.1f} {"OK " if fit else "LONG"} f0 {np.median(fl) if len(fl) else 0:3.0f} Hz  {len(strip(l[3])) / dur:.1f} ch/s  {l[3][:16]}')
    if write:
        with wave.open(os.path.join(D, f'{k:02d}.wav'), 'wb') as w: w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((np.clip(y, -1, 1) * 32767).astype('<i2').tobytes())
if write: open(os.path.join(D, 'voice.txt'), 'w').write(f'seed-audio-1.0 (ByteDance OpenSpeech), one take for all lines ({name}), directed as a soft Taiwanese-Mandarin female narrator\n')
print('ALL LINES FIT' if ok else 'SOME LINES DO NOT FIT')
