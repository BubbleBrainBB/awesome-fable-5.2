"""qc.py — checks on the finished film; writes qc_report.txt.
stream parameters, full decode, black / frozen frames, single-frame tile glitches, loudness and true peak of the encoded audio,
narration lines inside their shots, render-log scan, and an independent low-resolution re-render of sample frames."""
import os, sys, subprocess, json, glob, re, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import grade, variant
narration = variant
from develop import develop
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MP4 = os.path.join(ROOT, variant.CFG['mp4'])
FPS = grade.FPS
T = narration.shot_times()
total = sum(grade.frames(s) for _, s, _ in grade.SHOTS)
R = []; problems = []
def say(s=''): R.append(s); print(s)

# 1. container / streams
info = json.loads(subprocess.run(['ffprobe', '-v', 'error', '-show_streams', '-show_format', '-count_frames', '-of', 'json', MP4], capture_output=True, text=True).stdout)
v = [s for s in info['streams'] if s['codec_type'] == 'video'][0]; a = [s for s in info['streams'] if s['codec_type'] == 'audio'][0]
subs = [s for s in info['streams'] if s['codec_type'] == 'subtitle']
say('== 1. streams (ffprobe) ==')
say(f"video: {v['codec_name']} profile {v.get('profile')} {v['width']}x{v['height']} {v['pix_fmt']} r_frame_rate {v['r_frame_rate']} frames {v.get('nb_read_frames')} (expected {total})")
say(f"audio: {a['codec_name']} {a['sample_rate']} Hz channels {a['channels']} | subtitles: {[s['codec_name'] for s in subs]}")
say(f"duration {float(info['format']['duration']):.3f} s (expected {total / FPS:.3f}), size {int(info['format']['size']) / 1e6:.1f} MB, bitrate {int(info['format']['bit_rate']) / 1e6:.2f} Mbps")
checks = [v['codec_name'] == 'h264', v.get('profile') == 'High', v['pix_fmt'] == 'yuv420p', v['width'] == 1920 and v['height'] == 1080, v['r_frame_rate'] == f'{FPS}/1',
          int(v.get('nb_read_frames', 0)) == total, a['codec_name'] == 'aac', a['sample_rate'] == '48000', a['channels'] == 2, len(subs) == 1]
say('stream checks: ' + ('PASS' if all(checks) else f'FAIL {checks}'))
if not all(checks): problems.append('stream parameters')

# 2. full decode
err = subprocess.run(['ffmpeg', '-v', 'error', '-i', MP4, '-f', 'null', '-'], capture_output=True, text=True).stderr.strip()
say('\n== 2. full decode ==')
say('decode errors: ' + (err[:500] if err else 'none'))
if err: problems.append('decode errors')

# 3. frame statistics on a 160x90 luma proxy
raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', MP4, '-vf', 'scale=160:90:flags=area,format=gray', '-f', 'rawvideo', '-'], capture_output=True).stdout
F = np.frombuffer(raw, np.uint8).reshape(-1, 90, 160).astype(np.float32)
means = F.mean(axis=(1, 2)); diffs = np.abs(F[1:] - F[:-1]).mean(axis=(1, 2))
cuts = set(); k = 0
for name, sec, _ in grade.SHOTS: k += grade.frames(sec); cuts.add(k)
black = [i for i in range(len(F)) if means[i] < 6 and 12 <= i < len(F) - 16]
frozen = [i + 1 for i in range(len(diffs)) if diffs[i] < 0.2 and 12 <= i < len(F) - 16]
tiles = F.reshape(-1, 9, 10, 16, 10).mean(axis=(2, 4))
glitch = []
for i in range(1, len(F) - 1):
    if i in cuts or (i + 1) in cuts: continue
    dp, dn, dpn = np.abs(tiles[i] - tiles[i - 1]), np.abs(tiles[i] - tiles[i + 1]), np.abs(tiles[i - 1] - tiles[i + 1])
    m = (dp > 30) & (dn > 30) & (dpn < 8)
    if m.any(): glitch.append((i, int(m.sum())))
say('\n== 3. frames ==')
say(f'decoded {len(F)} frames; mean luma min {means.min():.1f} max {means.max():.1f}')
say(f'black frames outside the fades: {len(black)} {black[:10]}')
say(f'frozen frames (mean abs diff < 0.2 on the proxy): {len(frozen)} {frozen[:10]}')
say(f'single-frame tile glitches (tile differs >30 from both neighbours which agree within 8): {len(glitch)} {glitch[:10]}')
say('inter-frame difference per shot (median / max, proxy luma levels):')
k = 0
for name, sec, _ in grade.SHOTS:
    n = grade.frames(sec); d = diffs[k:k + n - 1]; say(f'  {name:9s} frames {k:4d}-{k + n - 1:4d}  median {np.median(d):5.2f}  max {d.max():5.2f}'); k += n
if black: problems.append('black frames')
if frozen: problems.append('frozen frames')
if glitch: problems.append('tile glitches (inspect)')

