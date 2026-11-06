# Handoff — 하직: GPU path tracer shaders

> **How to use:** paste this whole file into your AI assistant at the root of the cloned repo, then say "start with step 1".

## Context for the AI

Retro Tech Museum is a C++17 / OpenGL **4.1 core** app (CMake; Windows + macOS, both checked in CI). The headline feature is pressing one key to switch the whole gallery from raster to a **progressive GPU path tracer in a fragment shader**.

What already exists and works:

- `shaders/scene_access.glsl`: the scene in float textures plus **two-level BVH traversal**. It gives you `rtmIntersectScene` (closest-hit / any-hit), `rtmSurface` and `rtmMaterial`. Unit-tested on the CPU against brute force.
- `shaders/debug_cast.frag`: a working primary-ray + shadow-ray caster built on it. **This is your starting point.**
- `src/main.cpp`: the T key toggles Raster / Ray cast. Uniforms come from `gpuScene.bind()`.
- `docs/GPU_LAYOUT.md`: the exact data format. Section 4 is the GLSL API.

Read first: `README.md` (**OpenGL 4.1 rules**), `docs/GPU_LAYOUT.md`, `shaders/scene_access.glsl`, `shaders/debug_cast.frag`, and the render section of `src/main.cpp`.

**Don't edit** `shaders/scene_access.glsl` or `src/scene/gpu_scene.*`. That's 아미르's contract. Ask them for changes (e.g. new material fields). The BVH is currently a single leaf (brute force). 아미르 is replacing it with SAH, and the format won't change.

### OpenGL 4.1 limits (macOS), which CI enforces

There are no compute shaders, SSBOs, `imageStore` or `layout(binding=)`. Everything runs in a **fragment shader** over a fullscreen triangle (`shaders/fullscreen.vert`). Accumulation uses **ping-pong `GL_RGBA32F` FBO textures**. Each shader starts with `#version 410 core`, and you can `#include "scene_access.glsl"`. Run `python3 tools/check_shaders.py` (needs glslangValidator) before pushing.

## Your deliverables

### 1. Accumulation framework → `src/tracer/path_tracer.{h,cpp}`

- Two `GL_RGBA32F` textures + FBOs (ping-pong). Each frame: `new = mix(prev, sample, 1/(frame+1))`, then a display pass (tonemap + gamma).
- **Reset** accumulation when the camera moves (`camera.update()` returns true), an exhibit rotates, the light changes, or the window resizes.
- **Resolution scale slider (0.25–1.0).** This is essential: the debug caster already runs at about 10 fps at 2560×1440 on 아미르's Intel Iris Mac. Trace at low resolution and upscale.
- Samples-per-pixel-per-frame slider, max bounces slider, and a frame counter in ImGui.
- Add a `RenderMode::PathTrace` in `main.cpp` (keep the debug cast as a debug view).

### 2. Path tracer shader → `shaders/tracer.frag`

- RNG: PCG hash seeded by pixel + frame index.
- Camera ray with sub-pixel jitter (anti-aliasing for free).
- Bounce loop (start with 4 bounces, Russian roulette after 3).
- Materials from `rtmMaterial` (glTF metallic-roughness):
  - diffuse: cosine-weighted hemisphere
  - metal/rough: GGX microfacet (sample the VNDF)
  - **glass / refraction**: `transmission > 0`, Fresnel (Schlick) chooses reflect vs refract with `ior`. Use `s.frontFace` to know if you're entering or leaving. This is a proposal headline: the CRT screen and radio dial are good candidates.
- **Soft shadows**: sample a point on an *area* light (disk or quad at the spotlight position, spotlight cone falloff) with next-event estimation via any-hit `rtmIntersectScene(..., true)`. Also add `emission` from hit materials.
- Guard against NaN/Inf (`if (any(isnan(c))) c = vec3(0)`), or one bad sample poisons the accumulation forever.

### 3. Split-screen compare

Raster on the left half and tracer on the right, with a draggable divider (glScissor or a uniform in the display pass). It's in the proposal. Coordinate with 알리, because the raster side is theirs.

### 4. Lights must match raster

Raster and tracer have to light the scene identically, or the comparison is meaningless. Agree with 알리 on one shared light struct (e.g. `src/scene/lights.h`, an array of spotlights with position, direction, color, intensity, cone angles and radius) passed to both as uniforms. Today there's a single hard-coded `SpotLight` in `main.cpp`.

### 5. Textures in the tracer (after 하리스's loader lands)

`RtmMaterial.baseColorTex` is meant to be a layer in one `sampler2DArray`, with all textures resized to one size at load (e.g. 1024²). Use texture units 0–7; units 8–14 are scene data.

## Done when

- [ ] One key switches the gallery to the path tracer. The image converges (no fireflies after ~200 spp at a reduced resolution)
- [ ] It shows reflections, refraction (a glass test material) and soft shadows
- [ ] Moving resets cleanly. The resolution-scale slider works. It runs on macOS **and** Windows
- [ ] Split-screen works
- [ ] `tools/check_shaders.py` passes and CI is green

## Git

`git checkout -b hajik/path-tracer` → small commits → PR into `main` → CI green → merge.
