#pragma once
// Packs a Scene into RGBA32F data textures for the GLSL path tracer.
// Exact texel layout: docs/GPU_LAYOUT.md. GLSL side: shaders/scene_access.glsl.
// If you change anything here, change BOTH of those in the same PR.
#include <vector>

#include <glm/glm.hpp>

#include "core/gl.h"
#include "scene/scene.h"

namespace rtm {

namespace gpu {
constexpr int kTexWidth = 2048;  // every data texture is 2048 wide; texel i -> (i % W, i / W)

// Texels per record
constexpr int kTexelsPerNode = 2;
constexpr int kTexelsPerInstance = 4;
constexpr int kTexelsPerMaterial = 3;

// Texture units 8..14 are reserved for scene data (0..7 are free for material textures).
enum Unit : int {
    kUnitVertPos = 8,
    kUnitVertAttr = 9,
    kUnitTriangles = 10,
    kUnitBlasNodes = 11,
    kUnitTlasNodes = 12,
    kUnitInstances = 13,
    kUnitMaterials = 14,
};

// Integers are stored as plain float VALUES (not bit patterns), exact up to 2^24.
constexpr uint32_t kMaxIndex = 1u << 24;
}  // namespace gpu

// One BVH node, 2 texels. Shared by BLAS (leaf = triangles) and TLAS (leaf = instances).
//   count == 0 : interior, children at leftOrFirst and leftOrFirst + 1 (absolute node indices)
//   count  > 0 : leaf, items [leftOrFirst, leftOrFirst + count) (absolute triangle / instance indices)
struct BvhNode {
    AABB bounds;
    uint32_t leftOrFirst = 0;
    uint32_t count = 0;
};

// Where each mesh ended up in the global arrays.
struct GpuMeshRange {
    uint32_t vertexOffset = 0;
    uint32_t triFirst = 0;
    uint32_t triCount = 0;
    uint32_t blasRoot = 0;
};

// CPU-side packed arrays, exactly what gets uploaded. Pure data, no GL -> unit-testable.
struct GpuSceneData {
    std::vector<glm::vec4> vertPos;    // (position.xyz, uv.x)
    std::vector<glm::vec4> vertAttr;   // (normal.xyz,   uv.y)
    std::vector<glm::vec4> triangles;  // (i0, i1, i2, materialId)   absolute vertex indices
    std::vector<glm::vec4> blasNodes;  // BvhNode x2 texels
    std::vector<glm::vec4> tlasNodes;  // BvhNode x2 texels
    std::vector<glm::vec4> instances;  // 4 texels each, in TLAS leaf order
    std::vector<glm::vec4> materials;  // 3 texels each
    std::vector<GpuMeshRange> meshRanges;
};

// Builds the BLAS for one mesh. Appends nodes to `out` (absolute indices) and may
// reorder `triangles[triFirst, triFirst + triCount)`. Returns the root node index.
//
// TODO(아미르): binned SAH build. Right now this emits a single leaf (= brute force),
// which is a valid BVH, so traversal and the tracer work end-to-end already.
uint32_t buildBlas(std::vector<BvhNode>& out, std::vector<glm::vec4>& triangles,
                   const std::vector<glm::vec4>& vertPos, uint32_t triFirst, uint32_t triCount);

// Static part: geometry, BLASes, materials. Run once after loading.
GpuSceneData packScene(const Scene& scene);
// Dynamic part: instance transforms + TLAS. Cheap; run whenever an instance moves.
void packInstances(const Scene& scene, GpuSceneData& data);

// GL textures holding GpuSceneData.
class GpuScene {
public:
    GpuScene() = default;
    ~GpuScene();
    GpuScene(const GpuScene&) = delete;
    GpuScene& operator=(const GpuScene&) = delete;

    void upload(const Scene& scene);           // full upload
    void updateInstances(const Scene& scene);  // re-pack + re-upload instances and TLAS only
    // Binds textures to units 8..14 and sets the sampler/count uniforms used by scene_access.glsl.
    void bind(GLuint program) const;

    const GpuSceneData& data() const { return data_; }

private:
    enum { kVertPos, kVertAttr, kTriangles, kBlas, kTlas, kInstances, kMaterials, kCount };
    void uploadTexture(int slot, const std::vector<glm::vec4>& texels);

    GpuSceneData data_;
    GLuint tex_[kCount] = {};
};

}  // namespace rtm