# 4. audio of the encoded file
import audio as A
wav = os.path.join(ROOT, 'work', 'qc_audio.raw')
subprocess.check_call(['ffmpeg', '-v', 'error', '-y', '-i', MP4, '-f', 'f32le', '-ar', '48000', '-ac', '2', wav])
x = np.fromfile(wav, np.float32).reshape(-1, 2); os.remove(wav)
L, TP = A.lufs(x), A.true_peak(x)
say('\n== 4. audio (decoded from the mp4) ==')
say(f'samples {len(x)} ({len(x) / 48000:.3f} s); integrated loudness {L:.2f} LUFS (target -16); true peak {TP:.2f} dBTP (limit -1.0); clipped samples {(np.abs(x) >= 0.999).sum()}')
say(f'left/right correlation {np.corrcoef(x[:, 0], x[:, 1])[0, 1]:.2f}; silent (< -70 dBFS) stretches longer than 0.5 s inside the film: ' +
    str(sum(1 for i in range(int(1.5 * 48000), len(x) - int(2.5 * 48000), 24000) if np.sqrt(np.mean(x[i:i + 24000] ** 2)) < 10 ** (-70 / 20))))
if abs(L + 16) > 1.0: problems.append('loudness off target')
if TP > -1.0: problems.append('true peak above -1 dBTP')
rep = os.path.join(ROOT, 'work', variant.CFG['report'])
if os.path.exists(rep): say('mix report (before AAC):'); say(open(rep).read().rstrip())

# 5. narration inside shots
say('\n== 5. narration lines vs shots (from narration.srt) ==')
srt = open(os.path.join(ROOT, variant.CFG['srt'])).read().strip().split('\n\n')
def sec(t): h, m, s = t.replace(',', '.').split(':'); return int(h) * 3600 + int(m) * 60 + float(s)
okall = True
for blk, (shot, off, text) in zip(srt, narration.LINES):
    l = blk.split('\n'); a0, b0 = [sec(q) for q in l[1].split(' --> ')]
    ok = T[shot][0] + 0.2 <= a0 and b0 <= T[shot][1] - 0.2; okall &= ok
    say(f"  {a0:6.2f}-{b0:6.2f}  shot {shot:9s} {T[shot][0]:6.2f}-{T[shot][1]:6.2f}  margin in {a0 - T[shot][0]:.2f} s / out {T[shot][1] - b0:.2f} s  {'OK' if ok else 'TOO CLOSE TO A CUT'}")
if not okall: problems.append('narration crosses or touches a cut')
vt = os.path.join(ROOT, 'work', variant.CFG['narr_dir'], 'voice.txt')
say('narration engine: ' + (open(vt).read().strip() if os.path.exists(vt) else 'unknown'))

# 6. render logs
say('\n== 6. render logs ==')
tot_r = 0.0
for name, s2, spp in grade.SHOTS:
    p = os.path.join(ROOT, 'work', 'log', f'{name}.log')
    if not os.path.exists(p): say(f'  {name}: no log'); continue
    txt = open(p).read(); rt = [float(m) for m in re.findall(r'render ([\d.]+)s', txt)]
    bad = len(re.findall(r'RETRIED|FAILED|wrong sample count|GPU error', txt)); tot_r += sum(rt)
    say(f'  {name:9s} {len(rt):4d} frames logged, {spp:3d} spp, render {np.mean(rt):6.2f} s/frame (min {min(rt):.2f}, max {max(rt):.2f}), total {sum(rt) / 60:5.1f} min, retries/errors {bad}')
    if bad: problems.append(f'render retries in {name} (frames were re-rendered automatically)')
say(f'  sum of GPU render time {tot_r / 60:.1f} min (every pixel of every frame is checked for the full sample count before a frame is written)')

# 7. independent low-resolution re-render of sample frames
say('\n== 7. independent re-render (480x270, 32 spp, no grain) vs the finished film ==')
W2, H2 = 480, 270; k = 0; worst = []
for name, s2, spp in grade.SHOTS:
    n = grade.frames(s2)
    for fr in (n // 3, (2 * n) // 3):
        g = k + fr
        if g < 34 or g > total - 44: continue
        tmp = os.path.join(ROOT, 'work', 'qc_rr.raw')
        subprocess.run([os.path.join(ROOT, 'work', 'render'), name, str(fr), str(fr), str(W2), str(H2), '32', tmp, '1', os.path.join(ROOT, 'src')], capture_output=True, cwd=ROOT)
        lin = np.fromfile(tmp, np.float32).reshape(H2, W2, 3); os.remove(tmp)
        ref = (develop(lin, **grade.get(name)) * 255).astype(np.float32)
        raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', MP4, '-vf', f"select='eq(n\\,{g})',scale={W2}:{H2}:flags=area", '-vsync', '0', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'], capture_output=True).stdout
        fin = np.frombuffer(raw, np.uint8).reshape(H2, W2, 3).astype(np.float32)
        lum = lambda im: im @ np.array([0.2126, 0.7152, 0.0722], np.float32)
        d = np.abs(lum(ref) - lum(fin)).reshape(9, 30, 16, 30).mean(axis=(1, 3))
        say(f'  {name:9s} frame {fr:3d} (film frame {g:4d}): block difference median {np.median(d):5.2f}  95% {np.percentile(d, 95):5.2f}  max {d.max():5.2f}  (0-255 luma)')
        worst.append((d.max(), name, fr))
    k += n
flag = [w for w in worst if w[0] > 28]
say(f'frames with a block difference above 28: {len(flag)} {[(n2, f2, round(float(m), 1)) for m, n2, f2 in flag]}')
if flag: problems.append('re-render differences above threshold (inspect listed frames)')

say('\n== summary ==')
say('ALL AUTOMATIC CHECKS PASSED' if not problems else 'ITEMS TO LOOK AT: ' + '; '.join(problems))
open(os.path.join(ROOT, variant.CFG['qc']), 'w').write('\n'.join(R) + '\n')
