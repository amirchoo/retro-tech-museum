#include "scene/scene.h"

#include <glm/gtc/matrix_transform.hpp>

namespace rtm {

AABB AABB::transformed(const glm::mat4& m) const {
    AABB out;
    if (!valid()) return out;
    for (int i = 0; i < 8; ++i) {
        const glm::vec3 c((i & 1) ? max.x : min.x, (i & 2) ? max.y : min.y, (i & 4) ? max.z : min.z);
        out.grow(glm::vec3(m * glm::vec4(c, 1.0f)));
    }
    return out;
}

void Mesh::computeBounds() {
    bounds = AABB{};
    for (const Vertex& v : vertices) bounds.grow(v.position);
}

glm::mat4 Transform::matrix() const {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), position);
    m *= glm::mat4_cast(rotation);
    return glm::scale(m, scale);
}

size_t Scene::triangleCount() const {
    size_t n = 0;
    for (const Instance& inst : instances) n += meshes[static_cast<size_t>(inst.mesh)].triangleCount();
    return n;
}

}  // namespace rtm
