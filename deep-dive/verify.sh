#!/bin/bash
# Final container checks on deep_dive.mp4: stream properties, full decode, black-frame scan.
cd "$(dirname "$0")"
echo "== ffprobe"
ffprobe -v error -show_entries stream=codec_name,profile,pix_fmt,width,height,r_frame_rate,nb_frames,duration,sample_rate,channels,bit_rate -show_entries format=duration,size,bit_rate -of default=nw=1 deep_dive.mp4
echo "== moov before mdat (faststart)?"
python3 - <<'PY'
import struct
f=open('deep_dive.mp4','rb'); order=[]; pos=0
while True:
    h=f.read(8)
    if len(h)<8: break
    n,t=struct.unpack('>I4s',h)
    if n==1: n=struct.unpack('>Q',f.read(8))[0]
    order.append(t.decode()); pos+=n; f.seek(pos)
print(order, 'faststart OK' if order.index('moov')<order.index('mdat') else 'NOT faststart')
PY
echo "== full decode (errors would be printed here)"
ffmpeg -v error -i deep_dive.mp4 -f null - 2>&1 | head -20
echo "decode exit: ${PIPESTATUS[0]}"
echo "== frames decoded"
ffprobe -v error -count_frames -select_streams v:0 -show_entries stream=nb_read_frames -of default=nw=1 deep_dive.mp4
echo "== blackdetect (only the fade-in and fade-out should appear)"
ffmpeg -v info -i deep_dive.mp4 -vf blackdetect=d=0.2:pix_th=0.06 -an -f null - 2>&1 | grep blackdetect || echo "(none)"
