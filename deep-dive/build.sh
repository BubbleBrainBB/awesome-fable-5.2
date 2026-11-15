#!/bin/bash
# compile the renderer (the Metal shader itself is compiled at run time from src/render.metal)
set -e
cd "$(dirname "$0")"
mkdir -p work
clang++ -O3 -std=c++17 -fobjc-arc -ffast-math -Wno-deprecated-declarations src/main.mm -Isrc \
  -framework Metal -framework Foundation -framework CoreGraphics -lz -o work/dive
