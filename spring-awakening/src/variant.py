"""variant.py — which edition of the film the audio / assembly / QC scripts work on (environment variable VARIANT).
''   = the first edition (English narration, first render)                      -> spring_awakening.mp4
'v2' = same English sound track on the picture with the reworked beech leaves   -> spring_awakening_v2.mp4
'zh' = v2 picture with the Taiwanese-Mandarin narration (Seed Audio)            -> spring_awakening_v2_zh_tw.mp4"""
import os, importlib
V = os.environ.get('VARIANT', '')
CFG = {
    '':   dict(narr='narration', narr_dir='narr', mix='mix.wav', srt='narration.srt', report='audio_report.txt', mp4='spring_awakening.mp4', qc='qc_report.txt', contact='contact_sheet.jpg', sublang='eng'),
    'v2': dict(narr='narration', narr_dir='narr', mix='mix.wav', srt='narration.srt', report='audio_report.txt', mp4='spring_awakening_v2.mp4', qc='qc_report_v2.txt', contact='contact_sheet_v2.jpg', sublang='eng'),
    'zh': dict(narr='narration_zh', narr_dir='narr_zh', mix='mix_zh.wav', srt='narration_zh_tw.srt', report='audio_report_zh.txt', mp4='spring_awakening_v2_zh_tw.mp4', qc='qc_report_v2_zh_tw.txt', contact='contact_sheet_v2.jpg', sublang='chi'),
}[V]
_m = importlib.import_module(CFG['narr'])
shot_times = _m.shot_times
LINES = [(l[0], l[1], l[3] if len(l) > 3 else l[2]) for l in _m.LINES]     # (shot, offset, subtitle text)
