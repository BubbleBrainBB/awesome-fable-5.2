"""grade.py — shot list (order, length in seconds) and per-shot development settings."""
FPS = 24
SHOTS = [  # name, seconds, samples per pixel for the final render
    ('winter', 7.5, 40), ('icicles', 6.0, 80), ('drop', 4.5, 128), ('thaw', 7.0, 56), ('crocus', 7.5, 56),
    ('seedling', 9.5, 72), ('fern', 7.5, 64), ('bud', 7.5, 112), ('ladybird', 8.0, 112), ('takeoff', 4.0, 128),
    ('rain', 7.0, 96), ('snail', 7.0, 80), ('spring', 10.0, 32),
]
COOL = (0.94, 1.0, 1.10); NEUT = (1.0, 1.0, 1.0); WARM = (1.05, 1.0, 0.93)
GRADES = {
    None: dict(exposure=1.0),
    'test': dict(exposure=1.0),
    'winter': dict(exposure=1.7, wb=COOL, contrast=1.2, sat=0.95),
    'icicles': dict(exposure=1.15, wb=(0.97, 1.0, 1.06), contrast=1.3),
    'drop': dict(exposure=1.2, wb=(0.97, 1.0, 1.06), contrast=1.4, sat=1.25),
    'thaw': dict(exposure=0.95, wb=(0.98, 1.0, 1.03), contrast=1.25),
    'crocus': dict(exposure=1.0, wb=NEUT, contrast=1.25),
    'seedling': dict(exposure=1.15, wb=(1.03, 1.0, 0.96), contrast=1.25),
    'fern': dict(exposure=1.05, wb=(1.03, 1.0, 0.96), contrast=1.25),
    'bud': dict(exposure=1.0, wb=(1.03, 1.0, 0.97), contrast=1.25),
    'ladybird': dict(exposure=1.0, wb=WARM, contrast=1.25),
    'takeoff': dict(exposure=1.0, wb=WARM, contrast=1.25),
    'rain': dict(exposure=0.85, wb=(0.98, 1.0, 1.02), contrast=1.2, sat=0.95),
    'snail': dict(exposure=1.05, wb=WARM, contrast=1.25),
    'spring': dict(exposure=1.25, wb=(1.06, 1.0, 0.92), contrast=1.2, sat=1.08, glare_amt=0.09),
}
def get(shot):
    return dict(GRADES.get(shot, GRADES[None]))
def frames(sec):
    return int(round(sec * FPS))
