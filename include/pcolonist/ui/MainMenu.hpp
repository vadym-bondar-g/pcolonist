#pragma once

#include <glm/vec4.hpp>

#include <functional>
#include <string_view>

namespace pcolonist {

enum class MainMenuAction {
    None,
    Play,
    LoadGame,
    Settings,
    ToggleFullscreen,
    ToggleVsync,
    CycleFrameLimit,
    ToggleShadows,
    ToggleBloom,
    Quit,
};

struct MainMenuState {
    int width = 1280;
    int height = 720;
    double pointerX = 0.0;
    double pointerY = 0.0;
    bool fullscreen = false;
    bool vsync = true;
    int frameLimit = 0;
    bool shadows = true;
    bool bloom = true;
};

class MainMenu {
public:
    using Rectangle = std::function<void(float, float, float, float, const glm::vec4&, float)>;
    using Text = std::function<void(float, float, std::string_view, float, const glm::vec4&)>;

    void render(const MainMenuState& state, const Rectangle& rectangle, const Text& text) const;
    [[nodiscard]] MainMenuAction actionAt(double x, double y, int width, int height) const;

private:
    struct Layout {
        float left = 0.0F;
        float top = 0.0F;
        float width = 960.0F;
        float height = 590.0F;
        float storyLeft = 0.0F;
        float actionLeft = 0.0F;
        float buttonWidth = 330.0F;
    };

    [[nodiscard]] static Layout layout(int width, int height);
    [[nodiscard]] static bool contains(double x, double y, float left, float top, float width, float height);
};

} // namespace pcolonist
