#!/bin/bash
# render.sh — 从零复现整部短片（固定随机种子）。全程约 4.5 小时（M4 Max）；渲染分段可断点续跑。
# 依赖：clang++（系统自带）、Metal、python3 + numpy/scipy/Pillow、ffmpeg（只用于编码/封装/解码质检）。
set -e
cd "$(dirname "$0")"
./build.sh                                                   # 1. 编译侵蚀程序与渲染主机
[ -f work/terrain.bin ] || ./work/erosion work/terrain.bin 100   # 2. 地形：河流侵蚀模拟（约 1 分钟）
mkdir -p work/anim work/seg work/keys work/dbg
if [ ! -f work/exposure.txt ]; then                          # 3. 低分辨率测光一遍 → "眼睛明暗适应"曝光表
  rm -f work/anim/lum.txt
  ./work/bird seq --frames 0 3095 --fps 30 --res 480 270 --spp 2 --q 0 --crf 23 --exp 1 --out work/anim/lumpass.mp4 --lum work/anim/lum.txt
  python3 src/exposure.py work/anim/lum.txt 30 work/exposure.txt
fi
./src/render_all.sh 120                                      # 4. 正片 1920x1080@60，分 52 段（已有的分段跳过）
./work/bird path --out work/flight.csv                       # 5. 导出飞行数据 → 合成音轨
python3 src/audio.py work/flight.csv work/audio.wav
python3 src/audio_check.py work/audio.wav work/flight.csv work/audio_check
./src/assemble.sh 18                                         # 6. 拼接、编码、封装
python3 src/sheet.py bird_flight.mp4 contact_sheet.jpg --n 104 --cols 8 --w 320   # 7. 总览图与质检
python3 src/qc.py bird_flight.mp4 qc_report.txt 1.0 1.8
python3 src/verify.py bird_flight.mp4 60 work/verify_report.txt
