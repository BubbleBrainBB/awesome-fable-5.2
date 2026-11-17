#!/bin/bash
# builds the two host programs into work/
set -e
cd "$(dirname "$0")"
mkdir -p work
clang++ -O3 -std=c++17 -o work/terrain_gen src/terrain_gen.cpp
clang++ -O3 -std=c++17 -fobjc-arc -Wno-deprecated-declarations -o work/engine src/engine.mm -framework Metal -framework Foundation -framework CoreGraphics -framework ImageIO
