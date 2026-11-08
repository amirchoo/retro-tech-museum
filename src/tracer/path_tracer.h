#pragma once

#include <cstdint>

#include "core/shader.h"
#include "scene/gpu_scene.h"
#include "scene/lights.h"

namespace rtm {

struct TracerSettings {
    float resolutionScale = 0.5f;
    int samplesPerFrame = 1;
    int maxBounces = 4;
    float indirectClamp = 3.0f;  // 0 = unbiased; positive = suppress rare bright paths
};

// Owns linear HDR accumulation only. display() applies exposure/tonemapping;
// a split-screen divider changes presentation without resetting convergence.
class PathTracer {
public:
    PathTracer();
    ~PathTracer();
    PathTracer(const PathTracer&) = delete;
    PathTracer& operator=(const PathTracer&) = delete;

    bool ok() const { return trace_.ok() && display_.ok(); }
    bool reload();
    void reset();
    void setSettings(TracerSettings settings);
    const TracerSettings& settings() const { return settings_; }

    // Loader supplies an sRGB texture array; ownership stays with the loader.
    // Layer indices correspond to RtmMaterial.baseColorTex. 0 disables textures.
    void setBaseColorTextures(GLuint textureArray, int layers);

    bool render(const GpuScene& scene, const glm::mat4& invViewProj, const glm::vec3& camera,
                const SpotLight& light, int framebufferWidth, int framebufferHeight);
    void display(int framebufferWidth, int framebufferHeight, float exposure = 1.0f,
                 float divider = 0.0f) const;

    int width() const { return width_; }
    int height() const { return height_; }
    uint32_t frames() const { return frames_; }
    uint32_t samples() const { return samples_; }
    GLuint accumulationTexture() const { return textures_[read_]; }

private:
    bool resize(int framebufferWidth, int framebufferHeight);

    Shader trace_{"fullscreen.vert", "tracer.frag"};
    Shader display_{"fullscreen.vert", "tracer_display.frag"};
    TracerSettings settings_;
    GLuint textures_[2]{};
    GLuint fbos_[2]{};
    GLuint vao_ = 0;
    GLuint whiteArray_ = 0;
    GLuint baseColorArray_ = 0;
    int baseColorLayers_ = 0;
    int width_ = 0, height_ = 0;
    int framebufferWidth_ = 0, framebufferHeight_ = 0;
    int read_ = 0;
    uint32_t frames_ = 0, samples_ = 0;
    bool haveView_ = false;
    glm::mat4 lastInvViewProj_{1};
    glm::vec3 lastCamera_{0};
    SpotLight lastLight_;
};

}  // namespace rtm
