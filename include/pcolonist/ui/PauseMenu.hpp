#pragma once

#include "pcolonist/ui/Localization.hpp"

#include <glm/vec4.hpp>

#include <array>
#include <functional>
#include <string_view>

namespace pcolonist {

enum class PauseMenuAction {
    None,
    ResumeGame,
    SaveGame,
    LoadGame,
    OpenSettings,
    OpenControls,
    OpenAchievements,
    MainMenu,
    ExitGame,
    Back,
};

enum class PauseMenuCommand {
    Up,
    Down,
    Activate,
    Back,
};

struct PauseMenuState {
    int width = 1280;
    int height = 720;
    float time = 0.0F;
    float fade = 1.0F;
    double pointerX = 0.0;
    double pointerY = 0.0;
    bool saveAvailable = false;
    unsigned int fps = 0;
    std::string_view version;
    std::string_view saveInfo;
    std::string_view profile;
    std::string_view location;
    float health = 100.0F;
    int level = 1;
    int money = 0;
    float gameTime = 0.0F;
    UiLanguage language = UiLanguage::English;
};

class PauseMenu {
public:
    using Rectangle = std::function<void(float, float, float, float, const glm::vec4&, float)>;
    using Text = std::function<void(float, float, std::string_view, float, const glm::vec4&)>;

    void render(const PauseMenuState& state, const Rectangle& rectangle, const Text& text);
    [[nodiscard]] PauseMenuAction actionAt(double x, double y, int width, int height);
    [[nodiscard]] PauseMenuAction command(PauseMenuCommand command);

private:
    enum class Tab {
        Main,
        Settings,
        Controls,
        Achievements,
    };

    struct Layout {
        float left = 0.0F;
        float top = 0.0F;
        float width = 960.0F;
        float height = 590.0F;
        float navLeft = 0.0F;
        float navTop = 0.0F;
        float navWidth = 320.0F;
        float detailsLeft = 0.0F;
        float detailsWidth = 320.0F;
    };

    struct Item {
        PauseMenuAction action = PauseMenuAction::None;
        std::string_view label;
        std::string_view hint;
        bool disabled = false;
        bool destructive = false;
    };

    [[nodiscard]] static Layout layout(int width, int height);
    [[nodiscard]] static bool contains(double x, double y, float left, float top, float width, float height);
    [[nodiscard]] std::array<Item, 8> mainItems(const PauseMenuState& state) const;
    void moveSelection(int delta);
    void openTab(Tab tab);
    void renderBackdrop(const PauseMenuState& state, const Rectangle& rectangle) const;
    void renderPanel(const PauseMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& layout) const;
    void renderItem(const PauseMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& layout, const Item& item, int index) const;
    void renderDetails(const PauseMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& layout) const;
    void renderTab(const PauseMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& layout) const;
    void renderFooter(const PauseMenuState& state, const Rectangle& rectangle, const Text& text) const;

    PauseMenuState lastState_;
    Tab tab_ = Tab::Main;
    int selected_ = 0;
};

} // namespace pcolonist
