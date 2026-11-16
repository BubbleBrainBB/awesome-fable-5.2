#!/usr/bin/env python3
"""Final assembly: rendered segments -> despeckle (numpy) -> one H.264 encode + the synthesised sound track.

  assemble.py OUT.mp4 CRF [first_seg last_seg]

ffmpeg is used only to decode the intermediate segments and to encode/mux the result.
The despeckle is the only image operation done here: a pixel may not be brighter than its
2nd-brightest 8-neighbour (+10/255).  At 16 samples per pixel the out-of-focus areas carry isolated
bright samples (one lens sample that happened to hit a sunlit blade or a white petal); this removes
the isolated ones and leaves lines, edges and anything two or more pixels wide untouched.
"""
import sys, os, glob, subprocess
import numpy as np
from multiprocessing import Pool

W, H, FPS = 1920, 1080, 30
K, THR = 2, 10.0
LW = np.array([0.2126, 0.7152, 0.0722], np.float32)

def despeckle(buf):
    a = np.frombuffer(buf, np.uint8).reshape(H, W, 3).astype(np.float32)
    L = a @ LW
    P = np.pad(L, 1, mode='edge')
    nb = np.stack([P[1 + dy:1 + dy + H, 1 + dx:1 + dx + W]
                   for dy in (-1, 0, 1) for dx in (-1, 0, 1) if (dy, dx) != (0, 0)], 0)
    lim = np.partition(nb, 8 - K, axis=0)[8 - K] + THR
    s = np.where(L > lim, lim / np.maximum(L, 1e-3), 1.0).astype(np.float32)
    changed = float((s < 1.0).mean())
    return np.clip(a * s[..., None] + 0.5, 0, 255).astype(np.uint8).tobytes(), changed

def frames(pipe):
    n = W * H * 3
    while True:
        b = pipe.read(n)
        if len(b) < n:
            return
        yield b

def main():
    out, crf = sys.argv[1], sys.argv[2]
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    segs = sorted(glob.glob(os.path.join(root, 'work/seg/seg_[0-9][0-9][0-9].mp4')))
    part = len(sys.argv) > 4
    if part:
        segs = segs[int(sys.argv[3]):int(sys.argv[4]) + 1]
    lst = os.path.join(root, 'work/concat.txt')
    with open(lst, 'w') as f:
        for s in segs:
            f.write("file '%s'\n" % s)
    dec = subprocess.Popen(['ffmpeg', '-v', 'error', '-f', 'concat', '-safe', '0', '-i', lst,
                            '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], stdout=subprocess.PIPE, bufsize=1 << 24)
    cmd = ['ffmpeg', '-y', '-v', 'error', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', '%dx%d' % (W, H),
           '-r', str(FPS), '-i', '-']
    if not part:
        cmd += ['-i', os.path.join(root, 'work/audio.wav')]
    cmd += ['-c:v', 'libx264', '-preset', 'slow', '-crf', crf, '-pix_fmt', 'yuv420p', '-profile:v', 'high',
            '-g', '60', '-bf', '2', '-colorspace', 'bt709', '-color_primaries', 'bt709', '-color_trc', 'bt709',
            '-color_range', 'tv']
    if not part:
        cmd += ['-c:a', 'aac', '-b:a', '192k', '-ar', '48000', '-ac', '2', '-shortest']
    cmd += ['-movflags', '+faststart', out]
    enc = subprocess.Popen(cmd, stdin=subprocess.PIPE, bufsize=1 << 24)
    n, tot = 0, 0.0
    with Pool(max(2, (os.cpu_count() or 8) - 4)) as pool:
        for b, ch in pool.imap(despeckle, frames(dec.stdout), chunksize=1):
            enc.stdin.write(b)
            n += 1
            tot += ch
    enc.stdin.close()
    rc = enc.wait()
    dec.wait()
    print('frames %d, pixels clamped by the despeckle: %.2f %% on average, encoder rc=%d' % (n, 100 * tot / max(n, 1), rc))
    sys.exit(rc)

if __name__ == '__main__':
    main()
