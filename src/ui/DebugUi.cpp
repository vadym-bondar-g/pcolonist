#include "pcolonist/ui/DebugUi.hpp"

#ifdef PCOLONIST_ENABLE_IMGUI_DEBUG
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#endif

#include <algorithm>
#include <string>

namespace pcolonist {

#ifdef PCOLONIST_ENABLE_IMGUI_DEBUG

namespace {

void applyAdventureDebugStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0F;
    style.ChildRounding = 5.0F;
    style.FrameRounding = 4.0F;
    style.ScrollbarRounding = 6.0F;
    style.GrabRounding = 4.0F;
    style.WindowBorderSize = 1.0F;
    style.FrameBorderSize = 1.0F;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = {0.08F, 0.07F, 0.045F, 0.92F};
    colors[ImGuiCol_ChildBg] = {0.10F, 0.085F, 0.055F, 0.72F};
    colors[ImGuiCol_Border] = {0.63F, 0.47F, 0.25F, 0.72F};
    colors[ImGuiCol_FrameBg] = {0.18F, 0.145F, 0.09F, 0.86F};
    colors[ImGuiCol_FrameBgHovered] = {0.27F, 0.21F, 0.12F, 0.92F};
    colors[ImGuiCol_FrameBgActive] = {0.35F, 0.26F, 0.13F, 0.95F};
    colors[ImGuiCol_TitleBg] = {0.13F, 0.10F, 0.055F, 0.98F};
    colors[ImGuiCol_TitleBgActive] = {0.21F, 0.15F, 0.075F, 0.98F};
    colors[ImGuiCol_Button] = {0.24F, 0.18F, 0.09F, 0.92F};
    colors[ImGuiCol_ButtonHovered] = {0.36F, 0.27F, 0.12F, 0.95F};
    colors[ImGuiCol_ButtonActive] = {0.48F, 0.34F, 0.14F, 1.0F};
    colors[ImGuiCol_CheckMark] = {0.82F, 0.62F, 0.28F, 1.0F};
    colors[ImGuiCol_SliderGrab] = {0.74F, 0.54F, 0.22F, 1.0F};
    colors[ImGuiCol_Header] = {0.30F, 0.22F, 0.10F, 0.78F};
    colors[ImGuiCol_HeaderHovered] = {0.40F, 0.30F, 0.13F, 0.86F};
    colors[ImGuiCol_HeaderActive] = {0.50F, 0.36F, 0.16F, 0.92F};
    colors[ImGuiCol_PlotLines] = {0.56F, 0.78F, 0.42F, 1.0F};
    colors[ImGuiCol_Text] = {0.93F, 0.86F, 0.72F, 1.0F};
    colors[ImGuiCol_TextDisabled] = {0.55F, 0.49F, 0.40F, 1.0F};
}

float currentFps(float deltaTime) {
    return deltaTime > 0.000001F ? 1.0F / deltaTime : 0.0F;
}

} // namespace

