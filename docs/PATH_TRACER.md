# Progressive GPU path tracer

This change implements the path-tracer module assigned to 하직 in the proposal.
It builds on the existing scene textures and two-level traversal. No changes
are made to `scene_access.glsl`, `scene/scene.*`, `scene/gpu_scene.*`, or the
gallery layout/asset slots.

## What runs

- A fullscreen GLSL 410 fragment shader traces multiple surface interactions.
- Two RGBA32F textures/FBOs alternate read/write. They store a linear HDR
  sample-weighted mean; exposure, ACES approximation and gamma apply only in
  the display pass. The first sample never reads uninitialized history.
- PCG generates per-pixel/per-frame/per-sample randomness. Subpixel camera
  jitter antialiases silhouettes.
- Opaque materials use cosine-weighted diffuse and isotropic GGX visible-normal
  sampling. Their mixture PDF is used to weight the full metallic-roughness BRDF.
- Transmissive dielectrics use Schlick Fresnel, Snell refraction, total internal
  reflection, front/back IOR selection and radiance-mode eta-squared throughput.
  The glass diagnostic has both entry and exit surfaces.
- A disk emitter samples direct lighting with an any-hit shadow ray. Multiple
  importance sampling combines the light sample and BSDF hits on the disk;
  the last allowed vertex uses light sampling alone. Emissive mesh materials
  are included when hit by camera/continuation paths.
- Russian roulette starts after the third scattering event. Ray origins are
  offset to the correct side of the geometric surface. NaN/Inf samples are
  rejected before they enter the accumulated image.

## Controls and invalidation

| Control | Behavior |
|---|---|
| T | Raster / path tracing |
| Split comparison | Full-camera raster left, full-camera tracer right |
| Divider | Slider or drag the line with the mouse released |
| Debug ray cast | Original shaded, normal and instance-ID diagnostics |
| Trace resolution | 0.25–1.0 of the framebuffer size per dimension |
| Samples / frame | 1–16 new samples per pixel each frame |
| Max bounces | 1–16 surface vertices; default 4 |
| Indirect clamp | Default 3; limits rare bright secondary paths. 0 disables it |
| Exposure | Display-only; preserves accumulation |
| Reset accumulation | Discard history manually |
| F5 | Reload all shader programs and reset history |
| Light radius | 0 = point light; positive radius = disk / soft shadows |
| Material test scene | Separate procedural diagnostic; gallery is default |

The host detects changes to the camera/inverse view-projection, framebuffer
dimensions and every shared light field. `main.cpp` explicitly resets after
exhibit updates, scene replacement, mode changes and shader reloads. Settings
changes and texture-array replacement also reset. A zero-size framebuffer
suspends rendering. The counter stops at 2^24 samples to preserve exact float
weights; use Reset to restart.

## Integration with the other modules

`src/scene/lights.h` is the shared spotlight input. Position, target, linear
color, intensity, cone angles, disk radius and environment strength are passed
to both renderers. Direct illumination uses inverse-square attenuation and
the same cone/disk cosine. The raster baseline approximates the finite disk
as a point at its center. The tracer integrates the disk and indirect light;
the raster ambient term is an approximation, so the images need not match
pixel for pixel. Ali can replace the baseline raster pass with the final
shadow/normal-map implementation while preserving these inputs.

`PathTracer::setBaseColorTextures(array, layerCount)` accepts a loader-owned
`GL_TEXTURE_2D_ARRAY` on texture unit 2. Layers must follow
`Material.baseColorTex`, have common dimensions, and use an sRGB internal
format for automatic decoding to linear RGB. Negative/out-of-range indices
use the material color. Unit 0 is accumulation; units 8–14 retain their scene
bindings. Call `setBaseColorTextures` again after replacing/updating the array.
Call `tracer.reset()` after any new material upload, geometry/LOD replacement
or instance update performed outside the current UI.

The actual loader/texture-array construction and final device meshes are
Haris's handoff. SAH acceleration and collision remain Amir's handoff.
Their current single-leaf BVH is supported without changing its data contract.

## Validation

The normal build and the six existing CPU tests remain available. An optional
integration executable runs actual GL draws and reads back float pixels:

```powershell
cmake --preset windows -DRTM_BUILD_GPU_TESTS=ON
cmake --build --preset windows
ctest --preset windows
python tools/check_shaders.py
```

Use `mac` instead of `windows` for macOS. The optional tests return skip code
77 if no GLFW/OpenGL context is available. They check known HDR emission,
ping-pong accumulation, camera/projection/light/resize invalidation, SPP and
resolution controls, colored metal reflection, sRGB array sampling, two-sided
glass transmission, area-light penumbra, convergence to 200 spp, gallery and
material-scene rendering, split compositing, GL state restoration and reload.

Initial Linux verification used Mesa's OpenGL **4.1 core** context and GLSL
**4.10** on llvmpipe, with the same test body and an EGL context. All checks
passed with no GL errors. The soft-shadow scene's RGB squared error relative
to the 200-spp reference was **0.0128882 at 1 spp** and **0.000735912 at 16 spp**.
Those numbers describe this deterministic regression scene, not a guarantee
for every model or machine. Native Windows/macOS builds and interactive
performance must be checked on the team's actual machines/CI.

The full application, CPU test executable and optional GPU test executable also
built successfully through CMake in Debug mode with the pinned dependencies.
In this display-free Linux environment, validation supplied GLFW's null/EGL
context through a local link wrapper outside the deliverable source. The actual
application passed its path-tracing screenshot smoke test without GL errors.

## Current limits

- The default gallery still contains the team's placeholder boxes. The material
  test scene demonstrates glass/reflection independently of pending assets.
- One shared analytic spotlight is wired today. Per-exhibit lighting can extend
  that interface with the raster module.
- Glass is an ideal smooth dielectric; rough transmissive microfacets, nested
  overlapping media, volume absorption and refractive caustics are not included.
  Straight shadow rays treat glass as an occluder; transmission is sampled by
  continuation paths.
- Emissive meshes are hit-sampled, not explicitly sampled as area lights.
  Small bright meshes and difficult specular paths can remain noisy. The
  default indirect clamp suppresses fireflies at the cost of some lost bright
  indirect energy; set it to 0 for unclamped reference renders.
- Base-color texture consumption is ready. Normal maps/metal-roughness texture
  integration depend on the loader and shared format work.
- Split-screen compares the same full-camera image on both sides; moving the
  divider or changing exposure does not restart the tracer.

The complete team submission still needs final assets, the other assigned
modules, team-machine verification and the final report/presentation work.
