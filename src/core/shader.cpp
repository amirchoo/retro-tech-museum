#include "core/shader.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

namespace rtm {

std::string shaderPath(const std::string& relative) { return std::string(RTM_SHADER_DIR) + relative; }
std::string assetPath(const std::string& relative) { return std::string(RTM_ASSET_DIR) + relative; }

std::string readTextFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "Cannot open %s\n", path.c_str());
        return {};
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static GLuint compile(GLenum stage, const std::string& src, const std::string& path) {
    GLuint s = glCreateShader(stage);
    const char* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len > 1 ? len : 1));
        glGetShaderInfoLog(s, len, nullptr, log.data());
        std::fprintf(stderr, "[shader] compile failed: %s\n%s\n", path.c_str(), log.data());
        glDeleteShader(s);
        return 0;
    }
    return s;
}

Shader::Shader(std::string vertPath, std::string fragPath)
    : vertPath_(std::move(vertPath)), fragPath_(std::move(fragPath)) {
    reload();
}

Shader::~Shader() {
    if (program_) glDeleteProgram(program_);
}

bool Shader::reload() {
    const std::string vp = shaderPath(vertPath_), fp = shaderPath(fragPath_);
    const std::string vsrc = readTextFile(vp), fsrc = readTextFile(fp);
    if (vsrc.empty() || fsrc.empty()) return false;

    GLuint vs = compile(GL_VERTEX_SHADER, vsrc, vp);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsrc, fp);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len > 1 ? len : 1));
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::fprintf(stderr, "[shader] link failed: %s + %s\n%s\n", vp.c_str(), fp.c_str(), log.data());
        glDeleteProgram(prog);
        return false;
    }

    if (program_) glDeleteProgram(program_);
    program_ = prog;
    std::printf("[shader] loaded %s + %s\n", vertPath_.c_str(), fragPath_.c_str());
    return true;
}

void Shader::set(const char* n, int v) const { glUniform1i(loc(n), v); }
void Shader::set(const char* n, float v) const { glUniform1f(loc(n), v); }
void Shader::set(const char* n, const glm::vec2& v) const { glUniform2fv(loc(n), 1, glm::value_ptr(v)); }
void Shader::set(const char* n, const glm::vec3& v) const { glUniform3fv(loc(n), 1, glm::value_ptr(v)); }
void Shader::set(const char* n, const glm::vec4& v) const { glUniform4fv(loc(n), 1, glm::value_ptr(v)); }
void Shader::set(const char* n, const glm::mat3& m) const { glUniformMatrix3fv(loc(n), 1, GL_FALSE, glm::value_ptr(m)); }
void Shader::set(const char* n, const glm::mat4& m) const { glUniformMatrix4fv(loc(n), 1, GL_FALSE, glm::value_ptr(m)); }

}  // namespace rtm
