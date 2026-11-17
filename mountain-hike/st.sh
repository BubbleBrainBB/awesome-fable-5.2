#!/bin/bash
# st.sh name "args" ["args2" ...] : render stills and tile them vertically into work/<name>.png
name=$1; shift; i=0; files=()
for a in "$@"; do ./work/engine still $a --out work/_st_$i.png 2>&1 | grep -E "error|render|placed|incomplete|mean"; files+=("work/_st_$i.png"); i=$((i+1)); done
python3 - "$name" "${files[@]}" <<'PY'
import sys
from PIL import Image
ims=[Image.open(f) for f in sys.argv[2:]]
W=max(i.width for i in ims); H=sum(i.height for i in ims); c=Image.new('RGB',(W,H)); y=0
for i in ims: c.paste(i,(0,y)); y+=i.height
c.save('work/%s.png'%sys.argv[1])
PY
