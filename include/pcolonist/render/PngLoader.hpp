#pragma once

#include <cstddef>
#include <filesystem>
#include <string_view>
#include <vector>

namespace pcolonist {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;
};

class PngLoader {
public:
    static Image load(const std::filesystem::path& path);
    static Image load(std::string_view label, const std::vector<unsigned char>& encoded);
};

} // namespace pcolonist
