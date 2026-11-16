#!/bin/bash
# contact sheet: one frame every 2 seconds of the finished film
set -e
cd "$(dirname "$0")/.."
rm -rf work/sheet && mkdir -p work/sheet
ffmpeg -y -v error -i cat_garden.mp4 -vf "select='not(mod(n\,60))',scale=480:270" -vsync 0 -q:v 3 work/sheet/t_%04d.jpg
i=0; for f in work/sheet/t_*.jpg; do mv "$f" "$(printf 'work/sheet/f_%05d.jpg' $((i*60)))"; i=$((i+1)); done
python3 src/sheet.py work/sheet contact_sheet.jpg 8 400
