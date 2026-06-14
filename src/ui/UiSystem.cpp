#include "pcolonist/ui/UiSystem.hpp"

#include "pcolonist/audio/AudioSystem.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/gameplay/Inventory.hpp"
#include "pcolonist/world/WeatherSystem.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <string>
#include <sstream>
#include <iomanip>

namespace {

using Glyph = std::array<unsigned char, 7>;

constexpr glm::vec4 panel{0.018F, 0.022F, 0.028F, 0.96F};
constexpr glm::vec4 panelRaised{0.032F, 0.039F, 0.048F, 0.98F};
constexpr glm::vec4 panelHover{0.055F, 0.072F, 0.084F, 1.0F};
constexpr glm::vec4 border{0.20F, 0.24F, 0.27F, 0.92F};
constexpr glm::vec4 textPrimary{0.88F, 0.90F, 0.91F, 1.0F};
constexpr glm::vec4 textMuted{0.46F, 0.51F, 0.54F, 1.0F};
constexpr glm::vec4 cyan{0.20F, 0.68F, 0.76F, 1.0F};
constexpr glm::vec4 green{0.32F, 0.68F, 0.53F, 1.0F};
constexpr glm::vec4 amber{0.78F, 0.61F, 0.30F, 1.0F};
constexpr glm::vec4 red{0.76F, 0.31F, 0.33F, 1.0F};

struct MenuLayout {
    float left;
    float top;
    float width = 560.0F;
    float height = 590.0F;
    float contentLeft;
    float contentWidth = 492.0F;
};

MenuLayout menuLayout(int width, int height) {
    const float left = static_cast<float>(width) * 0.5F - 280.0F;
    const float top = static_cast<float>(height) * 0.5F - 295.0F;
    return {left, top, 560.0F, 590.0F, left + 34.0F, 492.0F};
}

struct DebugLayout {
    float left;
    float top;
    float width = 620.0F;
    float height = 500.0F;
};

DebugLayout debugLayout(int width, int height) {
    return {
        static_cast<float>(width) * 0.5F - 310.0F,
        static_cast<float>(height) * 0.5F - 250.0F,
    };
}

bool contains(double x, double y, float left, float top, float width, float height) {
    return x >= left && x <= left + width && y >= top && y <= top + height;
}

Glyph glyph(char character) {
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(character)))) {
    case 'A': return {14, 17, 17, 31, 17, 17, 17}; case 'B': return {30, 17, 17, 30, 17, 17, 30};
    case 'C': return {14, 17, 16, 16, 16, 17, 14}; case 'D': return {30, 17, 17, 17, 17, 17, 30};
    case 'E': return {31, 16, 16, 30, 16, 16, 31}; case 'F': return {31, 16, 16, 30, 16, 16, 16};
    case 'G': return {14, 17, 16, 23, 17, 17, 14}; case 'H': return {17, 17, 17, 31, 17, 17, 17};
    case 'I': return {31, 4, 4, 4, 4, 4, 31}; case 'L': return {16, 16, 16, 16, 16, 16, 31};
    case 'M': return {17, 27, 21, 21, 17, 17, 17}; case 'N': return {17, 25, 21, 19, 17, 17, 17};
    case 'O': return {14, 17, 17, 17, 17, 17, 14}; case 'P': return {30, 17, 17, 30, 16, 16, 16};
    case 'Q': return {14, 17, 17, 17, 21, 18, 13}; case 'R': return {30, 17, 17, 30, 20, 18, 17};
    case 'S': return {15, 16, 16, 14, 1, 1, 30}; case 'T': return {31, 4, 4, 4, 4, 4, 4};
    case 'U': return {17, 17, 17, 17, 17, 17, 14}; case 'V': return {17, 17, 17, 17, 17, 10, 4};
    case 'W': return {17, 17, 17, 21, 21, 21, 10}; case 'X': return {17, 17, 10, 4, 10, 17, 17};
    case 'Y': return {17, 17, 10, 4, 4, 4, 4}; case 'Z': return {31, 1, 2, 4, 8, 16, 31};
    case '0': return {14, 17, 19, 21, 25, 17, 14}; case '1': return {4, 12, 4, 4, 4, 4, 14};
    case '2': return {14, 17, 1, 2, 4, 8, 31}; case '3': return {30, 1, 1, 14, 1, 1, 30};
    case '4': return {2, 6, 10, 18, 31, 2, 2}; case '5': return {31, 16, 16, 30, 1, 1, 30};
    case '6': return {14, 16, 16, 30, 17, 17, 14}; case '7': return {31, 1, 2, 4, 8, 8, 8};
    case '8': return {14, 17, 17, 14, 17, 17, 14}; case '9': return {14, 17, 17, 15, 1, 1, 14};
    case ':': return {0, 4, 4, 0, 4, 4, 0}; case '/': return {1, 2, 2, 4, 8, 8, 16};
    case '-': return {0, 0, 0, 31, 0, 0, 0}; default: return {};
    }
}

} // namespace

