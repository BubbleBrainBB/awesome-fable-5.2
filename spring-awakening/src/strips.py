"""strips.py — per-shot strips of evenly spaced frames from the rendered segments (for review).
usage: strips.py <workdir> <W> <H> <out_prefix> [frames_per_shot] [rows_per_sheet] [thumb_w]"""
import sys, os, glob, subprocess, numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import grade
work, w, h, pref = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
per = int(sys.argv[5]) if len(sys.argv) > 5 else 6
rows = int(sys.argv[6]) if len(sys.argv) > 6 else 5
tw = int(sys.argv[7]) if len(sys.argv) > 7 else 320
th = tw * h // w
strips = []
for name, sec, _ in grade.SHOTS:
    fr = []
    for sp in sorted(glob.glob(os.path.join(work, 'seg', f'{name}_????.mkv'))):
        raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', sp, '-vf', f'scale={tw}:{th}', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True).stdout
        a = np.frombuffer(raw, np.uint8).reshape(-1, th, tw, 3); fr.append(a)
    if not fr: continue
    fr = np.concatenate(fr); n = len(fr)
    idx = [int(round(i * (n - 1) / (per - 1))) for i in range(per)]
    strips.append(np.concatenate([fr[i] for i in idx], axis=1))
for k in range(0, len(strips), rows):
    Image.fromarray(np.concatenate(strips[k:k + rows], axis=0)).save(f'{pref}_{k // rows}.jpg', quality=88)
    print(f'{pref}_{k // rows}.jpg', [n for n, _, _ in grade.SHOTS][k:k + rows])
