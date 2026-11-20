#!/bin/bash
# render.sh — one command to rebuild the whole film. Resumable: finished chunks are skipped.
#   ./render.sh            final 1920x1080 render + audio + assembly + QC  -> spring_awakening_v2.mp4
#   VARIANT=zh ./render.sh same picture with the Taiwanese-Mandarin narration        -> spring_awakening_v2_zh_tw.mp4 (set REUSE_VIDEO=1 to skip re-encoding the picture)
#   ./render.sh preview    quarter-resolution moving storyboard (work/preview)
# Chunks of 48 frames are kept as near-lossless segments in work/seg (or work/preview/seg).
set -u
cd "$(dirname "$0")"
MODE="${1:-final}"
./build.sh || exit 1
if [ "$MODE" = "preview" ]; then W=480; H=270; OUT=work/preview; else W=1920; H=1080; OUT=work; fi
mkdir -p "$OUT/seg" "$OUT/key" "$OUT/log"
python3 -c "
import sys; sys.path.insert(0,'src'); import grade
for n,s,spp in grade.SHOTS: print(n, grade.frames(s), spp)" > "$OUT/shots.txt"
while read -r NAME NF SPP; do
  if [ "$MODE" = "preview" ]; then SPP=12; fi
  F=0
  while [ "$F" -lt "$NF" ]; do
    L=$((F+47)); if [ "$L" -ge "$NF" ]; then L=$((NF-1)); fi
    SEG="$OUT/seg/${NAME}_$(printf %04d $F).mkv"
    WANT=$((L-F+1))
    if [ -f "$SEG.n" ] && [ "$(cat "$SEG.n")" = "$WANT" ]; then F=$((L+1)); continue; fi
    echo "[$(date +%H:%M:%S)] $NAME frames $F-$L spp $SPP" | tee -a "$OUT/log/render.log"
    ./work/render "$NAME" "$F" "$L" "$W" "$H" "$SPP" - 1 src 2>>"$OUT/log/$NAME.log" | python3 src/encode.py "$NAME" "$W" "$H" "$SEG" "$OUT/key" "$F" 2>>"$OUT/log/$NAME.log" || { echo "FAILED $NAME $F" | tee -a "$OUT/log/render.log"; exit 1; }
    F=$((L+1))
  done
done < "$OUT/shots.txt"
echo "[$(date +%H:%M:%S)] RENDER DONE" | tee -a "$OUT/log/render.log"
if [ "$MODE" = "preview" ]; then
  python3 src/assemble.py "$OUT" "$W" "$H" "$OUT/preview.mp4" preview || exit 1
else
  # Edition (see src/variant.py): v2 = English narration (default), zh = Taiwanese-Mandarin narration.
  # The first edition (spring_awakening.mp4, made before the beech leaves were reworked) is never overwritten by this script.
  export VARIANT="${VARIANT:-v2}"
  OUTMP4=$(python3 -c "import sys; sys.path.insert(0,'src'); import variant; print(variant.CFG['mp4'])")
  if [ "$VARIANT" = "zh" ]; then
    # Seed Audio narration: one take for all lines, cut into lines with a local whisper transcript (the take was chosen by hand, see README)
    if [ ! -f work/narr_zh/11.wav ]; then
      python3 src/tts_seed.py take e 8 || { echo "Seed Audio step failed (see README)"; exit 1; }
      whisper work/narr_zh/take_e.wav --model small --language zh --word_timestamps True --output_format json --output_dir work/narr_zh/_whisper --verbose False >/dev/null 2>&1
      python3 src/narr_zh_cut.py take_e write || exit 1
    fi
  elif [ ! -f work/narr/11.wav ]; then
    # narration: edge-tts inside a private virtualenv (needs network; tts.py falls back to the macOS `say` voice)
    [ -x work/venv/bin/python ] || { python3 -m venv work/venv && work/venv/bin/pip install -q edge-tts numpy; }
    work/venv/bin/python src/tts.py || echo "narration step failed (see README)"
  fi
  python3 src/audio.py > "work/log/audio_$VARIANT.log" 2>&1 || exit 1
  python3 src/assemble.py work "$W" "$H" "$OUTMP4" final || exit 1
  python3 src/contact.py || exit 1
  python3 src/qc.py || exit 1
fi
