#include "core/camera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace rtm {

glm::vec3 Camera::forward() const {
    const float y = glm::radians(yaw), p = glm::radians(pitch);
    return glm::normalize(glm::vec3(std::cos(y) * std::cos(p), std::sin(p), std::sin(y) * std::cos(p)));
}

glm::vec3 Camera::right() const { return glm::normalize(glm::cross(forward(), glm::vec3(0, 1, 0))); }

glm::mat4 Camera::view() const { return glm::lookAt(position, position + forward(), glm::vec3(0, 1, 0)); }

glm::mat4 Camera::projection(float aspect) const {
    return glm::perspective(glm::radians(fovY), aspect, nearZ, farZ);
}

bool Camera::update(GLFWwindow* w, float dt, bool cursorCaptured) {
    bool changed = false;

    // --- mouse look ---
    double mx, my;
    glfwGetCursorPos(w, &mx, &my);
    if (!cursorCaptured || firstMouse_) {
        lastX_ = mx;
        lastY_ = my;
        firstMouse_ = !cursorCaptured;
    }
    const double dx = mx - lastX_, dy = my - lastY_;
    lastX_ = mx;
    lastY_ = my;
    if (cursorCaptured && (dx != 0.0 || dy != 0.0)) {
        yaw += static_cast<float>(dx) * mouseSensitivity;
        pitch = std::clamp(pitch - static_cast<float>(dy) * mouseSensitivity, -89.0f, 89.0f);
        changed = true;
    }

    // --- movement ---
    auto down = [w](int key) { return glfwGetKey(w, key) == GLFW_PRESS; };
    glm::vec3 fwd = forward();
    if (!fly) fwd = glm::normalize(glm::vec3(fwd.x, 0.0f, fwd.z));
    const glm::vec3 rgt = right();

    glm::vec3 move(0.0f);
    if (down(GLFW_KEY_W)) move += fwd;
    if (down(GLFW_KEY_S)) move -= fwd;
    if (down(GLFW_KEY_D)) move += rgt;
    if (down(GLFW_KEY_A)) move -= rgt;
    if (fly && down(GLFW_KEY_SPACE)) move.y += 1.0f;
    if (fly && down(GLFW_KEY_LEFT_CONTROL)) move.y -= 1.0f;

    if (glm::dot(move, move) > 0.0f) {
        const float speed = moveSpeed * (down(GLFW_KEY_LEFT_SHIFT) ? 2.5f : 1.0f);
        position += glm::normalize(move) * speed * dt;
        changed = true;
    }
    return changed;
}

}  // namespace rtm
