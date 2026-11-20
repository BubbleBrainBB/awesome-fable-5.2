"""narr_zh_split.py <take> — analyse one full take: speech segments (by silence), pitch, and a local whisper transcript."""
import sys, os, wave, json, subprocess, numpy as np
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, 'work', 'narr_zh')
def read(p):
    with wave.open(p) as w: return np.frombuffer(w.readframes(w.getnframes()), '<i2').astype(np.float32) / 32768.0
def segments(x, sr=48000, gap=0.75):
    hop = 480; e = np.array([np.sqrt(np.mean(x[i:i + 960] ** 2)) for i in range(0, len(x) - 960, hop)])
    on = e > 0.035 * np.percentile(e, 99)
    segs = []; i = 0
    while i < len(on):
        if on[i]:
            j = i
            while j < len(on) and (on[j] or not on[j:j + int(gap * sr / hop)].sum() == 0): j += 1
            segs.append((i * hop / sr, j * hop / sr)); i = j
        else: i += 1
    return segs
def f0(x, sr=48000):
    out = []
    for i in range(0, len(x) - 2400, 960):
        fr = x[i:i + 2400]
        if np.sqrt(np.mean(fr ** 2)) < 0.02: continue
        fr = fr - fr.mean(); ac = np.correlate(fr, fr, 'full')[2399:]; lo, hi = sr // 500, sr // 90; k = lo + np.argmax(ac[lo:hi])
        if ac[k] > 0.45 * ac[0]: out.append(sr / k)
    return np.array(out)
if __name__ == '__main__':
    name = sys.argv[1]; p = os.path.join(D, f'{name}.wav'); x = read(p)
    segs = segments(x); f = f0(x)
    print(f'{name}: {len(x) / 48000:.2f} s, {len(segs)} speech segments, f0 median {np.median(f):.0f} Hz (10-90 %: {np.percentile(f, 10):.0f}-{np.percentile(f, 90):.0f})')
    for k, (a, b) in enumerate(segs): print(f'  {k:2d}  {a:6.2f}-{b:6.2f}  {b - a:5.2f} s')
    W = os.path.join(D, '_whisper'); os.makedirs(W, exist_ok=True)
    subprocess.run(['whisper', p, '--model', 'small', '--language', 'zh', '--word_timestamps', 'True', '--output_format', 'json', '--output_dir', W, '--verbose', 'False'], capture_output=True)
    try:
        d = json.load(open(os.path.join(W, f'{name}.json')))
        for s in d['segments']: print(f"  whisper {s['start']:6.2f}-{s['end']:6.2f}  {s['text']}")
    except Exception as e: print('whisper failed', e)