namespace pcolonist {

UiSystem::~UiSystem() {
    shutdown();
}

void UiSystem::shutdown() {
    shader_.reset();
    if (vertexArray_ != 0) {
        glDeleteVertexArrays(1, &vertexArray_);
        vertexArray_ = 0;
    }
}

void UiSystem::initialize() {
    const std::filesystem::path directory = std::filesystem::path(PCOLONIST_ASSET_DIR) / "shaders";
    shader_ = std::make_unique<Shader>(directory / "ui.vert", directory / "ui.frag");
    shader_->use();
    if (!shader_->validate()) {
        throw std::runtime_error("UI shader validation failed");
    }
    glGenVertexArrays(1, &vertexArray_);
}

void UiSystem::resize(int width, int height) {
    width_ = width;
    height_ = height;
}

void UiSystem::setPointerPosition(double x, double y) {
    pointerX_ = x;
    pointerY_ = y;
}

void UiSystem::render(
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
    bool debugPanelOpen) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(vertexArray_);

    const auto card = [this](float x, float y, float width, float height, float radius = 2.0F) {
        rectangle(x + 2.0F, y + 3.0F, width, height, {0.0F, 0.0F, 0.0F, 0.35F}, radius);
        rectangle(x, y, width, height, border, radius);
        rectangle(x + 1.0F, y + 1.0F, width - 2.0F, height - 2.0F, panel, radius - 1.0F);
    };
    const auto statusPill = [this](float x, float y, std::string_view label, const glm::vec4& accent) {
        rectangle(x, y, 74.0F, 24.0F, panelRaised, 1.0F);
        rectangle(x, y, 2.0F, 24.0F, accent);
        rectangle(x + 10.0F, y + 10.0F, 4.0F, 4.0F, accent);
        text(x + 22.0F, y + 6.0F, label, 1.5F, textPrimary);
    };

    const float playerY = static_cast<float>(height_ - 82);
    card(18.0F, playerY, 244.0F, 62.0F);
    rectangle(18.0F, playerY, 3.0F, 62.0F, cyan);
    text(36.0F, playerY + 13.0F, "EXPLORER", 2.0F, textPrimary);
    text(36.0F, playerY + 37.0F, cursorCaptured ? "FIELD MODE" : "CURSOR FREE", 1.5F, textMuted);
    rectangle(153.0F, playerY + 39.0F, 91.0F, 4.0F, {0.08F, 0.09F, 0.1F, 1.0F});
    rectangle(153.0F, playerY + 39.0F, 75.0F, 4.0F, cyan);

    const float timePanelX = static_cast<float>(width_) * 0.5F - 178.0F;
    card(timePanelX, 18.0F, 356.0F, 58.0F);
    rectangle(timePanelX + 18.0F, 31.0F, 2.0F, 32.0F, cyan);
    const int totalMinutes = static_cast<int>(weather.dayProgress() * 24.0F * 60.0F);
    const int hours = totalMinutes / 60;
    const int minutes = totalMinutes % 60;
    std::ostringstream clock;
    clock << std::setfill('0') << std::setw(2) << hours << ':' << std::setw(2) << minutes;
    text(timePanelX + 34.0F, 31.0F, clock.str(), 3.0F, textPrimary);
    const std::string dayLabel = "DAY " + std::to_string(weather.dayNumber());
    statusPill(timePanelX + 145.0F, 35.0F, dayLabel, weather.daylight() > 0.2F ? amber : cyan);
    statusPill(timePanelX + 230.0F, 35.0F, weather.weatherName(), cyan);

