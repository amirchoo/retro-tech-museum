// Retro Tech Museum -- entry point.
//
// Controls:  click the view to capture the mouse, Tab/Esc to release it
//            WASD move, Shift sprint, F fly mode (Space/Ctrl up/down)
//            T toggle raster / ray-cast, F5 reload shaders
//
// Flags:     --smoke-test <out.png>   render a few frames, save a screenshot, exit (0 = OK)
//            --mode raster|cast       start in this render mode
//            --view shaded|normals|instances   (cast mode debug view)

#include <cmath>
#include <cstdio>
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

#ifdef _WIN32
// Laptops with Intel + NVIDIA/AMD otherwise start us on the slow integrated GPU.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace {

enum class RenderMode { Raster = 0, Cast = 1 };  // Cast -> PathTrace once 하직's tracer lands

struct SpotLight {
    // One wide ceiling light for now; per-exhibit spotlights are 알리's (lighting).
    glm::vec3 position{0.0f, 3.4f, 0.0f};
    glm::vec3 target{0.0f, 0.0f, -0.5f};
    glm::vec3 color{1.0f, 0.92f, 0.8f};
    float intensity = 6.0f;
    float innerDeg = 50.0f, outerDeg = 72.0f;
    float ambient = 0.12f;

    void apply(const rtm::Shader& s) const {
        s.set("uLightPos", position);
        s.set("uLightDir", glm::normalize(target - position));
        s.set("uLightColor", color * intensity);
        s.set("uCosInner", std::cos(glm::radians(innerDeg)));
        s.set("uCosOuter", std::cos(glm::radians(outerDeg)));
        s.set("uAmbient", ambient);
    }
};

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
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--smoke-test") == 0 && i + 1 < argc) smokeOut = argv[++i];
        else if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc)
            mode = std::strcmp(argv[++i], "cast") == 0 ? RenderMode::Cast : RenderMode::Raster;
        else if (std::strcmp(argv[i], "--view") == 0 && i + 1 < argc) {
            const char* v = argv[++i];
            castView = std::strcmp(v, "normals") == 0 ? 1 : std::strcmp(v, "instances") == 0 ? 2 : 0;
        }
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
    if (!rasterShader.ok() || !castShader.ok()) return 1;

    rtm::Scene scene = rtm::makeGallery();
    std::vector<rtm::GpuMesh> gpuMeshes;
    gpuMeshes.reserve(scene.meshes.size());
    for (const rtm::Mesh& m : scene.meshes) gpuMeshes.emplace_back(m);
    rtm::GpuScene gpuScene;
    gpuScene.upload(scene);

    GLuint emptyVao = 0;  // core profile needs a bound VAO even for attribute-less draws
    glGenVertexArrays(1, &emptyVao);

    rtm::Camera camera;
    camera.position = {0.0f, 1.6f, 6.0f};  // hallway entrance, looking down the hall (-Z)
    SpotLight light;
    std::vector<float> yawDeg(scene.instances.size(), 0.0f);
    for (size_t i = 0; i < scene.instances.size(); ++i)
        yawDeg[i] = glm::degrees(glm::eulerAngles(scene.instances[i].transform.rotation).y);

    glEnable(GL_DEPTH_TEST);
    GL_CHECK("init");

    bool f5WasDown = false, fWasDown = false, tWasDown = false;
    double lastTime = glfwGetTime();
    int frame = 0;

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
        if (!window.cursorCaptured() && !io.WantCaptureMouse &&
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
        if (pressed(GLFW_KEY_F5, f5WasDown)) { rasterShader.reload(); castShader.reload(); }
        if (pressed(GLFW_KEY_F, fWasDown)) camera.fly = !camera.fly;
        if (pressed(GLFW_KEY_T, tWasDown)) mode = mode == RenderMode::Raster ? RenderMode::Cast : RenderMode::Raster;

        if (!io.WantCaptureKeyboard || window.cursorCaptured())
            camera.update(h, dt, window.cursorCaptured());
        // TODO(아미르): clamp camera.position against the BVH here (collision).

        // ---- UI ----
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::Begin("Museum", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("%.1f fps (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);
        ImGui::Text("GPU: %s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        ImGui::Text("Triangles: %zu (%zu instances)", scene.triangleCount(), scene.instances.size());
        ImGui::Text("Camera: %.2f %.2f %.2f", camera.position.x, camera.position.y, camera.position.z);
        ImGui::Separator();
        int modeInt = static_cast<int>(mode);
        ImGui::RadioButton("Raster", &modeInt, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Ray cast (T)", &modeInt, 1);
        mode = static_cast<RenderMode>(modeInt);
        if (mode == RenderMode::Cast) ImGui::Combo("View", &castView, "Shaded + shadows\0Normals\0Instance ID\0");
        ImGui::Checkbox("Fly mode (F)", &camera.fly);
        ImGui::SliderFloat("Move speed", &camera.moveSpeed, 0.5f, 10.0f);
        ImGui::SliderFloat("FOV", &camera.fovY, 30.0f, 100.0f);
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
            if (moved) gpuScene.updateInstances(scene);  // TLAS + instance texture only; BLAS untouched
        }
        if (ImGui::CollapsingHeader("Spotlight")) {
            ImGui::DragFloat3("Position", &light.position.x, 0.05f);
            ImGui::DragFloat3("Target", &light.target.x, 0.05f);
            ImGui::ColorEdit3("Color", &light.color.x);
            ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 20.0f);
            ImGui::SliderFloat("Inner cone", &light.innerDeg, 1.0f, 60.0f);
            ImGui::SliderFloat("Outer cone", &light.outerDeg, light.innerDeg, 80.0f);
            ImGui::SliderFloat("Ambient", &light.ambient, 0.0f, 0.5f);
        }
        if (ImGui::Button("Reload shaders (F5)")) { rasterShader.reload(); castShader.reload(); }
        ImGui::TextDisabled("Click view to look, Tab/Esc to release");
        ImGui::End();

        // ---- render ----
        int fbw, fbh;
        window.framebufferSize(fbw, fbh);
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (fbw > 0 && fbh > 0) {
            const glm::mat4 view = camera.view();
            const glm::mat4 proj = camera.projection(static_cast<float>(fbw) / static_cast<float>(fbh));
            if (mode == RenderMode::Raster) {
                rasterShader.use();
                rasterShader.set("uView", view);
                rasterShader.set("uProj", proj);
                rasterShader.set("uCameraPos", camera.position);
                light.apply(rasterShader);
                drawRaster(scene, gpuMeshes, rasterShader);
            } else {
                castShader.use();
                gpuScene.bind(castShader.id());
                castShader.set("uInvViewProj", glm::inverse(proj * view));
                castShader.set("uCameraPos", camera.position);
                castShader.set("uView", castView);
                light.apply(castShader);
                glDisable(GL_DEPTH_TEST);
                glBindVertexArray(emptyVao);
                glDrawArrays(GL_TRIANGLES, 0, 3);
                glBindVertexArray(0);
                glEnable(GL_DEPTH_TEST);
            }
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        GL_CHECK("frame");

        if (smokeOut && ++frame == 30) {
            const bool ok = saveScreenshot(smokeOut, fbw, fbh) && !rtm::checkGLErrors("smoke test", __FILE__, __LINE__);
            std::printf("[smoke-test] %s -> %s\n", ok ? "OK" : "FAILED", smokeOut);
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
    return 0;
}
