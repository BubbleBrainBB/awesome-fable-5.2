#!/bin/bash
# joins the rendered segments, despeckles them (numpy, see assemble.py), encodes once to the delivery
# bitrate and muxes the synthesised sound track.   usage: src/assemble.sh [CRF=26]
set -e
cd "$(dirname "$0")/.."
python3 src/assemble.py cat_garden.mp4 ${1:-26}
ffprobe -v error -show_entries format=duration,size,bit_rate -of default=nw=1 cat_garden.mp4
