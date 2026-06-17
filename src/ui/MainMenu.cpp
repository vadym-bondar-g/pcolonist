#include "pcolonist/ui/MainMenu.hpp"

#include <string>

namespace {

constexpr glm::vec4 backdrop{0.006F, 0.010F, 0.015F, 1.0F};
constexpr glm::vec4 panel{0.018F, 0.022F, 0.028F, 0.96F};
constexpr glm::vec4 panelRaised{0.032F, 0.039F, 0.048F, 0.98F};
constexpr glm::vec4 panelHover{0.055F, 0.072F, 0.084F, 1.0F};
constexpr glm::vec4 border{0.20F, 0.24F, 0.27F, 0.92F};
constexpr glm::vec4 textPrimary{0.88F, 0.90F, 0.91F, 1.0F};
constexpr glm::vec4 textMuted{0.46F, 0.51F, 0.54F, 1.0F};
constexpr glm::vec4 cyan{0.20F, 0.68F, 0.76F, 1.0F};
constexpr glm::vec4 amber{0.78F, 0.61F, 0.30F, 1.0F};
constexpr glm::vec4 red{0.76F, 0.31F, 0.33F, 1.0F};

struct ButtonSpec {
    float y;
    std::string_view label;
    std::string_view value;
    glm::vec4 accent;
    bool active;
    bool disabled;
    bool destructive;
};

} // namespace

