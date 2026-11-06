# Scene data contract

Owner: 아미르. Change this file, `src/scene/gpu_scene.*` and `shaders/scene_access.glsl` **in the same PR**, or the tracer breaks.

## 1. CPU scene (`src/scene/scene.h`) — for 하리스 and 알리

```
Scene
 ├─ meshes[]      Mesh: vertices[], indices[] (triangle list), submeshes[], bounds   ← OBJECT space
 ├─ materials[]   Material: baseColor, metallic, roughness, emission, ior, transmission, *Tex
 ├─ texturePaths[] relative to assets/
 └─ instances[]   Instance: mesh index + Transform (position, quat rotation, scale)
```

- Units are metres, +Y is up, right-handed, CCW front faces.
- A `Mesh` is in object space. Put the origin at the **bottom centre** of the exhibit, so it sits on a plinth at `position.y = plinth height`.
- One OBJ `usemtl` group becomes one `SubMesh`. `SubMesh::material` indexes `Scene::materials`.
- Fill `Vertex::tangent` if you can (it's needed for normal maps in raster). The tracer ignores it.
- Call `mesh.computeBounds()` after filling vertices.
- **QEM LODs:** give each LOD its own `Mesh`. Point an instance at whichever LOD you want traced.

## 2. Two-level BVH

```
TLAS (world space, over instances)  ── leaf ──> instance i ── worldToObject ──> BLAS of mesh (object space) ── leaf ──> triangles
```

- **BLAS:** one per mesh, built once at load (`buildBlas`). Rotating or moving an exhibit never rebuilds it.
- **TLAS:** over instance world AABBs, rebuilt by `GpuScene::updateInstances()` whenever a transform changes (about 10 instances, so effectively free).
- The ray goes into object space by multiplying with world→object. The direction is **not** renormalised, so `t` stays the same in both spaces.

Current state: both trees are a **single leaf** (brute force). That's a valid BVH, so everything works end to end. The SAH builder will replace `buildBlas` without changing this format. `tests/test_gpu_layout.cpp` checks traversal against brute force and must keep passing.

## 3. GPU textures

All textures are `GL_RGBA32F`, `GL_NEAREST`, 2048 texels wide. Record `i` is fetched with `texelFetch(tex, ivec2(i % 2048, i / 2048), 0)` (`rtmFetch` in GLSL).
Integers are stored as **float values** (`float(i)`, read back with `int(x)`), not bit patterns. They're exact up to 2^24 = 16.7 M.

| Uniform | Unit | Texels / record | Layout |
|---|---|---|---|
| `uVertPos` | 8 | 1 / vertex | `(pos.xyz, uv.x)` object space |
| `uVertAttr` | 9 | 1 / vertex | `(normal.xyz, uv.y)` object space |
| `uTriangles` | 10 | 1 / triangle | `(i0, i1, i2, materialId)`, absolute vertex indices |
| `uBlasNodes` | 11 | 2 / node | see node format, all meshes concatenated |
| `uTlasNodes` | 12 | 2 / node | see node format, root = `uTlasRoot` (0) |
| `uInstances` | 13 | 4 / instance | rows 0–2 of world→object (`xyz` + translation in `w`), then `(blasRoot, triFirst, triCount, sceneInstanceIndex)` |
| `uMaterials` | 14 | 3 / material | `(baseColor.rgb, metallic)`, `(emission.rgb, roughness)`, `(ior, transmission, baseColorTex, normalTex)` |

Texture units 0–7 are free for material/raster textures. 15 is free.

**BVH node (2 texels):**

```
texel 0: (bounds.min.xyz, leftOrFirst)
texel 1: (bounds.max.xyz, count)
count == 0 → interior: children are nodes leftOrFirst and leftOrFirst + 1 (absolute)
count  > 0 → leaf: items [leftOrFirst, leftOrFirst + count) (absolute triangle / instance index)
```

**Normals to world:** `mat3(row0.xyz, row1.xyz, row2.xyz) * n` = transpose(world→object) · n, which is correct even under non-uniform scale.

## 4. GLSL API (`shaders/scene_access.glsl`) — for 하직

```glsl
#version 410 core
#include "scene_access.glsl"   // our loader expands this; CI too

RtmHit hit = rtmIntersectScene(origin, dir, RTM_INF, false);   // closest hit
if (hit.t < RTM_INF) {
    RtmSurface s = rtmSurface(origin, dir, hit);   // position, normal, geomNormal, uv, material, frontFace
    RtmMaterial m = rtmMaterial(s.material);
}
RtmHit sh = rtmIntersectScene(p + s.geomNormal * 1e-3, L, distToLight, true);  // any-hit (shadow rays)
bool occluded = sh.t < RTM_INF;
```

C++ side: `gpuScene.bind(shader.id())` binds every texture and sampler uniform. `shaders/debug_cast.frag` is the minimal working example (primary ray + shadow ray), so start the path tracer from there.

**Tracer textures (TODO, 하직 + 하리스):** `baseColorTex` is meant as a layer in one `sampler2DArray`. Resize every texture to a single size (e.g. 1024²) at load, because arrays need equal sizes.