void DebugUi::initialize(GLFWwindow* window) {
    if (initialized_) {
        return;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    applyAdventureDebugStyle();
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL3_Init("#version 330");
    initialized_ = true;
}

void DebugUi::shutdown() {
    if (!initialized_) {
        return;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
    open_ = false;
}

void DebugUi::render(const DebugUiStats& stats) {
    if (!initialized_) {
        return;
    }

    frameTimes_[frameTimeCursor_ % frameTimes_.size()] = stats.deltaTime * 1000.0F;
    ++frameTimeCursor_;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (open_) {
        ImGui::SetNextWindowSize({380.0F, 360.0F}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Performance / Streaming", &open_)) {
            ImGui::Text("FPS %.1f", currentFps(stats.deltaTime));
            ImGui::Text("Frame %.2f ms", stats.deltaTime * 1000.0F);
            ImGui::PlotLines("Frame time", frameTimes_.data(), static_cast<int>(frameTimes_.size()), 0, nullptr, 0.0F, 40.0F, {0.0F, 82.0F});
            ImGui::Separator();
            ImGui::Text("Limit %s", stats.frameLimit > 0 ? std::to_string(stats.frameLimit).c_str() : "off");
            ImGui::Text("VSync %s  Shadows %s  Bloom %s", stats.vsync ? "on" : "off", stats.shadows ? "on" : "off", stats.bloom ? "on" : "off");
            ImGui::Separator();
            ImGui::Text("Chunks active %zu  pending %zu  missing %zu",
                stats.streaming.activeChunks,
                stats.streaming.pendingChunks,
                stats.streaming.missingChunks);
            ImGui::Text("LOD0 %zu  LOD1 %zu  LOD2 %zu",
                stats.streaming.lod0Chunks,
                stats.streaming.lod1Chunks,
                stats.streaming.lod2Chunks);
            ImGui::Text("Scene entities in chunks %zu", stats.streaming.sceneEntities);
            ImGui::Text("Center chunk [%d, %d]", stats.streaming.center.x, stats.streaming.center.z);
            ImGui::Text("Radius load %d  unload %d  size %.0f",
                stats.streaming.loadRadius,
                stats.streaming.unloadRadius,
                stats.streaming.chunkSize);
        }
        ImGui::End();

        ImGui::SetNextWindowSize({380.0F, 430.0F}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Island Debug", &open_)) {
            ImGui::Text("Player %.1f %.1f %.1f", stats.playerPosition.x, stats.playerPosition.y, stats.playerPosition.z);
            ImGui::Separator();
            ImGui::Text("Entities %zu", stats.entities);
            ImGui::Text("Meshes %zu  Terrain chunks %zu", stats.meshRenderers, stats.terrainChunks);
            ImGui::Text("Static colliders %zu  Dynamic colliders %zu", stats.staticColliders, stats.dynamicColliders);
            ImGui::Text("Resources %zu  Animations %zu", stats.resources, stats.animated);
            ImGui::Text("Water %zu  Lava %zu  Fire lights %zu", stats.waterSurfaces, stats.lavaSurfaces, stats.fireLights);
            ImGui::Separator();
            ImGui::Checkbox("Terrain", &rendererOptions_.showTerrain);
            ImGui::Checkbox("Objects / decor", &rendererOptions_.showObjects);
            ImGui::Checkbox("Resource nodes", &rendererOptions_.showResources);
            ImGui::Checkbox("Water", &rendererOptions_.showWater);
            ImGui::Checkbox("Lava", &rendererOptions_.showLava);
            ImGui::Checkbox("Fire / smoke", &rendererOptions_.showFire);
            ImGui::Separator();
            ImGui::Checkbox("Collider markers", &showColliders_);
            ImGui::Checkbox("Landmark markers", &showLandmarks_);
            ImGui::Separator();
            ImGui::TextDisabled("Collider and landmark markers are UI-only.");
        }
        ImGui::End();

        ImGui::SetNextWindowSize({380.0F, 300.0F}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Gameplay Tuning", &open_)) {
            ImGui::SliderFloat("Terrain distance", &rendererOptions_.terrainDrawDistance, 64.0F, 760.0F, "%.0f m");
            ImGui::SliderFloat("Object distance", &rendererOptions_.objectDrawDistance, 32.0F, 520.0F, "%.0f m");
            ImGui::Checkbox("Wireframe", &rendererOptions_.wireframe);
            ImGui::Separator();
            if (ImGui::Button("Low scene cost")) {
                rendererOptions_.terrainDrawDistance = 220.0F;
                rendererOptions_.objectDrawDistance = 140.0F;
                rendererOptions_.showFire = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Default")) {
                rendererOptions_ = RendererDebugOptions{};
            }
            ImGui::Separator();
            ImGui::TextDisabled("Runtime only. Values are not saved.");
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void DebugUi::toggleOpen() {
    if (initialized_) {
        open_ = !open_;
    }
}

void DebugUi::close() {
    open_ = false;
}

bool DebugUi::open() const {
    return open_;
}

bool DebugUi::available() const {
    return initialized_;
}

bool DebugUi::wantsMouse() const {
    return initialized_ && open_ && ImGui::GetIO().WantCaptureMouse;
}

bool DebugUi::wantsKeyboard() const {
    return initialized_ && open_ && ImGui::GetIO().WantCaptureKeyboard;
}

RendererDebugOptions DebugUi::rendererDebugOptions() const {
    return rendererOptions_;
}

void DebugUi::handleKey(GLFWwindow* window, int key, int scanCode, int action, int modifiers) {
    if (initialized_) {
        ImGui_ImplGlfw_KeyCallback(window, key, scanCode, action, modifiers);
    }
}

void DebugUi::handleCursorPosition(GLFWwindow* window, double x, double y) {
    if (initialized_) {
        ImGui_ImplGlfw_CursorPosCallback(window, x, y);
    }
}

void DebugUi::handleMouseButton(GLFWwindow* window, int button, int action, int modifiers) {
    if (initialized_) {
        ImGui_ImplGlfw_MouseButtonCallback(window, button, action, modifiers);
    }
}

#else

void DebugUi::initialize(GLFWwindow*) {}
void DebugUi::shutdown() {}
void DebugUi::render(const DebugUiStats&) {}
void DebugUi::toggleOpen() {}
void DebugUi::close() {}
bool DebugUi::open() const { return false; }
bool DebugUi::available() const { return false; }
bool DebugUi::wantsMouse() const { return false; }
bool DebugUi::wantsKeyboard() const { return false; }
RendererDebugOptions DebugUi::rendererDebugOptions() const { return RendererDebugOptions{}; }
void DebugUi::handleKey(GLFWwindow*, int, int, int, int) {}
void DebugUi::handleCursorPosition(GLFWwindow*, double, double) {}
void DebugUi::handleMouseButton(GLFWwindow*, int, int, int) {}

#endif

} // namespace pcolonist
