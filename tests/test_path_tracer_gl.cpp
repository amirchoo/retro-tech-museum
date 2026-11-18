#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "core/gl.h"
#include "scene/gallery.h"
#include "tracer/path_tracer.h"
#include "tracer/test_scene.h"

namespace {
int failures = 0;
void check(bool condition, const char* message) {
    std::printf("[%s] %s\n", condition ? " OK " : "FAIL", message);
    if (!condition) ++failures;
}

rtm::Mesh plane(float z, int material, bool reverse = false) {
    rtm::Mesh mesh;
    const glm::vec3 points[4] = {{-20,-20,z}, {20,-20,z}, {20,20,z}, {-20,20,z}};
    for (int i = 0; i < 4; ++i) {
        rtm::Vertex vertex;
        vertex.position = points[i];
        vertex.normal = {0, 0, reverse ? -1.0f : 1.0f};
        vertex.uv = {static_cast<float>(i & 1), static_cast<float>(i / 2)};
        mesh.vertices.push_back(vertex);
    }
    if (reverse) mesh.indices = {0,2,1,0,3,2};
    else mesh.indices = {0,1,2,0,2,3};
    mesh.submeshes.push_back({0,6,material});
    mesh.computeBounds();
    return mesh;
}

void append(rtm::Scene& scene, rtm::Mesh mesh) {
    rtm::Instance instance;
    instance.mesh = static_cast<int>(scene.meshes.size());
    scene.meshes.push_back(std::move(mesh));
    scene.instances.push_back(instance);
}

std::vector<glm::vec4> read(const rtm::PathTracer& tracer) {
    std::vector<glm::vec4> pixels(static_cast<size_t>(tracer.width()) * tracer.height());
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tracer.accumulationTexture());
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
    return pixels;
}

glm::vec3 average(const std::vector<glm::vec4>& pixels) {
    glm::vec3 result(0);
    for (const auto& pixel : pixels) result += glm::vec3(pixel);
    return result / static_cast<float>(pixels.size());
}

bool finite(const std::vector<glm::vec4>& pixels) {
    for (const auto& pixel : pixels)
        for (int c = 0; c < 4; ++c)
            if (!std::isfinite(pixel[c]) || pixel[c] < 0) return false;
    return true;
}
}  // namespace

