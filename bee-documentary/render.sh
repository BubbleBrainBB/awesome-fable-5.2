#!/bin/bash
# 一条命令重现全片：编译自写渲染器 -> 逐镜头渲染线性帧 -> 显影成 PNG -> 合成声音 -> 编码封装。
# 可断点续渲：已经显影完的镜头跳过；渲到一半的镜头从缺的那一帧接着渲。
# 用法：./render.sh            完整成片（Apple M4 Max 上实测耗时见 README）
#       QUICK=1 ./render.sh    1/4 分辨率、低采样的流程检查版（输出到 work_quick/）
#       旁白：需要 work/tts/line_XX.wav（由 src/tts.py 用 edge-tts 生成，需联网）；没有的话 audio.py 会报错退出。
set -e
cd "$(dirname "$0")"
WORK=${WORK:-work}; RW=1920; RH=1080; SPPK=1
if [ "$QUICK" = "1" ]; then WORK=work_quick; RW=480; RH=270; SPPK=4; fi
mkdir -p $WORK/lin $WORK/frames
clang++ -O2 -std=c++17 -fobjc-arc -Wno-unused-function -Wno-deprecated-declarations src/main.mm \
    -framework Metal -framework Foundation -framework CoreGraphics -o $WORK/render_final
FR=(0 192 168 150 216 216 192 192 168 144 168)
SPP=(0 64 64 64 64 56 56 56 64 64 64)
base=0
for s in 1 2 3 4 5 6 7 8 9 10; do
    n=${FR[$s]}
    have=0
    for ((f=0; f<n; f++)); do [ -f $WORK/frames/f_$(printf %05d $((base+f))).png ] && have=$((have+1)); done
    if [ $have -lt $n ]; then
        echo "== shot $s: rendering $n frames ($(date +%H:%M:%S))"
        $WORK/render_final -shot $s -w $RW -h $RH -spp $((SPP[$s]/SPPK)) -spplow $((192/SPPK)) -f0 0 -f1 $n -o $WORK/lin/shot_$s -src src 2>> $WORK/render_shot_$s.log
        POSTW=$RW POSTH=$RH python3 src/post.py shot $WORK/lin/shot_$s $WORK/frames $s
        [ "$KEEPLIN" = "1" ] || rm -rf $WORK/lin/shot_$s
    else
        echo "== shot $s: already done"
    fi
    base=$((base+n))
done
echo "== frames done ($(date +%H:%M:%S))"
if [ "$QUICK" = "1" ]; then exit 0; fi
python3 src/audio.py $WORK/audio.wav
python3 src/assemble.py $WORK
