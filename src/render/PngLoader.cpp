#include "pcolonist/render/PngLoader.hpp"

#include <png.h>

#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace pcolonist {

Image PngLoader::load(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw std::runtime_error("Cannot read PNG texture: " + path.string());
    }

    const std::streampos end = stream.tellg();
    if (end <= 0 || end > static_cast<std::streamoff>(std::numeric_limits<std::streamsize>::max())) {
        throw std::runtime_error("Invalid PNG texture size: " + path.string());
    }

    std::vector<unsigned char> encoded(static_cast<std::size_t>(end));
    stream.seekg(0);
    if (!stream.read(reinterpret_cast<char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()))) {
        throw std::runtime_error("Cannot read PNG texture: " + path.string());
    }

    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    if (png_image_begin_read_from_memory(&image, encoded.data(), encoded.size()) == 0) {
        throw std::runtime_error("Cannot decode PNG texture " + path.string() + ": " + image.message);
    }

    image.format = PNG_FORMAT_RGBA;
    Image result;
    result.width = static_cast<int>(image.width);
    result.height = static_cast<int>(image.height);
    result.pixels.resize(PNG_IMAGE_SIZE(image));
    if (png_image_finish_read(&image, nullptr, result.pixels.data(), 0, nullptr) == 0) {
        const std::string error = image.message;
        png_image_free(&image);
        throw std::runtime_error("Cannot decode PNG texture " + path.string() + ": " + error);
    }
    png_image_free(&image);
    return result;
}

} // namespace pcolonist
