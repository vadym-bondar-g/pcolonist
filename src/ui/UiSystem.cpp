#include "pcolonist/ui/UiSystem.hpp"

#include "pcolonist/audio/AudioSystem.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/world/WeatherSystem.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <array>
#include <cctype>
#include <stdexcept>
#include <string>
#include <sstream>
#include <iomanip>

namespace {

using Glyph = std::array<unsigned char, 7>;

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

void UiSystem::render(
    bool fullscreen,
    bool cursorCaptured,
    bool menuOpen,
    bool vsync,
    int frameLimit,
    bool shadows,
    bool bloom,
    const WeatherSystem& weather) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(vertexArray_);

    rectangle(19.0F, static_cast<float>(height_ - 55), 235.0F, 42.0F, {0.0F, 0.0F, 0.0F, 0.32F}, 10.0F);
    rectangle(16.0F, static_cast<float>(height_ - 58), 235.0F, 42.0F, {0.025F, 0.035F, 0.055F, 0.88F}, 10.0F);
    rectangle(30.0F, static_cast<float>(height_ - 43), 150.0F, 8.0F, {0.12F, 0.8F, 0.42F, 0.9F}, 4.0F);
    text(30.0F, static_cast<float>(height_ - 38), "PLAYER", 2.0F, {0.8F, 0.92F, 1.0F, 0.95F});

    const float timePanelX = static_cast<float>(width_) * 0.5F - 145.0F;
    rectangle(timePanelX + 3.0F, 19.0F, 290.0F, 44.0F, {0, 0, 0, 0.3F}, 11.0F);
    rectangle(timePanelX, 16.0F, 290.0F, 44.0F, {0.025F, 0.04F, 0.075F, 0.86F}, 11.0F);
    const int totalMinutes = static_cast<int>(weather.dayProgress() * 24.0F * 60.0F);
    const int hours = totalMinutes / 60;
    const int minutes = totalMinutes % 60;
    std::ostringstream clock;
    clock << std::setfill('0') << std::setw(2) << hours << ':' << std::setw(2) << minutes;
    text(timePanelX + 18.0F, 30.0F, clock.str(), 2.0F, {0.95F, 0.78F, 0.38F, 1.0F});
    text(timePanelX + 104.0F, 30.0F, weather.daylight() > 0.2F ? "DAY" : "NIGHT", 2.0F, weather.daylight() > 0.2F ? glm::vec4{0.95F, 0.72F, 0.28F, 1} : glm::vec4{0.48F, 0.68F, 1.0F, 1});
    text(timePanelX + 178.0F, 30.0F, weather.weatherName(), 2.0F, {0.72F, 0.85F, 1.0F, 1.0F});
    rectangle(static_cast<float>(width_) * 0.5F - 1.0F, static_cast<float>(height_) * 0.5F - 8.0F, 2.0F, 16.0F, {1, 1, 1, 0.8F});
    rectangle(static_cast<float>(width_) * 0.5F - 8.0F, static_cast<float>(height_) * 0.5F - 1.0F, 16.0F, 2.0F, {1, 1, 1, 0.8F});

    const glm::vec4 buttonColor = fullscreen
        ? glm::vec4{0.16F, 0.48F, 0.9F, 0.88F}
        : glm::vec4{0.08F, 0.11F, 0.17F, 0.88F};
    rectangle(static_cast<float>(width_ - 74), 16.0F, 58.0F, 42.0F, buttonColor, 9.0F);
    rectangle(static_cast<float>(width_ - 59), 28.0F, 28.0F, 3.0F, {1, 1, 1, 0.9F});
    rectangle(static_cast<float>(width_ - 59), 43.0F, 28.0F, 3.0F, {1, 1, 1, 0.9F});
    rectangle(static_cast<float>(width_ - 59), 28.0F, 3.0F, 18.0F, {1, 1, 1, 0.9F});
    rectangle(static_cast<float>(width_ - 34), 28.0F, 3.0F, 18.0F, {1, 1, 1, 0.9F});

    if (!cursorCaptured) {
        rectangle(16.0F, 16.0F, 245.0F, 36.0F, {0.05F, 0.08F, 0.13F, 0.82F}, 8.0F);
        rectangle(28.0F, 31.0F, 215.0F, 5.0F, {0.25F, 0.65F, 1.0F, 0.85F}, 3.0F);
        text(30.0F, 24.0F, "F1 CAPTURE CURSOR", 2.0F, {0.8F, 0.92F, 1.0F, 0.95F});
    }

