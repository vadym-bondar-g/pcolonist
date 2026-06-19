#pragma once

#include "pcolonist/render/Shader.hpp"
#include "pcolonist/ui/MainMenu.hpp"

#include <glm/vec3.hpp>

#include <cstdint>
#include <memory>
#include <string_view>
#include <unordered_map>

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
    CycleSkyQuality,
    RespawnPlayer,
    TeleportVolcano,
    TeleportNextLandmark,
    CycleWeather,
    SetNoon,
    SetNight,
    Quit,
};

class AudioSystem;
class Inventory;
class Registry;
class WeatherSystem;

struct ObjectiveHudState {
    glm::vec3 playerPosition{0.0F};
    bool hasWood = false;
    bool hasStone = false;
    bool hasWater = false;
    bool fireLit = false;
    bool nearShelter = false;
    bool nearRuin = false;
    std::string_view contextHint;
    float health = 100.0F;
    float thirst = 100.0F;
    float hunger = 100.0F;
    float fatigue = 0.0F;
    float bodyTemperature = 37.0F;
    bool sick = false;
    std::string_view survivalBiome;
    std::string_view survivalLocation;
    std::string_view survivalWarning;
    std::string_view craftingHint;
    int discoveredLocations = 0;
    int totalLocations = 0;
    int storyClues = 0;
    int totalStoryClues = 0;
    int secretsFound = 0;
    int totalSecrets = 0;
    bool discoveryBlocked = false;
    std::string_view discoveryMessage;
    std::string_view discoveryClue;
};

class UiSystem {
public:
    UiSystem() = default;
    ~UiSystem();

    void initialize();
    void shutdown();
    void resize(int width, int height);
    void setPointerPosition(double x, double y);
    void setFrameCounterVisible(bool visible);
    void render(
        bool fullscreen,
        bool cursorCaptured,
        bool menuOpen,
        bool vsync,
        int frameLimit,
        bool shadows,
        bool bloom,
        std::string_view skyQuality,
        const WeatherSystem& weather,
        const Inventory& inventory,
        const ObjectiveHudState& objectives,
        bool inventoryOpen,
        bool debugPanelOpen);
    void updateTitle(
        GLFWwindow* window,
        const Registry& registry,
        const AudioSystem& audio,
        float deltaTime,
        bool menuOpen);
    [[nodiscard]] bool fullscreenButtonContains(double x, double y) const;
    [[nodiscard]] UiAction menuActionAt(double x, double y);
    [[nodiscard]] UiAction debugActionAt(double x, double y) const;

private:
    struct TtfGlyph {
        unsigned int texture = 0;
        int width = 0;
        int height = 0;
        int bearingX = 0;
        int bearingY = 0;
        unsigned int advance = 0;
    };

    void rectangle(float x, float y, float width, float height, const glm::vec4& color, float radius = 0.0F);
    void text(float x, float y, std::string_view value, float scale, const glm::vec4& color);
    void bitmapText(float x, float y, std::string_view value, float scale, const glm::vec4& color);
    void ttfText(float x, float y, std::string_view value, float scale, const glm::vec4& color);
    [[nodiscard]] const TtfGlyph* glyphTexture(char32_t character, int pixelSize);
    [[nodiscard]] bool initializeFont();
    void shutdownFont();

    std::unique_ptr<Shader> shader_;
    unsigned int vertexArray_ = 0;
    int width_ = 1280;
    int height_ = 720;
    float elapsed_ = 0.0F;
    float animationTime_ = 0.0F;
    std::uint32_t frames_ = 0;
    unsigned int currentFps_ = 0;
    bool frameCounterVisible_ = false;
    double pointerX_ = 0.0;
    double pointerY_ = 0.0;
    MainMenu mainMenu_;
    void* fontLibrary_ = nullptr;
    void* fontFace_ = nullptr;
    int fontPixelSize_ = 0;
    std::unordered_map<std::uint64_t, TtfGlyph> ttfGlyphs_;
};

} // namespace pcolonist
