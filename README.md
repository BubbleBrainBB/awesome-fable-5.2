# Awesome Fable 5.2

English | [中文](README.zh-CN.md)

A collection of pictures and films computed entirely by code. The code for each work was written by Claude, an AI model (`claude-fable-5-2`), in Claude Code; the picture and the sound were computed by that code. No image or video generation model, no photos, textures, 3D models or sound assets, and no Blender, game engine or off-the-shelf renderer were used.

## Works

### [Deep Dive](deep-dive/)

[![Deep Dive](deep-dive/preview.jpg)](https://github.com/BubbleBrainBB/awesome-fable-5.2/releases/download/deep-dive-v1.0/deep_dive.mp4)

A 78-second first-person dive film (1920×1080, 30 fps, with a synthesised soundtrack). It starts under the surface looking up at Snell's window, crosses a shallow reef, drops down a wall, and meets a manta ray in the open blue. The renderer is a headless Metal compute-shader ray marcher written for this film, with FFT waves, a forward-traced caustic light field and flocking fish. The sound is synthesised with numpy and scipy.

- Film: [deep_dive.mp4](https://github.com/BubbleBrainBB/awesome-fable-5.2/releases/download/deep-dive-v1.0/deep_dive.mp4) (129 MB, attached to a release)
- Details, QC results and known shortcomings: [English](deep-dive/README.md) | [中文](deep-dive/README.zh-CN.md)
- Requires: macOS on Apple Silicon (Metal), clang++, Python 3 (numpy, scipy, Pillow), ffmpeg

### [Afternoon Light](afternoon-light/)

[![Afternoon Light](afternoon-light/preview.jpg)](afternoon-light/afternoon_light.png)

A still image (3840×2560): a quiet room on a late-autumn afternoon, with low sun coming through a French door and laying window-grid light and leaf shadows across a white wall and a herringbone floor. The renderer is a C++ path tracer written from scratch, and the whole scene is modelled in code.

- Image: [afternoon_light.png](afternoon-light/afternoon_light.png)
- Details: [English](afternoon-light/README.md) | [中文](afternoon-light/README.zh-CN.md)
- Requires: a C++17 compiler, Python 3 (numpy, scipy, Pillow)

## Notes

- Each work lives in its own directory and stands alone, with its source, render script and README. Each README lists what was verified, what was not, and where the work falls short.
- Intermediate render files (`work/`) are not in the repository. The script in each directory regenerates them.
