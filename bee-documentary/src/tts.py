#!/usr/bin/env python3
# tts.py — 用 edge-tts 生成英文旁白（这是全片唯一不是自己代码合成的声音；用户明确允许）。
# 用法: tts.py audition | tts.py final
import asyncio, sys, os, subprocess
sys.path.insert(0, os.path.dirname(__file__))
import edge_tts
from narration import LINES, VOICE, RATE

async def say(text, voice, rate, out):
    c = edge_tts.Communicate(text, voice, rate=rate)
    await c.save(out)

def to_wav(mp3, wav):
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", mp3, "-ar", "48000", "-ac", "1", "-c:a", "pcm_s16le", wav], check=True)

async def main():
    mode = sys.argv[1]
    os.makedirs("work/tts", exist_ok=True)
    if mode == "audition":
        sample = LINES[1][2] + " " + LINES[5][2]
        for v in ["en-GB-RyanNeural", "en-GB-ThomasNeural", "en-US-GuyNeural", "en-US-ChristopherNeural", "en-GB-SoniaNeural", "en-AU-WilliamNeural", "en-US-AndrewNeural", "en-US-BrianNeural"]:
            try:
                await say(sample, v, RATE, f"work/tts/aud_{v}.mp3"); to_wav(f"work/tts/aud_{v}.mp3", f"work/tts/aud_{v}.wav"); print("ok", v)
            except Exception as e:
                print("fail", v, repr(e)[:100])
    else:
        for i, (shot, t0, text) in enumerate(LINES):
            await say(text, VOICE, RATE, f"work/tts/line_{i:02d}.mp3"); to_wav(f"work/tts/line_{i:02d}.mp3", f"work/tts/line_{i:02d}.wav"); print("ok", i)
asyncio.run(main())
