#!/usr/bin/env python3
"""Checks on the finished film and on the rendered shots.
  qc.py film mountain_hike.mp4 work/edit.json      specs, full decode, black / frozen / jump detection
  qc.py verify work                                 independent low-resolution re-render of the kept frames
                                                    (every 30th frame of each shot), compared block by block
The second test exists because this Mac's GPU has silently corrupted frames under contention before."""
import sys, json, subprocess, os, glob
import numpy as np

def film(path, editp):
    edit = json.load(open(editp)); info = json.loads(subprocess.run(["ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json", path], capture_output=True, text=True).stdout)
    v = [s for s in info["streams"] if s["codec_type"] == "video"][0]; a = [s for s in info["streams"] if s["codec_type"] == "audio"][0]
    print("video:", v["codec_name"], v.get("profile"), v["pix_fmt"], f'{v["width"]}x{v["height"]}', v["r_frame_rate"], "frames", v.get("nb_frames"))
    print("audio:", a["codec_name"], a["sample_rate"], "Hz", a["channels"], "ch;  duration", info["format"]["duration"], "s;  size", int(info["format"]["size"]) / 1e6, "MB")
    ok = v["codec_name"] == "h264" and v["pix_fmt"] == "yuv420p" and v["width"] == 1920 and v["height"] == 1080 and v["r_frame_rate"] == "30/1" and a["codec_name"] == "aac" and a["channels"] == 2
    W, H = 160, 90; p = subprocess.Popen(["ffmpeg", "-v", "error", "-i", path, "-vf", f"scale={W}:{H}:flags=area", "-f", "rawvideo", "-pix_fmt", "gray", "-"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    raw, err = p.communicate(); n = len(raw) // (W * H); fr = np.frombuffer(raw[:n * W * H], np.uint8).reshape(n, H, W).astype(float)
    print("decoded", n, "frames; decoder messages:", err.decode().strip() or "none")
    mean = fr.mean((1, 2)); diff = np.abs(np.diff(fr, axis=0)).mean((1, 2)); cuts = {int(round(s["start"] * 30)) for s in edit["shots"][1:]}
    total = int(round(edit["total"] * 30)); black = [i for i in range(n) if mean[i] < 4 and 24 < i < total - 48]
    frozen = [i + 1 for i in range(n - 1) if diff[i] < 0.02 and 24 < i < total - 48]
    # big-region flicker: change of 6x4 block means between neighbours, compared with the local median of that statistic
    blk = fr.reshape(n, 6, H // 6, 4, W // 4).mean((2, 4)); bd = np.abs(np.diff(blk, axis=0)).max((1, 2)); jumps = []
    for i in range(len(bd)):
        if (i + 1) in cuts: continue
        lo, hi = max(0, i - 12), min(len(bd), i + 13); loc = [bd[j] for j in range(lo, hi) if (j + 1) not in cuts and j != i]
        if bd[i] > 6 and bd[i] > 3.0 * (np.median(loc) + 0.7): jumps.append((i + 1, round(float(bd[i]), 1), round(float(np.median(loc)), 1)))
    print("black frames (outside fades):", black); print("frozen frames:", frozen); print("block jumps not at cuts (frame, jump, local median):", jumps)
    print("cut frames:", sorted(cuts), " mean luma per shot:", [round(float(mean[int(s['start'] * 30):int((s['start'] + s['dur']) * 30)].mean()), 1) for s in edit["shots"]])
    print("SPEC OK" if ok and n == total else f"SPEC PROBLEM (frames {n} vs {total})")

def verify(work):
    from PIL import Image
    bad = []; worst = 0; count = 0
    for d in sorted(glob.glob(f"{work}/keep/shot*")):
        si = int(d.split("shot")[-1]); frames = sorted(int(os.path.basename(f)[2:6]) for f in glob.glob(d + "/f_*.png"))
        if not frames: continue
        out = f"{work}/verify/shot{si}"; os.makedirs(out, exist_ok=True)
        subprocess.run([f"{work}/engine", "shot", str(si), "--w", "480", "--h", "270", "--spp", "12", "--only", ",".join(map(str, frames)), "--out", out, "--force", "--work", work], capture_output=True)
        for f in frames:
            a = np.asarray(Image.open(f"{d}/f_{f:04d}.png").convert("RGB").resize((480, 270), Image.BOX)).astype(float); vp = f"{out}/f_{f:04d}.png"
            if not os.path.exists(vp): bad.append((si, f, "no verify render")); continue
            b = np.asarray(Image.open(vp).convert("RGB")).astype(float)
            ba = a.reshape(9, 30, 16, 30, 3).mean((1, 3)); bb = b.reshape(9, 30, 16, 30, 3).mean((1, 3)); dmax = float(np.abs(ba - bb).max()); worst = max(worst, dmax); count += 1
            if dmax > 16: bad.append((si, f, round(dmax, 1)))
    print(f"verified {count} frames against independent 480x270 re-renders; worst block difference {worst:.1f} of 255 (threshold 16)"); print("flagged:", bad)

if __name__ == "__main__":
    if sys.argv[1] == "film": film(sys.argv[2], sys.argv[3])
    else: verify(sys.argv[2])
