#include "pcolonist/core/RuntimeOptions.hpp"

#include "pcolonist/assets/AssetSystem.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view version = "pcolonist dev";

int parsePositiveInt(std::string_view value, std::string_view name) {
    std::size_t parsed = 0;
    int result = 0;
    try {
        result = std::stoi(std::string(value), &parsed);
    } catch (const std::exception&) {
        throw pcolonist::ArgumentError("Invalid " + std::string(name) + ": " + std::string(value));
    }
    if (parsed != value.size() || result <= 0) {
        throw pcolonist::ArgumentError("Invalid " + std::string(name) + ": " + std::string(value));
    }
    return result;
}

int parseNonNegativeInt(std::string_view value, std::string_view name) {
    std::size_t parsed = 0;
    int result = 0;
    try {
        result = std::stoi(std::string(value), &parsed);
    } catch (const std::exception&) {
        throw pcolonist::ArgumentError("Invalid " + std::string(name) + ": " + std::string(value));
    }
    if (parsed != value.size() || result < 0) {
        throw pcolonist::ArgumentError("Invalid " + std::string(name) + ": " + std::string(value));
    }
    return result;
}

std::filesystem::path defaultAssetRoot() {
    if (const char* environment = std::getenv("PCOLONIST_ASSET_DIR")) {
        if (*environment != '\0') {
            return environment;
        }
    }
    return PCOLONIST_ASSET_DIR;
}

pcolonist::UiLanguage parseLanguage(std::string_view value) {
    if (value == "en" || value == "EN" || value == "english") {
        return pcolonist::UiLanguage::English;
    }
    if (value == "ru" || value == "RU" || value == "russian") {
        return pcolonist::UiLanguage::Russian;
    }
    if (value == "pl" || value == "PL" || value == "polish") {
        return pcolonist::UiLanguage::Polish;
    }
    throw pcolonist::ArgumentError("Language must be one of: en, ru, pl");
}

pcolonist::SkyQuality parseSkyQuality(std::string_view value) {
    if (value == "off" || value == "OFF") {
        return pcolonist::SkyQuality::Off;
    }
    if (value == "low" || value == "LOW") {
        return pcolonist::SkyQuality::Low;
    }
    if (value == "medium" || value == "MEDIUM" || value == "med" || value == "MED") {
        return pcolonist::SkyQuality::Medium;
    }
    if (value == "high" || value == "HIGH") {
        return pcolonist::SkyQuality::High;
    }
    throw pcolonist::ArgumentError("Sky quality must be one of: off, low, medium, high");
}

void applyGraphicsQuality(pcolonist::ApplicationConfig& config, std::string_view value) {
    if (value == "low" || value == "LOW") {
        config.graphicsQuality = pcolonist::GraphicsQuality::Low;
        config.shadows = false;
        config.bloom = false;
        config.skyQuality = pcolonist::SkyQuality::Low;
        config.frameLimit = 60;
        return;
    }
    if (value == "medium" || value == "MEDIUM" || value == "med" || value == "MED") {
        config.graphicsQuality = pcolonist::GraphicsQuality::Medium;
        config.shadows = true;
        config.bloom = false;
        config.skyQuality = pcolonist::SkyQuality::Medium;
        config.frameLimit = 120;
        return;
    }
    if (value == "high" || value == "HIGH") {
        config.graphicsQuality = pcolonist::GraphicsQuality::High;
        config.shadows = true;
        config.bloom = true;
        config.skyQuality = pcolonist::SkyQuality::Medium;
        config.frameLimit = 120;
        return;
    }
    if (value == "cinematic" || value == "CINEMATIC" || value == "ultra" || value == "ULTRA") {
        config.graphicsQuality = pcolonist::GraphicsQuality::Cinematic;
        config.shadows = true;
        config.bloom = true;
        config.skyQuality = pcolonist::SkyQuality::High;
        config.frameLimit = 0;
        return;
    }
    throw pcolonist::ArgumentError("Graphics quality must be one of: low, medium, high, cinematic");
}

bool checkAsset(pcolonist::AssetSystem& assets, const std::filesystem::path& path, std::ostream& output) {
    try {
        const std::filesystem::path resolved = assets.resolve(path);
        if (!std::filesystem::exists(resolved)) {
            output << "fail " << path.generic_string() << ": file does not exist\n";
            return false;
        }
        output << "ok   " << path.generic_string() << '\n';
        return true;
    } catch (const std::exception& error) {
        output << "fail " << path.generic_string() << ": " << error.what() << '\n';
        return false;
    }
}

} // namespace

