# Team handoffs

Each teammate pastes their own file into their AI assistant from the repo root:

| Who | File | Scope |
|---|---|---|
| 아미르 | — | setup, camera, scene format, GPU layout, BVH (SAH next), collision |
| 하리스 | [handoff/haris.md](handoff/haris.md) | asset conversion, OBJ/MTL loader, QEM decimation |
| 하직 | [handoff/hajik.md](handoff/hajik.md) | path tracer: accumulation, materials, refraction, soft shadows, split-screen |
| 알리 | [handoff/ali.md](handoff/ali.md) | raster PBR, shadow + normal maps, lights, ImGui, report |

## Shared interfaces (talk before you change them)

| Interface | File | Owner | Used by |
|---|---|---|---|
| Scene format | `src/scene/scene.h` | 아미르 | everyone |
| Exhibit slots / hallway | `src/scene/gallery.*` | 아미르 | 하리스 (loading), 알리 (lights) |
| GPU scene layout + traversal | `docs/GPU_LAYOUT.md`, `shaders/scene_access.glsl` | 아미르 | 하직 |
| Light struct (to create) | `src/scene/lights.h` | 알리 + 하직 | raster + tracer |

## Dependency order

```
하리스 loader ──> real models in hallway ──> 하리스 QEM LODs ──> tracer stays interactive
아미르 SAH BVH ──────────────────────────────────────────────> tracer fast on real meshes
알리 + 하직 light struct ──> raster shadows + tracer NEE ──> split-screen compare
```

Nobody is blocked today. Placeholders and brute-force traversal work, so everyone can build against them now.
