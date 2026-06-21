#pragma once

#include "pcolonist/ui/Localization.hpp"

#include <filesystem>

namespace pcolonist {

struct ApplicationConfig {
    std::filesystem::path assetRoot{"assets"};
    std::filesystem::path savePath{"pcolonist.save"};
    int windowWidth = 1280;
    int windowHeight = 720;
    bool vsync = true;
    bool loadGame = false;
    UiLanguage language = UiLanguage::English;
};

} // namespace pcolonist
