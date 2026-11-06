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

// Resolves `#include "file.glsl"` (path relative to shaders/), recursively, each file once.
// GLSL has no #include; this is ours. CI does the same expansion in tools/expand_glsl.py.
static std::string expandIncludes(const std::string& src, std::vector<std::string>& seen, int depth = 0) {
    if (depth > 8) {
        std::fprintf(stderr, "[shader] #include nested too deep\n");
        return src;
    }
    std::stringstream in(src), out;
    std::string line;
    while (std::getline(in, line)) {
        const size_t p = line.find("#include");
        const size_t q1 = line.find('"');
        const size_t q2 = line.rfind('"');
        if (p != std::string::npos && line.find_first_not_of(" \t") == p && q1 != std::string::npos && q2 > q1) {
            const std::string name = line.substr(q1 + 1, q2 - q1 - 1);
            bool already = false;
            for (const std::string& s : seen) already |= (s == name);
            if (!already) {
                seen.push_back(name);
                out << expandIncludes(readTextFile(shaderPath(name)), seen, depth + 1) << "\n";
            }
        } else {
            out << line << "\n";
        }
    }
    return out.str();
}

static std::string loadShaderSource(const std::string& path) {
    std::vector<std::string> seen;
    const std::string src = readTextFile(path);
    return src.empty() ? src : expandIncludes(src, seen);
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
    const std::string vsrc = loadShaderSource(vp), fsrc = loadShaderSource(fp);
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
