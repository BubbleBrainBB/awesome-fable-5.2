#!/bin/bash
# One command to rebuild the film from source (fixed seeds; resumable — finished segments are skipped).
#   ./render.sh          full pipeline
# Needs: macOS on Apple Silicon (Metal), clang++, ffmpeg, python3 with numpy / scipy / Pillow.
set -e
cd "$(dirname "$0")"
mkdir -p work
./build.sh                                                          # 1. compile the renderer host
./catcam --timeline work/timeline.json                              # 2. event timeline for the sound track
[ -f work/meter.txt ] || ./catcam --w 320 --h 180 --spp 4 --from 0 --to 3299 --step 2 --meter work/meter.txt   # 3. metering pre-pass
python3 src/expo.py work/meter.txt work/expo.txt 3300 30            #    -> auto-exposure curve
python3 src/render_all.py 16                                        # 4. 55 segments x 60 frames, 1920x1080, 16 spp
python3 src/audio.py work/timeline.json work/audio.wav              # 5. synthesise the sound track
python3 src/audio_check.py work/audio.wav work/timeline.json | tee work/audio_report.txt
src/assemble.sh 26                                                  # 6. concat + despeckle + encode + mux -> cat_garden.mp4
mkdir -p work/verify                                                # 7. independent low-res re-render for the bad-frame check
[ -f work/verify/f_03290.ppm ] || ./catcam --w 320 --h 180 --spp 8 --from 0 --to 3299 --step 10 --expo work/expo.txt --frames work/verify
python3 src/qc.py cat_garden.mp4 work/verify qc_report.txt
src/make_sheet.sh                                                   # 8. contact sheet
ffprobe -v error -show_entries stream=codec_name,profile,pix_fmt,width,height,r_frame_rate,nb_frames,channels,sample_rate -of default=nw=1 cat_garden.mp4
