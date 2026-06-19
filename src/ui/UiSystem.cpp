#include "pcolonist/ui/UiSystem.hpp"

#include "pcolonist/audio/AudioSystem.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/gameplay/Inventory.hpp"
#include "pcolonist/world/WeatherSystem.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#if defined(PCOLONIST_ENABLE_FREETYPE_UI)
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#include <filesystem>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>

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

char32_t uppercase(char32_t character) {
    if (character >= U'a' && character <= U'z') {
        return character - 32;
    }
    if (character >= U'а' && character <= U'я') {
        return character - 32;
    }
    if (character == U'ё') {
        return U'Ё';
    }
    return character;
}

Glyph glyph(char32_t character) {
    switch (uppercase(character)) {
    case U'А': character = U'A'; break; case U'В': character = U'B'; break; case U'Е': character = U'E'; break;
    case U'Ё': character = U'E'; break; case U'К': character = U'K'; break; case U'М': character = U'M'; break;
    case U'Н': character = U'H'; break; case U'О': character = U'O'; break; case U'Р': character = U'P'; break;
    case U'С': character = U'C'; break; case U'Т': character = U'T'; break; case U'У': character = U'Y'; break;
    case U'Х': character = U'X'; break; default: break;
    }
    switch (character) {
    case 'A': return {14, 17, 17, 31, 17, 17, 17}; case 'B': return {30, 17, 17, 30, 17, 17, 30};
    case 'C': return {14, 17, 16, 16, 16, 17, 14}; case 'D': return {30, 17, 17, 17, 17, 17, 30};
    case 'E': return {31, 16, 16, 30, 16, 16, 31}; case 'F': return {31, 16, 16, 30, 16, 16, 16};
    case 'G': return {14, 17, 16, 23, 17, 17, 14}; case 'H': return {17, 17, 17, 31, 17, 17, 17};
    case 'I': return {31, 4, 4, 4, 4, 4, 31}; case 'J': return {7, 2, 2, 2, 18, 18, 12};
    case 'K': return {17, 18, 20, 24, 20, 18, 17}; case 'L': return {16, 16, 16, 16, 16, 16, 31};
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
    case '-': return {0, 0, 0, 31, 0, 0, 0};
    case U'Б': return {31, 16, 16, 30, 17, 17, 30}; case U'Г': return {31, 16, 16, 16, 16, 16, 16};
    case U'Д': return {6, 10, 10, 10, 10, 31, 17}; case U'Ж': return {21, 21, 14, 4, 14, 21, 21};
    case U'З': return {30, 1, 1, 14, 1, 1, 30}; case U'И': return {17, 19, 21, 21, 21, 25, 17};
    case U'Й': return {10, 4, 17, 19, 21, 25, 17}; case U'Л': return {7, 9, 9, 9, 9, 9, 17};
    case U'П': return {31, 17, 17, 17, 17, 17, 17}; case U'Ф': return {4, 14, 21, 21, 14, 4, 4};
    case U'Ц': return {17, 17, 17, 17, 17, 31, 1}; case U'Ч': return {17, 17, 17, 15, 1, 1, 1};
    case U'Ш': return {21, 21, 21, 21, 21, 21, 31}; case U'Щ': return {21, 21, 21, 21, 21, 31, 1};
    case U'Ъ': return {24, 8, 8, 14, 9, 9, 14}; case U'Ы': return {17, 17, 17, 29, 19, 19, 29};
    case U'Ь': return {16, 16, 16, 30, 17, 17, 30}; case U'Э': return {14, 17, 1, 7, 1, 17, 14};
    case U'Ю': return {17, 21, 21, 29, 21, 21, 17}; case U'Я': return {15, 17, 17, 15, 5, 9, 17};
    default: return {};
    }
}

