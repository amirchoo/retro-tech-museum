// Tests for the GPU scene layout + two-level BVH, entirely on the CPU.
// traceGpuData() is a line-by-line C++ replica of shaders/scene_access.glsl, reading the
// exact arrays that get uploaded. If it agrees with brute force, the GLSL will too
// (modulo float precision) -- and any future BVH builder is validated by the same test.
#include <algorithm>
#include <cmath>
#include <random>
#include <set>

#include <glm/gtc/quaternion.hpp>

#include "scene/gpu_scene.h"
#include "scene/gallery.h"
#include "test.h"

using namespace rtm;

namespace {

constexpr float kInf = 1e30f;

struct Hit { float t = kInf; int tri = -1; int instance = -1; };

float intersectAABB(glm::vec3 o, glm::vec3 invD, glm::vec3 bmin, glm::vec3 bmax, float tMax) {
    const glm::vec3 t0 = (bmin - o) * invD, t1 = (bmax - o) * invD;
    const glm::vec3 tmin = glm::min(t0, t1), tmax = glm::max(t0, t1);
    const float tn = std::max(std::max(tmin.x, tmin.y), std::max(tmin.z, 0.0f));
    const float tf = std::min(std::min(tmax.x, tmax.y), std::min(tmax.z, tMax));
    return tn <= tf ? tn : kInf;
}

bool intersectTri(glm::vec3 o, glm::vec3 d, glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, float tMax, float& t) {
    const glm::vec3 e1 = p1 - p0, e2 = p2 - p0, pv = glm::cross(d, e2);
    const float det = glm::dot(e1, pv);
    if (std::abs(det) < 1e-12f) return false;
    const float inv = 1.0f / det;
    const glm::vec3 tv = o - p0;
    const float u = glm::dot(tv, pv) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    const glm::vec3 qv = glm::cross(tv, e1);
    const float v = glm::dot(d, qv) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    t = glm::dot(e2, qv) * inv;
    return t > 1e-6f && t < tMax;
}

glm::vec3 safeInv(glm::vec3 d) {
    const float e = 1e-12f;
    return 1.0f / glm::vec3(std::abs(d.x) > e ? d.x : e, std::abs(d.y) > e ? d.y : e, std::abs(d.z) > e ? d.z : e);
}

int I(float f) { return static_cast<int>(f); }

// Generic stack traversal over a node array; leafFn(first, count) handles leaves.
template <class LeafFn>
void traverse(const std::vector<glm::vec4>& nodes, int root, glm::vec3 o, glm::vec3 d, const Hit& hit, LeafFn leafFn) {
    const glm::vec3 invD = safeInv(d);
    if (intersectAABB(o, invD, nodes[root * 2], nodes[root * 2 + 1], hit.t) == kInf) return;
    int stack[32], sp = 0, node = root;
    while (true) {
        const glm::vec4 a = nodes[node * 2], b = nodes[node * 2 + 1];
        const int first = I(a.w), count = I(b.w);
        if (count > 0) {
            leafFn(first, count);
        } else {
            int l = first, r = first + 1;
            float tl = intersectAABB(o, invD, nodes[l * 2], nodes[l * 2 + 1], hit.t);
            float tr = intersectAABB(o, invD, nodes[r * 2], nodes[r * 2 + 1], hit.t);
            if (tl > tr) { std::swap(tl, tr); std::swap(l, r); }
            if (tl < kInf) {
                if (tr < kInf && sp < 32) stack[sp++] = r;
                node = l;
                continue;
            }
        }
        if (sp == 0) break;
        node = stack[--sp];
    }
}

Hit traceGpuData(const GpuSceneData& g, glm::vec3 o, glm::vec3 d) {
    Hit hit;
    traverse(g.tlasNodes, 0, o, d, hit, [&](int first, int count) {
        for (int i = first; i < first + count; ++i) {
            const glm::vec4 r0 = g.instances[i * 4], r1 = g.instances[i * 4 + 1], r2 = g.instances[i * 4 + 2];
            const glm::vec4 info = g.instances[i * 4 + 3];
            const glm::vec3 oo(glm::dot(glm::vec3(r0), o) + r0.w, glm::dot(glm::vec3(r1), o) + r1.w, glm::dot(glm::vec3(r2), o) + r2.w);
            const glm::vec3 od(glm::dot(glm::vec3(r0), d), glm::dot(glm::vec3(r1), d), glm::dot(glm::vec3(r2), d));
            traverse(g.blasNodes, I(info.x), oo, od, hit, [&](int tf, int tc) {
                for (int t = tf; t < tf + tc; ++t) {
                    const glm::vec4 tri = g.triangles[t];
                    float tt;
                    if (intersectTri(oo, od, g.vertPos[I(tri.x)], g.vertPos[I(tri.y)], g.vertPos[I(tri.z)], hit.t, tt)) {
                        hit.t = tt; hit.tri = t; hit.instance = i;
                    }
                }
            });
        }
    });
    return hit;
}

// Ground truth: every triangle of every instance, transformed to world space.
Hit traceBruteForce(const Scene& s, glm::vec3 o, glm::vec3 d) {
    Hit hit;
    for (size_t i = 0; i < s.instances.size(); ++i) {
        const glm::mat4 m = s.instances[i].transform.matrix();
        const Mesh& mesh = s.meshes[static_cast<size_t>(s.instances[i].mesh)];
        for (size_t k = 0; k + 2 < mesh.indices.size(); k += 3) {
            const glm::vec3 p0 = m * glm::vec4(mesh.vertices[mesh.indices[k]].position, 1.0f);
            const glm::vec3 p1 = m * glm::vec4(mesh.vertices[mesh.indices[k + 1]].position, 1.0f);
            const glm::vec3 p2 = m * glm::vec4(mesh.vertices[mesh.indices[k + 2]].position, 1.0f);
            float t;
            if (intersectTri(o, d, p0, p1, p2, hit.t, t)) { hit.t = t; hit.instance = static_cast<int>(i); }
        }
    }
    return hit;
}

Scene rotatedTestScene() {
    Scene s = makeGallery();
    // Non-trivial transforms: arbitrary axis rotation + non-uniform scale on an exhibit.
    s.instances[2].transform.rotation = glm::angleAxis(0.7f, glm::normalize(glm::vec3(1, 2, 0.5f)));
    s.instances[2].transform.scale = {1.5f, 0.8f, 1.2f};
    return s;
}

// Checks a BVH node array: bounds contain children / items, every item referenced exactly once.
template <class ItemBounds>
bool validateBvh(const std::vector<glm::vec4>& nodes, int root, uint32_t itemFirst, uint32_t itemCount, ItemBounds itemBounds) {
    std::multiset<uint32_t> seen;
    bool ok = true;
    std::vector<int> stack{root};
    auto contains = [](const AABB& outer, const AABB& inner) {
        const float e = 1e-4f;
        return glm::all(glm::lessThanEqual(outer.min - e, inner.min)) && glm::all(glm::greaterThanEqual(outer.max + e, inner.max));
    };
    while (!stack.empty()) {
        const int n = stack.back();
        stack.pop_back();
        const AABB nb{glm::vec3(nodes[n * 2]), glm::vec3(nodes[n * 2 + 1])};
        const uint32_t first = static_cast<uint32_t>(nodes[n * 2].w), count = static_cast<uint32_t>(nodes[n * 2 + 1].w);
        if (count > 0) {
            for (uint32_t i = first; i < first + count; ++i) {
                seen.insert(i);
                ok &= contains(nb, itemBounds(i));
            }
        } else {
            for (int c = 0; c < 2; ++c) {
                const int child = static_cast<int>(first) + c;
                ok &= contains(nb, AABB{glm::vec3(nodes[child * 2]), glm::vec3(nodes[child * 2 + 1])});
                stack.push_back(child);
            }
        }
    }
    ok &= seen.size() == itemCount;
    for (uint32_t i = itemFirst; i < itemFirst + itemCount; ++i) ok &= seen.count(i) == 1;
    return ok;
}

}  // namespace

