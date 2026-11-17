#!/bin/bash
# pv.sh shot "frame list" [extra args]: render a few frames of a shot at half res and tile them into work/pv_<shot>.png
shot=$1; frames=$2; shift 2; rm -rf work/pv/shot$shot; mkdir -p work/pv/shot$shot
for f in $frames; do ./work/engine shot $shot --frames $f:$((f+1)) --w 960 --h 540 --spp 16 --out work/pv/shot$shot "$@" 2>&1 | grep -E "error|fail|incomplete|setup"; done
python3 - $shot $frames <<'PY'
import sys
from PIL import Image
shot=sys.argv[1]; fr=[int(x) for x in sys.argv[2:]]
ims=[Image.open('work/pv/shot%s/f_%04d.png'%(shot,f)) for f in fr]
cols=2 if len(ims)>3 else 1; rows=(len(ims)+cols-1)//cols
w,h=ims[0].size; sc=1 if cols==1 else 1; c=Image.new('RGB',(w*cols,h*rows))
for i,im in enumerate(ims): c.paste(im,((i%cols)*w,(i//cols)*h))
if cols==2: c=c.resize((w*cols*2//3, h*rows*2//3), Image.LANCZOS)
c.save('work/pv_%s.png'%shot)
PY
