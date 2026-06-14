#include "pcolonist/assets/AssetSystem.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace pcolonist {

AssetSystem::AssetSystem(std::filesystem::path root)
    : root_(std::filesystem::weakly_canonical(std::move(root))) {}

std::filesystem::path AssetSystem::resolve(const std::filesystem::path& relativePath) const {
    if (relativePath.is_absolute()) {
        throw std::runtime_error("Asset path must be relative: " + relativePath.string());
    }

    const std::filesystem::path resolved = std::filesystem::weakly_canonical(root_ / relativePath);
    const auto [rootEnd, resolvedEnd] = std::mismatch(root_.begin(), root_.end(), resolved.begin(), resolved.end());
    if (rootEnd != root_.end()) {
        throw std::runtime_error("Asset path escapes asset root: " + relativePath.string());
    }
    return resolved;
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
