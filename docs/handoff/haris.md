# Handoff — 하리스: OBJ loader, assets, QEM decimation

> **How to use:** paste this whole file into your AI assistant (Claude Code, Cursor, Copilot…) at the root of the cloned repo, then say "start with step 1".

## Context for the AI

Retro Tech Museum is a C++17 / OpenGL **4.1 core** app, built with CMake. Windows (MSVC) and macOS both have to work, and CI checks both on every PR. The codebase already has:

- a window, an FPS camera, ImGui, and a shader loader with `#include` support (`src/core/`)
- the CPU scene format (`src/scene/scene.h`), which is **your output target**
- a hallway with 7 exhibit slots that currently show placeholder boxes (`src/scene/gallery.*`)
- GPU packing + two-level BVH for the path tracer (`src/scene/gpu_scene.*`, `docs/GPU_LAYOUT.md`)
- CPU tests (`tests/`, run with `ctest --preset windows`)

Read these first: `README.md` (build steps + OpenGL 4.1 rules), `src/scene/scene.h`, `src/scene/gallery.h`, `docs/GPU_LAYOUT.md` section 1, `docs/CREDITS.md`.

**Don't edit** `src/core/`, `src/scene/gpu_scene.*`, `shaders/scene_access.glsl` or `external/`. Those are 아미르's. If you need a change there, ask them.

## Your deliverables

### 1. Asset conversion → `assets/models/<name>/`

Target paths are fixed in `kExhibits` (`src/scene/gallery.cpp`): `models/camera/camera.obj`, `models/telephone/telephone.obj`, `models/television/television.obj`, `models/typewriter/typewriter.obj`, `models/boombox/boombox.obj`, `models/radio/radio.obj`, `models/arcade/arcade.obj`.

Source files: ask 아미르 for the "Retro Object" folder, or download from the links in `docs/CREDITS.md`.

| Model | Source format | Gotchas |
|---|---|---|
| Camera, TV, telephone, boombox (Poly Haven) | `.blend` + 4K textures | Several maps are **.exr** (TV, boombox, telephone). **stb_image can't read EXR.** Re-download the 1K/2K **JPG/PNG** versions from Poly Haven, or bake to PNG in Blender |
| Arcade, radio (Sketchfab) | `.dae` inside a nested `source/model.zip` | Blender 4.5 imports Collada. **Blender 5 removed it**, so use 4.5 LTS. The arcade normal map is 25 MB PNG, so downsize it |
| Typewriter (Sketchfab) | zip inside a zip | The texture folder is messy (random screenshots). Keep only what the material actually uses |

Rules:

- Export OBJ + MTL from Blender: **Y up, −Z forward**, triangulated, normals + UVs on, scale in metres.
- Textures: **1K, at most 2K**, PNG/JPG only, stored next to the OBJ. Use relative paths in the MTL.
- Keep the whole `assets/` folder under ~150 MB.
- Ask 아미르 to enable **Git LFS** (see `.gitattributes`) *before* you commit any asset.
- **Licences:** the "Tube radio" in the proposal has *no licence* and can't be used. The downloaded `vintage-radio.zip` has no licence file either, so check its Sketchfab page before committing. Update `docs/CREDITS.md` for anything you change.
- The proposal says "rotary phone", but the model is a *telephone wall clock*. Tell 알리 (who writes the report) which one you're using.

### 2. OBJ/MTL loader → `src/geometry/obj_loader.{h,cpp}`

Write your own (no Assimp; that's in the proposal). Suggested API:

```cpp
namespace rtm {
// Appends meshes/materials/texture paths to `scene`. Returns the mesh index, or -1 on failure.
int loadObj(Scene& scene, const std::string& assetRelativePath);
}
```

- Output exactly `rtm::Mesh` / `rtm::Material` / `rtm::SubMesh` from `scene.h`. One `usemtl` group = one `SubMesh`.
- Handle `v/vt/vn` faces, negative indices, polygons (fan-triangulate), missing normals (compute them) and missing UVs.
- Deduplicate vertices on the (v, vt, vn) triple.
- **Compute tangents** (`Vertex::tangent`, w = handedness). 알리 needs them for normal maps.
- MTL → `Material`: `Kd`→baseColor, `map_Kd`→baseColorTex, `map_Bump`/`norm`→normalTex, `Pr`/`map_Pr`→roughness, `Pm`/`map_Pm`→metallic, `Ke`→emission, `Ni`→ior, `d`/`Tr`→transmission. Texture paths go into `Scene::texturePaths`, relative to `assets/`.
- Use `rtm::assetPath()` for files. Never use bare relative paths.
- Call `mesh.computeBounds()`.

**Hook it up:** in `makeGallery()`, each slot has a `TODO(하리스)` placeholder. Replace it with `loadObj(scene, slot.objPath)` and then set the instance transform to `fitToSlot(mesh.bounds, slot)`. That already places the model on its pedestal at the right scale and facing (it's tested). Keep the placeholder as a fallback if loading fails.

### 3. QEM decimation → `src/geometry/qem.{h,cpp}`

Garland–Heckbert quadric error metrics, your own implementation:

```cpp
Mesh decimate(const Mesh& in, float targetRatio);   // e.g. 0.25 = keep 25 % of triangles
```

- Per-vertex quadrics from face planes, plus boundary-edge penalty planes so the silhouette and open edges survive.
- Edge-collapse priority queue, optimal position (solve the 3×3; fall back to the edge midpoint).
- Keep UVs/normals sane: don't collapse across UV seams or material boundaries (or interpolate attributes).
- Use it to make a tracer LOD for each exhibit (the tracer gets slow above ~100k triangles on a laptop iGPU). Add an ImGui triangle count + ratio slider for the demo.

### 4. Tests (`tests/`)

Add tests in the style of `tests/test_gpu_layout.cpp` (it uses the tiny `TEST`/`CHECK` macros from `tests/test.h`, and new files go in `CMakeLists.txt` → `rtm_tests`):

- loader: a small hand-written OBJ string (quad + negative indices + missing normals) → correct counts and indices
- QEM: decimating a sphere to 25 % keeps it closed, and its bounds stay within ~5 %

## Done when

- [ ] All 7 models load in the hallway, textured (once 알리's raster lands) and placed correctly
- [ ] `ctest` passes on Windows; CI is green on the PR
- [ ] The QEM LOD is selectable and its triangle count is shown in ImGui
- [ ] `docs/CREDITS.md` is accurate

## Git

`git checkout -b haris/obj-loader` → small commits → PR into `dev` (the default branch; never `main`) → CI green → merge. Don't commit `build/`, and don't commit 4K textures.
