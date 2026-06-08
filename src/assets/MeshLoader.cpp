#include "pcolonist/assets/MeshLoader.hpp"

#include "pcolonist/assets/ObjLoader.hpp"

#include <stdexcept>

namespace pcolonist {

Mesh MeshLoader::load(const AssetSystem& assets, const std::filesystem::path& relativePath) {
    if (relativePath.extension() == ".obj") {
        return ObjLoader::load(assets, relativePath);
    }
    throw std::runtime_error("Unsupported mesh format: " + relativePath.extension().string());
}

} // namespace pcolonist
