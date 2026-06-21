#include "pcolonist/ui/PauseMenu.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace {

constexpr glm::vec4 overlay{0.004F, 0.006F, 0.010F, 0.58F};
constexpr glm::vec4 glass{0.026F, 0.034F, 0.046F, 0.72F};
constexpr glm::vec4 glassStrong{0.036F, 0.048F, 0.064F, 0.86F};
constexpr glm::vec4 stroke{0.42F, 0.74F, 0.82F, 0.28F};
constexpr glm::vec4 textPrimary{0.94F, 0.98F, 0.96F, 1.0F};
constexpr glm::vec4 textMuted{0.58F, 0.66F, 0.68F, 0.88F};
constexpr glm::vec4 accent{0.30F, 0.88F, 0.96F, 1.0F};
constexpr glm::vec4 accentWarm{0.95F, 0.68F, 0.28F, 1.0F};
constexpr glm::vec4 danger{0.96F, 0.30F, 0.26F, 1.0F};

constexpr float itemHeight = 52.0F;
constexpr float itemGap = 8.0F;

glm::vec4 alpha(glm::vec4 color, float fade) {
    color.a *= fade;
    return color;
}

std::string timeLabel(float seconds) {
    const int totalMinutes = static_cast<int>(seconds / 60.0F);
    const int hours = (totalMinutes / 60) % 24;
    const int minutes = totalMinutes % 60;
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << hours << ':' << std::setw(2) << minutes;
    return output.str();
}

} // namespace

