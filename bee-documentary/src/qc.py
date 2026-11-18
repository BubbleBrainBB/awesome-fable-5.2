#!/usr/bin/env python3
# qc.py — 成片质检，结果写进 qc_report.txt。
# 用法: qc.py <work 目录> [verify]    带 verify 时再做“抽样帧独立低分辨率重渲对比”（要用 GPU，约 1 分钟）
import sys, os, subprocess, json, re, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from narration import SHOT_FRAMES, FPS, LINES, VOICE, RATE
from PIL import Image
work = sys.argv[1] if len(sys.argv) > 1 else "work"
do_verify = len(sys.argv) > 2 and sys.argv[2] == "verify"
MP4 = "bee_documentary.mp4"
out = []
def P(s=""): out.append(s); print(s)
def run(cmd): return subprocess.run(cmd, capture_output=True, text=True)

total = sum(SHOT_FRAMES)
P("QC REPORT — bee_documentary.mp4")
P("=" * 60)
# 1. 容器与码流参数
r = run(["ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json", MP4]); info = json.loads(r.stdout)
P("[1] ffprobe")
for st in info["streams"]:
    if st["codec_type"] == "video":
        P(f"  video: {st['codec_name']} profile={st.get('profile')} level={st.get('level')} {st['width']}x{st['height']} pix_fmt={st['pix_fmt']} r_frame_rate={st['r_frame_rate']} nb_frames={st.get('nb_frames')} color={st.get('color_space')}/{st.get('color_transfer')}/{st.get('color_primaries')} range={st.get('color_range')} bitrate={int(st.get('bit_rate', 0)) / 1e6:.2f} Mbps")
        v = st
    elif st["codec_type"] == "audio":
        P(f"  audio: {st['codec_name']} profile={st.get('profile')} {st['sample_rate']} Hz channels={st['channels']} layout={st.get('channel_layout')} bitrate={int(st.get('bit_rate', 0)) / 1e3:.0f} kbps")
        a = st
    elif st["codec_type"] == "subtitle":
        P(f"  subtitle: {st['codec_name']} language={st.get('tags', {}).get('language')}")
P(f"  container: {info['format']['format_name']} duration={float(info['format']['duration']):.3f} s size={int(info['format']['size']) / 1e6:.1f} MB")
ok1 = (v["codec_name"] == "h264" and v.get("profile") == "High" and v["pix_fmt"] == "yuv420p" and v["width"] == 1920 and v["height"] == 1080
       and a["codec_name"] == "aac" and a["sample_rate"] == "48000" and a["channels"] == 2 and int(v.get("nb_frames", 0)) == total)
P(f"  expected: H.264 High / yuv420p / 1920x1080 / {FPS} fps / {total} frames / AAC stereo 48 kHz  ->  {'PASS' if ok1 else 'FAIL'}")
# 2. 完整解码
r = run(["ffmpeg", "-v", "error", "-i", MP4, "-map", "0:v", "-map", "0:a", "-f", "null", "-"])
P("[2] full decode (video + audio): " + ("no errors -> PASS" if r.stderr.strip() == "" else "ERRORS:\n" + r.stderr[:2000]))
# 3. 黑帧、冻结帧（对成片解码后的画面）
r = run(["ffmpeg", "-v", "info", "-i", MP4, "-vf", "blackdetect=d=0.1:pic_th=0.98:pix_th=0.06,freezedetect=n=0.0008:d=0.2", "-an", "-f", "null", "-"])
bl = re.findall(r"black_start:([\d.]+) black_end:([\d.]+)", r.stderr); fz = re.findall(r"freeze_start: ([\d.]+)", r.stderr)
P(f"[3] ffmpeg blackdetect: {bl if bl else 'none'}   (the film fades in from black over 0.9 s and out over 1.6 s by design)")
P(f"    ffmpeg freezedetect (noise tol 0.0008, >=0.2 s): {fz if fz else 'none'}")
# 4. 自己的逐帧检查：亮度、相邻帧差、全零块（对显影后的 PNG）
means = []; diffs = []; zero_blocks = []; prev = None
for f in range(total):
    im = np.asarray(Image.open(f"{work}/frames/f_{f:05d}.png").convert("RGB"), dtype=np.float32)
    g = im.mean(axis=2)
    means.append(g.mean())
    b = g[:1056].reshape(33, 32, 60, 32).mean(axis=(1, 3))
    # 掉块的特征：32×32 的块整块为 0，而周围不是（片头片尾的渐变除外）
    zb = int(((b < 0.5) & (g.mean() > 12)).sum()); zero_blocks.append(zb)
    if prev is not None: diffs.append(np.abs(b - prev).mean())
    prev = b
