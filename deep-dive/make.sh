#!/bin/bash
# One command to regenerate the film:  ./make.sh
# (render ~75 min on a 16-core M4 Max with an idle GPU; everything else ~3 min; resumable - run again if interrupted)
set -e
cd "$(dirname "$0")"
./render.sh
python3 audio.py work/frames/timeline.txt work/stats.txt work/audio.wav
python3 audio_check.py work/audio.wav work/frames/timeline.txt work/audio_spectrogram.png
python3 qc.py work/frames work/check 2340 qc_report.txt contact_sheet.jpg || { echo "QC flagged frames - see qc_report.txt (delete the flagged PNGs in work/frames and work/check, then run ./make.sh again)"; exit 1; }
ffmpeg -y -loglevel error -framerate 30 -i work/frames/f_%05d.png -i work/audio.wav \
  -vf "scale=in_range=full:out_range=tv:out_color_matrix=bt709,format=yuv420p" \
  -c:v libx264 -profile:v high -preset slow -crf 17 -x264-params "aq-mode=3:aq-strength=0.9" \
  -color_primaries bt709 -color_trc bt709 -colorspace bt709 -color_range tv \
  -c:a aac -b:a 256k -ar 48000 -shortest -movflags +faststart deep_dive.mp4
./verify.sh
