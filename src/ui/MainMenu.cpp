#include "pcolonist/ui/MainMenu.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace {

constexpr glm::vec4 ink{0.16F, 0.105F, 0.055F, 1.0F};
constexpr glm::vec4 inkMuted{0.34F, 0.24F, 0.14F, 0.88F};
constexpr glm::vec4 paper{0.70F, 0.58F, 0.38F, 0.98F};
constexpr glm::vec4 paperDark{0.45F, 0.33F, 0.19F, 0.98F};
constexpr glm::vec4 brass{0.74F, 0.52F, 0.22F, 1.0F};
constexpr glm::vec4 brassDark{0.34F, 0.22F, 0.10F, 1.0F};
constexpr glm::vec4 waxRed{0.55F, 0.12F, 0.08F, 1.0F};
constexpr glm::vec4 oceanInk{0.13F, 0.31F, 0.34F, 0.68F};
constexpr glm::vec4 jungleInk{0.16F, 0.35F, 0.16F, 0.74F};
constexpr glm::vec4 mountainInk{0.30F, 0.20F, 0.12F, 0.78F};
constexpr glm::vec4 lamp{1.0F, 0.70F, 0.28F, 1.0F};

struct ButtonSpec {
    float y;
    std::string_view label;
    std::string_view value;
    glm::vec4 accent;
    bool active;
    bool disabled;
    bool destructive;
};

float wrap(float value, float period) {
    return value - std::floor(value / period) * period;
}

} // namespace

