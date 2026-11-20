#!/bin/bash
# build.sh — compile the renderer host (the Metal kernel is compiled at run time).
set -e
cd "$(dirname "$0")"
mkdir -p work
clang++ -std=c++17 -O2 -fobjc-arc -Wno-deprecated-declarations src/main.mm -o work/render -framework Metal -framework Foundation -framework CoreGraphics
