#pragma once

#include "pcolonist/ui/Localization.hpp"

#include <glm/vec4.hpp>

#include <array>
#include <functional>
#include <string_view>

namespace pcolonist {

enum class MainMenuAction {
    None,
    Continue,
    ToggleFullscreen,
    ToggleVsync,
    CycleFrameLimit,
    ToggleShadows,
    ToggleBloom,
    CycleLanguage,
    NewGame,
    SaveGame,
    LoadGame,
    OpenLoadGame,
    OpenSettings,
    OpenMods,
    Back,
    Quit,
};

struct MainMenuState {
    int width = 1280;
    int height = 720;
    float time = 0.0F;
    double pointerX = 0.0;
    double pointerY = 0.0;
    bool fullscreen = false;
    bool vsync = true;
    int frameLimit = 0;
    bool shadows = true;
    bool bloom = true;
    bool saveAvailable = false;
    bool gameStarted = false;
    UiLanguage language = UiLanguage::English;
};

class MainMenu {
public:
    using Rectangle = std::function<void(float, float, float, float, const glm::vec4&, float)>;
    using Text = std::function<void(float, float, std::string_view, float, const glm::vec4&)>;

    void render(const MainMenuState& state, const Rectangle& rectangle, const Text& text) const;
    [[nodiscard]] MainMenuAction actionAt(double x, double y, int width, int height);

private:
    enum class Screen {
        Home,
        LoadGame,
        Settings,
        Mods,
    };

    struct Layout {
        float left = 0.0F;
        float top = 0.0F;
        float width = 960.0F;
        float height = 590.0F;
        float storyLeft = 0.0F;
        float actionLeft = 0.0F;
        float buttonWidth = 330.0F;
    };

    struct ButtonSpec {
        MainMenuAction action = MainMenuAction::None;
        float y = 0.0F;
        std::string_view label;
        std::string_view hint;
        glm::vec4 accent{1.0F};
        bool disabled = false;
        bool destructive = false;
    };

    [[nodiscard]] static Layout layout(int width, int height);
    [[nodiscard]] static bool contains(double x, double y, float left, float top, float width, float height);
    [[nodiscard]] std::array<ButtonSpec, 6> homeButtons(const MainMenuState& state, const Layout& frame) const;
    void renderBackground(const MainMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& frame, float intro) const;
    void renderBottomBar(const MainMenuState& state, const Rectangle& rectangle, const Text& text, float intro) const;
    void renderShell(const MainMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& frame, float intro) const;
    void renderButton(const MainMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& frame, const ButtonSpec& button, float intro) const;
    void renderHome(const MainMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& frame, float intro) const;
    void renderLoadGame(const MainMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& frame, float intro) const;
    void renderSettings(const MainMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& frame, float intro) const;
    void renderMods(const MainMenuState& state, const Rectangle& rectangle, const Text& text, const Layout& frame, float intro) const;

    Screen screen_ = Screen::Home;
    mutable MainMenuState lastState_;
};

} // namespace pcolonist
