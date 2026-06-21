#include "pcolonist/ui/MainMenu.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace {

constexpr glm::vec4 backgroundDeep{0.006F, 0.010F, 0.018F, 1.0F};
constexpr glm::vec4 backgroundCold{0.020F, 0.045F, 0.070F, 1.0F};
constexpr glm::vec4 backgroundRift{0.085F, 0.025F, 0.070F, 0.72F};
constexpr glm::vec4 panelGlass{0.020F, 0.035F, 0.050F, 0.74F};
constexpr glm::vec4 panelStroke{0.40F, 0.70F, 0.78F, 0.32F};
constexpr glm::vec4 textPrimary{0.93F, 0.98F, 0.97F, 1.0F};
constexpr glm::vec4 textMuted{0.56F, 0.67F, 0.68F, 0.86F};
constexpr glm::vec4 accentCyan{0.28F, 0.90F, 0.96F, 1.0F};
constexpr glm::vec4 accentGold{0.93F, 0.66F, 0.28F, 1.0F};
constexpr glm::vec4 accentDanger{0.94F, 0.25F, 0.22F, 1.0F};

constexpr std::string_view buildVersion = "v0.1-dev";

float wrap(float value, float period) {
    return value - std::floor(value / period) * period;
}

float easeOut(float value) {
    const float clamped = std::clamp(value, 0.0F, 1.0F);
    return 1.0F - (1.0F - clamped) * (1.0F - clamped);
}

glm::vec4 withAlpha(glm::vec4 color, float alpha) {
    color.a *= alpha;
    return color;
}

} // namespace

