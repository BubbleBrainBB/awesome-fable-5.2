# Awesome Fable 5.2

English | [中文](README.zh-CN.md)

A collection of pictures and films computed entirely by code. The code for each work was written by Claude, an AI model (`claude-fable-5-2`), in Claude Code; the picture and the sound were computed by that code. No image or video generation model, no photos, textures, 3D models or sound assets, and no Blender, game engine or off-the-shelf renderer were used. The one exception is stated where it applies: the English narration in Mountain Hike comes from a text-to-speech service.

## Works

### [Deep Dive](deep-dive/)

[![Deep Dive](deep-dive/preview.jpg)](https://github.com/BubbleBrainBB/awesome-fable-5.2/releases/download/deep-dive-v1.0/deep_dive.mp4)

A 78-second first-person dive film (1920×1080, 30 fps, with a synthesised soundtrack). It starts under the surface looking up at Snell's window, crosses a shallow reef, drops down a wall, and meets a manta ray in the open blue. The renderer is a headless Metal compute-shader ray marcher written for this film, with FFT waves, a forward-traced caustic light field and flocking fish. The sound is synthesised with numpy and scipy.

- Film: [deep_dive.mp4](https://github.com/BubbleBrainBB/awesome-fable-5.2/releases/download/deep-dive-v1.0/deep_dive.mp4) (129 MB, attached to a release)
- Details, QC results and known shortcomings: [English](deep-dive/README.md) | [中文](deep-dive/README.zh-CN.md)
- Requires: macOS on Apple Silicon (Metal), clang++, Python 3 (numpy, scipy, Pillow), ffmpeg

### [Cat Garden](cat-garden/)

[![Cat Garden](cat-garden/preview.jpg)](https://github.com/BubbleBrainBB/awesome-fable-5.2/releases/download/cat-garden-v1.0/cat_garden.mp4)

A 110-second single-shot film seen through a cat's eyes (1920×1080, 30 fps, with a synthesised soundtrack). The cat squeezes under a fence into a botanical garden, stares at a ladybird, meets a bumblebee, drinks at a koi pond, gets pricked by a cactus in the greenhouse, chases a butterfly, and ends up on a stone wall in the sun. The renderer is a path tracer written for this film in one Metal compute kernel, with its own BVH and about 380,000 instanced plants; the head motion is simulated from gait and gaze springs. The sound is synthesised with numpy and scipy.

- Film: [cat_garden.mp4](https://github.com/BubbleBrainBB/awesome-fable-5.2/releases/download/cat-garden-v1.0/cat_garden.mp4) (304 MB, attached to a release)
- Details, QC results and known shortcomings: [English](cat-garden/README.md) | [中文](cat-garden/README.zh-CN.md)
- Requires: macOS on Apple Silicon (Metal), clang++, Python 3 (numpy, scipy, Pillow), ffmpeg

### [Mountain Hike](mountain-hike/)

[![Mountain Hike](mountain-hike/preview.jpg)](https://github.com/BubbleBrainBB/awesome-fable-5.2/releases/download/mountain-hike-v1.0/mountain_hike.mp4)

A 95-second first-person hiking film in eight shots (1920×1080, 30 fps, with synthesised music and ambience and an English narration). It follows one trail from spruce forest at 1600 m over a footbridge, through misty woods and a treeline of golden larches, across a meadow, a saddle and a snow-dusted ridge, to a 2488 m summit above a sea of cloud at sunset. The renderer is a path tracer written for this film in a Metal compute shader, with height-field ray marching, its own BVH for up to 460,000 instanced trees, plants and rocks, a physical sky, and terrain grown by a landscape-evolution model. The music and ambience are synthesised with numpy and scipy; the seven lines of narration are the one thing not computed by this code (they come from `edge-tts`, an online text-to-speech service).

- Film: [mountain_hike.mp4](https://github.com/BubbleBrainBB/awesome-fable-5.2/releases/download/mountain-hike-v1.0/mountain_hike.mp4) (384 MB, attached to a release)
- Details, QC results and known shortcomings: [English](mountain-hike/README.md) | [中文](mountain-hike/README.zh-CN.md)
- Requires: macOS on Apple Silicon (Metal), clang++, Python 3 (numpy, scipy, Pillow), ffmpeg; the narration step needs the network

### [Afternoon Light](afternoon-light/)

[![Afternoon Light](afternoon-light/preview.jpg)](afternoon-light/afternoon_light.png)

A still image (3840×2560): a quiet room on a late-autumn afternoon, with low sun coming through a French door and laying window-grid light and leaf shadows across a white wall and a herringbone floor. The renderer is a C++ path tracer written from scratch, and the whole scene is modelled in code.

- Image: [afternoon_light.png](afternoon-light/afternoon_light.png)
- Details: [English](afternoon-light/README.md) | [中文](afternoon-light/README.zh-CN.md)
- Requires: a C++17 compiler, Python 3 (numpy, scipy, Pillow)

## Notes

- Each work lives in its own directory and stands alone, with its source, render script and README. Each README lists what was verified, what was not, and where the work falls short.
- Intermediate render files (`work/`) are not in the repository. The script in each directory regenerates them.