int runPathTracerTests() {
    rtm::Shader raster("raster_basic.vert", "raster_basic.frag");
    rtm::Shader cast("fullscreen.vert", "debug_cast.frag");
    rtm::PathTracer tracer;
    check(raster.ok() && cast.ok() && tracer.ok(), "all GLSL 410 programs compile and link");
    if (!raster.ok() || !cast.ok() || !tracer.ok()) return 1;
    tracer.setSettings({0.5f, 2, 4});
    const glm::vec3 camera(0, 0, 3);
    const glm::mat4 view = glm::lookAt(camera, glm::vec3(0), glm::vec3(0,1,0));
    const glm::mat4 inv = glm::inverse(glm::perspective(glm::radians(50.0f), 2.0f, 0.05f, 100.0f) * view);
    rtm::SpotLight light;
    light.intensity = light.ambient = 0;

    rtm::Scene scene;
    rtm::Material emitter;
    emitter.emission = {0.25f, 0.5f, 1.0f};
    scene.materials.push_back(emitter);
    append(scene, plane(0, 0));
    rtm::GpuScene gpu;
    gpu.upload(scene);
    check(tracer.render(gpu, inv, camera, light, 64, 32), "first accumulation pass succeeds");
    auto pixels = read(tracer);
    check(tracer.width() == 32 && tracer.height() == 16, "resolution scale uses framebuffer pixels");
    check(finite(pixels) && glm::length(average(pixels) - emitter.emission) < 1e-5f, "first frame overwrites uninitialized history with linear HDR");
    tracer.render(gpu, inv, camera, light, 64, 32);
    check(tracer.frames() == 2 && tracer.samples() == 4, "stationary view accumulates frames and samples");
    check(glm::length(average(read(tracer)) - emitter.emission) < 1e-5f, "ping-pong running average preserves known emission");
    tracer.render(gpu, inv, camera + glm::vec3(0.1f,0,0), light, 64, 32);
    check(tracer.samples() == 2, "camera changes reset history");
    tracer.render(gpu, inv, camera, light, 64, 32);
    tracer.render(gpu, inv, camera, light, 64, 32);
    light.radius += 0.1f;
    tracer.render(gpu, inv, camera, light, 64, 32);
    check(tracer.samples() == 2, "light changes reset history");
    tracer.render(gpu, inv * glm::scale(glm::mat4(1), glm::vec3(1.01f,1,1)), camera, light, 64, 32);
    check(tracer.samples() == 2, "projection/FOV changes reset history");
    tracer.render(gpu, inv, camera, light, 65, 33);
    check(tracer.samples() == 2 && tracer.width() == 32 && tracer.height() == 16, "resize resets even when scaled dimensions round to the same size");
    tracer.reset();
    tracer.render(gpu, inv, camera, light, 64, 32);
    check(tracer.samples() == 2, "scene invalidation restarts accumulation");
    tracer.render(gpu, inv, camera, light, 0, 0);
    check(tracer.samples() == 0, "minimized framebuffer suspends and resets tracing");
    tracer.setSettings({0.25f, 4, 3});
    tracer.render(gpu, inv, camera, light, 64, 32);
    check(tracer.width() == 16 && tracer.height() == 8 && tracer.samples() == 4, "resolution/SPP/bounce controls apply");
    tracer.setSettings({0.5f, 16, 4});

    // A metal plane reflects environment only after a continuation bounce.
    rtm::Material metal;
    metal.baseColor = {0.9f, 0.25f, 0.08f}; metal.metallic = 1; metal.roughness = 0.15f;
    scene.materials[0] = metal;
    gpu.upload(scene); tracer.reset();
    light.ambient = 0.3f;
    tracer.setSettings({0.5f,16,1});
    tracer.render(gpu, inv, camera, light, 64, 32);
    const float oneBounce = glm::length(average(read(tracer)));
    tracer.setSettings({0.5f,16,4});
    tracer.render(gpu, inv, camera, light, 64, 32);
    glm::vec3 reflection = average(read(tracer));
    check(oneBounce < 1e-6f && reflection.r > 0.2f && reflection.r > reflection.g * 2, "GGX metal continuation produces colored reflections");

    GLuint textureArray = 0;
    glGenTextures(1, &textureArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);
    const unsigned char red[4] = {128,0,0,255};
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_SRGB8_ALPHA8, 1,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,red);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    scene.materials[0].baseColor = glm::vec3(1);
    scene.materials[0].baseColorTex = 0;
    gpu.upload(scene);
    tracer.setBaseColorTextures(textureArray, 1);
    tracer.render(gpu, inv, camera, light, 64, 32);
    glm::vec3 textured = average(read(tracer));
    check(textured.r > 0.03f && textured.r < 0.1f && textured.g < 1e-4f, "loader texture-array hook samples sRGB as linear color");
    tracer.setBaseColorTextures(0, 0);
    glDeleteTextures(1, &textureArray);

    // Enter + exit interfaces in front of a green emitter test IOR and offsets.
    rtm::Material glass;
    glass.baseColor = glm::vec3(1); glass.transmission = 1; glass.ior = 1.5f;
    rtm::Material green; green.emission = {0.05f,2.0f,0.1f};
    scene = {};
    scene.materials = {glass, green};
    append(scene, plane(0, 0));
    append(scene, plane(-0.3f, 0, true));
    append(scene, plane(-1, 1));
    light.ambient = 0;
    gpu.upload(scene); tracer.reset();
    for (int i = 0; i < 4; ++i) tracer.render(gpu, inv, camera, light, 64, 32);
    glm::vec3 transmission = average(read(tracer));
    check(transmission.g > 1.5f && transmission.g > transmission.r * 10 && finite(read(tracer)), "Fresnel glass transmits through entering and exiting interfaces");
    scene.materials[1].emission = glm::vec3(100);
    gpu.upload(scene); tracer.reset();
    tracer.render(gpu, inv, camera, light, 64,32);
    const float clamped = average(read(tracer)).g;
    tracer.setSettings({0.5f,16,4,0.0f});
    tracer.render(gpu, inv, camera, light, 64,32);
    check(clamped > 2.0f && clamped <= 3.001f && average(read(tracer)).g > 50.0f,
          "indirect clamp suppresses bright transmitted paths and can be disabled");
    tracer.setSettings({0.5f,16,4});
    scene.materials[1] = green;
    scene.materials[0].transmission = 0;
    gpu.upload(scene); tracer.reset();
    tracer.render(gpu, inv, camera, light, 64, 32);
    check(glm::length(average(read(tracer))) < 1e-5f, "opaque material blocks the same emitter");

    light.position = {0,0,2}; light.target = {0,0,3}; light.radius = 0.5f; light.intensity = 4;
    tracer.render(gpu, inv, camera, light, 64,32);
    const auto visibleLight = read(tracer);
    check(visibleLight[visibleLight.size()/2 + static_cast<size_t>(tracer.width()/2)].g > 2.0f,
          "analytic disk emits toward the camera on its front side");

    // An off-axis light and blocker cast a visible shadow onto the back plane.
    scene = {};
    rtm::Material matte; matte.baseColor = glm::vec3(0.8f); matte.roughness = 0.7f;
    rtm::Material black; black.baseColor = glm::vec3(0); black.roughness = 1;
    scene.materials = {matte, black};
    append(scene, plane(0, 0));
    rtm::Mesh blocker = plane(1, 1);
    for (rtm::Vertex& vertex : blocker.vertices) {
        vertex.position.x *= 0.0125f;
        vertex.position.y = vertex.position.y * 0.0125f + 0.5f;
    }
    blocker.computeBounds();
    append(scene, std::move(blocker));
    gpu.upload(scene); tracer.reset();
    light.position = {0,1,2}; light.target = {0,0,0}; light.radius = 0; light.intensity = 8; light.ambient = 0;
    tracer.setSettings({1.0f,16,1});
    tracer.render(gpu, inv, camera, light, 64,32);
    const auto hard = read(tracer);
    light.radius = 0.65f;
    tracer.setSettings({1.0f,1,1});
    tracer.render(gpu, inv, camera, light, 64,32);
    const auto noisy = read(tracer);
    std::vector<glm::vec4> intermediate;
    for (int i = 1; i < 200; ++i) {
        tracer.render(gpu, inv, camera, light, 64,32);
        if (i == 15) intermediate = read(tracer);
    }
    const auto soft = read(tracer);
    int penumbra = 0;
    double error1 = 0, error16 = 0;
    for (size_t i = 0; i < soft.size(); ++i) {
        if (hard[i].r < 1e-5f && soft[i].r > 0.01f) ++penumbra;
        const glm::vec3 d1 = glm::vec3(noisy[i] - soft[i]);
        const glm::vec3 d16 = glm::vec3(intermediate[i] - soft[i]);
        error1 += glm::dot(d1,d1);
        error16 += glm::dot(d16,d16);
    }
    check(penumbra > 0, "disk emitter produces a penumbra where a point light casts hard shadow");
    check(finite(soft) && error16 < error1 && tracer.samples() == 200,
          "progressive area-light sampling converges toward a finite 200-SPP reference");
    std::printf("Convergence MSE: 1 spp %.6g, 16 spp %.6g (reference 200 spp); %d penumbra pixels\n",
                error1 / soft.size(), error16 / soft.size(), penumbra);

    // Render the actual gallery and diagnostic meshes through the GPU contract.
    scene = rtm::makeGallery();
    gpu.upload(scene); tracer.reset();
    light = {};
    const glm::vec3 galleryCamera(0,1.6f,6);
    const glm::mat4 galleryInv = glm::inverse(glm::perspective(glm::radians(60.0f), 2.0f, 0.05f, 100.0f) *
        glm::lookAt(galleryCamera, galleryCamera + glm::vec3(0,0,-1), glm::vec3(0,1,0)));
    tracer.setSettings({0.5f, 2, 4});
    tracer.render(gpu, galleryInv, galleryCamera, light, 64, 32);
    check(finite(read(tracer)) && glm::length(average(read(tracer))) > 0.02f, "gallery renders finite, nonblack paths");
    scene = rtm::makeTracerTestScene();
    gpu.upload(scene); tracer.reset();
    tracer.render(gpu, galleryInv, galleryCamera, light, 64, 32);
    check(finite(read(tracer)) && glm::length(average(read(tracer))) > 0.02f, "metal/glass diagnostic scene renders finite paths");

    // Presentation keeps the raster half intact and restores caller GL state.
    GLuint output = 0, fbo = 0;
    glGenTextures(1, &output);
    glBindTexture(GL_TEXTURE_2D, output);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,64,32,0,GL_RGBA,GL_FLOAT,nullptr);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,output,0);
    glViewport(2,3,50,20);
    glClearColor(0.1f,0.2f,0.9f,1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glEnable(GL_BLEND); glEnable(GL_SCISSOR_TEST);
    glScissor(0,0,1,1);
    tracer.display(64,32,1.0f,0.5f);
    glm::vec4 left(0), right(0);
    glReadPixels(8,16,1,1,GL_RGBA,GL_FLOAT,&left);
    glReadPixels(56,16,1,1,GL_RGBA,GL_FLOAT,&right);
    GLint viewport[4]; glGetIntegerv(GL_VIEWPORT,viewport);
    check(glm::length(glm::vec3(left) - glm::vec3(0.1f,0.2f,0.9f)) < 1e-6f && glm::length(glm::vec3(right) - glm::vec3(left)) > 0.05f,
          "split display preserves raster half and draws tracer half");
    check(viewport[0] == 2 && viewport[1] == 3 && viewport[2] == 50 && viewport[3] == 20 &&
          glIsEnabled(GL_DEPTH_TEST) && glIsEnabled(GL_BLEND) && glIsEnabled(GL_SCISSOR_TEST), "presentation restores GL state");
    glDisable(GL_BLEND); glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    glDeleteFramebuffers(1,&fbo); glDeleteTextures(1,&output);
    check(tracer.reload(), "F5 shader reload succeeds");
    check(tracer.samples() == 0, "shader reload resets accumulation");
    check(!rtm::checkGLErrors("GPU regression",__FILE__,__LINE__), "no OpenGL errors");
    std::printf("Path tracer integration: %d failures\n",failures);
    return failures == 0 ? 0 : 1;
}

#ifndef RTM_TEST_EXTERNAL_CONTEXT
int main() {
    if (!glfwInit()) return 77;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(128,64,"Tracer tests",nullptr,nullptr);
    if (!window) { glfwTerminate(); return 77; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) return 1;
    const int result = runPathTracerTests();
    glfwDestroyWindow(window);
    glfwTerminate();
    return result;
}
#endif