TEST(pack_ranges_and_indices) {
    const Scene s = makeGallery();
    const GpuSceneData g = packScene(s);

    size_t verts = 0, tris = 0;
    for (const Mesh& m : s.meshes) { verts += m.vertices.size(); tris += m.triangleCount(); }
    CHECK(g.vertPos.size() == verts);
    CHECK(g.vertAttr.size() == verts);
    CHECK(g.triangles.size() == tris);
    CHECK(g.meshRanges.size() == s.meshes.size());
    CHECK(g.instances.size() == s.instances.size() * gpu::kTexelsPerInstance);
    CHECK(g.materials.size() == s.materials.size() * gpu::kTexelsPerMaterial);

    for (size_t mi = 0; mi < s.meshes.size(); ++mi) {
        const GpuMeshRange& r = g.meshRanges[mi];
        const uint32_t vEnd = r.vertexOffset + static_cast<uint32_t>(s.meshes[mi].vertices.size());
        for (uint32_t t = r.triFirst; t < r.triFirst + r.triCount; ++t) {
            for (int k = 0; k < 3; ++k) {
                const uint32_t v = static_cast<uint32_t>(g.triangles[t][k]);
                CHECK(v >= r.vertexOffset && v < vEnd);  // triangles never point into another mesh
            }
            const int mat = static_cast<int>(g.triangles[t].w);
            CHECK(mat >= 0 && mat < static_cast<int>(s.materials.size()));
        }
    }
}