    const float centerX = static_cast<float>(width_) * 0.5F;
    const float centerY = static_cast<float>(height_) * 0.5F;
    rectangle(centerX - 1.0F, centerY - 13.0F, 2.0F, 7.0F, {0.85F, 0.95F, 1.0F, 0.82F}, 1.0F);
    rectangle(centerX - 1.0F, centerY + 6.0F, 2.0F, 7.0F, {0.85F, 0.95F, 1.0F, 0.82F}, 1.0F);
    rectangle(centerX - 13.0F, centerY - 1.0F, 7.0F, 2.0F, {0.85F, 0.95F, 1.0F, 0.82F}, 1.0F);
    rectangle(centerX + 6.0F, centerY - 1.0F, 7.0F, 2.0F, {0.85F, 0.95F, 1.0F, 0.82F}, 1.0F);
    rectangle(centerX - 1.5F, centerY - 1.5F, 3.0F, 3.0F, cyan, 1.5F);

    constexpr float slotSize = 62.0F;
    constexpr float slotGap = 8.0F;
    constexpr float hotbarWidth = slotSize * static_cast<float>(Inventory::hotbarSize)
        + slotGap * static_cast<float>(Inventory::hotbarSize - 1);
    const float hotbarX = centerX - hotbarWidth * 0.5F;
    const float hotbarY = static_cast<float>(height_) - 82.0F;
    for (std::size_t slot = 0; slot < Inventory::hotbarSize; ++slot) {
        const float x = hotbarX + static_cast<float>(slot) * (slotSize + slotGap);
        const bool selected = inventory.selectedSlot() == slot;
        rectangle(x + 2.0F, hotbarY + 3.0F, slotSize, slotSize, {0.0F, 0.0F, 0.0F, 0.32F}, 1.0F);
        rectangle(x, hotbarY, slotSize, slotSize, selected ? cyan : border, 1.0F);
        rectangle(x + 2.0F, hotbarY + 2.0F, slotSize - 4.0F, slotSize - 4.0F, panelRaised, 0.0F);
        rectangle(x + 2.0F, hotbarY + 2.0F, selected ? slotSize - 4.0F : 0.0F, 2.0F, cyan);
        text(x + 7.0F, hotbarY + 6.0F, std::to_string(slot + 1), 1.5F, selected ? cyan : textMuted);
        if (!inventory.toolName(slot).empty()) {
            text(x + 14.0F, hotbarY + 34.0F, inventory.toolName(slot), 1.5F, textPrimary);
        }
    }

    const float fullscreenX = static_cast<float>(width_ - 72);
    const bool fullscreenHovered = !cursorCaptured && contains(pointerX_, pointerY_, fullscreenX, 18.0F, 54.0F, 44.0F);
    card(fullscreenX, 18.0F, 54.0F, 44.0F);
    if (fullscreenHovered || fullscreen) {
        rectangle(fullscreenX + 1.0F, 19.0F, 52.0F, 42.0F, fullscreen ? glm::vec4{0.045F, 0.22F, 0.26F, 0.98F} : panelHover, 0.0F);
    }
    rectangle(fullscreenX + 15.0F, 31.0F, 24.0F, 2.0F, textPrimary);
    rectangle(fullscreenX + 15.0F, 47.0F, 24.0F, 2.0F, textPrimary);
    rectangle(fullscreenX + 15.0F, 31.0F, 2.0F, 18.0F, textPrimary);
    rectangle(fullscreenX + 37.0F, 31.0F, 2.0F, 18.0F, textPrimary);

    if (!cursorCaptured) {
        card(18.0F, 18.0F, 216.0F, 40.0F);
        rectangle(31.0F, 32.0F, 3.0F, 12.0F, cyan);
        text(51.0F, 30.0F, "F1 CAPTURE CURSOR", 1.5F, textPrimary);
    }

