#!/bin/bash
# render.sh — rebuild the whole film from source. Run from this folder:  ./render.sh
#   1. compile the two host programs (clang++, system frameworks only)
#   2. grow the terrain (fixed seeds; ~4 min)          -> work/fine.bin, work/far.bin
#   3. lay the frozen trail line onto it               -> work/trail.bin
#   4. render the eight clips, one GPU job at a time   -> work/seg/shotN.mp4   (resumable; ~5 h on an M4 Max)
#   5. narration with edge-tts (NEEDS NETWORK; optional — skipped with a note if it fails)
#   6. synthesise the soundtrack, cut, encode, contact sheet, QC
# Everything is seeded; the only non-deterministic input is the narration service.
set -e
cd "$(dirname "$0")"
./build.sh
[ -f work/fine.bin ] || ./work/terrain_gen fine work/fine.bin 4096
[ -f work/far.bin ]  || ./work/terrain_gen far  work/far.bin 1024
# The centre line of the trail was found by `engine route` on an earlier iteration of the terrain parameters and then
# frozen, so that the shots stay where they were composed. `engine rebed` only refits its heights to the final terrain.
cp src/data/trail_line.bin work/trail.bin && ./work/engine rebed
for n in 0 1 2 3 4 5 6 7; do ./work/engine shot $n --stepsonly; ./render_one.sh $n; done
if [ ! -f work/vo/lines.json ]; then
  ( python3 -m venv work/venv && work/venv/bin/pip install --quiet edge-tts && work/venv/bin/python src/audio/narration.py work/vo ) || { echo "narration unavailable: mixing without voice"; rm -rf work/vo; }
fi
src/pipeline/assemble.sh 0 1 2 3 4 5 6 7
python3 src/tools/qc.py film mountain_hike.mp4 work/edit.json | tee work/qc_film.txt
python3 src/tools/qc.py verify work | tee work/qc_verify.txt