TEST(instance_rows_are_world_to_object) {
    const Scene s = rotatedTestScene();
    const GpuSceneData g = packScene(s);
    for (size_t i = 0; i < s.instances.size(); ++i) {
        const glm::mat4 o2w = s.instances[i].transform.matrix();
        const glm::vec3 pObj(0.3f, -0.2f, 0.9f);
        const glm::vec3 pWorld = o2w * glm::vec4(pObj, 1.0f);
        const glm::vec4 r0 = g.instances[i * 4], r1 = g.instances[i * 4 + 1], r2 = g.instances[i * 4 + 2];
        const glm::vec3 back(glm::dot(glm::vec3(r0), pWorld) + r0.w, glm::dot(glm::vec3(r1), pWorld) + r1.w,
                             glm::dot(glm::vec3(r2), pWorld) + r2.w);
        CHECK(glm::length(back - pObj) < 1e-4f);
    }
}

TEST(bvh_invariants) {
    const Scene s = rotatedTestScene();
    const GpuSceneData g = packScene(s);
    for (const GpuMeshRange& r : g.meshRanges) {
        CHECK(validateBvh(g.blasNodes, static_cast<int>(r.blasRoot), r.triFirst, r.triCount, [&](uint32_t t) {
            AABB b;
            for (int k = 0; k < 3; ++k) b.grow(glm::vec3(g.vertPos[static_cast<size_t>(g.triangles[t][k])]));
            return b;
        }));
    }
    CHECK(validateBvh(g.tlasNodes, 0, 0, static_cast<uint32_t>(s.instances.size()), [&](uint32_t i) {
        const Instance& inst = s.instances[i];
        return s.meshes[static_cast<size_t>(inst.mesh)].bounds.transformed(inst.transform.matrix());
    }));
}

TEST(traversal_matches_brute_force) {
    const Scene s = rotatedTestScene();
    const GpuSceneData g = packScene(s);
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> U(-1.0f, 1.0f);

    int mismatches = 0, hits = 0;
    const int kRays = 20000;
    for (int i = 0; i < kRays; ++i) {
        const glm::vec3 o(U(rng) * 2.0f, 1.0f + U(rng) * 0.9f, U(rng) * 6.5f);  // inside the hallway
        const glm::vec3 d = glm::normalize(glm::vec3(U(rng), U(rng), U(rng)) + glm::vec3(0, 0, -0.3f));
        const Hit a = traceGpuData(g, o, d), b = traceBruteForce(s, o, d);
        const bool bothMiss = a.t >= kInf && b.t >= kInf;
        const bool sameHit = std::abs(a.t - b.t) < 1e-3f * std::max(1.0f, b.t);
        if (!bothMiss && !sameHit) ++mismatches;
        if (b.t < kInf) ++hits;
    }
    std::printf("  %d rays, %d hits, %d mismatches\n", kRays, hits, mismatches);
    CHECK(hits > kRays * 9 / 10);  // closed room: nearly every ray must hit something
    CHECK(mismatches == 0);
}

TEST(gallery_layout) {
    const Scene s = makeGallery();
    int pedestals = 0, exhibits = 0;
    for (const Instance& i : s.instances) {
        exhibits += i.exhibit ? 1 : 0;
        pedestals += s.meshes[static_cast<size_t>(i.mesh)].name == "pedestal" ? 1 : 0;
    }
    CHECK(exhibits == 7);
    CHECK(pedestals == 6);
    // Every exhibit inside the hallway and not intersecting a wall.
    for (const Instance& i : s.instances) {
        if (!i.exhibit) continue;
        const AABB w = s.meshes[static_cast<size_t>(i.mesh)].bounds.transformed(i.transform.matrix());
        CHECK(w.min.x > -kHallHalfWidth && w.max.x < kHallHalfWidth);
        CHECK(w.min.z > -kHallHalfLength && w.max.z < kHallHalfLength);
    }
}

TEST(fit_to_slot) {
    // A 2 x 4 x 1 model with an off-centre origin must land bottom-centre on the slot,
    // scaled so its largest side equals the slot's largest side.
    AABB b;
    b.grow({3.0f, 5.0f, -1.0f});
    b.grow({5.0f, 9.0f, 0.0f});
    for (const ExhibitSlot& slot : kExhibits) {
        const AABB w = b.transformed(fitToSlot(b, slot).matrix());
        const glm::vec3 e = w.extent();
        const float slotMax = std::max({slot.size.x, slot.size.y, slot.size.z});
        CHECK(std::abs(e.y - slotMax) < 1e-4f);  // height (4) is the model's largest side
        CHECK(std::abs(w.min.y - slot.position.y) < 1e-4f);
        CHECK(std::abs(w.center().x - slot.position.x) < 1e-4f);
        CHECK(std::abs(w.center().z - slot.position.z) < 1e-4f);
    }
}
