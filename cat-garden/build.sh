#!/bin/bash
# compiles the renderer host (the Metal shader is compiled at run time from src/render.metal)
set -e
cd "$(dirname "$0")"
clang++ -std=c++17 -O2 -fobjc-arc -Wno-deprecated-declarations src/main.mm -framework Metal -framework Foundation -framework CoreGraphics -o catcam
