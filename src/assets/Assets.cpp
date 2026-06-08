#include "pcolonist/assets/AssetSystem.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace pcolonist {

AssetSystem::AssetSystem(std::filesystem::path root)
    : root_(std::move(root)) {}

std::filesystem::path AssetSystem::resolve(const std::filesystem::path& relativePath) const {
    if (relativePath.is_absolute()) {
        throw std::runtime_error("Asset path must be relative: " + relativePath.string());
    }
    return root_ / relativePath;
}

std::string AssetSystem::readText(const std::filesystem::path& relativePath) const {
    const std::filesystem::path path = resolve(relativePath);
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Cannot open asset: " + path.string());
    }

    std::ostringstream contents;
    contents << stream.rdbuf();
    return contents.str();
}

} // namespace pcolonist
