"""encode.py — reads linear float frames from the renderer on stdin, develops them and pipes 16-bit RGB to ffmpeg.
usage: encode.py <shot> <W> <H> <out.mkv> [keyframe_dir] [first_frame_index]"""
import sys, subprocess, os, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from develop import develop
import grade
shot, w, h, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
keydir = sys.argv[5] if len(sys.argv) > 5 else None
f0 = int(sys.argv[6]) if len(sys.argv) > 6 else 0
g = grade.get(shot)
tmp = out + '.part.mkv'
ff = subprocess.Popen(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb48le', '-s', f'{w}x{h}', '-r', str(grade.FPS), '-i', '-',
                       '-c:v', 'libx264', '-preset', 'medium', '-crf', '9', '-pix_fmt', 'yuv444p10le', tmp], stdin=subprocess.PIPE)
n = 0; nb = w * h * 3 * 4
while True:
    buf = sys.stdin.buffer.read(nb)
    if len(buf) < nb: break
    lin = np.frombuffer(buf, np.float32).reshape(h, w, 3)
    img = develop(lin, **g)
    ff.stdin.write((np.clip(img, 0, 1) * 65535 + 0.5).astype('<u2').tobytes())
    if keydir and (f0 + n) % 24 == 0:
        from PIL import Image
        os.makedirs(keydir, exist_ok=True)
        Image.fromarray((img * 255 + 0.5).astype(np.uint8)).save(os.path.join(keydir, f'{shot}_{f0 + n:04d}.jpg'), quality=93)
    n += 1
ff.stdin.close(); rc = ff.wait()
if rc == 0 and n > 0:
    os.replace(tmp, out)
    open(out + '.n', 'w').write(str(n))
print(f'encoded {n} frames -> {out}', file=sys.stderr)
sys.exit(0 if rc == 0 and n > 0 else 1)
