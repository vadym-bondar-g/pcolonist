#include "pcolonist/render/PngLoader.hpp"

#include <png.h>

#include <stdexcept>
#include <string>

namespace pcolonist {

Image PngLoader::load(const std::filesystem::path& path) {
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    const std::string pathString = path.string();
    if (png_image_begin_read_from_file(&image, pathString.c_str()) == 0) {
        throw std::runtime_error("Cannot read PNG texture: " + pathString);
    }

    image.format = PNG_FORMAT_RGBA;
    Image result;
    result.width = static_cast<int>(image.width);
    result.height = static_cast<int>(image.height);
    result.pixels.resize(PNG_IMAGE_SIZE(image));
    if (png_image_finish_read(&image, nullptr, result.pixels.data(), 0, nullptr) == 0) {
        const std::string error = image.message;
        png_image_free(&image);
        throw std::runtime_error("Cannot decode PNG texture " + pathString + ": " + error);
    }
    png_image_free(&image);
    return result;
}

} // namespace pcolonist
