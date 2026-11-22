#!/bin/bash
# 编译主机程序与侵蚀程序
set -e
cd "$(dirname "$0")"
mkdir -p work/dbg work/seg work/keys
clang++ -O3 -std=c++17 -o work/erosion src/erosion.cpp
clang++ -O2 -std=c++17 -fobjc-arc -Wno-deprecated-declarations -o work/bird src/main.mm -framework Metal -framework Foundation -framework CoreGraphics -framework ImageIO
