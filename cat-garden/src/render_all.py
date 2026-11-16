#!/usr/bin/env python3
# renders the film in 60-frame segments (each a closed-GOP H.264 file); finished segments are skipped, so it can resume
import os, subprocess, sys, time
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(root)
NF, SEG, W, H = 3300, 60, 1920, 1080
spp = int(sys.argv[1]) if len(sys.argv) > 1 else 16
only = set(int(x) for x in sys.argv[2].split(',')) if len(sys.argv) > 2 else None
os.makedirs('work/seg', exist_ok=True)
t0 = time.time()
for s in range(NF // SEG):
    out = 'work/seg/seg_%03d.mp4' % s
    if only is not None:
        if s not in only: continue
        if os.path.exists(out): os.remove(out)
    if os.path.exists(out): continue
    ts = time.time()
    r = subprocess.run(['./catcam', '--w', str(W), '--h', str(H), '--spp', str(spp), '--from', str(s * SEG), '--to', str(s * SEG + SEG - 1),
                        '--expo', 'work/expo.txt', '--seg', out], stderr=subprocess.PIPE, text=True)
    bad = [l for l in r.stderr.splitlines() if 'error' in l.lower() or 'FAILED' in l or 'retry' in l]
    with open('work/render_log.txt', 'a') as f:
        f.write('seg %03d rc=%d %.1fs %s\n' % (s, r.returncode, time.time() - ts, ' | '.join(bad)))
    if r.returncode != 0:
        print('segment', s, 'failed:', r.stderr[-400:]); sys.exit(1)
print('render done in %.1f min' % ((time.time() - t0) / 60))
