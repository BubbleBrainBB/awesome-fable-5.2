#!/bin/bash
# render_one.sh N : meter, render every frame of shot N (resumable), check the count, encode a near-lossless
# segment into work/seg and drop the PNGs. One GPU job at a time: two at once ran ~3x slower in total.
set -e
cd "$(dirname "$0")"
N=$1; mkdir -p work/seg work/frames work/keep/shot$N
FRAMES=(360 330 330 330 270 330 300 600); want=${FRAMES[$N]}
[ -f work/seg/shot$N.mp4 ] && { echo "shot $N already encoded"; exit 0; }
rm -rf work/patch/shot$N      # single-frame repairs of an earlier render of this shot no longer apply
./work/engine shot $N --meter > work/log_shot$N.txt 2>&1
./work/engine shot $N >> work/log_shot$N.txt 2>&1
have=$(ls work/frames/shot$N/f_*.png | wc -l | tr -d ' ')
[ "$have" = "$want" ] || { echo "shot $N: $have of $want frames, not encoding"; exit 1; }
ffmpeg -y -loglevel error -framerate 30 -i work/frames/shot$N/f_%04d.png -c:v libx264 -preset slow -crf 10 -pix_fmt yuv420p -color_primaries bt709 -color_trc bt709 -colorspace bt709 work/seg/shot$N.tmp.mp4
got=$(ffprobe -v error -count_frames -select_streams v:0 -show_entries stream=nb_read_frames -of csv=p=0 work/seg/shot$N.tmp.mp4)
[ "$got" = "$want" ] || { echo "shot $N: encoded $got of $want"; exit 1; }
mv work/seg/shot$N.tmp.mp4 work/seg/shot$N.mp4
# keep every 30th frame (1 fps) for the contact sheet and QC, then free the disk
for f in work/frames/shot$N/f_*.png; do b=$(basename $f .png); i=$((10#${b#f_})); [ $((i % 30)) = 15 ] && cp $f work/keep/shot$N/; done
rm -rf work/frames/shot$N; echo "shot $N done ($got frames) $(date +%H:%M:%S)"
