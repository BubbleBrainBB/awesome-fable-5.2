# Deep Dive: a first-person dive film, computed entirely by code

English | [中文](README.zh-CN.md)

A 78-second first-person (POV) dive film: the camera is the diver's eyes. It starts just under the surface looking up at Snell's window, follows a sand channel across a shallow reef, drops over the reef edge and down a wall, switches on a dive torch at a little over twenty metres, and finally turns to the open blue to watch a manta ray glide across the sun overhead.

[![Deep Dive](preview.jpg)](https://github.com/BubbleBrainBB/awesome-fable-5.2/releases/download/deep-dive-v1.0/deep_dive.mp4)

- Film: [`deep_dive.mp4`](https://github.com/BubbleBrainBB/awesome-fable-5.2/releases/download/deep-dive-v1.0/deep_dive.mp4) (1920×1080, 30 fps, 78 s, H.264 High / yuv420p + AAC stereo, faststart). The file is 129 MB, above GitHub's per-file limit, so it is not in the repository; it is attached to a release. Running the steps under "Rendering it yourself" also produces it in this directory.
- Cover: `preview.jpg` (frame 2085 of the film)
- Contact sheet: [`contact_sheet.jpg`](contact_sheet.jpg) (one frame per second, 78 cells)
- QC report: [`qc_report.txt`](qc_report.txt); soundtrack spectrogram: [`audio_spectrogram.png`](audio_spectrogram.png)

The code was written by Claude, an AI model (`claude-fable-5-2`); the picture and the sound were computed by that code.

Picture, motion and sound all come from the code in this directory. No image or video generation model, no photos, footage, 3D models, textures, sound effects or music assets, no Blender, game engine or off-the-shelf renderer, and no video-making agent skill were used. The tools are clang++, the Metal framework that ships with macOS (running a compute shader written for this film), Python (numpy, scipy, Pillow), and ffmpeg for encoding and muxing only.

## Storyboard and route

World coordinates: y is up and the surface is at y = 0. The reef flat is at x < 0, the reef edge at x ≈ 0, and the wall drops straight down on the +x (open sea) side. The sun is out to sea at an elevation of 47° (about 30° from the zenith after refraction into the water).

| Time | Section | Depth | What you see |
| --- | --- | --- | --- |
| 0–6 s | Entry | 1.6 m | Looking up: the moving surface overhead, with the sky squeezed into a circular window (Snell's window). Outside the window is total internal reflection, mirroring the reef. Bubbles from the entry are rising. |
| 6–13 s | Looking down, descending | 2–5 m | The gaze drops from the surface to the way ahead. Light shafts slant down, and a moving net of caustics covers the sand channel and the reef flat. |
| 13–26 s | Along the sand channel | 6–7 m | Table corals, branching corals and boulder corals on both sides. A school of fusiliers (yellow stripe, blue back) crosses from the front left and parts around the diver. |
| 26–35 s | Reef top | 4–6 m | Up onto the reef top, where clouds of orange anthias hover above the coral heads. Deep blue starts to show beyond the edge. |
| 35–44 s | Over the edge | 4→11 m | The reef falls away underfoot. A right turn, then a descent along the wall looking down, with sea whips and sea fans reaching out from it. |
| 44–60 s | The wall | 11→21 m | The water goes from blue-green to deep blue, red disappears, and the sea fans become black silhouettes. At 45.6 s the torch comes on: fans and sponges in its spot get their red and orange back, and particles in the beam light up. |
| 60–66 s | Turning to the blue | 22 m | Looking away from the wall there is only blue, a distant school of fish, and a shape appearing out of the haze. |
| 66–75 s | Manta | 22 m | The manta swims towards the diver and passes across the sun overhead. The gaze follows it upwards: the surface is more than twenty metres away, with light radiating from it. The diver's own bubbles rise. |
| 75–78 s | Ending | 22 m | The manta swims away up the wall. Fade out. |

Breathing runs through the whole film: one breath roughly every 6 seconds (about 1.9 s in, 2.8 s out, each slightly different). The body rises a little on the inhale and sinks on the exhale. Exhaled bubbles rise from both sides of the bottom of the frame; in level swimming most of them are out of view, and they are visible when looking down or up.

## How it was made

### Rendering: a Metal ray marcher written for this film

`src/render.metal` is a compute shader that does a complete "underwater camera" calculation for every sample. `src/main.mm` is the host program: it handles the scene, the simulation, the water surface and caustics for each frame, GPU dispatch, post-processing and PNG output. All geometry is signed distance fields (SDF). There are no meshes and no texture files.

**Underwater light (the part that decides whether it works)**

- **Surface.** `src/water.h` uses its own FFT to turn a random wave spectrum (dispersion relation ω = √(gk), travelling downwind) into a periodic 1024² height field covering 25.6 m. It is recomputed for every sub-frame, so the surface is made of waves that really travel.
- **Caustics and light shafts from one source.** For every sub-frame about 4.2 million sun rays are refracted through that surface by Snell's law and splatted forward onto 36 horizontal slices (0.35 m to 66 m deep, sheared along the mean refracted direction). The result is a 3D field of focused sunlight. Sand, coral, fish, the manta and suspended particles all read their sunlight from it, and volume scattering along the view ray queries the same field. So the shafts in the water line up with the light net on the bottom, and both move because the same wave surface moves. The sun's angular size and the water's forward scattering blur the field with depth, so deep caustics soften naturally.
- **Water.** Absorption and scattering are computed per RGB channel (beam attenuation: red loses about 28% per metre, green about 9%, blue about 7%). Distant things dissolve into blue haze, deeper is bluer, and red goes first. Single scattering of sunlight is marched along the view ray (48 steps, importance-sampled by transmittance) with a strongly forward dual Henyey–Greenstein phase function, so shafts are clearest looking towards the sun. Multiple scattering is an analytic term that falls off with depth and is brighter upwards.
- **The surface from below.** Where a view ray reaches the surface, it is refracted using the wave normal. Refracted rays see the sky and the sun. Past the critical angle of 48.6° there is total internal reflection, and a reflected ray is traced to the reef (which is why the surface outside the window mirrors the reef). The shape of the window is computed from the physics, not drawn.
- **Torch.** A soft-edged spotlight follows the gaze with lag and hand shake. It lights surfaces and suspended particles and has a very faint volumetric beam. Its light has not been filtered by twenty metres of water, so red returns where it lands.
- **Exposure.** A low-resolution pre-pass measures the mean brightness of each frame, and `auto_exposure.py` compensates slowly and only partly, like a camera's auto-exposure. Deep water is darker than shallow water but still readable.

**Scene and animals (all procedural)**

- **Reef** (`src/shared.h`, one piece of code used by both the shader and the CPU): a base shape of reef flat, sand channel, reef edge and wall, with coral heads, knobs and finger-like bumps added from Perlin and Worley noise computed with integer hashes. Each Worley cell is a colony with its own colour.
- **Corals and sponges** (about 1,700 instances in a 3D grid for ray lookup): table corals, branching corals (a folded-space fractal), sea fans (a 2D fractal tree), sea whips, barrel sponges and boulder corals. Sea fans and sea whips bend with the surge.
- **Surge.** One back-and-forth current function. Soft bodies, particles, fish, bubbles and the diver are all pushed by it in the same phase, which is why things underwater move together.
- **Fusilier school** (280 fish): a flocking simulation with separation, alignment and cohesion. The school follows a moving anchor in a streamlined formation and avoids the reef, the diver and the manta. Tail beat follows swimming speed, bodies bend in turns, and the silver flanks reflect the surroundings.
- **Anthias** (about 630 fish in 46 clouds): each hovers in a small space above its coral head, faces the current and darts now and then. They pull back into the coral when the diver comes close.
- **Manta.** Modelled as an SDF (diamond-shaped disc, cephalic fins, tail, dorsal fin, gill slits and spots on the belly). The wings curl along a circular arc as they beat, which keeps their length, and the wing tips lag in phase. The path is a banked spline.
- **Bubbles.** A particle simulation (rise speed depends on radius, with a spiral wobble; large bubbles flatten into caps and wobble). The exhale rhythm comes from the breathing timeline. When rendered, the rim becomes a bright mirror ring through total internal reflection and the centre is transparent.
- **Marine snow.** Suspended particles from a spatial hash, with parallax, lit by the sun (including caustics) and by the torch.

**First-person camera** (`src/sim.h`)

A keyframe spline gives the route and the gaze. On top of that: second-order lag on head turns (the gaze has inertia), rise and fall from breathing (lung volume drives a damped spring), side-to-side sway and roll from fin kicks (scaled by swimming speed), slow head drift, and the push of the surge. Once the manta appears, the gaze locks onto it. The lens is a 104° wide angle with slight barrel distortion and a very light mask vignette.

**Temporal consistency**

- Each frame is 4 sub-frames × 6 samples = 24 samples with a 180° shutter. Every sub-frame has its own wave surface, caustic field and simulation state. Inside a sub-frame the camera, fish and bubbles also move continuously per sample according to their velocities, so motion blur is continuous instead of four ghost images.
- Apart from pixel jitter and jittered volume steps there are no random rays, so the picture has no Monte Carlo noise. A very light film grain and dither are added at the end so that blue gradients do not band in 8-bit encoding.
- All random seeds are fixed: the same command gives the same film.

### Sound: `audio.py` (synthesised with numpy and scipy)

The soundtrack shares the breathing timeline (`work/frames/timeline.txt`) and the camera depth curve with the picture.

- An underwater bed (low rumble plus mid-frequency water noise, darker with depth), surge wash driven by the same function as the picture, and surface slap at the start.
- Regulator: the inhale is an airflow hiss with hose resonance and a soft click as the valve opens. The exhale is the gurgle of the exhaust valve plus a cluster of bubbles, each one a short decaying tone at its Minnaert resonance frequency (inversely proportional to radius, rising with water pressure).
- Reef crackle (snapping shrimp), dense on the shallow reef and sparse at depth.
- Distant whale-like calls in the blue-water section (glides with long reverb).
- A very restrained pad (a few long notes, no rhythm), and a soft click for the torch switch.

The layers are mixed by their level while actually sounding. The regulator is the loudest layer, as it is for a diver.

### Files

| File | Contents |
| --- | --- |
| `make.sh` | One command to regenerate the whole film (render → soundtrack → QC → encode → verify) |
| `render.sh` / `build.sh` | Build and render (resumable: frames that already exist are skipped) |
| `src/main.mm` | Host: Metal dispatch, screen-tile binning, post-processing (glare, tone curve, vignette, grain), PNG |
| `src/render.metal` | Compute shader: ray marching, materials, water volume, surface, fish, manta, bubbles, marine snow |
| `src/shared.h` | Shared by CPU and GPU: terrain SDF, noise, surge, caustic slice layout |
| `src/water.h` | FFT wave surface, forward-traced caustic light field |
| `src/world.h` | Placement of coral instances and the lookup grid |
| `src/sim.h` | Simulation of the camera/diver, fish schools, manta and bubbles |
| `src/base.h` | Vectors, hashing, parallel loops, FFT, PNG encoder |
| `auto_exposure.py` | Builds the exposure curve from the pre-pass brightness statistics |
| `audio.py` / `audio_check.py` | Soundtrack synthesis / spectrum and breath-alignment check |
| `qc.py` / `verify.sh` / `sheet.py` | Per-frame QC and contact sheet / checks on the final file / debug contact sheet |

## Rendering it yourself

```sh
./make.sh
```

You need macOS on Apple Silicon (Metal), clang++ from the Xcode command line tools, Python 3 with numpy, scipy and Pillow, and ffmpeg. Intermediate files go to `work/` (about 5.5 GB of frames). The run can be interrupted and restarted at any time; frames already rendered are skipped.

Time on a 16-core M4 Max. These are the actual numbers from this run; other jobs were using the machine during the render, so an idle machine will be faster.

| Step | Time |
| --- | --- |
| Low-resolution pre-pass (exposure statistics, 390 frames) | 36 s |
| Final 2340 frames (1920×1080, 24 samples, 4 sub-frames) | 76.6 min (1.96 s per frame on average; about 0.9 s in the shallows, about 3 s on the wall) |
| Independent check render, 2340 frames (480×270) | 21 min |
| Soundtrack, QC, encode, verify | about 6 min |

About 1 hour 45 minutes in total. The delivered film took one extra step: after QC found a problem (see below), the shader was changed and the 771 affected frames were re-rendered (27 minutes, plus 10 minutes for the check render). The source here already includes that change, so a fresh `./make.sh` does not need the extra step.

Quick look: `SIZE=960x540 SPP=8 ./render.sh` (delete `work/frames` first).

## QC results

`qc_report.txt` is the raw output of `qc.py` on the final frames. The output of `verify.sh` is summarised below. All of these are results from actual runs.

**Per frame (2340 PNG frames)**

- All 2340 frames exist, decode, and are 1920×1080. The render log has 0 GPU errors. The renderer stores the number of samples each pixel received in the alpha channel of the accumulation buffer and refuses to write a frame if any pixel is short. That did not trigger in this run.
- Every frame was compared with an **independently re-rendered** low-resolution version (a separate process, 480×270) by the mean of 32×18 blocks. The median of the largest block difference is 0.95 levels (8-bit) and the maximum over the film is 10.07 levels (frame 1840), below the threshold of 14. So there are no frames that are silently damaged or missing a region.
- No black frames (lowest mean brightness outside the fades is 120.8) and no black blocks. The frame with the most clipped pixels has 0.59%.
- Flicker test (a large 480×360 region jumping relative to both the previous and the next frame): maximum 2.44 levels over the film against a threshold of 4, with 0 frames flagged. The same measure on small 60×60 blocks exceeds 9 levels on 402 frames. The highest of these were inspected (frames 85–90, 603–606, 659–660): they are bubbles close to the lens and fish crossing a small block within one frame. That is picture content, not render flicker, so it is listed for information only.
- **A real problem this step caught, now fixed.** After the first full render the large-region flicker test flagged frame 1504: a bubble touching the mask appeared for a single frame as a white flash at the lower left. It was also in the independent render, so it was scene content and not a damaged frame. The fix: bubbles fade out within 7–14 cm of the lens. The simulation was scanned for frames with a bubble passing close to the view (771 frames), those frames were re-rendered in both the final and the check version, and the whole film went through QC again, with the results above.
- The contact sheet `contact_sheet.jpg` (one frame per second, 78 cells) was looked at in full.

**The final file `deep_dive.mp4`**

- ffprobe: H.264 High, yuv420p, 1920×1080, 30 fps, 2340 frames, 78.000 s, video about 13.0 Mbps; AAC-LC 48 kHz stereo at about 263 kbps; 129 MB; `moov` before `mdat` (faststart).
- A full decode ran without errors and produced 2340 frames. blackdetect reported only the 0.3 s fade at the end.
- Frames extracted after compression were checked in the blue gradient areas: most of the grain survives (texture standard deviation 1.14 levels against 1.42 in the source frames) and no obvious banding was seen.

**Sound and sync (checked only as waveforms and spectra; nobody has listened to it)**

- Claude cannot hear the soundtrack. Only numbers can be confirmed: overall RMS −21 dBFS, peak −6.9 dBFS. In the inhale windows the hiss band (1.5–9 kHz) is about 17 dB above the gaps between breaths, and in the exhale windows the bubble band (150–1400 Hz) is about 13 dB above them. The spectrogram is `audio_spectrogram.png` (green bars at the bottom are inhales, orange bars are exhales).
- A/V sync: the number of bubbles born in each frame (from the render log) was cross-correlated with the bubble-band energy of the soundtrack in the final file. The peak is 0.56 at a lag of 0 frames. Bubbles in the picture are born only inside exhale windows (about 16 per second, and 0 outside them).
- Whether the regulator sounds real, and whether the whale calls and the pad sound good, could not be verified. Listen for yourself.

## Known shortcomings

- **Claude cannot actually watch or listen to the film.** Motion was judged from contact sheets, comparisons of neighbouring frames and numeric measures. The amount of camera sway, the rhythm of the school and whether the manta's wing beat looks natural have not been checked in continuous playback. Nobody has listened to the soundtrack.
- **Light.** Single scattering of sunlight, caustics, and refraction and total internal reflection are computed. Multiple scattering is only an analytic approximation, and the water's optical coefficients are three RGB numbers, not a spectrum. Light shafts are not shadowed by the reef or the manta (the volume has no shadows). Shadows on surfaces are distance-field soft shadows along the mean refracted direction. The reef mirrored in the surface outside the window uses simplified lighting. There are no clouds, the sun is rendered as a soft glow, and there are no star bursts or lens ghosts as a real lens would give.
- **Reef and corals.** Everything is distance fields plus noise. Up close it looks rounded and soft, without the pores and polyps of real coral. There are only six kinds of form (table, branching, sea fan, sea whip, barrel sponge, boulder). Branching corals and sea fans are symmetric fractals, and the regularity shows on a close look. There are no soft corals, anemones or feather stars. Sand ripples are a normal perturbation, not geometry.
- **Fish.** Only two species, with no pectoral or pelvic fins and no scales. The school is boids plus a designed route: it does not feed and does not react to the torch. Neither the fish nor the manta casts a shadow on the bottom. There is only one manta, its wing beat is a prescribed bending function and not swimming driven by forces, and its belly markings are procedural.
- **Bubbles.** They are ellipsoids that never split or merge. The centre is simply transparent, without the shrunken refracted background seen through a real bubble. They fade out within 7–14 cm of the lens (to remove the one-frame white flash).
- **First person.** The frame has no mask edge, hands, gauges, snorkel or fins, and the torch has no body, only light. There is no depth of field. Motion blur is 4 sub-frames per frame plus translation interpolated inside each sub-frame; camera rotation is not interpolated inside a sub-frame.
- **Look.** The film is close to a blue-cyan monochrome (true of unlit underwater footage, but a real camera's white balance and colour are richer). The picture is slightly soft (reconstruction filter, glare and haze). The few seconds turning to the blue at 60–64 s are rather empty.
- **Reproducibility.** Seeds are fixed and the pipeline is deterministic, but two full renders were not compared byte for byte. In the delivered film 771 frames were re-rendered after the fix. The other frames are not affected by that change (for the reason given under QC), but that was not verified byte for byte either.
- This is a procedural film that looks like dive footage. It is still clearly short of the richness of real underwater photography: a real reef has far more life, and far more variety in form and colour, than this one.
