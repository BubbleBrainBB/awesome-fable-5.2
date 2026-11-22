#!/bin/bash
# assemble.sh — 拼接分段、最终编码（H.264 High / yuv420p / 60 fps）并封装音轨（AAC 48 kHz 立体声）
cd "$(dirname "$0")/.."
CRF=${1:-18}
ls work/seg/seg_*.mp4 | sort | sed "s|^|file '$PWD/|; s|$|'|" > work/seg/list.txt
ffmpeg -loglevel error -y -f concat -safe 0 -i work/seg/list.txt -i work/audio.wav \
  -c:v libx264 -profile:v high -preset slow -crf $CRF -pix_fmt yuv420p -r 60 \
  -colorspace bt709 -color_primaries bt709 -color_trc bt709 \
  -c:a aac -b:a 256k -ar 48000 -ac 2 -shortest -movflags +faststart bird_flight.mp4
ffprobe -v error -show_entries stream=codec_name,profile,pix_fmt,width,height,r_frame_rate,nb_frames,sample_rate,channels,duration,bit_rate -of default=nw=1 bird_flight.mp4
ls -la bird_flight.mp4
