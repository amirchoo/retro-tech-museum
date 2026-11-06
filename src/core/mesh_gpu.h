#pragma once
// Minimal VAO/VBO/EBO upload of an rtm::Mesh for raster drawing.
// Attribute locations: 0 position, 1 normal, 2 uv, 3 tangent.
#include "core/gl.h"
#include "scene/scene.h"

namespace rtm {

class GpuMesh {
public:
    explicit GpuMesh(const Mesh& mesh);
    ~GpuMesh();
    GpuMesh(const GpuMesh&) = delete;
    GpuMesh& operator=(const GpuMesh&) = delete;
    GpuMesh(GpuMesh&& o) noexcept;
    GpuMesh& operator=(GpuMesh&&) = delete;

    void drawSubMesh(const SubMesh& sm) const;

private:
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
};

}  // namespace rtm