    if (inventoryOpen) {
        rectangle(0.0F, 0.0F, static_cast<float>(width_), static_cast<float>(height_), {0.005F, 0.009F, 0.016F, 0.62F});
        const float left = centerX - 310.0F;
        const float top = centerY - 220.0F;
        rectangle(left + 5.0F, top + 6.0F, 620.0F, 440.0F, {0.0F, 0.0F, 0.0F, 0.4F}, 2.0F);
        rectangle(left, top, 620.0F, 440.0F, border, 2.0F);
        rectangle(left + 1.0F, top + 1.0F, 618.0F, 438.0F, panel, 1.0F);
        rectangle(left + 1.0F, top + 1.0F, 4.0F, 438.0F, cyan);
        text(left + 30.0F, top + 28.0F, "INVENTORY", 3.0F, textPrimary);
        text(left + 420.0F, top + 34.0F, "TAB CLOSE", 1.5F, textMuted);
        rectangle(left + 30.0F, top + 76.0F, 560.0F, 1.0F, border);
        text(left + 34.0F, top + 105.0F, "RESOURCES", 1.5F, cyan);
        text(left + 34.0F, top + 138.0F, "WOOD", 2.0F, textPrimary);
        text(left + 180.0F, top + 138.0F, std::to_string(inventory.wood()), 2.0F, cyan);
        text(left + 34.0F, top + 205.0F, "TOOLS", 1.5F, cyan);
        for (std::size_t slot = 0; slot < Inventory::hotbarSize; ++slot) {
            const float x = left + 34.0F + static_cast<float>(slot) * 106.0F;
            rectangle(x, top + 240.0F, 88.0F, 88.0F, inventory.selectedSlot() == slot ? cyan : border, 1.0F);
            rectangle(x + 2.0F, top + 242.0F, 84.0F, 84.0F, panelRaised);
            text(x + 10.0F, top + 251.0F, std::to_string(slot + 1), 1.5F, textMuted);
            if (!inventory.toolName(slot).empty()) {
                text(x + 22.0F, top + 288.0F, inventory.toolName(slot), 1.5F, textPrimary);
            }
        }
    }

    if (debugPanelOpen) {
        rectangle(0.0F, 0.0F, static_cast<float>(width_), static_cast<float>(height_), {0.005F, 0.009F, 0.016F, 0.72F});
        const DebugLayout layout = debugLayout(width_, height_);
        rectangle(layout.left + 5.0F, layout.top + 6.0F, layout.width, layout.height, {0, 0, 0, 0.42F}, 2.0F);
        rectangle(layout.left, layout.top, layout.width, layout.height, border, 2.0F);
        rectangle(layout.left + 1.0F, layout.top + 1.0F, layout.width - 2.0F, layout.height - 2.0F, panel, 1.0F);
        rectangle(layout.left + 1.0F, layout.top + 1.0F, 4.0F, layout.height - 2.0F, cyan);
        text(layout.left + 32.0F, layout.top + 28.0F, "DEV PANEL", 3.0F, textPrimary);
        text(layout.left + 430.0F, layout.top + 34.0F, "TILDE CLOSE", 1.5F, textMuted);
        text(layout.left + 32.0F, layout.top + 68.0F, "WORLD AND RENDER TESTING", 1.5F, cyan);
        rectangle(layout.left + 32.0F, layout.top + 94.0F, layout.width - 64.0F, 1.0F, border);

        const auto debugButton = [this, &layout](
                                     int column,
                                     int row,
                                     std::string_view label,
                                     std::string_view value,
                                     const glm::vec4& accent) {
            constexpr float buttonWidth = 264.0F;
            constexpr float buttonHeight = 62.0F;
            const float x = layout.left + 32.0F + static_cast<float>(column) * 292.0F;
            const float y = layout.top + 116.0F + static_cast<float>(row) * 78.0F;
            const bool hovered = contains(pointerX_, pointerY_, x, y, buttonWidth, buttonHeight);
            rectangle(x, y, buttonWidth, buttonHeight, hovered ? panelHover : panelRaised, 1.0F);
            rectangle(x, y, 3.0F, buttonHeight, hovered ? accent : border);
            text(x + 18.0F, y + 16.0F, label, 1.8F, textPrimary);
            if (!value.empty()) {
                text(x + 18.0F, y + 39.0F, value, 1.35F, hovered ? accent : textMuted);
            }
        };

        debugButton(0, 0, "RESPAWN", "ARRIVAL CAMP", cyan);
        debugButton(1, 0, "VOLCANO", "TELEPORT", cyan);
        debugButton(0, 1, "NEXT GROTTO", "TELEPORT", cyan);
        debugButton(1, 1, "WEATHER", weather.weatherName(), cyan);
        debugButton(0, 2, "SET NOON", "DAY LIGHT", cyan);
        debugButton(1, 2, "SET NIGHT", "MOON LIGHT", cyan);
        debugButton(0, 3, "SHADOWS", shadows ? "ON" : "OFF", cyan);
        debugButton(1, 3, "BLOOM", bloom ? "ON" : "OFF", cyan);
    }

