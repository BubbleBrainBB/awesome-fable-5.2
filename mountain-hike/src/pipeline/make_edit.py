#!/usr/bin/env python3
"""The cut: which rendered shots go into the film, in what order. Writes work/edit.json for the sound and the assembly.
usage: make_edit.py work [shot ids ...]   (default: every shot whose segment exists, in trail order)"""
import sys, json, os
SHOTS = [(0, "forest", 12), (1, "stream", 11), (2, "shafts", 11), (3, "treeline", 11), (4, "meadow", 9), (5, "saddle", 11), (6, "ridge", 10), (7, "summit", 20)]
work = sys.argv[1] if len(sys.argv) > 1 else "work"
ids = [int(x) for x in sys.argv[2:]] or [i for i, _, _ in SHOTS if os.path.exists(f"{work}/seg/shot{i}.mp4")]
t = 0.0; out = []
for i, name, dur in SHOTS:
    if i not in ids: continue
    out.append(dict(id=i, name=name, start=t, dur=dur, seg=f"{work}/seg/shot{i}.mp4", steps=f"{work}/steps_shot{i}.txt")); t += dur
json.dump(dict(shots=out, vo=f"{work}/vo", fps=30, total=t), open(f"{work}/edit.json", "w"), indent=1)
print("edit:", " ".join(s["name"] for s in out), "=", t, "s")
