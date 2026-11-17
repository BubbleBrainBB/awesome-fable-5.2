#!/usr/bin/env python3
"""concat_video.py work/edit.json  ->  the film's picture as raw yuv420p 1920x1080 frames on stdout.
Reads the near-lossless shot segments (work/seg/shotN.mp4) in cut order. A frame that exists as
work/patch/shotN/f_XXXX.png replaces the segment's frame XXXX: that is how single frames are repaired
(src/pipeline/patch_frames.sh) without re-rendering or re-encoding the rest of the shot."""
import sys, json, subprocess, os, glob
W, H = 1920, 1080; FS = W * H * 3 // 2
edit = json.load(open(sys.argv[1])); work = os.path.dirname(sys.argv[1]) or "."; out = sys.stdout.buffer; total = 0
for s in edit["shots"]:
    n = s["id"]; patches = {int(os.path.basename(f)[2:6]): f for f in glob.glob(f"{work}/patch/shot{n}/f_*.png")}
    p = subprocess.Popen(["ffmpeg", "-v", "error", "-i", f"{work}/seg/shot{n}.mp4", "-f", "rawvideo", "-pix_fmt", "yuv420p", "-"], stdout=subprocess.PIPE)
    i = 0; used = 0
    while True:
        buf = p.stdout.read(FS)
        if len(buf) < FS: break
        if i in patches:
            buf = subprocess.run(["ffmpeg", "-v", "error", "-i", patches[i], "-vf", "scale=out_color_matrix=bt709:out_range=tv", "-f", "rawvideo", "-pix_fmt", "yuv420p", "-"], capture_output=True).stdout
            assert len(buf) == FS, f"bad patch frame {patches[i]}"; used += 1
        out.write(buf); i += 1
    p.wait(); want = int(round(s["dur"] * 30)); assert i == want, f"shot {n}: {i} frames, expected {want}"
    assert used == len(patches), f"shot {n}: patch frames outside the shot"; total += i
    print(f"shot {n} ({s['name']}): {i} frames, {used} patched", file=sys.stderr)
print(f"{total} frames", file=sys.stderr)
