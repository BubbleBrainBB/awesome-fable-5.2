"""tts_seed.py — Taiwanese-Mandarin narration with ByteDance Seed Audio (seed-audio-1.0).
Only the voice direction and the narration text are sent. The API key is read at run time from the environment:
SEED_AUDIO_API_KEY, or SEED_AUDIO_KEY_FILE naming a file with a line  API_KEY = "..."  (never printed, never stored here).
  tts_seed.py take <name> [speech_rate]   one take of the whole narration in a single call (one voice for all lines) -> work/narr_zh/take_<name>.wav
  tts_seed.py line <name> <i> [rate]      one line on its own -> work/narr_zh/line_<name>_<ii>.wav
"""
import base64, json, os, re, subprocess, sys, time
from pathlib import Path
import requests
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import narration_zh as NZ
ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'work' / 'narr_zh'; OUT.mkdir(parents=True, exist_ok=True)
KEY = os.environ.get('SEED_AUDIO_API_KEY')
if not KEY and os.environ.get('SEED_AUDIO_KEY_FILE'):
    KEY = re.search(r'API_KEY\s*=\s*"([^"]+)"', Path(os.environ['SEED_AUDIO_KEY_FILE']).expanduser().read_text()).group(1)
if not KEY: sys.exit('set SEED_AUDIO_API_KEY (or SEED_AUDIO_KEY_FILE) first')
H = {'Content-Type': 'application/json', 'X-Api-Key': KEY}
CREATE = 'https://openspeech.bytedance.com/api/v3/tts/create'; QUERY = 'https://openspeech.bytedance.com/api/v3/tts/query'

def find_audio(q):
    def walk(o):
        if isinstance(o, dict):
            for k, v in o.items(): yield k, v; yield from walk(v)
        elif isinstance(o, list):
            for v in o: yield from walk(v)
    if isinstance(q.get('audio'), str): return base64.b64decode(q['audio'])
    for k, v in walk(q):
        if isinstance(v, str) and v.startswith('http') and ('audio' in k.lower() or 'url' in k.lower()): return requests.get(v, timeout=120).content
    for k, v in walk(q):
        if isinstance(v, str) and len(v) > 5000 and 'audio' in k.lower(): return base64.b64decode(v)
    return None

def synth(prompt, rate):
    r = requests.post(CREATE, headers=H, timeout=180, json={'model': 'seed-audio-1.0', 'text_prompt': prompt,
        'audio_config': {'format': 'mp3', 'sample_rate': 48000, 'pitch_rate': 0, 'speech_rate': rate, 'loudness_rate': 0}, 'watermark': {}})
    d = r.json()
    if d.get('audio'): return find_audio(d)
    tid = d.get('task_id') or d.get('id') or (d.get('data') or {}).get('task_id')
    if not tid: sys.exit('create returned no task id: ' + json.dumps({k: v for k, v in d.items() if k != 'audio'}, ensure_ascii=False)[:400])
    for _ in range(90):
        q = requests.post(QUERY, headers=H, json={'task_id': tid}, timeout=120).json()
        s = q.get('status') or (q.get('data') or {}).get('status')
        if s in ('success', 'done', 'completed', 2): return find_audio(q)
        if s in ('failed', 'error', 3): sys.exit('failed: ' + json.dumps(q, ensure_ascii=False)[:400])
        time.sleep(4)
    sys.exit('query timed out')

def save(audio, base):
    mp3 = str(base) + '.mp3'; open(mp3, 'wb').write(audio)
    subprocess.check_call(['ffmpeg', '-v', 'error', '-y', '-i', mp3, '-ar', '48000', '-ac', '1', '-c:a', 'pcm_s16le', str(base) + '.wav'])
    print('wrote', os.path.basename(str(base)) + '.wav', len(audio), 'bytes')

mode, name = sys.argv[1], sys.argv[2]
if mode == 'take':
    rate = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    body = '\n\n'.join(f'旁白：【{tag}】{trad}' for _, _, trad, _, tag in NZ.LINES)
    prompt = NZ.DIRECTION.format(pause='每唸完一句，安靜地停頓兩秒，再唸下一句。', body=body)
    save(synth(prompt, rate), OUT / f'take_{name}')
else:
    i = int(sys.argv[3]); rate = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    _, _, trad, _, tag = NZ.LINES[i]
    prompt = NZ.DIRECTION.format(pause='只唸下面這一句，唸完立刻收聲。', body=f'旁白：【{tag}】{trad}')
    save(synth(prompt, rate), OUT / f'line_{name}_{i:02d}')