    if (menuOpen) {
        rectangle(0.0F, 0.0F, static_cast<float>(width_), static_cast<float>(height_), {0.005F, 0.008F, 0.015F, 0.72F});
        const float left = static_cast<float>(width_) * 0.5F - 190.0F;
        const float top = static_cast<float>(height_) * 0.5F - 270.0F;
        rectangle(left + 7.0F, top + 9.0F, 380.0F, 584.0F, {0, 0, 0, 0.42F}, 18.0F);
        rectangle(left, top, 380.0F, 584.0F, {0.025F, 0.04F, 0.07F, 0.98F}, 18.0F);
        rectangle(left + 2.0F, top + 2.0F, 376.0F, 3.0F, {0.2F, 0.65F, 1.0F, 0.8F}, 2.0F);
        rectangle(left + 34.0F, top + 54.0F, 312.0F, 3.0F, {0.2F, 0.65F, 1.0F, 0.7F}, 2.0F);
        text(left + 104.0F, top + 24.0F, "SETTINGS", 3.0F, {0.84F, 0.93F, 1.0F, 1.0F});
        rectangle(left + 34.0F, top + 82.0F, 312.0F, 54.0F, {0.08F, 0.42F, 0.72F, 0.95F}, 10.0F);
        rectangle(left + 34.0F, top + 150.0F, 312.0F, 54.0F, fullscreen ? glm::vec4{0.12F, 0.55F, 0.9F, 0.95F} : glm::vec4{0.08F, 0.12F, 0.2F, 0.95F}, 10.0F);
        rectangle(left + 34.0F, top + 218.0F, 312.0F, 54.0F, vsync ? glm::vec4{0.12F, 0.62F, 0.35F, 0.95F} : glm::vec4{0.08F, 0.12F, 0.2F, 0.95F}, 10.0F);
        rectangle(left + 34.0F, top + 286.0F, 312.0F, 54.0F, {0.1F, 0.32F, 0.6F, 0.95F}, 10.0F);
        rectangle(left + 34.0F, top + 354.0F, 312.0F, 54.0F, shadows ? glm::vec4{0.16F, 0.52F, 0.78F, 0.95F} : glm::vec4{0.08F, 0.12F, 0.2F, 0.95F}, 10.0F);
        rectangle(left + 34.0F, top + 422.0F, 312.0F, 54.0F, bloom ? glm::vec4{0.55F, 0.34F, 0.12F, 0.95F} : glm::vec4{0.08F, 0.12F, 0.2F, 0.95F}, 10.0F);
        rectangle(left + 34.0F, top + 490.0F, 312.0F, 54.0F, {0.65F, 0.12F, 0.16F, 0.95F}, 10.0F);
        text(left + 58.0F, top + 103.0F, "RESUME", 3.0F, {1, 1, 1, 0.95F});
        text(left + 58.0F, top + 171.0F, fullscreen ? "FULLSCREEN ON" : "FULLSCREEN OFF", 3.0F, {1, 1, 1, 0.95F});
        text(left + 58.0F, top + 239.0F, vsync ? "VSYNC ON" : "VSYNC OFF", 3.0F, {1, 1, 1, 0.95F});
        text(left + 58.0F, top + 307.0F, frameLimit == 0 ? "FPS UNLIMITED" : "FPS " + std::to_string(frameLimit), 3.0F, {1, 1, 1, 0.95F});
        text(left + 58.0F, top + 375.0F, shadows ? "SHADOWS ON" : "SHADOWS OFF", 3.0F, {1, 1, 1, 0.95F});
        text(left + 58.0F, top + 443.0F, bloom ? "BLOOM ON" : "BLOOM OFF", 3.0F, {1, 1, 1, 0.95F});
        text(left + 58.0F, top + 511.0F, "QUIT", 3.0F, {1, 1, 1, 0.95F});
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
    return x >= width_ - 74 && x <= width_ - 16 && y >= 16 && y <= 58;
}

UiAction UiSystem::menuActionAt(double x, double y) const {
    const double left = static_cast<double>(width_) * 0.5 - 156.0;
    const double top = static_cast<double>(height_) * 0.5 - 188.0;
    if (x < left || x > left + 312.0) {
        return UiAction::None;
    }
    if (y >= top && y <= top + 54.0) {
        return UiAction::Resume;
    }
    if (y >= top + 68.0 && y <= top + 122.0) {
        return UiAction::ToggleFullscreen;
    }
    if (y >= top + 136.0 && y <= top + 190.0) {
        return UiAction::ToggleVsync;
    }
    if (y >= top + 204.0 && y <= top + 258.0) {
        return UiAction::CycleFrameLimit;
    }
    if (y >= top + 272.0 && y <= top + 326.0) {
        return UiAction::ToggleShadows;
    }
    if (y >= top + 340.0 && y <= top + 394.0) {
        return UiAction::ToggleBloom;
    }
    if (y >= top + 408.0 && y <= top + 462.0) {
        return UiAction::Quit;
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
    shader_->setVec4("viewportRadius", {static_cast<float>(width_), static_cast<float>(height_), radius, 0.0F});
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

} // namespace pcolonist
