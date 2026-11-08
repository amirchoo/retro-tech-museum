// Retro Tech Museum -- entry point.
//
// Controls:  click the view to capture the mouse, Tab/Esc to release it
//            WASD move, Shift sprint, F fly mode (Space/Ctrl up/down)
//            T toggle raster / path tracing, F5 reload shaders
//
// Flags:     --smoke-test <out.png>   render a few frames, save a screenshot, exit (0 = OK)
//            --mode raster|cast|trace|split   start in this render mode
//            --view shaded|normals|instances   (cast mode debug view)
//            --frames N --spp N --bounces N --scale X --indirect-clamp X --test-scene --no-ui

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stb_image_write.h>

#include "core/camera.h"
#include "core/gl.h"
#include "core/mesh_gpu.h"
#include "core/shader.h"
#include "core/window.h"
#include "scene/gpu_scene.h"
#include "scene/scene.h"
#include "scene/gallery.h"
#include "scene/lights.h"
#include "tracer/path_tracer.h"
#include "tracer/test_scene.h"

#ifdef _WIN32
// Laptops with Intel + NVIDIA/AMD otherwise start us on the slow integrated GPU.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace {

enum class RenderMode { Raster = 0, PathTrace = 1, Split = 2, Cast = 3 };

bool saveScreenshot(const char* path, int w, int h) {
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    stbi_flip_vertically_on_write(1);
    return stbi_write_png(path, w, h, 3, px.data(), w * 3) != 0;
}

