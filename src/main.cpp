// Retro Tech Museum -- entry point.
//
// Controls:  click the view to capture the mouse, Tab/Esc to release it
//            WASD move, Shift sprint, F fly mode (Space/Ctrl up/down), F5 reload shaders
//
// Flags:     --smoke-test <out.png>   render a few frames, save a screenshot, exit (0 = OK)

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stb_image_write.h>

#include "core/camera.h"
#include "core/gl.h"
#include "core/shader.h"
#include "core/test_scene.h"
#include "core/window.h"

namespace {

struct SpotLight {
    glm::vec3 position{0.0f, 3.4f, -1.5f};
    glm::vec3 target{0.0f, 0.0f, -2.5f};
    glm::vec3 color{1.0f, 0.92f, 0.8f};
    float intensity = 6.0f;
    float innerDeg = 25.0f, outerDeg = 40.0f;
    float ambient = 0.08f;
};

bool saveScreenshot(const char* path, int w, int h) {
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    stbi_flip_vertically_on_write(1);
    return stbi_write_png(path, w, h, 3, px.data(), w * 3) != 0;
}

}  // namespace

int main(int argc, char** argv) {
    const char* smokeOut = nullptr;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--smoke-test") == 0 && i + 1 < argc) smokeOut = argv[++i];

    rtm::Window window(1280, 720, "Retro Tech Museum");
    if (!window.ok()) return 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;  // don't litter imgui.ini into the repo
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window.handle(), true);
    ImGui_ImplOpenGL3_Init(rtm::kGLSLVersion);

    rtm::Shader shader("test_scene.vert", "test_scene.frag");
    if (!shader.ok()) return 1;
    rtm::TestScene scene;
    rtm::Camera camera;
    SpotLight light;

    glEnable(GL_DEPTH_TEST);
    GL_CHECK("init");

    bool f5WasDown = false, fWasDown = false;
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

        const bool f5 = glfwGetKey(h, GLFW_KEY_F5) == GLFW_PRESS;
        if (f5 && !f5WasDown) shader.reload();
        f5WasDown = f5;
        const bool fKey = glfwGetKey(h, GLFW_KEY_F) == GLFW_PRESS;
        if (fKey && !fWasDown && !io.WantCaptureKeyboard) camera.fly = !camera.fly;
        fWasDown = fKey;

        if (!io.WantCaptureKeyboard || window.cursorCaptured())
            camera.update(h, dt, window.cursorCaptured());
        // TODO(아미르): clamp camera.position against the BVH here (collision).

        // ---- UI ----
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::Begin("Museum", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("%.1f fps (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);
        ImGui::Text("GPU: %s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        ImGui::Text("Triangles: %zu", scene.triangleCount());
        ImGui::Text("Camera: %.2f %.2f %.2f", camera.position.x, camera.position.y, camera.position.z);
        ImGui::Separator();
        ImGui::Checkbox("Fly mode (F)", &camera.fly);
        ImGui::SliderFloat("Move speed", &camera.moveSpeed, 0.5f, 10.0f);
        ImGui::SliderFloat("FOV", &camera.fovY, 30.0f, 100.0f);
        if (ImGui::CollapsingHeader("Spotlight", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Position", &light.position.x, 0.05f);
            ImGui::DragFloat3("Target", &light.target.x, 0.05f);
            ImGui::ColorEdit3("Color", &light.color.x);
            ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 20.0f);
            ImGui::SliderFloat("Inner cone", &light.innerDeg, 1.0f, 60.0f);
            ImGui::SliderFloat("Outer cone", &light.outerDeg, light.innerDeg, 80.0f);
            ImGui::SliderFloat("Ambient", &light.ambient, 0.0f, 0.5f);
        }
        if (ImGui::Button("Reload shaders (F5)")) shader.reload();
        ImGui::TextDisabled("Click view to look, Tab/Esc to release");
        ImGui::End();

        // ---- render ----
        int fbw, fbh;
        window.framebufferSize(fbw, fbh);
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (fbw > 0 && fbh > 0) {
            const glm::mat4 model(1.0f);
            shader.use();
            shader.set("uModel", model);
            shader.set("uView", camera.view());
            shader.set("uProj", camera.projection(static_cast<float>(fbw) / static_cast<float>(fbh)));
            shader.set("uNormalMatrix", glm::mat3(glm::transpose(glm::inverse(model))));
            shader.set("uCameraPos", camera.position);
            shader.set("uLightPos", light.position);
            shader.set("uLightDir", glm::normalize(light.target - light.position));
            shader.set("uLightColor", light.color * light.intensity);
            shader.set("uCosInner", std::cos(glm::radians(light.innerDeg)));
            shader.set("uCosOuter", std::cos(glm::radians(light.outerDeg)));
            shader.set("uAmbient", light.ambient);
            scene.draw();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        GL_CHECK("frame");

        if (smokeOut && ++frame == 30) {
            const bool ok = saveScreenshot(smokeOut, fbw, fbh) && !rtm::checkGLErrors("smoke test", __FILE__, __LINE__);
            std::printf("[smoke-test] %s -> %s\n", ok ? "OK" : "FAILED", smokeOut);
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            return ok ? 0 : 1;
        }
        window.swap();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    return 0;
}
