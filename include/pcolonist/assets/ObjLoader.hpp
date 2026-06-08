#pragma once

#include "pcolonist/render/Mesh.hpp"

#include <filesystem>

namespace pcolonist {

class AssetSystem;

class ObjLoader {
public:
    static Mesh load(const AssetSystem& assets, const std::filesystem::path& relativePath);
};

} // namespace pcolonist
