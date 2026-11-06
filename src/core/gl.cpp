#include "core/gl.h"

#include <cstdio>

namespace rtm {

static const char* glErrorName(GLenum e) {
    switch (e) {
        case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
        default: return "unknown GL error";
    }
}

bool checkGLErrors(const char* what, const char* file, int line) {
    bool any = false;
    for (GLenum e = glGetError(); e != GL_NO_ERROR; e = glGetError()) {
        std::fprintf(stderr, "[GL] %s after '%s' (%s:%d)\n", glErrorName(e), what, file, line);
        any = true;
    }
    return any;
}

}  // namespace rtm
