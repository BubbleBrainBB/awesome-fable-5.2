#!/bin/bash
# assemble.sh [shot ids...] : cut list -> soundtrack -> final H.264/AAC file. Run from the project root.
set -e -o pipefail
cd "$(dirname "$0")/../.."
python3 src/pipeline/make_edit.py work "$@"
python3 src/audio/synth.py work/edit.json work/audio.wav > work/audio_report.txt
python3 src/tools/audio_check.py work/audio.wav work/edit.json work/audio_spectrogram.png
python3 src/pipeline/concat_video.py work/edit.json | ffmpeg -y -loglevel error \
  -f rawvideo -pix_fmt yuv420p -s 1920x1080 -framerate 30 -color_range tv -colorspace bt709 -i - -i work/audio.wav \
  -c:v libx264 -preset slow -crf 17 -maxrate 32M -bufsize 64M -pix_fmt yuv420p -profile:v high -r 30 \
  -x264-params colorprim=bt709:transfer=bt709:colormatrix=bt709 -color_range tv \
  -c:a aac -b:a 256k -ar 48000 -ac 2 -movflags +faststart -shortest mountain_hike.mp4
python3 src/tools/contact_sheet.py mountain_hike.mp4 contact_sheet.jpg
ffprobe -v error -show_entries stream=codec_name,profile,pix_fmt,width,height,r_frame_rate,nb_frames,sample_rate,channels:format=duration,size,bit_rate -of default=nw=1 mountain_hike.mp4