namespace pcolonist {

PauseMenu::Layout PauseMenu::layout(int width, int height) {
    const float screenWidth = static_cast<float>(width);
    const float screenHeight = static_cast<float>(height);
    const float panelWidth = std::clamp(screenWidth - 96.0F, 900.0F, 1220.0F);
    const float panelHeight = std::clamp(screenHeight - 88.0F, 560.0F, 720.0F);
    const float left = screenWidth * 0.5F - panelWidth * 0.5F;
    const float top = screenHeight * 0.5F - panelHeight * 0.5F;
    const float navWidth = std::clamp(panelWidth * 0.32F, 300.0F, 370.0F);
    const float detailWidth = std::clamp(panelWidth * 0.28F, 280.0F, 350.0F);
    return {
        left,
        top,
        panelWidth,
        panelHeight,
        left + 42.0F,
        top + 132.0F,
        navWidth,
        left + panelWidth - detailWidth - 42.0F,
        detailWidth,
    };
}

bool PauseMenu::contains(double x, double y, float left, float top, float width, float height) {
    return x >= left && x <= left + width && y >= top && y <= top + height;
}

std::array<PauseMenu::Item, 8> PauseMenu::mainItems(const PauseMenuState& state) const {
    return {
        Item{PauseMenuAction::ResumeGame, tr(state.language, UiText::ResumeGame), "ESC"},
        Item{PauseMenuAction::SaveGame, tr(state.language, UiText::SaveGame), tr(state.language, UiText::Quick), false},
        Item{PauseMenuAction::LoadGame, tr(state.language, UiText::LoadGame), state.saveAvailable ? tr(state.language, UiText::Ready) : tr(state.language, UiText::Empty), !state.saveAvailable},
        Item{PauseMenuAction::OpenSettings, tr(state.language, UiText::Settings), ""},
        Item{PauseMenuAction::OpenControls, tr(state.language, UiText::Controls), ""},
        Item{PauseMenuAction::OpenAchievements, tr(state.language, UiText::Achievements), ""},
        Item{PauseMenuAction::MainMenu, tr(state.language, UiText::MainMenu), ""},
        Item{PauseMenuAction::ExitGame, tr(state.language, UiText::ExitGame), "", false, true},
    };
}

void PauseMenu::moveSelection(int delta) {
    constexpr int count = 8;
    selected_ = (selected_ + delta + count) % count;
}

void PauseMenu::openTab(Tab tab) {
    tab_ = tab;
}

void PauseMenu::renderBackdrop(const PauseMenuState& state, const Rectangle& rectangle) const {
    const float width = static_cast<float>(state.width);
    const float height = static_cast<float>(state.height);
    const float fade = state.fade;
    rectangle(0.0F, 0.0F, width, height, alpha(overlay, fade), 0.0F);
    rectangle(0.0F, 0.0F, width, height * 0.16F, {0.0F, 0.0F, 0.0F, 0.26F * fade}, 0.0F);
    rectangle(0.0F, height * 0.78F, width, height * 0.22F, {0.0F, 0.0F, 0.0F, 0.34F * fade}, 0.0F);
    rectangle(0.0F, 0.0F, width * 0.20F, height, {0.0F, 0.0F, 0.0F, 0.20F * fade}, 0.0F);
    rectangle(width * 0.80F, 0.0F, width * 0.20F, height, {0.0F, 0.0F, 0.0F, 0.20F * fade}, 0.0F);

    for (int index = 0; index < 12; ++index) {
        const float y = height * 0.18F + static_cast<float>(index) * 34.0F;
        const float x = std::sin(state.time * 0.42F + static_cast<float>(index)) * 34.0F;
        rectangle(x - 60.0F, y, width + 120.0F, 1.0F, {0.45F, 0.90F, 0.95F, 0.020F * fade}, 0.0F);
    }
}

void PauseMenu::renderPanel(const PauseMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& frame) const {
    const float fade = state.fade;
    const float slide = (1.0F - fade) * 24.0F;
    rectangle(frame.left + 10.0F, frame.top + 14.0F + slide, frame.width, frame.height, {0.0F, 0.0F, 0.0F, 0.36F * fade}, 12.0F);
    rectangle(frame.left, frame.top + slide, frame.width, frame.height, alpha(stroke, fade), 12.0F);
    rectangle(frame.left + 1.0F, frame.top + 1.0F + slide, frame.width - 2.0F, frame.height - 2.0F, alpha(glass, fade), 11.0F);
    rectangle(frame.left + 24.0F, frame.top + 26.0F + slide, frame.width - 48.0F, 1.0F, {0.80F, 0.96F, 1.0F, 0.18F * fade}, 0.0F);
    rectangle(frame.left + 24.0F, frame.top + frame.height - 74.0F + slide, frame.width - 48.0F, 1.0F, {0.80F, 0.96F, 1.0F, 0.12F * fade}, 0.0F);
    text(frame.left + 42.0F, frame.top + 52.0F + slide, tr(state.language, UiText::Paused), 4.0F, alpha(textPrimary, fade));
    text(frame.left + 46.0F, frame.top + 100.0F + slide, tr(state.language, UiText::WorldStateHeld), 1.18F, alpha(textMuted, fade));
}

void PauseMenu::renderItem(
    const PauseMenuState& state,
    const Rectangle& rectangle,
    const Text& text,
    const Layout& frame,
    const Item& item,
    int index) const {
    const float fade = state.fade;
    const float y = frame.navTop + static_cast<float>(index) * (itemHeight + itemGap);
    const bool hovered = !item.disabled && contains(state.pointerX, state.pointerY, frame.navLeft, y, frame.navWidth, itemHeight);
    const bool selected = selected_ == index;
    const float active = hovered || selected ? 1.0F : 0.0F;
    const glm::vec4 itemAccent = item.destructive ? danger : index == 1 ? accentWarm : accent;
    const glm::vec4 label = item.disabled ? glm::vec4{0.38F, 0.44F, 0.45F, 0.62F * fade} : alpha(textPrimary, fade);
    const float x = frame.navLeft + active * 8.0F;

    rectangle(x + 8.0F, y + 8.0F, frame.navWidth, itemHeight, {0.0F, 0.0F, 0.0F, 0.24F * fade}, 8.0F);
    rectangle(x, y, frame.navWidth, itemHeight, {0.36F, 0.76F, 0.86F, (0.12F + active * 0.30F) * fade}, 8.0F);
    rectangle(x + 1.0F, y + 1.0F, frame.navWidth - 2.0F, itemHeight - 2.0F, alpha(active > 0.0F ? glassStrong : glass, fade), 7.0F);
    rectangle(x + 1.0F, y + 1.0F, active > 0.0F ? frame.navWidth - 2.0F : 6.0F, 2.0F, alpha(itemAccent, fade), 1.0F);
    rectangle(x + 18.0F, y + 15.0F, 3.0F, 22.0F, alpha(itemAccent, item.disabled ? fade * 0.38F : fade), 1.0F);
    text(x + 40.0F, y + 17.0F, item.label, 1.72F + active * 0.08F, label);
    if (!item.hint.empty()) {
        text(x + frame.navWidth - 86.0F, y + 19.0F, item.hint, 1.08F, item.disabled ? alpha(textMuted, fade * 0.48F) : alpha(itemAccent, fade * 0.88F));
    }
}

void PauseMenu::renderDetails(const PauseMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& frame) const {
    const float fade = state.fade;
    const float top = frame.top + 132.0F;
    rectangle(frame.detailsLeft, top, frame.detailsWidth, 324.0F, alpha(stroke, fade), 9.0F);
    rectangle(frame.detailsLeft + 1.0F, top + 1.0F, frame.detailsWidth - 2.0F, 322.0F, alpha(glassStrong, fade), 8.0F);
    text(frame.detailsLeft + 24.0F, top + 24.0F, tr(state.language, UiText::Character), 1.28F, alpha(accent, fade));
    text(frame.detailsLeft + 24.0F, top + 58.0F, state.profile, 1.58F, alpha(textPrimary, fade));

    const auto row = [&](int index, std::string_view label, std::string_view value, glm::vec4 color = textPrimary) {
        const float y = top + 106.0F + static_cast<float>(index) * 36.0F;
        text(frame.detailsLeft + 24.0F, y, label, 1.12F, alpha(textMuted, fade));
        text(frame.detailsLeft + frame.detailsWidth - 118.0F, y, value, 1.22F, alpha(color, fade));
    };
    const std::string level = std::to_string(state.level);
    const std::string health = std::to_string(static_cast<int>(state.health)) + "%";
    const std::string money = std::to_string(state.money) + " CR";
    const std::string time = timeLabel(state.gameTime);
    row(0, tr(state.language, UiText::Level), level, accentWarm);
    row(1, tr(state.language, UiText::Health), health, state.health < 35.0F ? danger : accent);
    row(2, tr(state.language, UiText::Money), money, accentWarm);
    row(3, tr(state.language, UiText::Time), time);
    row(4, tr(state.language, UiText::Location), state.location.empty() ? tr(state.language, UiText::Unknown) : state.location);

    rectangle(frame.detailsLeft, top + 352.0F, frame.detailsWidth, 118.0F, alpha(stroke, fade * 0.86F), 9.0F);
    rectangle(frame.detailsLeft + 1.0F, top + 353.0F, frame.detailsWidth - 2.0F, 116.0F, alpha(glass, fade), 8.0F);
    text(frame.detailsLeft + 24.0F, top + 378.0F, tr(state.language, UiText::FutureTabs), 1.15F, alpha(accent, fade));
    text(frame.detailsLeft + 24.0F, top + 410.0F, tr(state.language, UiText::FutureTabsLineOne), 1.00F, alpha(textMuted, fade));
    text(frame.detailsLeft + 24.0F, top + 438.0F, tr(state.language, UiText::FutureTabsLineTwo), 1.00F, alpha(textMuted, fade));
}

void PauseMenu::renderTab(const PauseMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& frame) const {
    if (tab_ == Tab::Main) {
        return;
    }

    const float fade = state.fade;
    const float left = frame.navLeft + frame.navWidth + 34.0F;
    const float top = frame.navTop;
    const float width = frame.detailsLeft - left - 34.0F;
    rectangle(left, top, width, 330.0F, alpha(stroke, fade), 9.0F);
    rectangle(left + 1.0F, top + 1.0F, width - 2.0F, 328.0F, alpha(glassStrong, fade), 8.0F);

    if (tab_ == Tab::Settings) {
        text(left + 24.0F, top + 28.0F, tr(state.language, UiText::Settings), 2.0F, alpha(textPrimary, fade));
        text(left + 24.0F, top + 76.0F, tr(state.language, UiText::DisplayOptionsMainMenu), 1.05F, alpha(textMuted, fade));
        text(left + 24.0F, top + 114.0F, tr(state.language, UiText::FutureSettings), 1.05F, alpha(textMuted, fade));
    } else if (tab_ == Tab::Controls) {
        text(left + 24.0F, top + 28.0F, tr(state.language, UiText::Controls), 2.0F, alpha(textPrimary, fade));
        text(left + 24.0F, top + 78.0F, tr(state.language, UiText::PauseBack), 1.18F, alpha(textMuted, fade));
        text(left + 24.0F, top + 112.0F, tr(state.language, UiText::MoveLook), 1.18F, alpha(textMuted, fade));
        text(left + 24.0F, top + 146.0F, tr(state.language, UiText::ActivateMenuItem), 1.18F, alpha(textMuted, fade));
    } else {
        text(left + 24.0F, top + 28.0F, tr(state.language, UiText::Achievements), 2.0F, alpha(textPrimary, fade));
        text(left + 24.0F, top + 76.0F, tr(state.language, UiText::AchievementPlaceholder), 1.05F, alpha(textMuted, fade));
        text(left + 24.0F, top + 114.0F, tr(state.language, UiText::DiscoveriesFeedPanel), 1.05F, alpha(textMuted, fade));
    }
}

void PauseMenu::renderFooter(const PauseMenuState& state, const Rectangle& rectangle, const Text& text) const {
    const float fade = state.fade;
    const float width = static_cast<float>(state.width);
    const float height = static_cast<float>(state.height);
    rectangle(0.0F, height - 48.0F, width, 48.0F, {0.006F, 0.010F, 0.014F, 0.70F * fade}, 0.0F);
    rectangle(0.0F, height - 48.0F, width, 1.0F, {0.65F, 0.92F, 0.96F, 0.18F * fade}, 0.0F);
    text(24.0F, height - 30.0F, state.version.empty() ? "v0.1-dev" : state.version, 1.06F, alpha(textMuted, fade));
    const std::string fps = "FPS " + std::to_string(state.fps);
    text(width * 0.5F - 34.0F, height - 30.0F, fps, 1.06F, alpha(textMuted, fade));
    text(width - 292.0F, height - 30.0F, state.saveInfo.empty() ? tr(state.language, UiText::SaveNotWritten) : state.saveInfo, 1.06F, alpha(textMuted, fade));
}

void PauseMenu::render(const PauseMenuState& state, const Rectangle& rectangle, const Text& text) {
    lastState_ = state;
    const Layout frame = layout(state.width, state.height);
    renderBackdrop(state, rectangle);
    renderPanel(state, rectangle, text, frame);

    const auto items = mainItems(state);
    for (int index = 0; index < static_cast<int>(items.size()); ++index) {
        renderItem(state, rectangle, text, frame, items[static_cast<std::size_t>(index)], index);
    }
    renderTab(state, rectangle, text, frame);
    renderDetails(state, rectangle, text, frame);
    renderFooter(state, rectangle, text);
}

PauseMenuAction PauseMenu::actionAt(double x, double y, int width, int height) {
    const Layout frame = layout(width, height);
    const auto items = mainItems(lastState_);
    for (int index = 0; index < static_cast<int>(items.size()); ++index) {
        const float itemTop = frame.navTop + static_cast<float>(index) * (itemHeight + itemGap);
        const Item& item = items[static_cast<std::size_t>(index)];
        if (contains(x, y, frame.navLeft, itemTop, frame.navWidth, itemHeight)) {
            if (item.disabled) {
                return PauseMenuAction::None;
            }
            selected_ = index;
            if (item.action == PauseMenuAction::OpenSettings) {
                openTab(Tab::Settings);
                return PauseMenuAction::None;
            }
            if (item.action == PauseMenuAction::OpenControls) {
                openTab(Tab::Controls);
                return PauseMenuAction::None;
            }
            if (item.action == PauseMenuAction::OpenAchievements) {
                openTab(Tab::Achievements);
                return PauseMenuAction::None;
            }
            return item.action;
        }
    }
    return PauseMenuAction::None;
}

PauseMenuAction PauseMenu::command(PauseMenuCommand command) {
    if (command == PauseMenuCommand::Up) {
        moveSelection(-1);
        return PauseMenuAction::None;
    }
    if (command == PauseMenuCommand::Down) {
        moveSelection(1);
        return PauseMenuAction::None;
    }
    if (command == PauseMenuCommand::Back) {
        if (tab_ != Tab::Main) {
            tab_ = Tab::Main;
            return PauseMenuAction::None;
        }
        return PauseMenuAction::ResumeGame;
    }
    const auto items = mainItems(lastState_);
    const Item& item = items[static_cast<std::size_t>(std::clamp(selected_, 0, static_cast<int>(items.size()) - 1))];
    if (item.disabled) {
        return PauseMenuAction::None;
    }
    if (item.action == PauseMenuAction::OpenSettings) {
        openTab(Tab::Settings);
        return PauseMenuAction::None;
    }
    if (item.action == PauseMenuAction::OpenControls) {
        openTab(Tab::Controls);
        return PauseMenuAction::None;
    }
    if (item.action == PauseMenuAction::OpenAchievements) {
        openTab(Tab::Achievements);
        return PauseMenuAction::None;
    }
    return item.action;
}

} // namespace pcolonist
