"""assemble.py — joins the developed segments, adds fades and film grain, encodes, muxes audio and subtitles.
usage: assemble.py <workdir> <W> <H> <out.mp4> <preview|final>"""
import sys, os, subprocess, glob, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import grade, variant
from scipy.ndimage import gaussian_filter
work, w, h, out, mode = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4], sys.argv[5]
FPS = grade.FPS
total = sum(grade.frames(s) for _, s, _ in grade.SHOTS)
FADE_IN, FADE_OUT = 30, 40
rng = np.random.default_rng(20261119)

def reader(path):
    p = subprocess.Popen(['ffmpeg', '-v', 'error', '-i', path, '-f', 'rawvideo', '-pix_fmt', 'rgb48le', '-'], stdout=subprocess.PIPE)
    nb = w * h * 6
    while True:
        b = p.stdout.read(nb)
        if len(b) < nb: break
        yield np.frombuffer(b, '<u2').reshape(h, w, 3).astype(np.float32) / 65535.0
    p.wait()

def grain(img):
    """Film grain: slightly soft luminance noise, stronger in the mid-tones, a little chroma noise.
    (First assembly used 0.022 / 0.004: read as high-ISO noise at 100 % and cost 88 Mbps; halved.)"""
    n = rng.standard_normal((h, w)).astype(np.float32)
    n = gaussian_filter(n, 0.55 * w / 1920.0) * 1.9
    l = img.mean(axis=-1)
    amp = 0.010 * (0.35 + 1.3 * np.sqrt(np.clip(l * (1 - l), 0, 1)))
    c = rng.standard_normal((h // 2 + 1, w // 2 + 1, 3)).astype(np.float32).repeat(2, 0).repeat(2, 1)[:h, :w] * 0.002
    return img + (n * amp)[..., None] + c

tmpv = os.path.join(work, 'video_only.mp4')
REUSE = os.environ.get('REUSE_VIDEO') == '1' and os.path.exists(tmpv)   # same picture, other sound track: only mux again
crf = '20' if mode == 'final' else '22'
enc = None if REUSE else subprocess.Popen(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', f'{w}x{h}', '-r', str(FPS), '-i', '-',
                        '-c:v', 'libx264', '-profile:v', 'high', '-preset', 'slow', '-crf', crf, '-maxrate', '60M', '-bufsize', '120M', '-level', '4.2', '-pix_fmt', 'yuv420p',
                        '-colorspace', 'bt709', '-color_primaries', 'bt709', '-color_trc', 'bt709', '-movflags', '+faststart', tmpv], stdin=subprocess.PIPE)
k = 0
for name, sec, _ in ([] if REUSE else grade.SHOTS):
    segs = sorted(glob.glob(os.path.join(work, 'seg', f'{name}_????.mkv')))
    patch = os.path.join(work, 'patch')
    cnt = 0
    for sp in segs:
        for img in reader(sp):
            pf = os.path.join(patch, f'{name}_{cnt:04d}.npy')         # single re-rendered frames replace bad ones
            if os.path.exists(pf): img = np.load(pf)
            f = 1.0
            if k < FADE_IN: f = (k / FADE_IN) ** 1.6
            if k >= total - FADE_OUT: f = max(0.0, (total - 1 - k) / FADE_OUT) ** 1.4
            img = grain(img) * f
            d = (rng.random((h, w, 1)).astype(np.float32) - 0.5) / 255.0
            enc.stdin.write((np.clip(img + d, 0, 1) * 255 + 0.5).astype(np.uint8).tobytes())
            k += 1; cnt += 1
    if cnt != grade.frames(sec): print(f'WARNING: {name} has {cnt} frames, expected {grade.frames(sec)}', file=sys.stderr)
if enc: enc.stdin.close(); enc.wait(); print(f'video frames: {k} (expected {total})', file=sys.stderr)
dur = total / FPS
if mode == 'preview':
    os.replace(tmpv, out)
else:
    wav = os.path.join(work, variant.CFG['mix']); srt = variant.CFG['srt']
    cmd = ['ffmpeg', '-v', 'error', '-y', '-i', tmpv, '-i', wav]
    if os.path.exists(srt): cmd += ['-i', srt]
    cmd += ['-map', '0:v', '-map', '1:a', '-c:v', 'copy', '-c:a', 'aac', '-b:a', '256k', '-ar', '48000', '-ac', '2']
    if os.path.exists(srt): cmd += ['-map', '2:s', '-c:s', 'mov_text', '-metadata:s:s:0', 'language=' + variant.CFG['sublang']]
    cmd += ['-t', f'{dur:.6f}', '-movflags', '+faststart', out]
    subprocess.check_call(cmd)
    print('wrote', out, file=sys.stderr)
