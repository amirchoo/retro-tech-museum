#pragma once
#include <string>

#include <glm/glm.hpp>

#include "core/gl.h"

namespace rtm {

// Vertex + fragment program loaded from shaders/ (paths relative to RTM_SHADER_DIR).
// reload() recompiles from disk; on failure the old program is kept, so a typo
// while editing a shader never kills the running app.
class Shader {
public:
    Shader() = default;
    Shader(std::string vertPath, std::string fragPath);
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool reload();
    bool ok() const { return program_ != 0; }
    void use() const { glUseProgram(program_); }
    GLuint id() const { return program_; }

    void set(const char* name, int v) const;
    void set(const char* name, float v) const;
    void set(const char* name, const glm::vec2& v) const;
    void set(const char* name, const glm::vec3& v) const;
    void set(const char* name, const glm::vec4& v) const;
    void set(const char* name, const glm::mat3& m) const;
    void set(const char* name, const glm::mat4& m) const;

private:
    GLint loc(const char* name) const { return glGetUniformLocation(program_, name); }

    std::string vertPath_, fragPath_;
    GLuint program_ = 0;
};

// Reads a file under RTM_SHADER_DIR / RTM_ASSET_DIR. Empty string + stderr message on failure.
std::string readTextFile(const std::string& path);
std::string shaderPath(const std::string& relative);
std::string assetPath(const std::string& relative);

}  // namespace rtm
