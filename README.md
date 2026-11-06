# Retro Tech Museum

First-person OpenGL gallery of retro devices, with a switchable raster PBR / GPU path-traced renderer.
Team CATDAWG — Computer Graphics, Fall 2026.

## Build

You only need **CMake ≥ 3.24** and a C++17 compiler. GLFW, GLM, ImGui and stb are downloaded automatically on the first configure (takes 1–2 minutes, needs internet).

### Windows (Visual Studio)

Easiest: **File → Open → Folder…**, pick this repo, choose the `windows` preset in the toolbar, set `museum.exe` as the startup item, then press F5.

Or from a terminal (Developer PowerShell):

```powershell
cmake --preset windows
cmake --build --preset windows          # Release
.\build\windows\Release\museum.exe
```

`cmake --build --preset windows-debug` builds a Debug version with `GL_CHECK` error reporting turned on.

### macOS

```bash
cmake --preset mac                      # or mac-debug
cmake --build --preset mac
./build/mac/museum
```

### Tests

```bash
ctest --preset mac        # or: ctest --preset windows   (CPU tests: layout, BVH traversal, gallery)
python3 tools/check_shaders.py   # GLSL 410 validation, needs glslangValidator
```

### Smoke test

```bash
museum --smoke-test out.png             # renders 30 frames, saves a screenshot, exits 0 if OK
```

## Controls

| Input | Action |
|---|---|
| Click view | capture mouse (look around) |
| Tab / Esc | release mouse (use ImGui) |
| WASD, Shift | move, sprint |
| F | toggle fly mode (Space / Ctrl = up / down) |
| T | toggle raster / ray cast |
| F5 | reload shaders from disk (no rebuild needed) |

## Rules (read this — it keeps the Mac build alive)

macOS stops at **OpenGL 4.1**. Windows drivers will happily run newer features that then **crash on macOS**. CI checks every PR on both OSes.

- Every shader starts with `#version 410 core`.
- **Not allowed (4.2+):** compute shaders, SSBOs, `imageLoad/Store`, `layout(binding = N)`, `glTexStorage*` (use `glTexImage2D`), `glDebugMessageCallback`, DSA (`glCreate*`, `glNamed*`). If GLAD doesn't have the function, you can't use it.
- Set sampler units in C++: `shader.set("uAlbedo", 0)`.
- Big scene data for the tracer goes in **float textures** (`GL_RGBA32F` + `texelFetch`). Max texture size on Mac is 16384, so wrap long arrays into 2D textures.
- Always use `window.framebufferSize()` for viewports and render targets (Retina = 2× window size).
- Load files with `rtm::shaderPath("x.frag")` / `rtm::assetPath("models/x.obj")`. Never use bare relative paths.
- Include `core/gl.h`, not `glad.h` or `glfw3.h` directly.
- Wrap suspicious GL calls with `GL_CHECK("what")`. It's free in Release.

## Workflow

```
your-name/feature  --PR-->  dev (integration, default branch)  --PR-->  main (production)
```

1. `git checkout dev && git pull`, then `git checkout -b <name>/<feature>` (e.g. `hajik/path-tracer`)
2. Commit small, push, open a PR **into `dev`**. CI must be green (Windows + macOS + GLSL check)
3. When `dev` is stable and demo-ready, 아미르 opens a PR `dev → main`. Only `dev` can merge into `main`, it needs 1 approval, and it uses a merge commit (not squash)
4. Nobody pushes directly to `dev` or `main`. Rulesets live in `.github/rulesets/` (apply with `bash tools/apply_rulesets.sh`)

## Layout

```
src/core/      window, shader, camera, GL helpers                    (아미르)
src/scene/     scene structs, BVH, collision                         (아미르)
src/geometry/  OBJ loader, QEM decimation                            (하리스)
src/tracer/    path tracer host code       shaders/tracer*           (하직)
src/raster/    PBR, shadow/normal maps, ImGui panels                  (알리)
shaders/       GLSL 410 (.vert / .frag)
assets/        models/ and textures/ — see docs/CREDITS.md
external/glad/ generated loader: OpenGL 4.1 core (don't edit)
```

The hallway layout (6 pedestals + arcade) lives in `src/scene/gallery.*`. Exhibits are placeholder boxes until the OBJ loader lands. See `docs/HANDOFF.md`.
