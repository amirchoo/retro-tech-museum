#pragma once
#include <string>

#include "core/gl.h"

namespace rtm {

// GLFW window + OpenGL 4.1 core context, identical on Windows and macOS.
class Window {
public:
    Window(int width, int height, const std::string& title, bool visible = true);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool ok() const { return handle_ != nullptr; }
    GLFWwindow* handle() const { return handle_; }
    bool shouldClose() const { return glfwWindowShouldClose(handle_); }
    void swap() { glfwSwapBuffers(handle_); }

    // Pixel size of the framebuffer. On Retina this is 2x the window size --
    // always use this for glViewport and render targets, never the window size.
    void framebufferSize(int& w, int& h) const { glfwGetFramebufferSize(handle_, &w, &h); }

    void setCursorCaptured(bool captured);
    bool cursorCaptured() const { return captured_; }

private:
    GLFWwindow* handle_ = nullptr;
    bool captured_ = false;
};

}  // namespace rtm
