#pragma once

#include "pcolonist/render/Shader.hpp"

#include <cstdint>
#include <memory>
#include <string_view>

struct GLFWwindow;

namespace pcolonist {

enum class UiAction {
    None,
    Resume,
    ToggleFullscreen,
    ToggleVsync,
    CycleFrameLimit,
    ToggleShadows,
    ToggleBloom,
    RespawnPlayer,
    TeleportVolcano,
    TeleportNextGrotto,
    CycleWeather,
    SetNoon,
    SetNight,
    Quit,
};

class AudioSystem;
class Inventory;
class Registry;
class WeatherSystem;

class UiSystem {
public:
    UiSystem() = default;
    ~UiSystem();

    void initialize();
    void shutdown();
    void resize(int width, int height);
    void setPointerPosition(double x, double y);
    void render(
        bool fullscreen,
        bool cursorCaptured,
        bool menuOpen,
        bool vsync,
        int frameLimit,
        bool shadows,
        bool bloom,
        const WeatherSystem& weather,
        const Inventory& inventory,
        bool inventoryOpen,
        bool debugPanelOpen);
    void updateTitle(
        GLFWwindow* window,
        const Registry& registry,
        const AudioSystem& audio,
        float deltaTime,
        bool menuOpen);
    [[nodiscard]] bool fullscreenButtonContains(double x, double y) const;
    [[nodiscard]] UiAction menuActionAt(double x, double y) const;
    [[nodiscard]] UiAction debugActionAt(double x, double y) const;

private:
    void rectangle(float x, float y, float width, float height, const glm::vec4& color, float radius = 0.0F);
    void text(float x, float y, std::string_view value, float scale, const glm::vec4& color);

    std::unique_ptr<Shader> shader_;
    unsigned int vertexArray_ = 0;
    int width_ = 1280;
    int height_ = 720;
    float elapsed_ = 0.0F;
    std::uint32_t frames_ = 0;
    double pointerX_ = 0.0;
    double pointerY_ = 0.0;
};

} // namespace pcolonist