void drawRaster(const rtm::Scene& scene, const std::vector<rtm::GpuMesh>& meshes, const rtm::Shader& sh) {
    for (const rtm::Instance& inst : scene.instances) {
        const glm::mat4 model = inst.transform.matrix();
        sh.set("uModel", model);
        sh.set("uNormalMatrix", glm::mat3(glm::transpose(glm::inverse(model))));
        const rtm::Mesh& mesh = scene.meshes[static_cast<size_t>(inst.mesh)];
        for (const rtm::SubMesh& sm : mesh.submeshes) {
            const rtm::Material& m = scene.materials[static_cast<size_t>(sm.material)];
            sh.set("uBaseColor", m.baseColor);
            sh.set("uMetallic", m.metallic);
            sh.set("uRoughness", m.roughness);
            sh.set("uEmission", m.emission);
            meshes[static_cast<size_t>(inst.mesh)].drawSubMesh(sm);
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    const char* smokeOut = nullptr;
    RenderMode mode = RenderMode::Raster;
    int castView = 0;
    int smokeFrames = 30;
    bool testScene = false, noUi = false;
    rtm::TracerSettings initialSettings;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--smoke-test") == 0 && i + 1 < argc) smokeOut = argv[++i];
        else if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            const char* value = argv[++i];
            mode = std::strcmp(value, "cast") == 0 ? RenderMode::Cast :
                   std::strcmp(value, "trace") == 0 ? RenderMode::PathTrace :
                   std::strcmp(value, "split") == 0 ? RenderMode::Split : RenderMode::Raster;
        }
        else if (std::strcmp(argv[i], "--view") == 0 && i + 1 < argc) {
            const char* v = argv[++i];
            castView = std::strcmp(v, "normals") == 0 ? 1 : std::strcmp(v, "instances") == 0 ? 2 : 0;
        }
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) smokeFrames = std::max(1, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--spp") == 0 && i + 1 < argc) initialSettings.samplesPerFrame = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--bounces") == 0 && i + 1 < argc) initialSettings.maxBounces = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--scale") == 0 && i + 1 < argc) initialSettings.resolutionScale = static_cast<float>(std::atof(argv[++i]));
        else if (std::strcmp(argv[i], "--indirect-clamp") == 0 && i + 1 < argc) initialSettings.indirectClamp = static_cast<float>(std::atof(argv[++i]));
        else if (std::strcmp(argv[i], "--test-scene") == 0) testScene = true;
        else if (std::strcmp(argv[i], "--no-ui") == 0) noUi = true;
    }

    rtm::Window window(1280, 720, "Retro Tech Museum");
    if (!window.ok()) return 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;  // don't litter imgui.ini into the repo
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window.handle(), true);
    ImGui_ImplOpenGL3_Init(rtm::kGLSLVersion);

    rtm::Shader rasterShader("raster_basic.vert", "raster_basic.frag");
    rtm::Shader castShader("fullscreen.vert", "debug_cast.frag");
    rtm::PathTracer tracer;
    tracer.setSettings(initialSettings);
    if (!rasterShader.ok() || !castShader.ok() || !tracer.ok()) return 1;

    rtm::Scene scene = testScene ? rtm::makeTracerTestScene() : rtm::makeGallery();
    std::vector<rtm::GpuMesh> gpuMeshes;
    gpuMeshes.reserve(scene.meshes.size());
    for (const rtm::Mesh& m : scene.meshes) gpuMeshes.emplace_back(m);
    rtm::GpuScene gpuScene;
    gpuScene.upload(scene);

    GLuint emptyVao = 0;  // core profile needs a bound VAO even for attribute-less draws
    glGenVertexArrays(1, &emptyVao);

    rtm::Camera camera;
    camera.position = {0.0f, 1.6f, 6.0f};  // hallway entrance, looking down the hall (-Z)
    rtm::SpotLight light;
    float exposure = 1.0f, divider = 0.5f;
    std::vector<float> yawDeg(scene.instances.size(), 0.0f);
    for (size_t i = 0; i < scene.instances.size(); ++i)
        yawDeg[i] = glm::degrees(glm::eulerAngles(scene.instances[i].transform.rotation).y);

    glEnable(GL_DEPTH_TEST);
    GL_CHECK("init");

    bool f5WasDown = false, fWasDown = false, tWasDown = false;
    bool draggingDivider = false;
    double lastTime = glfwGetTime();
    int frame = 0;
    bool renderOk = true;
    auto reloadShaders = [&] {
        rasterShader.reload();
        castShader.reload();
        tracer.reload();
    };

    while (!window.shouldClose()) {
        glfwPollEvents();
        const double now = glfwGetTime();
        const float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ---- input ----
        GLFWwindow* h = window.handle();
        ImGuiIO& io = ImGui::GetIO();
        const bool overDivider = mode == RenderMode::Split && !noUi &&
            std::abs(io.MousePos.x - divider * io.DisplaySize.x) < 12.0f;
        if (!window.cursorCaptured() && !io.WantCaptureMouse && overDivider && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            draggingDivider = true;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || mode != RenderMode::Split) draggingDivider = false;
        if (draggingDivider && io.DisplaySize.x > 0)
            divider = std::clamp(io.MousePos.x / io.DisplaySize.x, 0.0f, 1.0f);
        if (!window.cursorCaptured() && !io.WantCaptureMouse && !overDivider && !draggingDivider &&
            glfwGetMouseButton(h, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
            window.setCursorCaptured(true);
        if (window.cursorCaptured() &&
            (glfwGetKey(h, GLFW_KEY_TAB) == GLFW_PRESS || glfwGetKey(h, GLFW_KEY_ESCAPE) == GLFW_PRESS))
            window.setCursorCaptured(false);

        auto pressed = [&](int key, bool& wasDown) {
            const bool down = glfwGetKey(h, key) == GLFW_PRESS;
            const bool edge = down && !wasDown && (!io.WantCaptureKeyboard || window.cursorCaptured());
            wasDown = down;
            return edge;
        };
        const RenderMode previousMode = mode;
        if (pressed(GLFW_KEY_F5, f5WasDown)) reloadShaders();
        if (pressed(GLFW_KEY_F, fWasDown)) camera.fly = !camera.fly;
        if (pressed(GLFW_KEY_T, tWasDown)) mode = mode == RenderMode::Raster ? RenderMode::PathTrace : RenderMode::Raster;

        if ((!io.WantCaptureKeyboard || window.cursorCaptured()) && camera.update(h, dt, window.cursorCaptured()))
            tracer.reset();
        // TODO(아미르): clamp camera.position against the BVH here (collision).

        // ---- UI ----
        if (!noUi) {
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(300, 0), ImVec2(520, std::max(100.0f, io.DisplaySize.y - 20)));
            ImGui::Begin("Museum", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("%.1f fps (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);
            ImGui::Text("GPU: %s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
            ImGui::Text("Triangles: %zu (%zu instances)", scene.triangleCount(), scene.instances.size());
            ImGui::Text("Camera: %.2f %.2f %.2f", camera.position.x, camera.position.y, camera.position.z);
            ImGui::Separator();
            int modeInt = static_cast<int>(mode);
            ImGui::RadioButton("Raster", &modeInt, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Path trace (T)", &modeInt, 1);
            ImGui::RadioButton("Split comparison", &modeInt, 2);
            ImGui::SameLine();
            ImGui::RadioButton("Debug ray cast", &modeInt, 3);
            mode = static_cast<RenderMode>(modeInt);
            if (mode == RenderMode::Cast) ImGui::Combo("View", &castView, "Shaded + shadows\0Normals\0Instance ID\0");
            ImGui::Checkbox("Fly mode (F)", &camera.fly);
            ImGui::SliderFloat("Move speed", &camera.moveSpeed, 0.5f, 10.0f);
            ImGui::SliderFloat("FOV", &camera.fovY, 30.0f, 100.0f);
            ImGui::SliderFloat("Exposure", &exposure, 0.1f, 4.0f);
            if (mode == RenderMode::PathTrace || mode == RenderMode::Split) {
                rtm::TracerSettings settings = tracer.settings();
                ImGui::SliderFloat("Trace resolution", &settings.resolutionScale, 0.25f, 1.0f, "%.2f");
                ImGui::SliderInt("Samples / frame", &settings.samplesPerFrame, 1, 16);
                ImGui::SliderInt("Max bounces", &settings.maxBounces, 1, 16);
                ImGui::SliderFloat("Indirect clamp (0 off)", &settings.indirectClamp, 0.0f, 20.0f, "%.1f");
                tracer.setSettings(settings);
                ImGui::Text("%u frames / %u spp (%d x %d)", tracer.frames(), tracer.samples(), tracer.width(), tracer.height());
                if (ImGui::Button("Reset accumulation")) tracer.reset();
            }
            if (mode == RenderMode::Split) {
                ImGui::SliderFloat("Divider", &divider, 0.0f, 1.0f, "%.2f");
                ImGui::TextDisabled("Raster left / path trace right");
            }
            if (ImGui::Checkbox("Material test scene", &testScene)) {
                scene = testScene ? rtm::makeTracerTestScene() : rtm::makeGallery();
                gpuMeshes.clear();
                for (const rtm::Mesh& mesh : scene.meshes) gpuMeshes.emplace_back(mesh);
                gpuScene.upload(scene);
                yawDeg.resize(scene.instances.size());
                for (size_t i = 0; i < scene.instances.size(); ++i)
                    yawDeg[i] = glm::degrees(glm::eulerAngles(scene.instances[i].transform.rotation).y);
                camera.position = {0.0f, 1.6f, 6.0f};
                camera.yaw = -90.0f;
                camera.pitch = 0.0f;
                tracer.reset();
            }
            if (ImGui::CollapsingHeader("Exhibits", ImGuiTreeNodeFlags_DefaultOpen)) {
                bool moved = false;
                for (size_t i = 0; i < scene.instances.size(); ++i) {
                    rtm::Instance& inst = scene.instances[i];
                    if (!inst.exhibit) continue;
                    if (ImGui::SliderFloat(inst.name.c_str(), &yawDeg[i], -180.0f, 180.0f, "%.0f deg")) {
                        inst.transform.rotation = glm::angleAxis(glm::radians(yawDeg[i]), glm::vec3(0, 1, 0));
                        moved = true;
                    }
                }
                if (moved) {
                    gpuScene.updateInstances(scene);
                    tracer.reset();
                }
            }
            if (ImGui::CollapsingHeader("Spotlight")) {
                ImGui::DragFloat3("Position", &light.position.x, 0.05f);
                ImGui::DragFloat3("Target", &light.target.x, 0.05f);
                ImGui::ColorEdit3("Color", &light.color.x);
                ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 150.0f);
                ImGui::SliderFloat("Light radius", &light.radius, 0.0f, 1.0f);
                ImGui::SliderFloat("Inner cone", &light.innerDeg, 1.0f, 60.0f);
                light.outerDeg = std::max(light.outerDeg, light.innerDeg + 0.1f);
                ImGui::SliderFloat("Outer cone", &light.outerDeg, light.innerDeg + 0.1f, 80.0f);
                ImGui::SliderFloat("Ambient", &light.ambient, 0.0f, 0.5f);
            }
            if (ImGui::Button("Reload shaders (F5)")) reloadShaders();
            ImGui::TextDisabled("Click view to look, Tab/Esc to release");
            ImGui::End();
        }
        if (mode != previousMode) tracer.reset();

        // ---- render ----
        int fbw, fbh;
        window.framebufferSize(fbw, fbh);
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (fbw > 0 && fbh > 0) {
            const glm::mat4 view = camera.view();
            const glm::mat4 proj = camera.projection(static_cast<float>(fbw) / static_cast<float>(fbh));
            if (mode == RenderMode::Raster || mode == RenderMode::Split) {
                rasterShader.use();
                rasterShader.set("uView", view);
                rasterShader.set("uProj", proj);
                rasterShader.set("uCameraPos", camera.position);
                rasterShader.set("uExposure", exposure);
                light.apply(rasterShader);
                drawRaster(scene, gpuMeshes, rasterShader);
            }
            if (mode == RenderMode::PathTrace || mode == RenderMode::Split) {
                renderOk = tracer.render(gpuScene, glm::inverse(proj * view), camera.position, light, fbw, fbh);
                if (!renderOk) break;
                tracer.display(fbw, fbh, exposure, mode == RenderMode::Split ? divider : 0.0f);
                if (mode == RenderMode::Split && !noUi) {
                    const ImVec2 displaySize = io.DisplaySize;
                    const float x = divider * displaySize.x;
                    ImDrawList* overlay = ImGui::GetForegroundDrawList();
                    overlay->AddLine(ImVec2(x, 0), ImVec2(x, displaySize.y), IM_COL32(255, 255, 255, 200), 2.0f);
                }
            } else if (mode == RenderMode::Cast) {
                castShader.use();
                gpuScene.bind(castShader.id());
                castShader.set("uInvViewProj", glm::inverse(proj * view));
                castShader.set("uCameraPos", camera.position);
                castShader.set("uView", castView);
                castShader.set("uExposure", exposure);
                light.apply(castShader);
                glDisable(GL_DEPTH_TEST);
                glBindVertexArray(emptyVao);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                glBindVertexArray(0);
                glEnable(GL_DEPTH_TEST);
            }
        } else tracer.reset();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        GL_CHECK("frame");

        if (smokeOut && ++frame == smokeFrames) {
            const bool ok = saveScreenshot(smokeOut, fbw, fbh) && !rtm::checkGLErrors("smoke test", __FILE__, __LINE__);
            std::printf("[smoke-test] %s -> %s (%u spp, %d x %d)\n", ok ? "OK" : "FAILED", smokeOut,
                        tracer.samples(), tracer.width(), tracer.height());
            glDeleteVertexArrays(1, &emptyVao);
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            return ok ? 0 : 1;
        }
        window.swap();
    }

    glDeleteVertexArrays(1, &emptyVao);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    return renderOk ? 0 : 1;
}
