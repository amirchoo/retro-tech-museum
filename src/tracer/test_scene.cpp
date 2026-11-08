#include "tracer/test_scene.h"

#include <cmath>

namespace rtm {
namespace {

void quad(Mesh& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, int material) {
    const glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
    const uint32_t first = static_cast<uint32_t>(mesh.vertices.size());
    const uint32_t index = static_cast<uint32_t>(mesh.indices.size());
    const glm::vec3 p[4] = {a, b, c, d};
    const glm::vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i) {
        Vertex v;
        v.position = p[i]; v.normal = n; v.uv = uv[i];
        mesh.vertices.push_back(v);
    }
    mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
    mesh.submeshes.push_back({index, 6, material});
}

Mesh sphere(int material) {
    Mesh mesh;
    mesh.name = "diagnostic sphere";
    constexpr int rings = 16, segments = 32;
    constexpr float pi = 3.14159265358979323846f;
    for (int j = 0; j <= rings; ++j) {
        const float theta = pi * static_cast<float>(j) / rings;
        for (int i = 0; i <= segments; ++i) {
            const float phi = 2 * pi * static_cast<float>(i) / segments;
            Vertex v;
            v.normal = {std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi)};
            v.position = v.normal;
            v.uv = {static_cast<float>(i) / segments, static_cast<float>(j) / rings};
            mesh.vertices.push_back(v);
        }
    }
    for (int j = 0; j < rings; ++j) {
        for (int i = 0; i < segments; ++i) {
            const uint32_t a = static_cast<uint32_t>(j * (segments + 1) + i), b = a + segments + 1;
            if (j != 0) mesh.indices.insert(mesh.indices.end(), {a, a + 1, b});
            if (j != rings - 1) mesh.indices.insert(mesh.indices.end(), {a + 1, b + 1, b});
        }
    }
    mesh.submeshes.push_back({0, static_cast<uint32_t>(mesh.indices.size()), material});
    mesh.computeBounds();
    return mesh;
}

void add(Scene& scene, Mesh mesh, const char* name, glm::vec3 position = glm::vec3(0), float scale = 1.0f) {
    mesh.computeBounds();
    Instance instance;
    instance.name = name;
    instance.mesh = static_cast<int>(scene.meshes.size());
    instance.transform.position = position;
    instance.transform.scale = glm::vec3(scale);
    scene.meshes.push_back(std::move(mesh));
    scene.instances.push_back(instance);
}

}  // namespace

Scene makeTracerTestScene() {
    Scene scene;
    Material neutral; neutral.name = "matte white"; neutral.baseColor = glm::vec3(0.65f); neutral.roughness = 0.8f;
    Material red = neutral; red.name = "red wall"; red.baseColor = {0.65f, 0.08f, 0.04f};
    Material blue = neutral; blue.name = "blue wall"; blue.baseColor = {0.06f, 0.18f, 0.65f};
    Material metal; metal.name = "polished metal"; metal.baseColor = {0.95f, 0.77f, 0.4f}; metal.metallic = 1; metal.roughness = 0.13f;
    Material glass; glass.name = "clear glass"; glass.baseColor = {0.96f, 0.99f, 1}; glass.transmission = 1; glass.ior = 1.5f; glass.roughness = 0;
    Material emitter; emitter.name = "emissive target"; emitter.emission = {0.1f, 1.8f, 0.3f}; emitter.baseColor = {0.02f, 0.3f, 0.04f};
    scene.materials = {neutral, red, blue, metal, glass, emitter};

    Mesh room;
    quad(room, {-3,0,8}, {3,0,8}, {3,0,-8}, {-3,0,-8}, 0);
    quad(room, {-3,4,-8}, {3,4,-8}, {3,4,8}, {-3,4,8}, 0);
    quad(room, {-3,0,-8}, {3,0,-8}, {3,4,-8}, {-3,4,-8}, 0);
    quad(room, {3,0,8}, {-3,0,8}, {-3,4,8}, {3,4,8}, 0);
    quad(room, {-3,0,8}, {-3,0,-8}, {-3,4,-8}, {-3,4,8}, 1);
    quad(room, {3,0,-8}, {3,0,8}, {3,4,8}, {3,4,-8}, 2);
    add(scene, std::move(room), "diagnostic room");
    add(scene, sphere(3), "metal sphere", {-1.05f, 1, -0.5f}, 0.85f);
    add(scene, sphere(4), "glass sphere", {1.05f, 1, 0}, 0.85f);
    add(scene, sphere(0), "diffuse sphere", {0, 0.65f, -2.4f}, 0.65f);
    Mesh target;
    quad(target, {0.5f,0.2f,-2}, {1.6f,0.2f,-2}, {1.6f,2,-2}, {0.5f,2,-2}, 5);
    add(scene, std::move(target), "target behind glass");
    return scene;
}

}  // namespace rtm