    if (menuOpen) {
        rectangle(0.0F, 0.0F, static_cast<float>(width_), static_cast<float>(height_), {0.005F, 0.009F, 0.016F, 0.78F});
        const MenuLayout layout = menuLayout(width_, height_);
        rectangle(layout.left + 5.0F, layout.top + 6.0F, layout.width, layout.height, {0, 0, 0, 0.42F}, 2.0F);
        rectangle(layout.left, layout.top, layout.width, layout.height, border, 2.0F);
        rectangle(layout.left + 1.0F, layout.top + 1.0F, layout.width - 2.0F, layout.height - 2.0F, panel, 1.0F);
        rectangle(layout.left + 1.0F, layout.top + 1.0F, 4.0F, layout.height - 2.0F, cyan);
        text(layout.contentLeft, layout.top + 28.0F, "SYSTEM", 1.5F, cyan);
        text(layout.contentLeft, layout.top + 51.0F, "SETTINGS", 3.0F, textPrimary);
        text(layout.contentLeft + 330.0F, layout.top + 55.0F, "ESC RESUME", 1.5F, textMuted);
        rectangle(layout.contentLeft, layout.top + 82.0F, layout.contentWidth, 1.0F, border);

        const auto menuButton = [this, &layout](
                                    float y,
                                    std::string_view label,
                                    std::string_view value,
                                    const glm::vec4& accent,
                                    bool active,
                                    bool destructive = false) {
            const bool hovered = contains(pointerX_, pointerY_, layout.contentLeft, y, layout.contentWidth, 52.0F);
            rectangle(layout.contentLeft, y, layout.contentWidth, 52.0F, hovered ? panelHover : panelRaised, 1.0F);
            rectangle(layout.contentLeft, y, 3.0F, 52.0F, hovered || active ? accent : border);
            text(layout.contentLeft + 20.0F, y + 19.0F, label, 2.0F, destructive ? red : textPrimary);
            if (!value.empty()) {
                const float pillWidth = 112.0F;
                rectangle(
                    layout.contentLeft + layout.contentWidth - pillWidth - 14.0F,
                    y + 12.0F,
                    pillWidth,
                    28.0F,
                    active ? glm::vec4{accent.r * 0.22F, accent.g * 0.22F, accent.b * 0.22F, 1.0F} : panel,
                    1.0F);
                text(layout.contentLeft + layout.contentWidth - pillWidth, y + 20.0F, value, 1.5F, active ? accent : textMuted);
            }
        };

        menuButton(layout.top + 102.0F, "RESUME", "ENTER", cyan, true);
        menuButton(layout.top + 166.0F, "FULLSCREEN", fullscreen ? "ON" : "OFF", cyan, fullscreen);
        menuButton(layout.top + 230.0F, "VSYNC", vsync ? "ON" : "OFF", cyan, vsync);
        menuButton(layout.top + 294.0F, "FRAME LIMIT", frameLimit == 0 ? "UNLIMITED" : std::to_string(frameLimit), cyan, frameLimit != 0);
        menuButton(layout.top + 358.0F, "SHADOWS", shadows ? "ON" : "OFF", cyan, shadows);
        menuButton(layout.top + 422.0F, "BLOOM", bloom ? "ON" : "OFF", cyan, bloom);
        menuButton(layout.top + 506.0F, "QUIT TO DESKTOP", "", red, false, true);
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void UiSystem::updateTitle(
    GLFWwindow* window,
    const Registry& registry,
    const AudioSystem& audio,
    float deltaTime,
    bool menuOpen) {
    elapsed_ += deltaTime;
    ++frames_;
    if (elapsed_ < 0.25F) {
        return;
    }
    const auto fps = static_cast<unsigned int>(static_cast<float>(frames_) / elapsed_);
    const std::string title = menuOpen
        ? "pcolonist | SETTINGS: Resume / Fullscreen / VSync / Quit"
        : "pcolonist | FPS " + std::to_string(fps)
            + " | entities " + std::to_string(registry.size())
            + " | ESC settings | F11 fullscreen | F1 cursor";
    glfwSetWindowTitle(window, title.c_str());
    static_cast<void>(audio);
    elapsed_ = 0.0F;
    frames_ = 0;
}

bool UiSystem::fullscreenButtonContains(double x, double y) const {
    return contains(x, y, static_cast<float>(width_ - 72), 18.0F, 54.0F, 44.0F);
}

UiAction UiSystem::menuActionAt(double x, double y) const {
    const MenuLayout layout = menuLayout(width_, height_);
    if (x < layout.contentLeft || x > layout.contentLeft + layout.contentWidth) {
        return UiAction::None;
    }
    if (y >= layout.top + 102.0 && y <= layout.top + 154.0) {
        return UiAction::Resume;
    }
    if (y >= layout.top + 166.0 && y <= layout.top + 218.0) {
        return UiAction::ToggleFullscreen;
    }
    if (y >= layout.top + 230.0 && y <= layout.top + 282.0) {
        return UiAction::ToggleVsync;
    }
    if (y >= layout.top + 294.0 && y <= layout.top + 346.0) {
        return UiAction::CycleFrameLimit;
    }
    if (y >= layout.top + 358.0 && y <= layout.top + 410.0) {
        return UiAction::ToggleShadows;
    }
    if (y >= layout.top + 422.0 && y <= layout.top + 474.0) {
        return UiAction::ToggleBloom;
    }
    if (y >= layout.top + 506.0 && y <= layout.top + 558.0) {
        return UiAction::Quit;
    }
    return UiAction::None;
}

UiAction UiSystem::debugActionAt(double x, double y) const {
    const DebugLayout layout = debugLayout(width_, height_);
    constexpr float buttonWidth = 264.0F;
    constexpr float buttonHeight = 62.0F;
    constexpr std::array actions = {
        UiAction::RespawnPlayer,
        UiAction::TeleportVolcano,
        UiAction::TeleportNextGrotto,
        UiAction::CycleWeather,
        UiAction::SetNoon,
        UiAction::SetNight,
        UiAction::ToggleShadows,
        UiAction::ToggleBloom,
    };
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 2; ++column) {
            const float left = layout.left + 32.0F + static_cast<float>(column) * 292.0F;
            const float top = layout.top + 116.0F + static_cast<float>(row) * 78.0F;
            if (contains(x, y, left, top, buttonWidth, buttonHeight)) {
                return actions[static_cast<std::size_t>(row * 2 + column)];
            }
        }
    }
    return UiAction::None;
}

void UiSystem::text(float x, float y, std::string_view value, float scale, const glm::vec4& color) {
    float cursor = x;
    for (char character : value) {
        const Glyph bitmap = glyph(character);
        for (std::size_t row = 0; row < bitmap.size(); ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((bitmap[row] & (1U << (4 - column))) != 0) {
                    rectangle(cursor + static_cast<float>(column) * scale, y + static_cast<float>(row) * scale, scale, scale, color);
                }
            }
        }
        cursor += scale * 6.0F;
    }
}

void UiSystem::rectangle(float x, float y, float width, float height, const glm::vec4& color, float radius) {
    shader_->use();
    shader_->setVec4("rect", {x, y, width, height});
    shader_->setVec4("color", color);
    shader_->setVec4("viewportRadius", {static_cast<float>(width_), static_cast<float>(height_), std::min(radius, 2.0F), 0.0F});
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

} // namespace pcolonist
