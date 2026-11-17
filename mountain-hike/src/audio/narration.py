#!/usr/bin/env python3
"""English narration with edge-tts (the one non-synthesised sound in the film; needs the network).
Run with the virtualenv python:  work/venv/bin/python src/audio/narration.py work/vo
Writes one mp3 per line plus lines.json; synth.py places them. If this step fails the film is mixed without a voice."""
import asyncio, json, sys, os
import edge_tts

VOICE = "en-GB-RyanNeural"
# (key, shot the line belongs to, seconds after that shot starts, text)
LINES = [
    ("l1", "forest",   1.4, "First light. The trail begins in the spruce, at sixteen hundred metres."),
    ("l1b", "stream",  1.0, "A footbridge carries the path over the brook."),
    ("l2", "shafts",   1.5, "Mist still hangs between the trunks."),
    ("l3", "treeline", 1.2, "Higher up, the spruce thin out, and the larches have turned gold."),
    ("l4", "saddle",   1.5, "Above the last trees, the valleys lie under a sea of cloud."),
    ("l5", "ridge",    1.2, "The ridge is bare rock, dusted with the first snow."),
    ("l6", "summit",   9.6, "The summit, late in the afternoon. Nothing above us but sky."),
]

async def main(out):
    os.makedirs(out, exist_ok=True)
    for key, shot, t, text in LINES:
        await edge_tts.Communicate(text, VOICE, rate="-12%", pitch="-3Hz").save(os.path.join(out, key + ".mp3"))
    json.dump([{"key": k, "shot": s, "t": t, "text": x} for k, s, t, x in LINES], open(os.path.join(out, "lines.json"), "w"), indent=1)

if __name__ == "__main__":
    asyncio.run(main(sys.argv[1] if len(sys.argv) > 1 else "work/vo"))