namespace pcolonist {

MainMenu::Layout MainMenu::layout(int width, int height) {
    const float screenWidth = static_cast<float>(width);
    const float screenHeight = static_cast<float>(height);
    const float menuWidth = std::clamp(screenWidth - 96.0F, 820.0F, 1180.0F);
    const float menuHeight = std::clamp(screenHeight - 72.0F, 590.0F, 720.0F);
    const float left = screenWidth * 0.5F - menuWidth * 0.5F;
    const float top = screenHeight * 0.5F - menuHeight * 0.5F;
    const float actionWidth = std::clamp(menuWidth * 0.34F, 330.0F, 390.0F);
    return {
        left,
        top,
        menuWidth,
        menuHeight,
        left + std::clamp(menuWidth * 0.060F, 34.0F, 72.0F),
        left + menuWidth - actionWidth - std::clamp(menuWidth * 0.055F, 34.0F, 64.0F),
        actionWidth,
    };
}

bool MainMenu::contains(double x, double y, float left, float top, float width, float height) {
    return x >= left && x <= left + width && y >= top && y <= top + height;
}

void MainMenu::renderBackground(
    const MainMenuState& state,
    const Rectangle& rectangle,
    const Text& text,
    const Layout& frame,
    float intro) const {
    const float width = static_cast<float>(state.width);
    const float height = static_cast<float>(state.height);
    const float t = state.time;
    const float drift = std::sin(t * 0.10F) * 22.0F;

    rectangle(0.0F, 0.0F, width, height, backgroundDeep, 0.0F);
    rectangle(0.0F, 0.0F, width, height * 0.58F, {0.018F, 0.040F, 0.067F, 0.96F}, 0.0F);
    rectangle(0.0F, height * 0.20F, width, height * 0.48F, backgroundRift, 0.0F);
    rectangle(0.0F, height * 0.58F, width, height * 0.42F, backgroundCold, 0.0F);

    const auto fogBand = [&](float y, float alpha, float speed, float bandWidth) {
        const float x = wrap(t * speed, width + bandWidth) - bandWidth * 0.62F;
        rectangle(x, y, bandWidth, 26.0F, {0.65F, 0.86F, 0.94F, alpha}, 13.0F);
        rectangle(x + bandWidth * 0.28F, y - 18.0F, bandWidth * 0.78F, 44.0F, {0.58F, 0.76F, 0.86F, alpha * 0.66F}, 22.0F);
        rectangle(x + bandWidth * 0.82F, y + 8.0F, bandWidth * 0.64F, 22.0F, {0.75F, 0.52F, 0.92F, alpha * 0.42F}, 11.0F);
    };
    fogBand(height * 0.22F, 0.12F, 7.0F, 340.0F);
    fogBand(height * 0.42F, 0.085F, 4.2F, 430.0F);
    fogBand(height * 0.66F, 0.070F, 2.6F, 520.0F);

    for (int index = 0; index < 32; ++index) {
        const float seed = static_cast<float>(index);
        const float x = wrap(seed * 97.0F + t * (7.0F + std::fmod(seed, 5.0F)), width + 80.0F) - 40.0F;
        const float y = wrap(seed * 53.0F + std::sin(t * 0.2F + seed) * 18.0F, height);
        const float size = 1.0F + std::fmod(seed, 4.0F);
        const float alpha = (0.08F + std::fmod(seed, 6.0F) * 0.018F) * intro;
        rectangle(x, y, size, size, {0.45F, 0.95F, 1.0F, alpha}, size * 0.5F);
    }

    rectangle(width * 0.52F + drift, height * 0.15F, width * 0.17F, height * 0.36F, {0.030F, 0.035F, 0.045F, 0.74F}, 8.0F);
    rectangle(width * 0.55F + drift, height * 0.13F, width * 0.075F, height * 0.12F, {0.18F, 0.06F, 0.13F, 0.82F}, 12.0F);
    rectangle(width * 0.17F - drift * 0.3F, height * 0.55F, width * 0.42F, height * 0.14F, {0.018F, 0.070F, 0.055F, 0.88F}, 64.0F);
    rectangle(width * 0.34F - drift * 0.16F, height * 0.47F, width * 0.18F, height * 0.10F, {0.030F, 0.088F, 0.066F, 0.90F}, 32.0F);

    rectangle(0.0F, 0.0F, width, height, {0.0F, 0.0F, 0.0F, 0.22F}, 0.0F);
    rectangle(0.0F, 0.0F, width * 0.26F, height, {0.0F, 0.0F, 0.0F, 0.42F}, 0.0F);
    rectangle(width * 0.72F, 0.0F, width * 0.28F, height, {0.0F, 0.0F, 0.0F, 0.34F}, 0.0F);
    rectangle(0.0F, 0.0F, width, height * 0.12F, {0.0F, 0.0F, 0.0F, 0.35F}, 0.0F);
    rectangle(0.0F, height * 0.82F, width, height * 0.18F, {0.0F, 0.0F, 0.0F, 0.52F}, 0.0F);

    const float scanAlpha = 0.035F * intro;
    for (int index = 0; index < 10; ++index) {
        const float y = frame.top + 20.0F + static_cast<float>(index) * 54.0F + std::sin(t + static_cast<float>(index)) * 3.0F;
        rectangle(frame.left, y, frame.width, 1.0F, {0.30F, 0.80F, 0.88F, scanAlpha}, 0.0F);
    }

    text(frame.storyLeft, frame.top + frame.height - 114.0F, tr(state.language, UiText::SignalLost), 1.15F, withAlpha(textMuted, intro * 0.72F));
}

void MainMenu::renderBottomBar(
    const MainMenuState& state,
    const Rectangle& rectangle,
    const Text& text,
    float intro) const {
    const float width = static_cast<float>(state.width);
    const float height = static_cast<float>(state.height);
    constexpr float barHeight = 52.0F;
    rectangle(0.0F, height - barHeight, width, barHeight, {0.010F, 0.016F, 0.022F, 0.82F * intro}, 0.0F);
    rectangle(0.0F, height - barHeight, width, 1.0F, {0.48F, 0.92F, 0.96F, 0.22F * intro}, 0.0F);
    text(28.0F, height - 32.0F, buildVersion, 1.15F, withAlpha(textMuted, intro));
    text(width * 0.5F - 175.0F, height - 32.0F, tr(state.language, UiText::Copyright), 1.05F, withAlpha(textMuted, intro * 0.82F));
    text(width - 260.0F, height - 32.0F, tr(state.language, UiText::Profile), 1.10F, withAlpha(textPrimary, intro * 0.90F));
}

void MainMenu::renderShell(
    const MainMenuState& state,
    const Rectangle& rectangle,
    const Text& text,
    const Layout& frame,
    float intro) const {
    const float titleSlide = (1.0F - intro) * -36.0F;
    const float pulse = 0.5F + std::sin(state.time * 1.5F) * 0.5F;

    rectangle(frame.storyLeft - 24.0F, frame.top + 82.0F, 86.0F, 2.0F, withAlpha(accentGold, intro), 0.0F);
    text(frame.storyLeft + titleSlide, frame.top + 112.0F, tr(state.language, UiText::GameTitle), 5.2F, withAlpha(textPrimary, intro));
    text(frame.storyLeft + 4.0F + titleSlide * 0.4F, frame.top + 176.0F, tr(state.language, UiText::GameSubtitle), 1.55F, withAlpha(accentGold, intro * 0.92F));
    rectangle(frame.storyLeft, frame.top + 224.0F, std::clamp(frame.width * 0.36F, 310.0F, 430.0F), 1.0F, {0.72F, 0.96F, 1.0F, 0.18F * intro}, 0.0F);
    text(frame.storyLeft + 2.0F, frame.top + 258.0F, tr(state.language, UiText::StoryLinePrimary), 1.38F, withAlpha(textPrimary, intro * 0.82F));
    text(frame.storyLeft + 2.0F, frame.top + 288.0F, tr(state.language, UiText::StoryLineSecondary), 1.38F, withAlpha(textMuted, intro * 0.82F));
    rectangle(frame.storyLeft + 2.0F, frame.top + 342.0F, 12.0F, 12.0F, {0.28F, 0.90F, 0.96F, (0.40F + pulse * 0.30F) * intro}, 6.0F);
    text(frame.storyLeft + 28.0F, frame.top + 340.0F, state.gameStarted ? tr(state.language, UiText::SessionActive) : tr(state.language, UiText::AwaitingDeployment), 1.18F, withAlpha(textMuted, intro));

    rectangle(frame.actionLeft - 20.0F, frame.top + 82.0F, frame.buttonWidth + 40.0F, frame.height - 138.0F, {0.0F, 0.0F, 0.0F, 0.30F * intro}, 10.0F);
    rectangle(frame.actionLeft - 30.0F, frame.top + 72.0F, frame.buttonWidth + 42.0F, frame.height - 140.0F, withAlpha(panelStroke, intro), 9.0F);
    rectangle(frame.actionLeft - 28.0F, frame.top + 74.0F, frame.buttonWidth + 38.0F, frame.height - 144.0F, withAlpha(panelGlass, intro), 8.0F);
    rectangle(frame.actionLeft - 8.0F, frame.top + 98.0F, frame.buttonWidth - 2.0F, 1.0F, {0.80F, 0.96F, 1.0F, 0.18F * intro}, 0.0F);
    text(frame.actionLeft - 4.0F, frame.top + 118.0F, tr(state.language, UiText::Command), 1.24F, withAlpha(accentCyan, intro * 0.90F));
}

void MainMenu::renderButton(
    const MainMenuState& state,
    const Rectangle& rectangle,
    const Text& text,
    const Layout& frame,
    const MainMenu::ButtonSpec& button,
    float intro) const {
    const float buttonHeight = 52.0F;
    const bool hovered = !button.disabled && contains(state.pointerX, state.pointerY, frame.actionLeft, button.y, frame.buttonWidth, buttonHeight);
    const float hover = hovered ? 1.0F : 0.0F;
    const glm::vec4 accent = button.destructive ? accentDanger : button.accent;
    const glm::vec4 labelColor = button.disabled
        ? glm::vec4{0.36F, 0.45F, 0.46F, 0.70F * intro}
        : button.destructive ? withAlpha(glm::vec4{1.0F, 0.46F, 0.40F, 1.0F}, intro) : withAlpha(textPrimary, intro);
    const float x = frame.actionLeft + hover * 8.0F;
    const float glow = 0.18F + hover * 0.34F;

    rectangle(x + 9.0F, button.y + 9.0F, frame.buttonWidth, buttonHeight, {0.0F, 0.0F, 0.0F, 0.30F * intro}, 6.0F);
    rectangle(x, button.y, frame.buttonWidth, buttonHeight, {0.34F, 0.76F, 0.84F, glow * intro}, 6.0F);
    rectangle(x + 1.0F, button.y + 1.0F, frame.buttonWidth - 2.0F, buttonHeight - 2.0F, {0.018F, 0.032F, 0.044F, (0.78F + hover * 0.10F) * intro}, 5.0F);
    rectangle(x + 1.0F, button.y + 1.0F, hovered ? frame.buttonWidth - 2.0F : 7.0F, 2.0F, withAlpha(accent, intro), 1.0F);
    rectangle(x + 18.0F, button.y + 15.0F, 3.0F, 22.0F, withAlpha(accent, intro), 1.0F);
    text(x + 38.0F, button.y + 17.0F, button.label, 1.90F, labelColor);
    if (!button.hint.empty()) {
        text(x + frame.buttonWidth - 100.0F, button.y + 20.0F, button.hint, 1.10F, button.disabled ? withAlpha(textMuted, intro * 0.46F) : withAlpha(accent, intro * 0.82F));
    }
}

std::array<MainMenu::ButtonSpec, 6> MainMenu::homeButtons(const MainMenuState& state, const Layout& frame) const {
    const float first = frame.top + 158.0F;
    constexpr float step = 62.0F;
    return {
        MainMenu::ButtonSpec{MainMenuAction::Continue, first + step * 0.0F, tr(state.language, UiText::Continue), state.gameStarted ? tr(state.language, UiText::Live) : state.saveAvailable ? tr(state.language, UiText::Save) : tr(state.language, UiText::None), accentCyan, !state.gameStarted && !state.saveAvailable, false},
        MainMenu::ButtonSpec{MainMenuAction::NewGame, first + step * 1.0F, tr(state.language, UiText::NewGame), tr(state.language, UiText::Start), accentGold, false, false},
        MainMenu::ButtonSpec{MainMenuAction::OpenLoadGame, first + step * 2.0F, tr(state.language, UiText::LoadGame), state.saveAvailable ? tr(state.language, UiText::Ready) : tr(state.language, UiText::Empty), accentCyan, false, false},
        MainMenu::ButtonSpec{MainMenuAction::OpenSettings, first + step * 3.0F, tr(state.language, UiText::Settings), "", accentCyan, false, false},
        MainMenu::ButtonSpec{MainMenuAction::OpenMods, first + step * 4.0F, tr(state.language, UiText::Mods), tr(state.language, UiText::Local), accentCyan, false, false},
        MainMenu::ButtonSpec{MainMenuAction::Quit, first + step * 5.0F, tr(state.language, UiText::Exit), "", accentDanger, false, true},
    };
}

void MainMenu::renderHome(const MainMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& frame, float intro) const {
    for (const MainMenu::ButtonSpec& button : homeButtons(state, frame)) {
        renderButton(state, rectangle, text, frame, button, intro);
    }
    if (state.gameStarted) {
        const MainMenu::ButtonSpec save{MainMenuAction::SaveGame, frame.top + frame.height - 102.0F, tr(state.language, UiText::SaveGame), tr(state.language, UiText::Quick), accentGold, false, false};
        renderButton(state, rectangle, text, frame, save, intro * 0.94F);
    }
}

void MainMenu::renderLoadGame(const MainMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& frame, float intro) const {
    text(frame.actionLeft - 4.0F, frame.top + 162.0F, tr(state.language, UiText::LoadGame), 2.25F, withAlpha(textPrimary, intro));
    rectangle(frame.actionLeft, frame.top + 210.0F, frame.buttonWidth, 146.0F, {0.018F, 0.032F, 0.044F, 0.78F * intro}, 6.0F);
    rectangle(frame.actionLeft + 18.0F, frame.top + 232.0F, 3.0F, 86.0F, withAlpha(state.saveAvailable ? accentCyan : textMuted, intro), 1.0F);
    text(frame.actionLeft + 40.0F, frame.top + 238.0F, state.saveAvailable ? tr(state.language, UiText::SaveSlot) : tr(state.language, UiText::NoSaveData), 1.70F, withAlpha(textPrimary, intro));
    text(frame.actionLeft + 40.0F, frame.top + 278.0F, state.saveAvailable ? tr(state.language, UiText::RestoreSave) : tr(state.language, UiText::StartNewFirst), 1.05F, withAlpha(textMuted, intro));
    renderButton(state, rectangle, text, frame, {MainMenuAction::LoadGame, frame.top + 386.0F, tr(state.language, UiText::LoadSelected), "", accentCyan, !state.saveAvailable, false}, intro);
    renderButton(state, rectangle, text, frame, {MainMenuAction::Back, frame.top + 454.0F, tr(state.language, UiText::Back), "", accentGold, false, false}, intro);
}

void MainMenu::renderSettings(const MainMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& frame, float intro) const {
    text(frame.actionLeft - 4.0F, frame.top + 162.0F, tr(state.language, UiText::Settings), 2.25F, withAlpha(textPrimary, intro));
    const auto optionButton = [&](int index, MainMenuAction action, std::string_view label, std::string_view value, bool active) {
        const float y = frame.top + 214.0F + static_cast<float>(index) * 48.0F;
        const bool hovered = contains(state.pointerX, state.pointerY, frame.actionLeft, y, frame.buttonWidth, 44.0F);
        rectangle(frame.actionLeft, y, frame.buttonWidth, 44.0F, hovered ? glm::vec4{0.10F, 0.20F, 0.22F, 0.78F * intro} : glm::vec4{0.018F, 0.032F, 0.044F, 0.70F * intro}, 4.0F);
        rectangle(frame.actionLeft, y, 4.0F, 44.0F, active ? withAlpha(accentCyan, intro) : withAlpha(textMuted, intro * 0.45F), 1.0F);
        text(frame.actionLeft + 24.0F, y + 15.0F, label, 1.34F, withAlpha(textPrimary, intro * 0.90F));
        text(frame.actionLeft + frame.buttonWidth - 116.0F, y + 15.0F, value, 1.28F, active ? withAlpha(accentCyan, intro) : withAlpha(textMuted, intro));
        static_cast<void>(action);
    };
    optionButton(0, MainMenuAction::ToggleFullscreen, tr(state.language, UiText::Fullscreen), state.fullscreen ? tr(state.language, UiText::On) : tr(state.language, UiText::Off), state.fullscreen);
    optionButton(1, MainMenuAction::ToggleVsync, tr(state.language, UiText::Vsync), state.vsync ? tr(state.language, UiText::On) : tr(state.language, UiText::Off), state.vsync);
    optionButton(2, MainMenuAction::CycleFrameLimit, tr(state.language, UiText::FpsLimit), state.frameLimit == 0 ? std::string(tr(state.language, UiText::None)) : std::to_string(state.frameLimit), state.frameLimit != 0);
    optionButton(3, MainMenuAction::ToggleShadows, tr(state.language, UiText::Shadows), state.shadows ? tr(state.language, UiText::On) : tr(state.language, UiText::Off), state.shadows);
    optionButton(4, MainMenuAction::ToggleBloom, tr(state.language, UiText::Bloom), state.bloom ? tr(state.language, UiText::On) : tr(state.language, UiText::Off), state.bloom);
    optionButton(5, MainMenuAction::CycleLanguage, tr(state.language, UiText::Language), languageName(state.language), true);
    renderButton(state, rectangle, text, frame, {MainMenuAction::Back, frame.top + 512.0F, tr(state.language, UiText::Back), "", accentGold, false, false}, intro);
}

void MainMenu::renderMods(const MainMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& frame, float intro) const {
    text(frame.actionLeft - 4.0F, frame.top + 162.0F, tr(state.language, UiText::Mods), 2.25F, withAlpha(textPrimary, intro));
    rectangle(frame.actionLeft, frame.top + 214.0F, frame.buttonWidth, 168.0F, {0.018F, 0.032F, 0.044F, 0.78F * intro}, 6.0F);
    rectangle(frame.actionLeft + 18.0F, frame.top + 238.0F, 3.0F, 98.0F, withAlpha(accentGold, intro), 1.0F);
    text(frame.actionLeft + 40.0F, frame.top + 244.0F, tr(state.language, UiText::ModLoaderOffline), 1.55F, withAlpha(textPrimary, intro));
    text(frame.actionLeft + 40.0F, frame.top + 284.0F, tr(state.language, UiText::ReservedForMods), 1.02F, withAlpha(textMuted, intro));
    text(frame.actionLeft + 40.0F, frame.top + 312.0F, tr(state.language, UiText::NoDependencies), 1.02F, withAlpha(textMuted, intro * 0.82F));
    renderButton(state, rectangle, text, frame, {MainMenuAction::Back, frame.top + 454.0F, tr(state.language, UiText::Back), "", accentGold, false, false}, intro);
}

void MainMenu::render(const MainMenuState& state, const Rectangle& rectangle, const Text& text) const {
    lastState_ = state;
    const Layout frame = layout(state.width, state.height);
    const float intro = easeOut(state.time * 0.75F);

    renderBackground(state, rectangle, text, frame, intro);
    renderShell(state, rectangle, text, frame, intro);

    switch (screen_) {
    case Screen::Home:
        renderHome(state, rectangle, text, frame, intro);
        break;
    case Screen::LoadGame:
        renderLoadGame(state, rectangle, text, frame, intro);
        break;
    case Screen::Settings:
        renderSettings(state, rectangle, text, frame, intro);
        break;
    case Screen::Mods:
        renderMods(state, rectangle, text, frame, intro);
        break;
    }

    renderBottomBar(state, rectangle, text, intro);
}

MainMenuAction MainMenu::actionAt(double x, double y, int width, int height) {
    const Layout frame = layout(width, height);
    const MainMenuState& state = lastState_;

    if (screen_ == Screen::Home) {
        for (const MainMenu::ButtonSpec& button : homeButtons(state, frame)) {
            if (!button.disabled && contains(x, y, frame.actionLeft, button.y, frame.buttonWidth, 52.0F)) {
                if (button.action == MainMenuAction::OpenSettings) {
                    screen_ = Screen::Settings;
                    return MainMenuAction::None;
                }
                if (button.action == MainMenuAction::OpenLoadGame) {
                    screen_ = Screen::LoadGame;
                    return MainMenuAction::None;
                }
                if (button.action == MainMenuAction::OpenMods) {
                    screen_ = Screen::Mods;
                    return MainMenuAction::None;
                }
                return button.action;
            }
        }
        if (state.gameStarted && contains(x, y, frame.actionLeft, frame.top + frame.height - 102.0F, frame.buttonWidth, 52.0F)) {
            return MainMenuAction::SaveGame;
        }
        return MainMenuAction::None;
    }

    if (screen_ == Screen::LoadGame) {
        if (state.saveAvailable && contains(x, y, frame.actionLeft, frame.top + 386.0F, frame.buttonWidth, 52.0F)) {
            return MainMenuAction::LoadGame;
        }
        if (contains(x, y, frame.actionLeft, frame.top + 454.0F, frame.buttonWidth, 52.0F)) {
            screen_ = Screen::Home;
        }
        return MainMenuAction::None;
    }

    if (screen_ == Screen::Settings) {
        for (int index = 0; index < 6; ++index) {
            const float top = frame.top + 214.0F + static_cast<float>(index) * 48.0F;
            if (!contains(x, y, frame.actionLeft, top, frame.buttonWidth, 44.0F)) {
                continue;
            }
            switch (index) {
            case 0: return MainMenuAction::ToggleFullscreen;
            case 1: return MainMenuAction::ToggleVsync;
            case 2: return MainMenuAction::CycleFrameLimit;
            case 3: return MainMenuAction::ToggleShadows;
            case 4: return MainMenuAction::ToggleBloom;
            case 5: return MainMenuAction::CycleLanguage;
            default: break;
            }
        }
        if (contains(x, y, frame.actionLeft, frame.top + 512.0F, frame.buttonWidth, 52.0F)) {
            screen_ = Screen::Home;
        }
        return MainMenuAction::None;
    }

    if (contains(x, y, frame.actionLeft, frame.top + 454.0F, frame.buttonWidth, 52.0F)) {
        screen_ = Screen::Home;
    }
    return MainMenuAction::None;
}

} // namespace pcolonist
