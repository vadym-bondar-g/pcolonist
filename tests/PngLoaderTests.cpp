#include "pcolonist/render/PngLoader.hpp"

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

void runPngLoaderTests() {
    const std::filesystem::path root = std::filesystem::temp_directory_path()
        / ("pcolonist-png-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);

    try {
        const std::filesystem::path texture = root / std::filesystem::path(
            std::u8string{u8"\u0442\u0435\u043a\u0441\u0442\u0443\u0440\u0430.png"});
        std::filesystem::copy_file(
            std::filesystem::path(PCOLONIST_TEST_ASSET_DIR) / "textures/terrain/earth.png",
            texture);

        const pcolonist::Image image = pcolonist::PngLoader::load(texture);
        if (image.width <= 0 || image.height <= 0
            || image.pixels.size() != static_cast<std::size_t>(image.width * image.height * 4)) {
            throw std::runtime_error("PNG loader must decode RGBA pixels from a Unicode filesystem path");
        }
    } catch (...) {
        std::filesystem::remove_all(root);
        throw;
    }

    std::filesystem::remove_all(root);
}
