#!/bin/bash
# render_all.sh — 分段渲染全片（可断点续跑：已存在的分段跳过）。同一时间只跑这一个 GPU 任务。
# 用法：src/render_all.sh [每段帧数=120]
cd "$(dirname "$0")/.."
SEG=${1:-120}; FPS=60
TOTAL=$(./work/bird path --out work/flight.csv 2>&1 | grep timeline | sed 's/.*end //' | awk -v f=$FPS '{printf "%d", $1*f+0.5}')
LAND=$(grep tLand work/flight.csv.events | awk -v f=$FPS '{printf "%d", $2*f}')
mkdir -p work/seg work/keys
echo "total frames $TOTAL, landing at frame $LAND" | tee -a work/render.log
f=0
while [ $f -lt $TOTAL ]; do
  e=$((f+SEG)); [ $e -gt $TOTAL ] && e=$TOTAL
  out=$(printf "work/seg/seg_%05d.mp4" $f)
  if [ ! -f "$out" ]; then
    spp=12; [ $f -ge $((LAND-SEG)) ] && spp=16
    ./work/bird seq --frames $f $e --fps $FPS --res 1920 1080 --spp $spp --q 1 --crf 12 --keep 60 --keepdir work/keys --out "$out" 2>&1 | grep -E "seg|retry|fail|error" | tee -a work/render.log
  fi
  f=$e
done
echo "ALL DONE $(date)" | tee -a work/render.log
