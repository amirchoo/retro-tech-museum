#pragma once
// CPU-side scene description. This is the CONTRACT between modules:
//   - 하리스's OBJ loader / QEM produce rtm::Mesh + rtm::Material
//   - 알리's raster renderer draws Scene::instances
//   - 아미르's BVH + GpuScene pack it for 하직's path tracer (see docs/GPU_LAYOUT.md)
//
// Units: metres, +Y up, right-handed, counter-clockwise front faces.
#include <cfloat>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace rtm {

struct AABB {
    glm::vec3 min{FLT_MAX};
    glm::vec3 max{-FLT_MAX};

    void grow(const glm::vec3& p) { min = glm::min(min, p); max = glm::max(max, p); }
    void grow(const AABB& b) { min = glm::min(min, b.min); max = glm::max(max, b.max); }
    bool valid() const { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }
    glm::vec3 extent() const { return max - min; }
    glm::vec3 center() const { return 0.5f * (min + max); }
    float surfaceArea() const {
        if (!valid()) return 0.0f;
        const glm::vec3 e = extent();
        return 2.0f * (e.x * e.y + e.y * e.z + e.z * e.x);
    }
    AABB transformed(const glm::mat4& m) const;  // AABB of the 8 transformed corners
};

struct Vertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f};
    glm::vec4 tangent{0.0f};  // xyz = tangent, w = bitangent sign (+1/-1). 0 = not computed.
};

// Metallic-roughness PBR (glTF-style), plus glass parameters for the tracer.
struct Material {
    std::string name;
    glm::vec3 baseColor{0.8f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    glm::vec3 emission{0.0f};      // linear RGB radiance; > 0 makes it a light in the tracer
    float ior = 1.5f;
    float transmission = 0.0f;     // 0 = opaque, 1 = clear glass (CRT screen, radio dial)
    int32_t baseColorTex = -1;     // index into Scene::texturePaths, -1 = none
    int32_t normalTex = -1;
    int32_t metalRoughTex = -1;    // glTF packing: G = roughness, B = metallic
};

// A contiguous index range drawn with one material.
struct SubMesh {
    uint32_t firstIndex = 0;  // into Mesh::indices
    uint32_t indexCount = 0;  // multiple of 3
    int32_t material = 0;     // into Scene::materials
};

// Geometry in OBJECT space. Shared by every Instance that references it.
struct Mesh {
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;  // triangle list
    std::vector<SubMesh> submeshes;
    AABB bounds;                    // object space; call computeBounds() after filling vertices

    void computeBounds();
    size_t triangleCount() const { return indices.size() / 3; }
};

struct Transform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    glm::mat4 matrix() const;  // object -> world
};

// One placed copy of a mesh. Rotating an exhibit = changing transform; no BLAS rebuild.
struct Instance {
    std::string name;
    int32_t mesh = 0;  // into Scene::meshes
    Transform transform;
    bool exhibit = false;  // true = visitor can rotate it (ImGui), false = architecture
};

struct Scene {
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<std::string> texturePaths;  // relative to assets/
    std::vector<Instance> instances;

    size_t triangleCount() const;  // triangles actually drawn (sum over instances)
};

}  // namespace rtm
