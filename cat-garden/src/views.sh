#!/bin/bash
# look-dev helper: renders a list of manual camera views into one sheet.  usage: views.sh out.jpg W H SPP "cx cy cz lx ly lz focus" ...
out=$1; W=$2; H=$3; SPP=$4; shift 4
i=0; files=()
for v in "$@"; do
  set -- $v
  ./catcam --cam $1 $2 $3 --look $4 $5 $6 --focus $7 --w $W --h $H --spp $SPP --out work/v_$i.ppm 2>&1 | grep -E "error|FAILED|frame 0" | sed "s/^/view $i: /"
  files+=(work/v_$i.ppm); i=$((i+1))
done
python3 src/view.py "${files[@]}" $out
