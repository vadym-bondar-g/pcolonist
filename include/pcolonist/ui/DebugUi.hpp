#pragma once

#include "pcolonist/render/Renderer.hpp"
#include "pcolonist/world/ChunkManager.hpp"

#include <glm/vec3.hpp>

#include <array>
#include <cstddef>

struct GLFWwindow;

namespace pcolonist {

struct DebugUiStats {
    double totalTime = 0.0;
    float deltaTime = 0.0F;
    int frameLimit = 0;
    bool vsync = true;
    bool shadows = true;
    bool bloom = true;
    glm::vec3 playerPosition{0.0F};
    ChunkManager::RuntimeStats streaming{};
    std::size_t entities = 0;
    std::size_t meshRenderers = 0;
    std::size_t terrainSurfaces = 0;
    std::size_t terrainChunks = 0;
    std::size_t staticColliders = 0;
    std::size_t dynamicColliders = 0;
    std::size_t resources = 0;
    std::size_t waterSurfaces = 0;
    std::size_t lavaSurfaces = 0;
    std::size_t fireLights = 0;
    std::size_t animated = 0;
};

class DebugUi {
public:
    void initialize(GLFWwindow* window);
    void shutdown();
    void render(const DebugUiStats& stats);

    void toggleOpen();
    void close();
    [[nodiscard]] bool open() const;
    [[nodiscard]] bool available() const;
    [[nodiscard]] bool wantsMouse() const;
    [[nodiscard]] bool wantsKeyboard() const;
    [[nodiscard]] RendererDebugOptions rendererDebugOptions() const;

    void handleKey(GLFWwindow* window, int key, int scanCode, int action, int modifiers);
    void handleCursorPosition(GLFWwindow* window, double x, double y);
    void handleMouseButton(GLFWwindow* window, int button, int action, int modifiers);

private:
    std::array<float, 180> frameTimes_{};
    std::size_t frameTimeCursor_ = 0;
    bool initialized_ = false;
    bool open_ = false;
    bool showColliders_ = false;
    bool showLandmarks_ = true;
    RendererDebugOptions rendererOptions_;
};

} // namespace pcolonist
