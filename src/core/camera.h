#pragma once
#include <glm/glm.hpp>

#include "core/gl.h"

namespace rtm {

// First-person camera. WASD to move, mouse to look, Space/Ctrl up/down, Shift to sprint.
// Collision is NOT handled here: update() returns the desired position and the
// caller (later: BVH collision) decides where the camera actually ends up.
class Camera {
public:
    glm::vec3 position{0.0f, 1.6f, 4.0f};  // metres; 1.6 m = eye height
    float yaw = -90.0f;                    // degrees, -90 looks down -Z
    float pitch = 0.0f;
    float fovY = 60.0f;
    float nearZ = 0.05f, farZ = 100.0f;
    float moveSpeed = 2.5f;                // m/s
    float mouseSensitivity = 0.1f;         // degrees per pixel
    bool fly = false;                      // false = walk on the floor plane (y locked)

    // Reads keyboard + mouse. Mouse look only when the cursor is captured.
    // Returns true if the camera moved or rotated (the path tracer resets accumulation on this).
    bool update(GLFWwindow* window, float dt, bool cursorCaptured);

    glm::vec3 forward() const;
    glm::vec3 right() const;
    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;

private:
    double lastX_ = 0.0, lastY_ = 0.0;
    bool firstMouse_ = true;
};

}  // namespace rtm
