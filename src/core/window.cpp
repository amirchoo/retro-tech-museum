#include "core/window.h"

#include <cstdio>

namespace rtm {

static void glfwErrorCallback(int code, const char* desc) {
    std::fprintf(stderr, "[GLFW] error %d: %s\n", code, desc);
}

Window::Window(int width, int height, const std::string& title, bool visible) {
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialise GLFW\n");
        return;
    }

    // Same context on every OS: 4.1 core, forward-compatible (required on macOS,
    // and on Windows it stops drivers from silently accepting 4.3+ features).
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, kGLMajor);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, kGLMinor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    handle_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!handle_) {
        std::fprintf(stderr, "Failed to create an OpenGL %d.%d core window\n", kGLMajor, kGLMinor);
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(handle_);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::fprintf(stderr, "Failed to load OpenGL functions (GLAD)\n");
        glfwDestroyWindow(handle_);
        handle_ = nullptr;
        glfwTerminate();
        return;
    }

    std::printf("OpenGL  %s\nGLSL    %s\nGPU     %s (%s)\n",
                reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)),
                reinterpret_cast<const char*>(glGetString(GL_RENDERER)),
                reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
}

Window::~Window() {
    if (handle_) glfwDestroyWindow(handle_);
    glfwTerminate();
}

void Window::setCursorCaptured(bool captured) {
    captured_ = captured;
    glfwSetInputMode(handle_, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported())
        glfwSetInputMode(handle_, GLFW_RAW_MOUSE_MOTION, captured ? GLFW_TRUE : GLFW_FALSE);
}

}  // namespace rtm
