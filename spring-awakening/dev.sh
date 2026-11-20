#!/bin/bash
# dev.sh <shot> <f0> <f1> <step> <W> <H> <spp> <cols> [maxw] — render sample frames and make a sheet in work/dev_<shot>.jpg
cd "$(dirname "$0")"
./work/render "$1" "$2" "$3" "$5" "$6" "$7" "work/dev_$1.raw" "$4" src || exit 1
python3 src/sheet.py "work/dev_$1.raw" "$5" "$6" "work/dev_$1.jpg" "$1" "$8" "${9:-1920}"
rm -f "work/dev_$1.raw"
