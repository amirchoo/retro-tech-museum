#pragma once
// Single include for OpenGL. Always include this instead of <glad/glad.h> / <GLFW/glfw3.h>
// directly, so GLAD is guaranteed to come before GLFW on every platform.
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// OpenGL 4.1 core is the hard ceiling (macOS). Do NOT use anything newer:
//   - no compute shaders, SSBOs, image load/store       (4.2 / 4.3)
//   - no layout(binding = N) in GLSL                    (4.2) -> set sampler units with glUniform1i
//   - no glDebugMessageCallback / KHR_debug             (4.3) -> use GL_CHECK
//   - shaders must start with: #version 410 core
// CI validates every shader against GLSL 410, so violations fail the build.
namespace rtm {
constexpr int kGLMajor = 4;
constexpr int kGLMinor = 1;
constexpr const char* kGLSLVersion = "#version 410 core";

// Prints every pending glGetError with file/line. Returns true if any error was found.
bool checkGLErrors(const char* what, const char* file, int line);
}  // namespace rtm

#ifndef NDEBUG
#define GL_CHECK(what) ::rtm::checkGLErrors((what), __FILE__, __LINE__)
#else
#define GL_CHECK(what) ((void)0)
#endif