namespace pcolonist {

ArgumentError::ArgumentError(const std::string& message)
    : std::runtime_error(message) {}

RuntimeOptions parseRuntimeOptions(const std::vector<std::string>& arguments) {
    RuntimeOptions options;
    options.application.assetRoot = defaultAssetRoot();

    for (std::size_t index = 1; index < arguments.size(); ++index) {
        const std::string& argument = arguments[index];
        const auto requireValue = [&](std::string_view name) -> const std::string& {
            if (index + 1 >= arguments.size()) {
                throw ArgumentError("Missing value for " + std::string(name));
            }
            return arguments[++index];
        };

        if (argument == "--help" || argument == "-h") {
            options.mode = LaunchMode::Help;
        } else if (argument == "--version") {
            options.mode = LaunchMode::Version;
        } else if (argument == "--validate-assets") {
            options.mode = LaunchMode::ValidateAssets;
        } else if (argument == "--dump-config") {
            options.mode = LaunchMode::DumpConfig;
        } else if (argument == "--probe-vulkan") {
            options.mode = LaunchMode::ProbeVulkan;
        } else if (argument == "--assets") {
            options.application.assetRoot = requireValue("--assets");
        } else if (argument == "--save") {
            options.application.savePath = requireValue("--save");
        } else if (argument == "--load") {
            options.application.loadGame = true;
            if (index + 1 < arguments.size() && !arguments[index + 1].starts_with('-')) {
                options.application.savePath = arguments[++index];
            }
        } else if (argument == "--width") {
            options.application.windowWidth = parsePositiveInt(requireValue("--width"), "window width");
        } else if (argument == "--height") {
            options.application.windowHeight = parsePositiveInt(requireValue("--height"), "window height");
        } else if (argument == "--fps-limit") {
            const int limit = parseNonNegativeInt(requireValue("--fps-limit"), "FPS limit");
            options.application.frameLimit = limit;
        } else if (argument == "--window-size") {
            const std::string& value = requireValue("--window-size");
            const std::size_t separator = value.find('x');
            if (separator == std::string::npos) {
                throw ArgumentError("Window size must use WIDTHxHEIGHT");
            }
            options.application.windowWidth = parsePositiveInt(std::string_view(value).substr(0, separator), "window width");
            options.application.windowHeight = parsePositiveInt(std::string_view(value).substr(separator + 1), "window height");
        } else if (argument == "--no-vsync") {
            options.application.vsync = false;
        } else if (argument == "--vsync") {
            options.application.vsync = true;
        } else if (argument == "--language" || argument == "--lang") {
            options.application.language = parseLanguage(requireValue("--language"));
        } else if (argument == "--graphics") {
            applyGraphicsQuality(options.application, requireValue("--graphics"));
        } else if (argument == "--shadows") {
            options.application.shadows = true;
        } else if (argument == "--no-shadows") {
            options.application.shadows = false;
        } else if (argument == "--bloom") {
            options.application.bloom = true;
        } else if (argument == "--no-bloom") {
            options.application.bloom = false;
        } else if (argument == "--sky-quality") {
            options.application.skyQuality = parseSkyQuality(requireValue("--sky-quality"));
        } else {
            throw ArgumentError("Unknown argument: " + argument);
        }
    }

    return options;
}

void printUsage(std::ostream& output, const char* executable) {
    output
        << "Usage: " << executable << " [options]\n\n"
        << "Options:\n"
        << "  -h, --help              Show this help text\n"
        << "      --version           Show version information\n"
        << "      --assets PATH       Use PATH as runtime asset root\n"
        << "      --save PATH         Use PATH as the save file\n"
        << "      --load [PATH]       Load a save file at startup\n"
        << "      --validate-assets   Check required assets without creating a window\n"
        << "      --dump-config       Print resolved startup configuration without creating a window\n"
        << "      --probe-vulkan      Print Vulkan loader and device capabilities\n"
        << "      --window-size WxH   Set initial window size, for example 1600x900\n"
        << "      --width N           Set initial window width\n"
        << "      --height N          Set initial window height\n"
        << "      --fps-limit N       Limit frames per second\n"
        << "      --vsync             Enable vertical sync\n"
        << "      --no-vsync          Disable vertical sync\n"
        << "      --graphics QUALITY  Set graphics preset: low, medium, high, or cinematic\n"
        << "      --shadows           Enable shadow rendering\n"
        << "      --no-shadows        Disable shadow rendering\n"
        << "      --bloom             Enable bloom post-processing\n"
        << "      --no-bloom          Disable bloom post-processing\n"
        << "      --sky-quality Q     Set sky quality: off, low, medium, or high\n"
        << "      --language CODE     Set UI language: en, ru, or pl\n";
}

void printVersion(std::ostream& output) {
    output << version << '\n';
}

bool validateRuntimeAssets(const std::filesystem::path& assetRoot, std::ostream& output) {
    output << "Validating assets in " << assetRoot << '\n';
    AssetSystem assets(assetRoot);
    bool valid = true;

    const std::vector<std::filesystem::path> requiredAssets = {
        "maps/demo_map_structures.obj",
        "maps/demo_map_rocks.obj",
        "maps/internal_water.obj",
        "maps/landmarks.json",
        "maps/world_config.json",
        "scripts/startup.script",
        "scripts/models.scene",
        "scripts/chunks/scene_0_0.scene",
        "scripts/island_locations_100.json",
        "shaders/basic.vert",
        "shaders/basic.frag",
        "shaders/shadow.vert",
        "shaders/shadow.frag",
        "shaders/sky.vert",
        "shaders/sky.frag",
        "shaders/post.vert",
        "shaders/post.frag",
        "shaders/ui.vert",
        "shaders/ui.frag",
    };

    for (const auto& path : requiredAssets) {
        valid = checkAsset(assets, path, output) && valid;
    }

    return valid;
}

} // namespace pcolonist
