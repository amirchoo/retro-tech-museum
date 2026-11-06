#include "core/mesh_gpu.h"

#include <cstddef>
#include <cstdint>

namespace rtm {

GpuMesh::GpuMesh(const Mesh& mesh) {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)),
                 mesh.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(uint32_t)),
                 mesh.indices.data(), GL_STATIC_DRAW);

    auto attr = [](GLuint loc, GLint n, size_t offset) {
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, n, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offset));
    };
    attr(0, 3, offsetof(Vertex, position));
    attr(1, 3, offsetof(Vertex, normal));
    attr(2, 2, offsetof(Vertex, uv));
    attr(3, 4, offsetof(Vertex, tangent));
    glBindVertexArray(0);
    GL_CHECK("GpuMesh upload");
}

GpuMesh::GpuMesh(GpuMesh&& o) noexcept : vao_(o.vao_), vbo_(o.vbo_), ebo_(o.ebo_) {
    o.vao_ = o.vbo_ = o.ebo_ = 0;
}

GpuMesh::~GpuMesh() {
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

void GpuMesh::drawSubMesh(const SubMesh& sm) const {
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(sm.indexCount), GL_UNSIGNED_INT,
                   reinterpret_cast<void*>(static_cast<size_t>(sm.firstIndex) * sizeof(uint32_t)));
    glBindVertexArray(0);
}

}  // namespace rtm
