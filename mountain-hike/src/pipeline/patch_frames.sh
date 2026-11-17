#!/bin/bash
# patch_frames.sh N f1,f2,... : re-render single frames of shot N at full quality into work/patch/shotN.
# assemble.sh then uses them in place of the segment's frames. Used for frames flagged by the checks
# (and, for this delivery, for the camera fix described in the README). Run from the project root.
set -e
cd "$(dirname "$0")/../.."
N=$1; mkdir -p work/patch/shot$N
./work/engine shot $N --only "$2" --force --out work/patch/shot$N > work/log_patch_shot$N.txt 2>&1
# frames that are also QC reference frames (every 30th, offset 15) must match what goes into the film
for f in work/patch/shot$N/f_*.png; do b=$(basename $f .png); i=$((10#${b#f_})); if [ $((i % 30)) = 15 ]; then cp $f work/keep/shot$N/; fi; done
echo "shot $N: $(ls work/patch/shot$N | wc -l | tr -d ' ') patch frames"