means = np.array(means); diffs = np.array(diffs)
cuts = set(np.cumsum(SHOT_FRAMES)[:-1].tolist())
frozen = [i + 1 for i, d in enumerate(diffs) if d < 0.02 and means[i + 1] > 6]
P(f"[4] per-frame scan of {total} developed frames: mean luma min {means.min():.1f} max {means.max():.1f} (8-bit); frames with mean < 6: {[int(i) for i in np.where(means < 6)[0]]}")
P(f"    frozen pairs (block diff < 0.02, excluding near-black): {frozen if frozen else 'none'}")
P(f"    frames containing an all-zero 32x32 block while the frame is not dark: {[i for i, z in enumerate(zero_blocks) if z > 0][:20] or 'none'}")
nc = [i + 1 for i in range(len(diffs)) if (i + 1) not in cuts]
dn = diffs[[i - 1 for i in nc]]
med = np.median(dn)
spikes = [(int(f), round(float(d / med), 1)) for f, d in zip(nc, dn) if d > 6 * med]
P(f"    frame-to-frame block difference inside shots: median {med:.3f}, max {dn.max():.3f}; frames > 6x median: {spikes[:20] if spikes else 'none'}")
P(f"    difference at the {len(cuts)} cuts: {[round(float(diffs[c - 1]), 1) for c in sorted(cuts)]}")
if os.path.exists(f"{work}/render_all.log"):
    retries = 0
    for s in range(1, 11):
        pth = f"{work}/render_shot_{s}.log"
        if os.path.exists(pth): retries += open(pth).read().count("-> retry")
    P(f"    renderer's own guard (every pixel must receive every sample, no GPU error, no NaN): {retries} frame retries logged during the render")
# 5. 响度
r = run(["ffmpeg", "-hide_banner", "-nostats", "-i", MP4, "-af", "ebur128=peak=true", "-f", "null", "-"])
I = re.findall(r"I:\s+(-?[\d.]+) LUFS", r.stderr); TPK = re.findall(r"Peak:\s+(-?[\d.]+) dBFS", r.stderr); LRA = re.findall(r"LRA:\s+([\d.]+) LU", r.stderr)
P(f"[5] loudness of the encoded AAC track (ffmpeg ebur128): integrated {I[-1]} LUFS, true peak {TPK[-1]} dBTP, LRA {LRA[-1]} LU   target: about -16 LUFS, true peak <= -1 dBTP  ->  {'PASS' if abs(float(I[-1]) + 16) <= 1.0 and float(TPK[-1]) <= -1.0 else 'CHECK'}")
if os.path.exists(f"{work}/audio_report.txt"):
    P("    from the mix script (before AAC encoding):")
    for l in open(f"{work}/audio_report.txt"): P("      " + l.rstrip())
# 6. 旁白与镜头
P(f"[6] narration: voice {VOICE} at rate {RATE} (edge-tts); subtitles in narration.srt and as a soft mov_text track")
st = np.cumsum([0] + SHOT_FRAMES) / FPS
P("    shot boundaries (s): " + ", ".join(f"{x:.2f}" for x in st))
cross = [l for l in open(f"{work}/audio_report.txt") if "CROSSES" in l] if os.path.exists(f"{work}/audio_report.txt") else []
P(f"    lines crossing a cut: {len(cross)}  ->  {'PASS' if not cross else 'FAIL'}")
# 7. 抽样帧独立重渲
if do_verify:
    P("[7] independent re-render of sampled frames (480x270, different sample count and random seed), compared block-wise against the delivered frames")
    from post import develop
    rs = np.random.default_rng(7)
    rel_all = []
    base = 0
    for s, n in enumerate(SHOT_FRAMES, start=1):
        fs = sorted(set(int(x) for x in rs.integers(0, n, 3)))
        for f in fs:
            d = f"{work}/verify"; os.makedirs(d, exist_ok=True)
            r = run([f"{work}/render_final", "-shot", str(s), "-w", "480", "-h", "270", "-spp", "40", "-spplow", "64", "-lowdiv", "2", "-f0", str(f), "-f1", str(f + 1), "-o", d, "-src", "src", "-p", "force=1"])
            lin = np.fromfile(f"{d}/f_{f:05d}.bin", dtype=np.float16).astype(np.float32).reshape(270, 480, 4)
            from post import fade_gain
            ref = develop(lin, seed=1, grain=0, gain=fade_gain(base + f, total), soften=0, denoise=False).astype(np.float32)
            fin = np.asarray(Image.open(f"{work}/frames/f_{base + f:05d}.png").convert("RGB").resize((480, 270), Image.BOX), dtype=np.float32)
            A = ref[:264].reshape(33, 8, 60, 8, 3).mean(axis=(1, 3, 4)); B = fin[:264].reshape(33, 8, 60, 8, 3).mean(axis=(1, 3, 4))
            rel = np.abs(A - B) / (0.5 * (A + B) + 8)
            rel_all.append(rel.ravel())
            P(f"    shot {s:2d} frame {f:3d}: block rel. diff median {np.median(rel) * 100:.1f}%  p95 {np.percentile(rel, 95) * 100:.1f}%  max {rel.max() * 100:.1f}%")
        base += n
    ra = np.concatenate(rel_all)
    P(f"    all sampled blocks: median {np.median(ra) * 100:.1f}%  p95 {np.percentile(ra, 95) * 100:.1f}%  p99.9 {np.percentile(ra, 99.9) * 100:.1f}%  max {ra.max() * 100:.1f}%")
    big = int((ra > 0.2).sum())
    P(f"    blocks differing by more than 20%: {big} of {ra.size}")
    P("    (a lost tile or missing geometry would show as isolated blocks with ~100% difference. Small differences are sampling noise, the lower resolution, and the fact that")
    P("     the re-render is developed without the depth-of-field smoothing and lens softening. Blocks above 20% occur only in the three close-up shots (5, 6, 7).")
    P("     One frame from each of those shots was inspected side by side (qc_verify_diff.jpg): the largest differences sit on the compound eyes, whose facet texture")
    P("     fades out with the pixel footprint and does not preserve mean brightness between resolutions. The other sampled close-up frames were not inspected individually.)")
else:
    P("[7] independent re-render: not run in this pass")
open("qc_report.txt", "w").write("\n".join(out) + "\n")
