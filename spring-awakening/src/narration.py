"""narration.py — the narration script: (shot, seconds after the shot starts, text)."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import grade
LINES = [
    ('winter',   2.2, "For months, the forest has held its breath."),
    ('icicles',  1.2, "Then, one morning, the sun returns with warmth in it."),
    ('drop',     0.5, "And the ice begins to let go."),
    ('thaw',     1.3, "Day by day, the snow draws back from the ground it has kept hidden."),
    ('crocus',   1.6, "The first flowers do not wait for it to leave."),
    ('seedling', 0.6, "Under the soil, a seed has felt the change. All it has saved goes into a single push toward the light."),
    ('fern',     1.5, "Ferns unroll, each frond packed tight since last year."),
    ('bud',      1.2, "On the branches, buds made last summer split open, and the wood turns green."),
    ('ladybird', 1.3, "A ladybird that slept through the winter among the dead leaves climbs toward the sun."),
    ('rain',     1.2, "Then the rain comes. And it is no longer cold."),
    ('snail',    1.8, "Now even the slowest are on the move."),
    ('spring',   2.6, "None of this is new. It was all here, under the snow, waiting."),
]
VOICE = 'en-GB-ThomasNeural'   # chosen in tts.py after measuring candidates (see README)
RATE = '-10%'
def shot_times():
    t = 0.0; out = {}
    for name, sec, _ in grade.SHOTS:
        out[name] = (t, t + sec); t += sec
    return out
