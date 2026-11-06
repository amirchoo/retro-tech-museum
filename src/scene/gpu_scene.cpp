#include "scene/gpu_scene.h"

#include <algorithm>
#include <cassert>
#include <cstdio>

namespace rtm {

namespace {

float asF(uint32_t i) {
    assert(i < gpu::kMaxIndex && "index too large to store exactly in a float texel");
    return static_cast<float>(i);
}

void appendNodes(std::vector<glm::vec4>& texels, const std::vector<BvhNode>& nodes) {
    for (const BvhNode& n : nodes) {
        texels.emplace_back(n.bounds.min, asF(n.leftOrFirst));
        texels.emplace_back(n.bounds.max, asF(n.count));
    }
}

glm::vec3 triVertex(const std::vector<glm::vec4>& tris, const std::vector<glm::vec4>& pos, uint32_t t, int k) {
    return glm::vec3(pos[static_cast<size_t>(tris[t][k])]);
}

}  // namespace

uint32_t buildBlas(std::vector<BvhNode>& out, std::vector<glm::vec4>& triangles,
                   const std::vector<glm::vec4>& vertPos, uint32_t triFirst, uint32_t triCount) {
    BvhNode root;
    for (uint32_t t = triFirst; t < triFirst + triCount; ++t)
        for (int k = 0; k < 3; ++k) root.bounds.grow(triVertex(triangles, vertPos, t, k));
    root.leftOrFirst = triFirst;
    root.count = triCount;
    out.push_back(root);
    return static_cast<uint32_t>(out.size() - 1);
}

GpuSceneData packScene(const Scene& scene) {
    GpuSceneData d;
    std::vector<BvhNode> blas;

    for (const Mesh& mesh : scene.meshes) {
        GpuMeshRange r;
        r.vertexOffset = static_cast<uint32_t>(d.vertPos.size());
        r.triFirst = static_cast<uint32_t>(d.triangles.size());

        for (const Vertex& v : mesh.vertices) {
            d.vertPos.emplace_back(v.position, v.uv.x);
            d.vertAttr.emplace_back(v.normal, v.uv.y);
        }
        for (const SubMesh& sm : mesh.submeshes) {
            for (uint32_t i = sm.firstIndex; i + 2 < sm.firstIndex + sm.indexCount; i += 3) {
                d.triangles.emplace_back(asF(r.vertexOffset + mesh.indices[i]),
                                         asF(r.vertexOffset + mesh.indices[i + 1]),
                                         asF(r.vertexOffset + mesh.indices[i + 2]),
                                         asF(static_cast<uint32_t>(sm.material)));
            }
        }
        r.triCount = static_cast<uint32_t>(d.triangles.size()) - r.triFirst;
        r.blasRoot = buildBlas(blas, d.triangles, d.vertPos, r.triFirst, r.triCount);
        d.meshRanges.push_back(r);
    }
    appendNodes(d.blasNodes, blas);

    for (const Material& m : scene.materials) {
        d.materials.emplace_back(m.baseColor, m.metallic);
        d.materials.emplace_back(m.emission, m.roughness);
        d.materials.emplace_back(m.ior, m.transmission, static_cast<float>(m.baseColorTex),
                                 static_cast<float>(m.normalTex));
    }

    packInstances(scene, d);
    return d;
}

void packInstances(const Scene& scene, GpuSceneData& d) {
    d.instances.clear();
    d.tlasNodes.clear();

    // TLAS over instance world AABBs. Few instances (~10), so a single leaf is
    // genuinely fine for now; swap for a small SAH/median build if we ever get many.
    BvhNode root;
    for (uint32_t i = 0; i < scene.instances.size(); ++i) {
        const Instance& inst = scene.instances[i];
        const GpuMeshRange& r = d.meshRanges[static_cast<size_t>(inst.mesh)];
        const glm::mat4 o2w = inst.transform.matrix();
        const glm::mat4 w2o = glm::inverse(o2w);
        root.bounds.grow(scene.meshes[static_cast<size_t>(inst.mesh)].bounds.transformed(o2w));

        // Rows of world->object (GLM is column-major: row k = (m[0][k], m[1][k], m[2][k], m[3][k])).
        for (int k = 0; k < 3; ++k) d.instances.emplace_back(w2o[0][k], w2o[1][k], w2o[2][k], w2o[3][k]);
        d.instances.emplace_back(asF(r.blasRoot), asF(r.triFirst), asF(r.triCount), asF(i));
    }
    root.leftOrFirst = 0;
    root.count = static_cast<uint32_t>(scene.instances.size());
    appendNodes(d.tlasNodes, {root});
}

// ---------------------------------------------------------------------------

GpuScene::~GpuScene() { glDeleteTextures(kCount, tex_); }

void GpuScene::uploadTexture(int slot, const std::vector<glm::vec4>& texels) {
    if (!tex_[slot]) glGenTextures(1, &tex_[slot]);
    const int w = gpu::kTexWidth;
    const int h = std::max(1, static_cast<int>((texels.size() + w - 1) / w));
    std::vector<glm::vec4> padded(static_cast<size_t>(w) * h, glm::vec4(0.0f));
    std::copy(texels.begin(), texels.end(), padded.begin());

    glBindTexture(GL_TEXTURE_2D, tex_[slot]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, padded.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    GL_CHECK("GpuScene::uploadTexture");
}

void GpuScene::upload(const Scene& scene) {
    data_ = packScene(scene);
    uploadTexture(kVertPos, data_.vertPos);
    uploadTexture(kVertAttr, data_.vertAttr);
    uploadTexture(kTriangles, data_.triangles);
    uploadTexture(kBlas, data_.blasNodes);
    uploadTexture(kTlas, data_.tlasNodes);
    uploadTexture(kInstances, data_.instances);
    uploadTexture(kMaterials, data_.materials);

    const size_t bytes = 16 * (data_.vertPos.size() + data_.vertAttr.size() + data_.triangles.size() +
                               data_.blasNodes.size() + data_.tlasNodes.size() + data_.instances.size() +
                               data_.materials.size());
    std::printf("[GpuScene] %zu verts, %zu tris, %zu BLAS nodes, %zu instances, %.2f MB\n",
                data_.vertPos.size(), data_.triangles.size(), data_.blasNodes.size() / 2,
                data_.instances.size() / gpu::kTexelsPerInstance, static_cast<double>(bytes) / (1024.0 * 1024.0));
}

void GpuScene::updateInstances(const Scene& scene) {
    packInstances(scene, data_);
    uploadTexture(kTlas, data_.tlasNodes);
    uploadTexture(kInstances, data_.instances);
}

void GpuScene::bind(GLuint program) const {
    static const struct { const char* name; int slot; int unit; } kBindings[] = {
        {"uVertPos", kVertPos, gpu::kUnitVertPos},       {"uVertAttr", kVertAttr, gpu::kUnitVertAttr},
        {"uTriangles", kTriangles, gpu::kUnitTriangles}, {"uBlasNodes", kBlas, gpu::kUnitBlasNodes},
        {"uTlasNodes", kTlas, gpu::kUnitTlasNodes},      {"uInstances", kInstances, gpu::kUnitInstances},
        {"uMaterials", kMaterials, gpu::kUnitMaterials},
    };
    for (const auto& b : kBindings) {
        glActiveTexture(GL_TEXTURE0 + b.unit);
        glBindTexture(GL_TEXTURE_2D, tex_[b.slot]);
        glUniform1i(glGetUniformLocation(program, b.name), b.unit);
    }
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(program, "uTlasRoot"), 0);
    GL_CHECK("GpuScene::bind");
}

}  // namespace rtm
