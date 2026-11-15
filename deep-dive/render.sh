#!/bin/bash
# Render all frames of "Deep Dive". Resumable: frames that already exist are skipped, so just run it again
# after an interruption. FRAMES / SIZE / SPP can be overridden for a quick look, e.g.
#   SIZE=960x540 SPP=8 ./render.sh
set -e
cd "$(dirname "$0")"
FRAMES=${FRAMES:-0:2339}; SIZE=${SIZE:-1920x1080}; SPP=${SPP:-24}
./build.sh
mkdir -p work
# 1. low-res pre-pass (every 6th frame, ~40 s): scene luminance for the auto-exposure curve, camera clearance log
if [ ! -s work/exposure.txt ]; then
  ./work/dive --out work/pre --size 480x270 --spp 2 --sub 1 --frames 0:2339:6 --stats work/stats.txt --exposure 1.0 --force
  python3 auto_exposure.py work/stats.txt work/exposure.txt
fi
# 2. the film frames: 4 motion-blur sub-frames, each with its own wave field and caustic volume
#    (the program exits non-zero if any frame failed its GPU checks; run again to fill the gaps)
for attempt in 1 2 3; do
  if ./work/dive --out work/frames --size $SIZE --spp $SPP --sub 4 --frames $FRAMES --expfile work/exposure.txt; then break; fi
  echo "render.sh: some frames failed, retrying (attempt $attempt)"
done
# 3. an independent low-res render of every frame, used by qc.py to catch silently damaged frames
for attempt in 1 2 3; do
  if ./work/dive --out work/check --size 480x270 --spp 8 --sub 4 --frames $FRAMES --expfile work/exposure.txt; then break; fi
done
