#pragma once

#include <algorithm>
#include <cmath>

#include "core/shader.h"

namespace rtm {

// Shared raster/tracer light. Intensity is radiant intensity in linear RGB;
// radius gives the tracer a disk emitter, with the same far-field brightness.
struct SpotLight {
    glm::vec3 position{0.0f, 3.4f, 0.0f};
    glm::vec3 target{0.0f, 0.0f, -0.5f};
    glm::vec3 color{1.0f, 0.92f, 0.8f};
    float intensity = 60.0f;
    float innerDeg = 50.0f;
    float outerDeg = 72.0f;
    float radius = 0.3f;
    float ambient = 0.12f;

    glm::vec3 direction() const {
        const glm::vec3 d = target - position;
        return glm::dot(d, d) > 1e-8f ? glm::normalize(d) : glm::vec3(0, -1, 0);
    }

    void apply(const Shader& shader) const {
        const float inner = std::clamp(innerDeg, 0.0f, 89.8f);
        const float outer = std::clamp(outerDeg, inner + 0.1f, 89.9f);
        shader.set("uLightPos", position);
        shader.set("uLightDir", direction());
        shader.set("uLightColor", glm::max(color, glm::vec3(0)) * std::max(intensity, 0.0f));
        shader.set("uCosInner", std::cos(glm::radians(inner)));
        shader.set("uCosOuter", std::cos(glm::radians(outer)));
        shader.set("uLightRadius", std::max(radius, 0.0f));
        shader.set("uAmbient", std::max(ambient, 0.0f));
    }

    bool operator==(const SpotLight& other) const {
        return position == other.position && target == other.target && color == other.color &&
               intensity == other.intensity && innerDeg == other.innerDeg && outerDeg == other.outerDeg &&
               radius == other.radius && ambient == other.ambient;
    }
};

}  // namespace rtm
