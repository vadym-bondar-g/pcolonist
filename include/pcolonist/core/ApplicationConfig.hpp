#pragma once

#include "pcolonist/render/Skybox.hpp"
#include "pcolonist/ui/Localization.hpp"

#include <filesystem>

namespace pcolonist {

enum class GraphicsQuality {
    Low,
    Medium,
    High,
    Cinematic,
};

struct ApplicationConfig {
    std::filesystem::path assetRoot{"assets"};
    std::filesystem::path savePath{"pcolonist.save"};
    int windowWidth = 1280;
    int windowHeight = 720;
    int frameLimit = 120;
    bool vsync = true;
    bool loadGame = false;
    bool shadows = true;
    bool bloom = true;
    GraphicsQuality graphicsQuality = GraphicsQuality::High;
    SkyQuality skyQuality = SkyQuality::Medium;
    UiLanguage language = UiLanguage::English;
};

} // namespace pcolonist
