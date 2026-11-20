"""tts.py — narration via edge-tts (the only external generator used; only the narration text is sent).
Falls back to the macOS `say` command if the network call fails. Writes work/narr/NN.wav (48 kHz mono) and work/narr/voice.txt.
usage: tts.py [audition]   — 'audition' renders two test lines with several voices and prints measurements."""
import asyncio, os, subprocess, sys, numpy as np, wave
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import narration
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'work', 'narr'); os.makedirs(OUT, exist_ok=True)

def to_wav(src, dst):
    subprocess.check_call(['ffmpeg', '-v', 'error', '-y', '-i', src, '-ar', '48000', '-ac', '1', '-c:a', 'pcm_s16le', dst])
def read_wav(p):
    with wave.open(p) as w: return np.frombuffer(w.readframes(w.getnframes()), '<i2').astype(np.float32) / 32768.0
def measure(x, sr=48000):
    """median pitch (autocorrelation on voiced frames), duration of the spoken part, pause share"""
    hop, win = 480, 1920; f0 = []; act = 0; n = 0
    thr = 0.06 * np.max(np.abs(x))
    for i in range(0, len(x) - win, hop):
        fr = x[i:i + win]; n += 1
        if np.sqrt(np.mean(fr ** 2)) < thr * 0.5: continue
        act += 1
        fr = fr - fr.mean(); ac = np.correlate(fr, fr, 'full')[win - 1:]
        lo, hi = sr // 300, sr // 60
        k = lo + np.argmax(ac[lo:hi])
        if ac[k] > 0.4 * ac[0]: f0.append(sr / k)
    return (float(np.median(f0)) if f0 else 0.0, len(x) / sr, 1 - act / max(n, 1))

async def speak(text, voice, rate, path):
    import edge_tts
    await edge_tts.Communicate(text, voice, rate=rate).save(path)

def synth(text, voice, rate, base):
    mp3 = base + '.mp3'; wav = base + '.wav'
    try:
        asyncio.run(speak(text, voice, rate, mp3)); to_wav(mp3, wav); os.remove(mp3); return 'edge-tts'
    except Exception as e:
        print('edge-tts failed:', repr(e)[:200], '-> falling back to say', file=sys.stderr)
        aiff = base + '.aiff'
        subprocess.check_call(['say', '-v', 'Daniel', '-r', '150', '-o', aiff, text]); to_wav(aiff, wav); os.remove(aiff); return 'say'

if len(sys.argv) > 1 and sys.argv[1] == 'audition':
    tests = [narration.LINES[0][2], narration.LINES[5][2]]
    for v in ['en-GB-RyanNeural', 'en-GB-ThomasNeural', 'en-US-ChristopherNeural', 'en-US-GuyNeural', 'en-US-AndrewMultilingualNeural', 'en-US-BrianMultilingualNeural', 'en-AU-WilliamNeural']:
        res = []
        for i, t in enumerate(tests):
            base = os.path.join(OUT, f'aud_{v}_{i}')
            try: synth(t, v, narration.RATE, base); res.append(measure(read_wav(base + '.wav')))
            except Exception as e: res.append((0, 0, 0))
        print(v, ' | '.join(f'f0 {a:.0f} Hz, {b:.2f} s, pauses {c:.2f}' for a, b, c in res))
    sys.exit(0)

engine = set()
for i, (shot, off, text) in enumerate(narration.LINES):
    engine.add(synth(text, narration.VOICE, narration.RATE, os.path.join(OUT, f'{i:02d}')))
    f0, dur, pz = measure(read_wav(os.path.join(OUT, f'{i:02d}.wav')))
    print(f'{i:02d} {dur:5.2f} s  f0 {f0:.0f} Hz  {text}')
open(os.path.join(OUT, 'voice.txt'), 'w').write(('+'.join(sorted(engine))) + ' ' + narration.VOICE + ' ' + narration.RATE + '\n')