namespace pcolonist {

MainMenu::Layout MainMenu::layout(int width, int height) {
    constexpr float menuWidth = 960.0F;
    constexpr float menuHeight = 590.0F;
    const float left = static_cast<float>(width) * 0.5F - menuWidth * 0.5F;
    const float top = static_cast<float>(height) * 0.5F - menuHeight * 0.5F;
    return {
        left,
        top,
        menuWidth,
        menuHeight,
        left + 36.0F,
        left + 590.0F,
        330.0F,
    };
}

bool MainMenu::contains(double x, double y, float left, float top, float width, float height) {
    return x >= left && x <= left + width && y >= top && y <= top + height;
}

void MainMenu::render(const MainMenuState& state, const Rectangle& rectangle, const Text& text) const {
    const Layout frame = layout(state.width, state.height);
    rectangle(0.0F, 0.0F, static_cast<float>(state.width), static_cast<float>(state.height), backdrop, 0.0F);
    rectangle(0.0F, 0.0F, static_cast<float>(state.width), 96.0F, {0.020F, 0.036F, 0.044F, 1.0F}, 0.0F);
    rectangle(0.0F, static_cast<float>(state.height) - 120.0F, static_cast<float>(state.width), 120.0F, {0.014F, 0.022F, 0.026F, 1.0F}, 0.0F);
    rectangle(0.0F, 96.0F, static_cast<float>(state.width), 2.0F, {0.11F, 0.31F, 0.35F, 1.0F}, 0.0F);
    rectangle(frame.left + 7.0F, frame.top + 8.0F, frame.width, frame.height, {0.0F, 0.0F, 0.0F, 0.44F}, 2.0F);
    rectangle(frame.left, frame.top, frame.width, frame.height, border, 2.0F);
    rectangle(frame.left + 1.0F, frame.top + 1.0F, frame.width - 2.0F, frame.height - 2.0F, panel, 1.0F);
    rectangle(frame.left + 1.0F, frame.top + 1.0F, 5.0F, frame.height - 2.0F, cyan, 1.0F);

    rectangle(frame.storyLeft, frame.top + 32.0F, 500.0F, 290.0F, panelRaised, 1.0F);
    rectangle(frame.storyLeft, frame.top + 32.0F, 5.0F, 290.0F, amber, 1.0F);
    text(frame.storyLeft + 28.0F, frame.top + 55.0F, "ТАИНСТВЕННЫЙ", 2.6F, cyan);
    text(frame.storyLeft + 28.0F, frame.top + 95.0F, "ОСТРОВ", 4.1F, textPrimary);
    rectangle(frame.storyLeft + 28.0F, frame.top + 146.0F, 430.0F, 1.0F, border, 0.0F);
    text(frame.storyLeft + 28.0F, frame.top + 172.0F, "ВЫЖИВАЙ, ДОБЫВАЙ РЕСУРСЫ", 1.7F, textPrimary);
    text(frame.storyLeft + 28.0F, frame.top + 202.0F, "И ИССЛЕДУЙ ОСТРОВ", 1.7F, textPrimary);
    text(frame.storyLeft + 28.0F, frame.top + 248.0F, "WASD ДВИЖЕНИЕ   E ДЕЙСТВИЕ", 1.35F, textMuted);
    text(frame.storyLeft + 28.0F, frame.top + 272.0F, "TAB ИНВЕНТАРЬ   ESC МЕНЮ", 1.35F, textMuted);

    rectangle(frame.storyLeft, frame.top + 344.0F, 500.0F, 198.0F, panelRaised, 1.0F);
    rectangle(frame.storyLeft, frame.top + 344.0F, 5.0F, 198.0F, cyan, 1.0F);
    text(frame.storyLeft + 28.0F, frame.top + 368.0F, "ПЕРВЫЕ ЦЕЛИ", 2.1F, textPrimary);
    text(frame.storyLeft + 28.0F, frame.top + 410.0F, "1 ДОБЫТЬ ДЕРЕВО И КАМЕНЬ", 1.45F, textMuted);
    text(frame.storyLeft + 28.0F, frame.top + 438.0F, "2 РАЗЖЕЧЬ КОСТЕР У ЛАГЕРЯ", 1.45F, textMuted);
    text(frame.storyLeft + 28.0F, frame.top + 466.0F, "3 НАЙТИ ВОДУ И УКРЫТИЕ", 1.45F, textMuted);
    text(frame.storyLeft + 28.0F, frame.top + 506.0F, "ESC ТАКЖЕ ВОЗВРАЩАЕТ В ИГРУ", 1.25F, cyan);

    const auto menuButton = [&](const ButtonSpec& button) {
        const bool hovered = !button.disabled && contains(state.pointerX, state.pointerY, frame.actionLeft, button.y, frame.buttonWidth, 58.0F);
        const glm::vec4 labelColor = button.disabled ? textMuted : (button.destructive ? red : textPrimary);
        rectangle(frame.actionLeft, button.y, frame.buttonWidth, 58.0F, hovered ? panelHover : panelRaised, 1.0F);
        rectangle(frame.actionLeft, button.y, 4.0F, 58.0F, hovered || button.active ? button.accent : border, 1.0F);
        text(frame.actionLeft + 22.0F, button.y + 21.0F, button.label, 2.1F, labelColor);
        if (!button.value.empty()) {
            rectangle(
                frame.actionLeft + frame.buttonWidth - 110.0F,
                button.y + 15.0F,
                88.0F,
                28.0F,
                button.active ? glm::vec4{0.045F, 0.17F, 0.19F, 1.0F} : panel,
                1.0F);
            text(frame.actionLeft + frame.buttonWidth - 96.0F, button.y + 23.0F, button.value, 1.45F, button.disabled ? textMuted : button.accent);
        }
    };

    if (screen_ == Screen::Home) {
        text(frame.actionLeft, frame.top + 39.0F, "МЕНЮ", 2.6F, textPrimary);
        text(frame.actionLeft + 190.0F, frame.top + 46.0F, "НАЖМИ ESC ДЛЯ ИГРЫ", 1.2F, textMuted);
        menuButton({frame.top + 96.0F, "ИГРАТЬ", "ESC", cyan, true, false, false});
        menuButton({frame.top + 174.0F, "ЗАГРУЗИТЬ ИГРУ", "", amber, false, false, false});
        menuButton({frame.top + 252.0F, "НАСТРОЙКИ", "", cyan, false, false, false});
        menuButton({frame.top + 500.0F, "ВЫХОД", "", red, false, false, true});
        return;
    }

    if (screen_ == Screen::LoadGame) {
        text(frame.actionLeft, frame.top + 39.0F, "ЗАГРУЗКА", 2.6F, textPrimary);
        rectangle(frame.actionLeft, frame.top + 104.0F, frame.buttonWidth, 210.0F, panelRaised, 1.0F);
        rectangle(frame.actionLeft, frame.top + 104.0F, 4.0F, 210.0F, amber, 1.0F);
        text(frame.actionLeft + 24.0F, frame.top + 136.0F, "СОХРАНЕНИЙ НЕТ", 2.0F, textPrimary);
        text(frame.actionLeft + 24.0F, frame.top + 182.0F, "СИСТЕМА СОХРАНЕНИЙ", 1.35F, textMuted);
        text(frame.actionLeft + 24.0F, frame.top + 208.0F, "ЕЩЕ НЕ ПОДКЛЮЧЕНА", 1.35F, textMuted);
        text(frame.actionLeft + 24.0F, frame.top + 262.0F, "ИГРА НАЧИНАЕТСЯ С НОВОГО ОСТРОВА", 1.05F, cyan);
        menuButton({frame.top + 344.0F, "НАЗАД", "", cyan, false, false, false});
        menuButton({frame.top + 500.0F, "ВЫХОД", "", red, false, false, true});
        return;
    }

    text(frame.actionLeft, frame.top + 39.0F, "НАСТРОЙКИ", 2.4F, textPrimary);
    text(frame.actionLeft, frame.top + 80.0F, "ГРАФИКА", 1.35F, cyan);

    const auto optionButton = [&](int index, std::string_view label, std::string_view value, bool active) {
        const float y = frame.top + 122.0F + static_cast<float>(index) * 58.0F;
        const bool hovered = contains(state.pointerX, state.pointerY, frame.actionLeft, y, frame.buttonWidth, 46.0F);
        rectangle(frame.actionLeft, y, frame.buttonWidth, 46.0F, hovered ? panelHover : panelRaised, 1.0F);
        rectangle(frame.actionLeft, y, 4.0F, 46.0F, active ? cyan : border, 1.0F);
        text(frame.actionLeft + 24.0F, y + 16.0F, label, 1.55F, textPrimary);
        text(frame.actionLeft + 228.0F, y + 16.0F, value, 1.55F, active ? cyan : textMuted);
    };

    optionButton(0, "ПОЛНЫЙ ЭКРАН", state.fullscreen ? "ВКЛ" : "ВЫКЛ", state.fullscreen);
    optionButton(1, "ВЕРТ СИНХР", state.vsync ? "ВКЛ" : "ВЫКЛ", state.vsync);
    optionButton(2, "ЛИМИТ FPS", state.frameLimit == 0 ? "НЕТ" : std::to_string(state.frameLimit), state.frameLimit != 0);
    optionButton(3, "ТЕНИ", state.shadows ? "ВКЛ" : "ВЫКЛ", state.shadows);
    optionButton(4, "СВЕЧЕНИЕ", state.bloom ? "ВКЛ" : "ВЫКЛ", state.bloom);
    menuButton({frame.top + 500.0F, "НАЗАД", "", cyan, false, false, false});
}

MainMenuAction MainMenu::actionAt(double x, double y, int width, int height) {
    const Layout frame = layout(width, height);

    if (screen_ == Screen::Home) {
        if (contains(x, y, frame.actionLeft, frame.top + 96.0F, frame.buttonWidth, 58.0F)) {
            return MainMenuAction::Play;
        }
        if (contains(x, y, frame.actionLeft, frame.top + 174.0F, frame.buttonWidth, 58.0F)) {
            screen_ = Screen::LoadGame;
            return MainMenuAction::None;
        }
        if (contains(x, y, frame.actionLeft, frame.top + 252.0F, frame.buttonWidth, 58.0F)) {
            screen_ = Screen::Settings;
            return MainMenuAction::None;
        }
        if (contains(x, y, frame.actionLeft, frame.top + 500.0F, frame.buttonWidth, 58.0F)) {
            return MainMenuAction::Quit;
        }
        return MainMenuAction::None;
    }

    if (screen_ == Screen::LoadGame) {
        if (contains(x, y, frame.actionLeft, frame.top + 344.0F, frame.buttonWidth, 58.0F)) {
            screen_ = Screen::Home;
        } else if (contains(x, y, frame.actionLeft, frame.top + 500.0F, frame.buttonWidth, 58.0F)) {
            return MainMenuAction::Quit;
        }
        return MainMenuAction::None;
    }

    for (int index = 0; index < 5; ++index) {
        const float top = frame.top + 122.0F + static_cast<float>(index) * 58.0F;
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
