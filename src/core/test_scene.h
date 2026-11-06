#pragma once
// TEMPORARY: a procedural room + plinths so the skeleton has something to render
// before the OBJ loader (src/geometry) and Scene structs (src/scene) exist.
// Delete once real assets load.
#include <vector>

#include <glm/glm.hpp>

#include "core/gl.h"

namespace rtm {

struct TestVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

class TestScene {
public:
    TestScene();
    ~TestScene();
    TestScene(const TestScene&) = delete;
    TestScene& operator=(const TestScene&) = delete;

    void draw() const;
    size_t triangleCount() const { return indexCount_ / 3; }

private:
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    GLsizei indexCount_ = 0;
};

}  // namespace rtm
