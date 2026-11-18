# narration.py — 旁白文本与落点（成片时间，秒）。唯一发给外部服务（edge-tts）的内容就是这些英文句子。
VOICE = "en-GB-ThomasNeural"
RATE = "-10%"
LINES = [
    # (镜头号, 最早开始时间, 文本)
    (1, 1.6,  "Morning. The cosmos are open, and the first foragers are already at work."),
    (2, 8.7,  "A worker honey bee. Her wings beat more than two hundred times a second."),
    (3, 15.5, "She weighs about a tenth of a gram. Enough to make the flower bow."),
    (4, 22.0, "Her antennae taste the surface as she walks. Every floret on this disc is a flower of its own."),
    (5, 31.0, "Then the tongue unfolds, and reaches down for the nectar. Sugar water: fuel for the flight home."),
    (6, 40.0, "Her coat does the rest. Each hair is branched, like a feather, and pollen clings."),
    (7, 48.0, "She combs it out with her legs, and packs it into the baskets on her hind legs."),
    (8, 55.9, "What she drinks is not yet honey. That is made later, in the hive."),
    (9, 62.8, "Loaded, she lifts off. For the hive, or the next flower."),
    (10, 68.9, "And the flower has been paid in return: a dusting of pollen from somewhere else."),
]
# 各镜头在成片里的起止帧（24 fps）
SHOT_FRAMES = [192, 168, 150, 216, 216, 192, 192, 168, 144, 168]
FPS = 24
