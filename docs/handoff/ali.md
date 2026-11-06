# Handoff — 알리: raster PBR, shadow + normal maps, ImGui, report

> **How to use:** paste this whole file into your AI assistant at the root of the cloned repo, then say "start with step 1".

## Context for the AI

Retro Tech Museum is a C++17 / OpenGL **4.1 core** app (CMake; Windows + macOS, both checked in CI). It's a first-person hallway with 7 retro exhibits and two render modes, raster PBR and a GPU path tracer, switched with one key.

What already exists and works:

- Window, FPS camera, ImGui, a shader loader with `#include`, F5 shader hot-reload (`src/core/`)
- Scene format (`src/scene/scene.h`): `Mesh` / `SubMesh` / `Material` (glTF metallic-roughness) / `Instance`
- The hallway (`src/scene/gallery.*`): 6 pedestals, 3 per side, and the arcade cabinet at the end. Exhibits are placeholder boxes until 하리스's OBJ loader lands.
- Placeholder raster: `drawRaster()` in `src/main.cpp` + `shaders/raster_basic.{vert,frag}` (Blinn-Phong, one spotlight, no textures). **You replace this.**
- `core/mesh_gpu.h`: VAO per mesh, with attributes 0 position, 1 normal, 2 uv, 3 tangent

Read first: `README.md` (**OpenGL 4.1 rules**), `src/scene/scene.h`, `src/main.cpp`, `shaders/raster_basic.*`.

**Don't edit** `src/core/`, `src/scene/gpu_scene.*` or `shaders/scene_access.glsl` (아미르's), or `shaders/tracer*` (하직's).

### OpenGL 4.1 limits (macOS), which CI enforces

No `layout(binding=)`: set sampler units in C++ with `shader.set("uAlbedo", 0)`. No `glTexStorage`, DSA or debug callbacks. Shaders start with `#version 410 core`. Use texture **units 0–7** (8–14 are reserved for tracer scene data). Always size viewports and FBOs with `window.framebufferSize()` (Retina is 2×). Run `python3 tools/check_shaders.py` before pushing.

## Your deliverables

### 1. Raster renderer → `src/raster/raster_renderer.{h,cpp}` + `shaders/pbr.{vert,frag}`

- Move `drawRaster()` out of `main.cpp` into a class. It draws `scene.instances`, with one draw per `SubMesh` and its `Material`.
- Texture cache: load `Scene::texturePaths` with stb_image (`#include <stb_image.h>`; the implementation is already compiled in `core/stb_impl.cpp`). Base colour in **sRGB** (`GL_SRGB8_ALPHA8`), everything else linear. Generate mipmaps.
- PBR: Cook-Torrance (GGX D, Smith G, Schlick F), metallic-roughness. Linear lighting, then tonemap (ACES or Reinhard) + gamma.
- The telephone model has an alpha texture, so use alpha test (discard).

### 2. Spotlights + shadow maps

- **Several spotlights**, ideally one per exhibit plus ceiling fill. One `SpotLight` in `main.cpp` lights the whole hallway today, and it looks flat.
- **Agree on one shared light struct with 하직** (e.g. `src/scene/lights.h`). The tracer must light the scene the same way or split-screen compare is meaningless.
- One shadow map per spotlight (perspective depth FBO, `GL_DEPTH_COMPONENT24`, `sampler2DShadow` with `GL_COMPARE_REF_TO_TEXTURE`), plus PCF for soft edges and slope-scaled bias. Watch the 16-units limit on Mac when you have many lights. An atlas or a `sampler2DArrayShadow` avoids it.

### 3. Normal maps

- TBN from `Vertex::tangent` (xyz + w handedness). 하리스's loader computes it.
- Poly Haven normal maps are **OpenGL convention** (`nor_gl`), so they need no green flip. Check the Sketchfab ones, which may be DirectX (flip G).

### 4. ImGui

Panels for render mode, light controls (per light: color, intensity, cone, shadow on/off), material overrides for testing, the exhibit list with rotation, and stats. Keep it in `src/raster/ui.*` or similar, and coordinate with whoever touches `main.cpp`.

### 5. Report

You own the write-up. Collect from each person: the technique, a short explanation, screenshots (`museum --smoke-test out.png` takes one), and performance numbers (fps for raster vs tracer, BVH vs brute force, QEM triangle counts). Asset credits are in `docs/CREDITS.md`.

Proposal mismatches to fix in the report:
- The phone model is a *telephone wall clock*, not a rotary phone.
- The original tube radio had no licence and is being replaced.
- The proposal said "7 objects, two spare", but no spares are listed.

## Done when

- [ ] Textured PBR exhibits with normal maps and per-spotlight soft shadows, on macOS **and** Windows
- [ ] Lights are shared with the tracer
- [ ] ImGui controls for lighting and rendering
- [ ] `tools/check_shaders.py` passes and CI is green
- [ ] Report drafted with everyone's sections

## Git

`git checkout -b ali/raster-pbr` → small commits → PR into `dev` (the default branch; never `main`) → CI green → merge.
