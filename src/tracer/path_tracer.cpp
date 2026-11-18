#include "tracer/path_tracer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace rtm {

namespace {

// Render passes preserve caller state, including separate read/draw targets.
struct PassState {
    GLint draw = 0, read = 0, viewport[4]{}, program = 0, vao = 0, active = 0;
    GLint texture0 = 0, array2 = 0;
    GLboolean depth = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blend = glIsEnabled(GL_BLEND);
    GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
    PassState() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);
        glActiveTexture(GL_TEXTURE2);
        glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &array2);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_FRAMEBUFFER_SRGB);
    }
    ~PassState() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read));
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glUseProgram(static_cast<GLuint>(program));
        glBindVertexArray(static_cast<GLuint>(vao));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture0));
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D_ARRAY, static_cast<GLuint>(array2));
        glActiveTexture(static_cast<GLenum>(active));
        if (depth) glEnable(GL_DEPTH_TEST);
        if (blend) glEnable(GL_BLEND);
        if (scissor) glEnable(GL_SCISSOR_TEST);
        if (srgb) glEnable(GL_FRAMEBUFFER_SRGB);
    }
};

}  // namespace

PathTracer::PathTracer() {
    glGenVertexArrays(1, &vao_);
    PassState state;
    glGenTextures(1, &whiteArray_);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D_ARRAY, whiteArray_);
    const unsigned char white[4] = {255, 255, 255, 255};
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_SRGB8_ALPHA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

PathTracer::~PathTracer() {
    glDeleteFramebuffers(2, fbos_);
    glDeleteTextures(2, textures_);
    glDeleteTextures(1, &whiteArray_);
    glDeleteVertexArrays(1, &vao_);
}

bool PathTracer::reload() {
    const bool traceOk = trace_.reload();
    const bool displayOk = display_.reload();
    reset();
    return traceOk && displayOk;
}

void PathTracer::reset() {
    // No clear needed: the first frame never fetches the old accumulation.
    frames_ = samples_ = 0;
}

void PathTracer::setSettings(TracerSettings settings) {
    if (!std::isfinite(settings.resolutionScale)) settings.resolutionScale = 0.5f;
    settings.resolutionScale = std::clamp(settings.resolutionScale, 0.25f, 1.0f);
    settings.samplesPerFrame = std::clamp(settings.samplesPerFrame, 1, 16);
    settings.maxBounces = std::clamp(settings.maxBounces, 1, 16);
    if (!std::isfinite(settings.indirectClamp)) settings.indirectClamp = 3.0f;
    settings.indirectClamp = std::clamp(settings.indirectClamp, 0.0f, 100.0f);
    if (settings.resolutionScale != settings_.resolutionScale ||
        settings.samplesPerFrame != settings_.samplesPerFrame || settings.maxBounces != settings_.maxBounces ||
        settings.indirectClamp != settings_.indirectClamp)
        reset();
    settings_ = settings;
}

void PathTracer::setBaseColorTextures(GLuint textureArray, int layers) {
    baseColorArray_ = textureArray;
    baseColorLayers_ = textureArray ? std::max(layers, 0) : 0;
    reset();
}

bool PathTracer::resize(int framebufferWidth, int framebufferHeight) {
    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    const int w = std::clamp(static_cast<int>(std::floor(framebufferWidth * settings_.resolutionScale)), 1, maxSize);
    const int h = std::clamp(static_cast<int>(std::floor(framebufferHeight * settings_.resolutionScale)), 1, maxSize);
    const bool framebufferChanged = framebufferWidth != framebufferWidth_ || framebufferHeight != framebufferHeight_;
    framebufferWidth_ = framebufferWidth;
    framebufferHeight_ = framebufferHeight;
    if (framebufferChanged) reset();
    if (w == width_ && h == height_) return true;

    PassState state;
    glActiveTexture(GL_TEXTURE0);
    if (!textures_[0]) glGenTextures(2, textures_);
    if (!fbos_[0]) glGenFramebuffers(2, fbos_);
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, textures_[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos_[i]);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures_[i], 0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::fprintf(stderr, "[tracer] incomplete accumulation framebuffer\n");
            width_ = height_ = 0;
            return false;
        }
    }
    width_ = w;
    height_ = h;
    read_ = 0;
    reset();
    return true;
}

bool PathTracer::render(const GpuScene& scene, const glm::mat4& invViewProj, const glm::vec3& camera,
                        const SpotLight& light, int framebufferWidth, int framebufferHeight) {
    if (framebufferWidth <= 0 || framebufferHeight <= 0) { reset(); return true; }
    if (!ok() || !resize(framebufferWidth, framebufferHeight)) return false;
    if (!haveView_ || lastInvViewProj_ != invViewProj || lastCamera_ != camera || !(lastLight_ == light)) reset();
    lastInvViewProj_ = invViewProj;
    lastCamera_ = camera;
    lastLight_ = light;
    haveView_ = true;

    // Stop before float sample weights lose integer precision; UI can reset.
    if (samples_ + static_cast<uint32_t>(settings_.samplesPerFrame) > (1u << 24)) return true;
    PassState state;
    const int write = 1 - read_;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos_[write]);
    glViewport(0, 0, width_, height_);
    trace_.use();
    scene.bind(trace_.id());
    light.apply(trace_);
    trace_.set("uInvViewProj", invViewProj);
    trace_.set("uCameraPos", camera);
    trace_.set("uResolution", glm::vec2(width_, height_));
    trace_.set("uPreviousSamples", static_cast<int>(samples_));
    trace_.set("uFrame", static_cast<int>(frames_));
    trace_.set("uSamplesPerFrame", settings_.samplesPerFrame);
    trace_.set("uMaxBounces", settings_.maxBounces);
    trace_.set("uIndirectClamp", settings_.indirectClamp);
    trace_.set("uPrevious", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures_[read_]);
    trace_.set("uBaseColorTextures", 2);
    trace_.set("uBaseColorLayerCount", baseColorLayers_);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D_ARRAY, baseColorLayers_ > 0 ? baseColorArray_ : whiteArray_);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    read_ = write;
    ++frames_;
    samples_ += static_cast<uint32_t>(settings_.samplesPerFrame);
    GL_CHECK("PathTracer::render");
    return true;
}

void PathTracer::display(int framebufferWidth, int framebufferHeight, float exposure, float divider) const {
    if (!samples_ || framebufferWidth <= 0 || framebufferHeight <= 0) return;
    PassState state;
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    display_.use();
    display_.set("uAccumulation", 0);
    display_.set("uExposure", std::max(exposure, 0.0f));
    display_.set("uDivider", std::clamp(divider, 0.0f, 1.0f));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures_[read_]);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

}  // namespace rtm
