#include "core/test_scene.h"

#include <cstddef>

namespace rtm {

namespace {

// Axis-aligned box. inward = true flips normals/winding (for the room shell).
void addBox(std::vector<TestVertex>& v, std::vector<GLuint>& idx, glm::vec3 mn, glm::vec3 mx, bool inward) {
    const glm::vec3 c[8] = {
        {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z},
        {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z}, {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z},
    };
    struct Face { int a, b, cc, d; glm::vec3 n; };
    const Face faces[6] = {
        {4, 5, 6, 7, {0, 0, 1}},  {1, 0, 3, 2, {0, 0, -1}}, {5, 1, 2, 6, {1, 0, 0}},
        {0, 4, 7, 3, {-1, 0, 0}}, {7, 6, 2, 3, {0, 1, 0}},  {0, 1, 5, 4, {0, -1, 0}},
    };
    const glm::vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (const Face& f : faces) {
        const GLuint base = static_cast<GLuint>(v.size());
        const int ids[4] = {f.a, f.b, f.cc, f.d};
        const glm::vec3 n = inward ? -f.n : f.n;
        for (int i = 0; i < 4; ++i) v.push_back({c[ids[i]], n, uv[i]});
        if (!inward) idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        else         idx.insert(idx.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
    }
}

}  // namespace

TestScene::TestScene() {
    std::vector<TestVertex> verts;
    std::vector<GLuint> idx;

    addBox(verts, idx, {-6.0f, 0.0f, -4.0f}, {6.0f, 3.5f, 4.0f}, true);  // gallery shell (12 x 3.5 x 8 m)
    for (int i = 0; i < 4; ++i) {                                         // plinths for exhibits
        const float x = -4.5f + 3.0f * static_cast<float>(i);
        addBox(verts, idx, {x - 0.4f, 0.0f, -2.9f}, {x + 0.4f, 1.0f, -2.1f}, false);
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(TestVertex)), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(idx.size() * sizeof(GLuint)), idx.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TestVertex), reinterpret_cast<void*>(offsetof(TestVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TestVertex), reinterpret_cast<void*>(offsetof(TestVertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(TestVertex), reinterpret_cast<void*>(offsetof(TestVertex, uv)));
    glBindVertexArray(0);

    indexCount_ = static_cast<GLsizei>(idx.size());
    GL_CHECK("TestScene upload");
}

TestScene::~TestScene() {
    glDeleteBuffers(1, &ebo_);
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
}

void TestScene::draw() const {
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

}  // namespace rtm