char32_t nextCodepoint(std::string_view text, std::size_t& index) {
    const auto first = static_cast<unsigned char>(text[index++]);
    if (first < 0x80U) {
        return first;
    }
    if ((first & 0xE0U) == 0xC0U && index < text.size()) {
        const auto second = static_cast<unsigned char>(text[index++]);
        return static_cast<char32_t>(((first & 0x1FU) << 6U) | (second & 0x3FU));
    }
    if ((first & 0xF0U) == 0xE0U && index + 1 < text.size()) {
        const auto second = static_cast<unsigned char>(text[index++]);
        const auto third = static_cast<unsigned char>(text[index++]);
        return static_cast<char32_t>(((first & 0x0FU) << 12U) | ((second & 0x3FU) << 6U) | (third & 0x3FU));
    }
    return U'?';
}

#if defined(PCOLONIST_ENABLE_FREETYPE_UI)
std::uint64_t glyphKey(char32_t character, int pixelSize) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(character)) << 32U)
        | static_cast<std::uint32_t>(pixelSize);
}
#endif

} // namespace

namespace pcolonist {

UiSystem::~UiSystem() {
    shutdown();
}

void UiSystem::shutdown() {
    shutdownFont();
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
    shader_->setInt("glyphTexture", 0);
#if defined(PCOLONIST_ENABLE_FREETYPE_UI)
    static_cast<void>(initializeFont());
#endif
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

void UiSystem::setFrameCounterVisible(bool visible) {
    frameCounterVisible_ = visible;
}

void UiSystem::render(
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
        rectangle(x, y, 94.0F, 24.0F, panelRaised, 1.0F);
        rectangle(x, y, 2.0F, 24.0F, accent);
        rectangle(x + 10.0F, y + 10.0F, 4.0F, 4.0F, accent);
        text(x + 22.0F, y + 6.0F, label, 1.5F, textPrimary);
    };
    const auto questRow = [this](float x, float y, std::string_view number, std::string_view label, bool done, bool active) {
        const glm::vec4 accent = done ? green : (active ? cyan : textMuted);
        rectangle(x, y, 286.0F, 40.0F, active ? panelRaised : panel, 1.0F);
        rectangle(x, y, 4.0F, 40.0F, accent);
        text(x + 14.0F, y + 10.0F, done ? "OK" : number, 1.7F, accent);
        text(x + 46.0F, y + 10.0F, label, 1.7F, done ? textMuted : textPrimary);
    };
    const auto survivalBar = [this](float x, float y, std::string_view label, float value, const glm::vec4& accent) {
        const float clamped = std::clamp(value, 0.0F, 100.0F);
        rectangle(x, y, 184.0F, 16.0F, {0.07F, 0.08F, 0.09F, 1.0F}, 1.0F);
        rectangle(x, y, 184.0F * (clamped / 100.0F), 16.0F, accent, 1.0F);
        text(x + 7.0F, y + 3.0F, label, 1.15F, textPrimary);
        const std::string valueText = std::to_string(static_cast<int>(std::round(clamped)));
        text(x + 150.0F, y + 3.0F, valueText, 1.15F, textPrimary);
    };

    const float playerY = static_cast<float>(height_ - 82);
    card(18.0F, playerY, 244.0F, 62.0F);
    rectangle(18.0F, playerY, 3.0F, 62.0F, cyan);
    text(36.0F, playerY + 13.0F, "ИССЛЕДОВАТЕЛЬ", 1.5F, textPrimary);
    text(36.0F, playerY + 37.0F, cursorCaptured ? "РЕЖИМ ИГРЫ" : "КУРСОР СВОБОДЕН", 1.3F, textMuted);
    rectangle(153.0F, playerY + 39.0F, 91.0F, 4.0F, {0.08F, 0.09F, 0.1F, 1.0F});
    rectangle(153.0F, playerY + 39.0F, 75.0F, 4.0F, cyan);

    const float questX = 18.0F;
    const float questY = 86.0F;
    card(questX, questY, 316.0F, 340.0F);
    rectangle(questX, questY, 4.0F, 340.0F, amber);
    text(questX + 20.0F, questY + 18.0F, "ЗАДАЧИ", 2.4F, textPrimary);
    text(questX + 20.0F, questY + 48.0F, objectives.contextHint, 1.35F, cyan);
    text(questX + 20.0F, questY + 66.0F, objectives.craftingHint, 1.15F, textMuted);
    rectangle(questX + 20.0F, questY + 84.0F, 276.0F, 1.0F, border);
    questRow(questX + 14.0F, questY + 96.0F, "1", "ДОБЫТЬ ДЕРЕВО", objectives.hasWood, !objectives.hasWood);
    questRow(questX + 14.0F, questY + 142.0F, "2", "ДОБЫТЬ КАМЕНЬ", objectives.hasStone, objectives.hasWood && !objectives.hasStone);
    questRow(questX + 14.0F, questY + 188.0F, "3", "РАЗЖЕЧЬ КОСТЕР", objectives.fireLit, objectives.hasWood && objectives.hasStone && !objectives.fireLit);
    questRow(questX + 14.0F, questY + 234.0F, "4", "НАЙТИ ВОДУ", objectives.hasWater, !objectives.hasWater);
    questRow(questX + 14.0F, questY + 280.0F, "5", "НАЙТИ УКРЫТИЕ", objectives.nearShelter, !objectives.nearShelter);

    const float timePanelX = static_cast<float>(width_) * 0.5F - 178.0F;
    card(timePanelX, 18.0F, 356.0F, 58.0F);
    rectangle(timePanelX + 18.0F, 31.0F, 2.0F, 32.0F, cyan);
    const int totalMinutes = static_cast<int>(weather.dayProgress() * 24.0F * 60.0F);
    const int hours = totalMinutes / 60;
    const int minutes = totalMinutes % 60;
    std::ostringstream clock;
    clock << std::setfill('0') << std::setw(2) << hours << ':' << std::setw(2) << minutes;
    text(timePanelX + 34.0F, 31.0F, clock.str(), 3.0F, textPrimary);
    const std::string dayLabel = "ДЕНЬ " + std::to_string(weather.dayNumber());
    statusPill(timePanelX + 145.0F, 35.0F, dayLabel, weather.daylight() > 0.2F ? amber : cyan);
    statusPill(timePanelX + 250.0F, 35.0F, weather.weatherName(), cyan);

    const float survivalX = static_cast<float>(width_) - 246.0F;
    const float survivalY = 126.0F;
    card(survivalX, survivalY, 228.0F, 202.0F);
    rectangle(survivalX, survivalY, 4.0F, 202.0F, objectives.sick ? amber : green);
    text(survivalX + 18.0F, survivalY + 15.0F, "ВЫЖИВАНИЕ", 1.8F, textPrimary);
    text(survivalX + 18.0F, survivalY + 38.0F, objectives.survivalBiome, 1.2F, cyan);
    text(survivalX + 18.0F, survivalY + 58.0F, objectives.survivalLocation, 1.05F, textMuted);
    survivalBar(survivalX + 18.0F, survivalY + 82.0F, "HP", objectives.health, green);
    survivalBar(survivalX + 18.0F, survivalY + 104.0F, "H2O", objectives.thirst, cyan);
    survivalBar(survivalX + 18.0F, survivalY + 126.0F, "FOOD", objectives.hunger, amber);
    survivalBar(survivalX + 18.0F, survivalY + 148.0F, "FAT", 100.0F - objectives.fatigue, {0.58F, 0.62F, 0.70F, 1.0F});
    const std::string temperature = "TEMP " + std::to_string(static_cast<int>(std::round(objectives.bodyTemperature))) + "C";
    text(survivalX + 18.0F, survivalY + 174.0F, temperature, 1.15F, textMuted);
    text(survivalX + 96.0F, survivalY + 174.0F, objectives.survivalWarning, 1.05F, objectives.sick ? amber : textPrimary);

    const float discoveryY = survivalY + 208.0F;
    card(survivalX, discoveryY, 228.0F, 94.0F);
    rectangle(survivalX, discoveryY, 4.0F, 94.0F, objectives.discoveryBlocked ? amber : cyan);
    text(survivalX + 18.0F, discoveryY + 14.0F, "ОТКРЫТИЯ", 1.65F, textPrimary);
    const std::string poiProgress = "POI " + std::to_string(objectives.discoveredLocations)
        + "/" + std::to_string(objectives.totalLocations);
    const std::string clueProgress = "УЛИКИ " + std::to_string(objectives.storyClues)
        + "/" + std::to_string(objectives.totalStoryClues);
    const std::string secretProgress = "ТАЙНЫ " + std::to_string(objectives.secretsFound)
        + "/" + std::to_string(objectives.totalSecrets);
    text(survivalX + 18.0F, discoveryY + 38.0F, poiProgress, 1.15F, cyan);
    text(survivalX + 98.0F, discoveryY + 38.0F, clueProgress, 1.15F, textMuted);
    text(survivalX + 18.0F, discoveryY + 58.0F, secretProgress, 1.15F, objectives.discoveryBlocked ? amber : green);
    text(survivalX + 18.0F, discoveryY + 76.0F, objectives.discoveryMessage, 0.9F, textPrimary);

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

    if (frameCounterVisible_) {
        const std::string fpsLabel = "FPS " + std::to_string(currentFps_);
        const float frameCounterX = static_cast<float>(width_ - 126);
        card(frameCounterX, 72.0F, 108.0F, 38.0F);
        rectangle(frameCounterX + 12.0F, 86.0F, 3.0F, 10.0F, green);
        text(frameCounterX + 26.0F, 84.0F, fpsLabel, 1.45F, textPrimary);
    }

    const float minimapSize = 168.0F;
    const float minimapX = static_cast<float>(width_) - minimapSize - 18.0F;
    const float minimapY = static_cast<float>(height_) - minimapSize - 112.0F;
    card(minimapX, minimapY, minimapSize, minimapSize);
    text(minimapX + 16.0F, minimapY + 13.0F, "МИНИКАРТА", 1.35F, textPrimary);
    rectangle(minimapX + 16.0F, minimapY + 36.0F, 136.0F, 112.0F, {0.018F, 0.085F, 0.105F, 1.0F}, 1.0F);
    rectangle(minimapX + 39.0F, minimapY + 54.0F, 84.0F, 70.0F, {0.10F, 0.26F, 0.13F, 1.0F}, 2.0F);
    rectangle(minimapX + 55.0F, minimapY + 46.0F, 46.0F, 22.0F, {0.16F, 0.31F, 0.17F, 1.0F}, 2.0F);
    rectangle(minimapX + 77.0F, minimapY + 69.0F, 24.0F, 24.0F, {0.19F, 0.16F, 0.14F, 1.0F}, 2.0F);
    rectangle(minimapX + 103.0F, minimapY + 86.0F, 24.0F, 20.0F, {0.04F, 0.22F, 0.30F, 1.0F}, 2.0F);
    rectangle(minimapX + 28.0F, minimapY + 130.0F, 12.0F, 7.0F, {0.18F, 0.30F, 0.12F, 1.0F}, 1.0F);
    rectangle(minimapX + 130.0F, minimapY + 57.0F, 10.0F, 7.0F, {0.18F, 0.30F, 0.12F, 1.0F}, 1.0F);
    const float markerX = minimapX + 16.0F + std::clamp((objectives.playerPosition.x + 132.0F) / 264.0F, 0.0F, 1.0F) * 136.0F;
    const float markerY = minimapY + 36.0F + std::clamp((objectives.playerPosition.z + 112.0F) / 224.0F, 0.0F, 1.0F) * 112.0F;
    rectangle(markerX - 5.0F, markerY - 5.0F, 10.0F, 10.0F, cyan, 4.0F);
    rectangle(markerX - 2.0F, markerY - 2.0F, 4.0F, 4.0F, textPrimary, 2.0F);

    if (!cursorCaptured) {
        card(18.0F, 18.0F, 216.0F, 40.0F);
        rectangle(31.0F, 32.0F, 3.0F, 12.0F, cyan);
        text(51.0F, 30.0F, "F1 ЗАХВАТ КУРСОРА", 1.3F, textPrimary);
    }

    if (inventoryOpen) {
        rectangle(0.0F, 0.0F, static_cast<float>(width_), static_cast<float>(height_), {0.005F, 0.009F, 0.016F, 0.62F});
        const float left = centerX - 310.0F;
        const float top = centerY - 220.0F;
        rectangle(left + 5.0F, top + 6.0F, 620.0F, 440.0F, {0.0F, 0.0F, 0.0F, 0.4F}, 2.0F);
        rectangle(left, top, 620.0F, 440.0F, border, 2.0F);
        rectangle(left + 1.0F, top + 1.0F, 618.0F, 438.0F, panel, 1.0F);
        rectangle(left + 1.0F, top + 1.0F, 4.0F, 438.0F, cyan);
        text(left + 30.0F, top + 28.0F, "ИНВЕНТАРЬ", 2.5F, textPrimary);
        text(left + 420.0F, top + 34.0F, "TAB ЗАКРЫТЬ", 1.25F, textMuted);
        rectangle(left + 30.0F, top + 76.0F, 560.0F, 1.0F, border);
        text(left + 34.0F, top + 105.0F, "РЕСУРСЫ", 1.5F, cyan);
        text(left + 34.0F, top + 138.0F, "ДЕРЕВО", 2.0F, textPrimary);
        text(left + 180.0F, top + 138.0F, std::to_string(inventory.wood()), 2.0F, cyan);
        text(left + 34.0F, top + 165.0F, "КАМЕНЬ", 2.0F, textPrimary);
        text(left + 180.0F, top + 165.0F, std::to_string(inventory.stone()), 2.0F, cyan);
        text(left + 34.0F, top + 192.0F, "ВОДА", 2.0F, textPrimary);
        text(left + 180.0F, top + 192.0F, std::to_string(inventory.water()), 2.0F, cyan);
        text(left + 300.0F, top + 138.0F, "ВОЛОКНО", 2.0F, textPrimary);
        text(left + 470.0F, top + 138.0F, std::to_string(inventory.fiber()), 2.0F, cyan);
        text(left + 300.0F, top + 165.0F, "МЕТАЛЛ", 2.0F, textPrimary);
        text(left + 470.0F, top + 165.0F, std::to_string(inventory.metal()), 2.0F, cyan);
        text(left + 300.0F, top + 192.0F, objectives.craftingHint, 1.2F, textMuted);
        text(left + 300.0F, top + 214.0F, objectives.discoveryClue, 0.95F, textMuted);
        text(left + 34.0F, top + 232.0F, "ИНСТРУМЕНТЫ", 1.5F, cyan);
        for (std::size_t slot = 0; slot < Inventory::hotbarSize; ++slot) {
            const float x = left + 34.0F + static_cast<float>(slot) * 106.0F;
            rectangle(x, top + 252.0F, 88.0F, 88.0F, inventory.selectedSlot() == slot ? cyan : border, 1.0F);
            rectangle(x + 2.0F, top + 254.0F, 84.0F, 84.0F, panelRaised);
            text(x + 10.0F, top + 263.0F, std::to_string(slot + 1), 1.5F, textMuted);
            if (!inventory.toolName(slot).empty()) {
                text(x + 22.0F, top + 300.0F, inventory.toolName(slot), 1.5F, textPrimary);
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
        debugButton(0, 1, "NEXT LANDMARK", "TELEPORT", cyan);
        debugButton(1, 1, "WEATHER", weather.weatherName(), cyan);
        debugButton(0, 2, "SET NOON", "DAY LIGHT", cyan);
        debugButton(1, 2, "SET NIGHT", "MOON LIGHT", cyan);
        debugButton(0, 3, "SHADOWS", shadows ? "ON" : "OFF", cyan);
        debugButton(1, 3, "BLOOM", bloom ? "ON" : "OFF", cyan);
        debugButton(0, 4, "SKY QUALITY", skyQuality, cyan);
    }

    if (menuOpen) {
        const MainMenuState state{
            width_,
            height_,
            animationTime_,
            pointerX_,
            pointerY_,
            fullscreen,
            vsync,
            frameLimit,
            shadows,
            bloom,
        };
        mainMenu_.render(
            state,
            [this](float x, float y, float width, float height, const glm::vec4& color, float radius) {
                rectangle(x, y, width, height, color, radius);
            },
            [this](float x, float y, std::string_view value, float scale, const glm::vec4& color) {
                text(x, y, value, scale, color);
            });
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
    animationTime_ += deltaTime;
    ++frames_;
    if (elapsed_ < 0.25F) {
        return;
    }
    currentFps_ = static_cast<unsigned int>(static_cast<float>(frames_) / elapsed_);
    const std::string title = menuOpen
        ? "pcolonist | Меню: Играть / Загрузить игру / Настройки / Выход"
        : "pcolonist | FPS " + std::to_string(currentFps_)
            + " | entities " + std::to_string(registry.size())
            + " | ESC меню | F11 экран | F1 курсор";
    glfwSetWindowTitle(window, title.c_str());
    static_cast<void>(audio);
    elapsed_ = 0.0F;
    frames_ = 0;
}

bool UiSystem::fullscreenButtonContains(double x, double y) const {
    return contains(x, y, static_cast<float>(width_ - 72), 18.0F, 54.0F, 44.0F);
}

UiAction UiSystem::menuActionAt(double x, double y) {
    switch (mainMenu_.actionAt(x, y, width_, height_)) {
    case MainMenuAction::Play:
        return UiAction::Resume;
    case MainMenuAction::ToggleFullscreen:
        return UiAction::ToggleFullscreen;
    case MainMenuAction::ToggleVsync:
        return UiAction::ToggleVsync;
    case MainMenuAction::CycleFrameLimit:
        return UiAction::CycleFrameLimit;
    case MainMenuAction::ToggleShadows:
        return UiAction::ToggleShadows;
    case MainMenuAction::ToggleBloom:
        return UiAction::ToggleBloom;
    case MainMenuAction::Quit:
        return UiAction::Quit;
    case MainMenuAction::None:
        return UiAction::None;
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
        UiAction::TeleportNextLandmark,
        UiAction::CycleWeather,
        UiAction::SetNoon,
        UiAction::SetNight,
        UiAction::ToggleShadows,
        UiAction::ToggleBloom,
        UiAction::CycleSkyQuality,
        UiAction::None,
    };
    for (int row = 0; row < 5; ++row) {
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

bool UiSystem::initializeFont() {
#if !defined(PCOLONIST_ENABLE_FREETYPE_UI)
    return false;
#else
    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library) != 0) {
        return false;
    }

    const std::vector<std::filesystem::path> candidates{
        "assets/fonts/ComicSansMS.ttf",
        "assets/fonts/NotoSans-Regular.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
    };
    for (const auto& path : candidates) {
        FT_Face face = nullptr;
        if (std::filesystem::exists(path) && FT_New_Face(library, path.string().c_str(), 0, &face) == 0) {
            fontLibrary_ = library;
            fontFace_ = face;
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            return true;
        }
    }

    FT_Done_FreeType(library);
    return false;
#endif
}

void UiSystem::shutdownFont() {
    for (const auto& [key, glyph] : ttfGlyphs_) {
        static_cast<void>(key);
        if (glyph.texture != 0) {
            glDeleteTextures(1, &glyph.texture);
        }
    }
    ttfGlyphs_.clear();

#if defined(PCOLONIST_ENABLE_FREETYPE_UI)
    if (fontFace_ != nullptr) {
        FT_Done_Face(static_cast<FT_Face>(fontFace_));
        fontFace_ = nullptr;
    }
    if (fontLibrary_ != nullptr) {
        FT_Done_FreeType(static_cast<FT_Library>(fontLibrary_));
        fontLibrary_ = nullptr;
    }
#else
    fontFace_ = nullptr;
    fontLibrary_ = nullptr;
#endif
    fontPixelSize_ = 0;
}

const UiSystem::TtfGlyph* UiSystem::glyphTexture(char32_t character, int pixelSize) {
#if !defined(PCOLONIST_ENABLE_FREETYPE_UI)
    static_cast<void>(character);
    static_cast<void>(pixelSize);
    return nullptr;
#else
    if (fontFace_ == nullptr) {
        return nullptr;
    }

    const std::uint64_t key = glyphKey(character, pixelSize);
    if (const auto iterator = ttfGlyphs_.find(key); iterator != ttfGlyphs_.end()) {
        return &iterator->second;
    }

    auto face = static_cast<FT_Face>(fontFace_);
    if (fontPixelSize_ != pixelSize) {
        if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixelSize)) != 0) {
            return nullptr;
        }
        fontPixelSize_ = pixelSize;
    }
    if (FT_Load_Char(face, static_cast<FT_ULong>(character), FT_LOAD_RENDER) != 0) {
        return nullptr;
    }

    const FT_GlyphSlot slot = face->glyph;
    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RED,
        static_cast<int>(slot->bitmap.width),
        static_cast<int>(slot->bitmap.rows),
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        slot->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    const TtfGlyph glyph{
        texture,
        static_cast<int>(slot->bitmap.width),
        static_cast<int>(slot->bitmap.rows),
        slot->bitmap_left,
        slot->bitmap_top,
        static_cast<unsigned int>(slot->advance.x),
    };
    return &ttfGlyphs_.emplace(key, glyph).first->second;
#endif
}

void UiSystem::text(float x, float y, std::string_view value, float scale, const glm::vec4& color) {
    if (fontFace_ != nullptr) {
        ttfText(x, y, value, scale, color);
        return;
    }
    bitmapText(x, y, value, scale, color);
}

void UiSystem::ttfText(float x, float y, std::string_view value, float scale, const glm::vec4& color) {
    const int pixelSize = std::max(10, static_cast<int>(std::lround(scale * 8.0F)));
    float cursor = x;
    shader_->use();
    shader_->setInt("useTexture", 1);
    shader_->setVec4("color", color);
    glActiveTexture(GL_TEXTURE0);
    for (std::size_t index = 0; index < value.size();) {
        const char32_t character = nextCodepoint(value, index);
        if (character == U' ') {
            cursor += static_cast<float>(pixelSize) * 0.38F;
            continue;
        }
        const TtfGlyph* glyph = glyphTexture(character, pixelSize);
        if (glyph == nullptr || glyph->texture == 0) {
            continue;
        }
        const float left = cursor + static_cast<float>(glyph->bearingX);
        const float top = y + static_cast<float>(pixelSize - glyph->bearingY);
        shader_->setVec4("rect", {left, top, static_cast<float>(glyph->width), static_cast<float>(glyph->height)});
        shader_->setVec4("viewportRadius", {static_cast<float>(width_), static_cast<float>(height_), 0.0F, 0.0F});
        glBindTexture(GL_TEXTURE_2D, glyph->texture);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        cursor += static_cast<float>(glyph->advance >> 6U);
    }
    shader_->setInt("useTexture", 0);
}

void UiSystem::bitmapText(float x, float y, std::string_view value, float scale, const glm::vec4& color) {
    float cursor = x;
    for (std::size_t index = 0; index < value.size();) {
        const char32_t character = nextCodepoint(value, index);
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
    shader_->setInt("useTexture", 0);
    shader_->setVec4("rect", {x, y, width, height});
    shader_->setVec4("color", color);
    shader_->setVec4("viewportRadius", {static_cast<float>(width_), static_cast<float>(height_), std::min(radius, 2.0F), 0.0F});
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

} // namespace pcolonist
