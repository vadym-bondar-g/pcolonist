#pragma once

#include <cstddef>
#include <filesystem>
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
};

} // namespace pcolonist