namespace pcolonist {

MainMenu::Layout MainMenu::layout(int width, int height) {
    const float menuWidth = std::clamp(static_cast<float>(width) - 96.0F, 920.0F, 1120.0F);
    const float menuHeight = std::clamp(static_cast<float>(height) - 76.0F, 560.0F, 650.0F);
    const float left = static_cast<float>(width) * 0.5F - menuWidth * 0.5F;
    const float top = static_cast<float>(height) * 0.5F - menuHeight * 0.5F;
    return {
        left,
        top,
        menuWidth,
        menuHeight,
        left + 48.0F,
        left + menuWidth - 392.0F,
        344.0F,
    };
}

bool MainMenu::contains(double x, double y, float left, float top, float width, float height) {
    return x >= left && x <= left + width && y >= top && y <= top + height;
}

void MainMenu::render(const MainMenuState& state, const Rectangle& rectangle, const Text& text) const {
    const Layout frame = layout(state.width, state.height);
    const float width = static_cast<float>(state.width);
    const float height = static_cast<float>(state.height);
    const float t = state.time;
    const float intro = std::clamp(t * 0.95F, 0.0F, 1.0F);
    const float drift = std::sin(t * 0.18F) * 16.0F;
    const float pulse = 0.72F + std::sin(t * 1.25F) * 0.12F;

    rectangle(0.0F, 0.0F, width, height, {0.012F, 0.026F, 0.038F, 1.0F}, 0.0F);
    rectangle(0.0F, 0.0F, width, height * 0.58F, {0.050F, 0.115F, 0.155F, 0.96F}, 0.0F);
    rectangle(0.0F, height * 0.30F, width, height * 0.34F, {0.86F, 0.40F, 0.18F, 0.22F}, 0.0F);
    rectangle(0.0F, height * 0.54F, width, height * 0.46F, {0.030F, 0.095F, 0.120F, 1.0F}, 0.0F);
    rectangle(0.0F, height * 0.68F, width, height * 0.32F, {0.008F, 0.030F, 0.043F, 0.88F}, 0.0F);

    const auto haze = [&](float y, float alpha, float speed) {
        const float x = wrap(t * speed, width + 320.0F) - 260.0F;
        rectangle(x, y, 260.0F, 30.0F, {0.70F, 0.80F, 0.84F, alpha}, 15.0F);
        rectangle(x + 126.0F, y - 18.0F, 210.0F, 42.0F, {0.70F, 0.80F, 0.84F, alpha * 0.82F}, 21.0F);
        rectangle(x + 294.0F, y + 3.0F, 250.0F, 28.0F, {0.70F, 0.80F, 0.84F, alpha * 0.62F}, 14.0F);
    };
    haze(height * 0.20F, 0.12F, 8.0F);
    haze(height * 0.36F, 0.08F, 4.4F);

    rectangle(width * 0.16F + drift * 0.25F, height * 0.35F, width * 0.55F, height * 0.19F, {0.035F, 0.115F, 0.070F, 0.94F}, 88.0F);
    rectangle(width * 0.28F + drift * 0.16F, height * 0.28F, width * 0.30F, height * 0.12F, {0.038F, 0.135F, 0.078F, 0.96F}, 48.0F);
    rectangle(width * 0.47F + drift * 0.10F, height * 0.22F, width * 0.12F, height * 0.17F, {0.055F, 0.058F, 0.052F, 0.92F}, 14.0F);
    rectangle(width * 0.50F + drift * 0.10F, height * 0.205F, width * 0.055F, height * 0.060F, {0.25F, 0.085F, 0.035F, 0.86F}, 10.0F);
    rectangle(width * 0.60F + drift * 0.06F, height * 0.42F, width * 0.22F, height * 0.07F, {0.040F, 0.115F, 0.072F, 0.86F}, 28.0F);

    for (int index = 0; index < 9; ++index) {
        const float y = height * 0.58F + static_cast<float>(index) * 18.0F;
        const float offset = std::sin(t * 0.9F + static_cast<float>(index) * 0.7F) * 30.0F;
        rectangle(offset - 40.0F, y, width + 80.0F, 2.0F, {0.35F, 0.70F, 0.78F, 0.060F}, 1.0F);
    }

    rectangle(width * 0.12F, height * 0.66F, width * 0.15F, 22.0F, {0.11F, 0.068F, 0.038F, 0.88F}, 3.0F);
    rectangle(width * 0.18F, height * 0.61F, 7.0F, height * 0.10F, {0.10F, 0.064F, 0.035F, 0.92F}, 1.0F);
    rectangle(width * 0.18F, height * 0.61F, width * 0.09F, 3.0F, {0.11F, 0.068F, 0.038F, 0.90F}, 1.0F);
    rectangle(width * 0.26F, height * 0.615F, 4.0F, height * 0.055F, {0.11F, 0.068F, 0.038F, 0.88F}, 1.0F);
    rectangle(width * 0.175F, height * 0.69F, 22.0F, 16.0F, {0.95F, 0.55F, 0.19F, 0.18F + 0.13F * pulse}, 8.0F);

    rectangle(0.0F, 0.0F, width, height, {0.0F, 0.0F, 0.0F, 0.20F}, 0.0F);
    rectangle(0.0F, 0.0F, width * 0.34F, height, {0.0F, 0.0F, 0.0F, 0.34F}, 0.0F);
    rectangle(width * 0.68F, 0.0F, width * 0.32F, height, {0.0F, 0.0F, 0.0F, 0.28F}, 0.0F);

    const auto glassPanel = [&](float x, float y, float panelWidth, float panelHeight, float alpha) {
        rectangle(x + 10.0F, y + 12.0F, panelWidth, panelHeight, {0.0F, 0.0F, 0.0F, 0.34F * alpha}, 8.0F);
        rectangle(x, y, panelWidth, panelHeight, {0.52F, 0.70F, 0.72F, 0.18F * alpha}, 8.0F);
        rectangle(x + 1.0F, y + 1.0F, panelWidth - 2.0F, panelHeight - 2.0F, {0.035F, 0.060F, 0.070F, 0.58F * alpha}, 7.0F);
        rectangle(x + 20.0F, y + 16.0F, panelWidth - 40.0F, 1.0F, {0.85F, 0.98F, 1.0F, 0.22F * alpha}, 0.0F);
    };

    glassPanel(frame.storyLeft - 12.0F, frame.top + 70.0F, 492.0F, 418.0F, intro);
    glassPanel(frame.actionLeft - 18.0F, frame.top + 86.0F, frame.buttonWidth + 36.0F, 486.0F, intro);

    rectangle(frame.storyLeft + 8.0F, frame.top + 94.0F, 58.0F, 3.0F, {0.90F, 0.62F, 0.30F, intro}, 0.0F);
    text(frame.storyLeft + 8.0F, frame.top + 118.0F, "COLONIST", 5.1F, {0.94F, 0.97F, 0.92F, intro});
    text(frame.storyLeft + 12.0F, frame.top + 178.0F, "ПОСЛЕ КОРАБЛЕКРУШЕНИЯ", 1.55F, {0.90F, 0.67F, 0.36F, 0.92F * intro});
    text(frame.storyLeft + 12.0F, frame.top + 224.0F, "ЗАГАДОЧНЫЙ ОСТРОВ", 2.1F, {0.94F, 0.97F, 0.92F, 0.94F * intro});
    text(frame.storyLeft + 12.0F, frame.top + 260.0F, "ВЫЖИВАНИЕ, ИССЛЕДОВАНИЕ", 1.55F, {0.72F, 0.83F, 0.82F, 0.82F * intro});
    text(frame.storyLeft + 12.0F, frame.top + 288.0F, "И ОТКРЫТИЕ НОВЫХ ЗЕМЕЛЬ", 1.55F, {0.72F, 0.83F, 0.82F, 0.82F * intro});
    rectangle(frame.storyLeft + 12.0F, frame.top + 338.0F, 384.0F, 1.0F, {0.85F, 0.98F, 1.0F, 0.16F * intro}, 0.0F);
    text(frame.storyLeft + 12.0F, frame.top + 368.0F, "EXPEDITION STATUS", 1.25F, {0.90F, 0.67F, 0.36F, 0.90F * intro});
    text(frame.storyLeft + 12.0F, frame.top + 402.0F, "UNKNOWN LANDMASS", 1.45F, {0.94F, 0.97F, 0.92F, 0.90F * intro});
    text(frame.storyLeft + 12.0F, frame.top + 430.0F, "SIGNAL FIRE REQUIRED", 1.45F, {0.94F, 0.97F, 0.92F, 0.76F * intro});
    text(frame.storyLeft + 12.0F, frame.top + 458.0F, "OCEAN ROUTES UNCHARTED", 1.45F, {0.94F, 0.97F, 0.92F, 0.62F * intro});

    const auto menuButton = [&](const ButtonSpec& button) {
        const bool hovered = !button.disabled && contains(state.pointerX, state.pointerY, frame.actionLeft, button.y, frame.buttonWidth, 58.0F);
        const glm::vec4 labelColor = button.disabled
            ? glm::vec4{0.45F, 0.54F, 0.55F, 0.75F}
            : button.destructive ? glm::vec4{0.95F, 0.42F, 0.34F, 1.0F} : glm::vec4{0.94F, 0.98F, 0.96F, 1.0F};
        const glm::vec4 accent = button.destructive ? glm::vec4{0.95F, 0.24F, 0.18F, 1.0F} : glm::vec4{0.34F, 0.88F, 0.92F, 1.0F};
        rectangle(frame.actionLeft + 8.0F, button.y + 8.0F, frame.buttonWidth, 58.0F, {0.0F, 0.0F, 0.0F, 0.28F}, 6.0F);
        rectangle(frame.actionLeft, button.y, frame.buttonWidth, 58.0F, hovered ? glm::vec4{0.52F, 0.82F, 0.86F, 0.42F} : glm::vec4{0.38F, 0.54F, 0.56F, 0.20F}, 6.0F);
        rectangle(frame.actionLeft + 1.0F, button.y + 1.0F, frame.buttonWidth - 2.0F, 56.0F, hovered ? glm::vec4{0.090F, 0.145F, 0.155F, 0.88F} : glm::vec4{0.030F, 0.055F, 0.064F, 0.76F}, 5.0F);
        rectangle(frame.actionLeft + 1.0F, button.y + 1.0F, hovered ? frame.buttonWidth - 2.0F : 5.0F, 2.0F, accent, 1.0F);
        rectangle(frame.actionLeft + 18.0F, button.y + 17.0F, 3.0F, 24.0F, accent, 1.0F);
        text(frame.actionLeft + 38.0F, button.y + 19.0F, button.label, 2.05F, labelColor);
        if (!button.value.empty()) {
            rectangle(
                frame.actionLeft + frame.buttonWidth - 110.0F,
                button.y + 15.0F,
                88.0F,
                28.0F,
                button.active ? glm::vec4{0.10F, 0.25F, 0.27F, 0.74F} : glm::vec4{0.02F, 0.04F, 0.05F, 0.52F},
                1.0F);
            text(frame.actionLeft + frame.buttonWidth - 96.0F, button.y + 23.0F, button.value, 1.45F, labelColor);
        }
    };

    if (screen_ == Screen::Home) {
        text(frame.actionLeft + 4.0F, frame.top + 116.0F, "MAIN MENU", 1.45F, {0.52F, 0.88F, 0.90F, 0.88F});
        text(frame.actionLeft + 4.0F, frame.top + 146.0F, "SURVIVAL STARTS HERE", 1.75F, {0.94F, 0.98F, 0.96F, 0.92F});
        rectangle(frame.actionLeft + 4.0F, frame.top + 192.0F, frame.buttonWidth - 8.0F, 1.0F, {0.85F, 0.98F, 1.0F, 0.18F}, 0.0F);
        menuButton({frame.top + 226.0F, "НОВАЯ ИГРА", "", brass, true, false, false});
        menuButton({frame.top + 298.0F, "ПРОДОЛЖИТЬ", "", brass, false, false, false});
        menuButton({frame.top + 370.0F, "НАСТРОЙКИ", "", brass, false, false, false});
        menuButton({frame.top + 500.0F, "ВЫХОД", "", waxRed, false, false, true});
        return;
    }

    if (screen_ == Screen::LoadGame) {
        text(frame.actionLeft + 4.0F, frame.top + 116.0F, "CONTINUE", 2.5F, {0.94F, 0.98F, 0.96F, 0.92F});
        rectangle(frame.actionLeft, frame.top + 174.0F, frame.buttonWidth, 150.0F, {0.030F, 0.055F, 0.064F, 0.72F}, 6.0F);
        rectangle(frame.actionLeft + 18.0F, frame.top + 198.0F, 3.0F, 88.0F, {0.34F, 0.88F, 0.92F, 0.82F}, 1.0F);
        text(frame.actionLeft + 40.0F, frame.top + 205.0F, "СОХРАНЕНИЙ НЕТ", 2.0F, {0.94F, 0.98F, 0.96F, 0.92F});
        text(frame.actionLeft + 40.0F, frame.top + 246.0F, "ЭКСПЕДИЦИЯ ЕЩЕ НЕ НАЧАТА", 1.15F, {0.65F, 0.75F, 0.75F, 0.78F});
        text(frame.actionLeft + 40.0F, frame.top + 274.0F, "СОЗДАЙТЕ НОВУЮ ИГРУ", 1.05F, {0.65F, 0.75F, 0.75F, 0.70F});
        menuButton({frame.top + 370.0F, "НАЗАД", "", brass, false, false, false});
        menuButton({frame.top + 500.0F, "ВЫХОД", "", waxRed, false, false, true});
        return;
    }

    text(frame.actionLeft + 4.0F, frame.top + 116.0F, "НАСТРОЙКИ", 2.6F, {0.94F, 0.98F, 0.96F, 0.92F});
    text(frame.actionLeft + 4.0F, frame.top + 158.0F, "DISPLAY AND RENDERING", 1.15F, {0.52F, 0.88F, 0.90F, 0.78F});

    const auto optionButton = [&](int index, std::string_view label, std::string_view value, bool active) {
        const float y = frame.top + 198.0F + static_cast<float>(index) * 54.0F;
        const bool hovered = contains(state.pointerX, state.pointerY, frame.actionLeft, y, frame.buttonWidth, 46.0F);
        rectangle(frame.actionLeft, y, frame.buttonWidth, 46.0F, hovered ? glm::vec4{0.10F, 0.20F, 0.22F, 0.78F} : glm::vec4{0.030F, 0.055F, 0.064F, 0.68F}, 4.0F);
        rectangle(frame.actionLeft, y, 4.0F, 46.0F, active ? glm::vec4{0.34F, 0.88F, 0.92F, 1.0F} : glm::vec4{0.38F, 0.54F, 0.56F, 0.55F}, 1.0F);
        text(frame.actionLeft + 24.0F, y + 16.0F, label, 1.45F, {0.94F, 0.98F, 0.96F, 0.90F});
        text(frame.actionLeft + 228.0F, y + 16.0F, value, 1.45F, active ? glm::vec4{0.52F, 0.95F, 0.98F, 0.92F} : glm::vec4{0.65F, 0.75F, 0.75F, 0.72F});
    };

    optionButton(0, "ПОЛНЫЙ ЭКРАН", state.fullscreen ? "ВКЛ" : "ВЫКЛ", state.fullscreen);
    optionButton(1, "ВЕРТ СИНХР", state.vsync ? "ВКЛ" : "ВЫКЛ", state.vsync);
    optionButton(2, "ЛИМИТ FPS", state.frameLimit == 0 ? "НЕТ" : std::to_string(state.frameLimit), state.frameLimit != 0);
    optionButton(3, "ТЕНИ", state.shadows ? "ВКЛ" : "ВЫКЛ", state.shadows);
    optionButton(4, "СВЕЧЕНИЕ", state.bloom ? "ВКЛ" : "ВЫКЛ", state.bloom);
    menuButton({frame.top + 500.0F, "НАЗАД", "", brass, false, false, false});
}

MainMenuAction MainMenu::actionAt(double x, double y, int width, int height) {
    const Layout frame = layout(width, height);

    if (screen_ == Screen::Home) {
        if (contains(x, y, frame.actionLeft, frame.top + 226.0F, frame.buttonWidth, 58.0F)) {
            return MainMenuAction::Play;
        }
        if (contains(x, y, frame.actionLeft, frame.top + 298.0F, frame.buttonWidth, 58.0F)) {
            screen_ = Screen::LoadGame;
            return MainMenuAction::None;
        }
        if (contains(x, y, frame.actionLeft, frame.top + 370.0F, frame.buttonWidth, 58.0F)) {
            screen_ = Screen::Settings;
            return MainMenuAction::None;
        }
        if (contains(x, y, frame.actionLeft, frame.top + 500.0F, frame.buttonWidth, 58.0F)) {
            return MainMenuAction::Quit;
        }
        return MainMenuAction::None;
    }

    if (screen_ == Screen::LoadGame) {
        if (contains(x, y, frame.actionLeft, frame.top + 370.0F, frame.buttonWidth, 58.0F)) {
            screen_ = Screen::Home;
        } else if (contains(x, y, frame.actionLeft, frame.top + 500.0F, frame.buttonWidth, 58.0F)) {
            return MainMenuAction::Quit;
        }
        return MainMenuAction::None;
    }

    for (int index = 0; index < 5; ++index) {
        const float top = frame.top + 198.0F + static_cast<float>(index) * 54.0F;
        if (!contains(x, y, frame.actionLeft, top, frame.buttonWidth, 46.0F)) {
            continue;
        }
        switch (index) {
        case 0: return MainMenuAction::ToggleFullscreen;
        case 1: return MainMenuAction::ToggleVsync;
        case 2: return MainMenuAction::CycleFrameLimit;
        case 3: return MainMenuAction::ToggleShadows;
        case 4: return MainMenuAction::ToggleBloom;
        default: break;
        }
    }
    if (contains(x, y, frame.actionLeft, frame.top + 500.0F, frame.buttonWidth, 58.0F)) {
        screen_ = Screen::Home;
    }
    return MainMenuAction::None;
}

} // namespace pcolonist
