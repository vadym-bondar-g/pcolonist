#include "pcolonist/assets/AssetSystem.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace {

std::string assetCacheKey(const std::filesystem::path& relativePath) {
    if (relativePath.is_absolute()) {
        throw std::runtime_error("Asset path must be relative: " + relativePath.string());
    }

    const std::filesystem::path normalized = relativePath.lexically_normal();
    for (const std::filesystem::path& part : normalized) {
        if (part == "..") {
            throw std::runtime_error("Asset path escapes asset root: " + relativePath.string());
        }
    }
    return normalized.generic_string();
}

std::vector<unsigned char> readFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw std::runtime_error("Cannot open asset: " + path.string());
    }

    const std::streampos end = stream.tellg();
    if (end < 0 || end > static_cast<std::streamoff>(std::numeric_limits<std::streamsize>::max())) {
        throw std::runtime_error("Invalid asset size: " + path.string());
    }

    std::vector<unsigned char> bytes(static_cast<std::size_t>(end));
    stream.seekg(0);
    if (!bytes.empty()
        && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error("Cannot read asset: " + path.string());
    }
    return bytes;
}

} // namespace

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

const std::vector<unsigned char>& AssetSystem::readBinary(const std::filesystem::path& relativePath) const {
    const std::string key = assetCacheKey(relativePath);
    {
        std::lock_guard lock(cacheMutex_);
        if (const auto iterator = cache_.find(key); iterator != cache_.end()) {
            return iterator->second;
        }
    }

    std::vector<unsigned char> bytes = readFile(resolve(relativePath));

    std::lock_guard lock(cacheMutex_);
    return cache_.try_emplace(key, std::move(bytes)).first->second;
}

std::string AssetSystem::readText(const std::filesystem::path& relativePath) const {
    const std::vector<unsigned char>& bytes = readBinary(relativePath);
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

void AssetSystem::preloadAll() const {
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root_)) {
        if (entry.is_symlink() || !entry.is_regular_file()) {
            continue;
        }
        const std::string key = std::filesystem::relative(entry.path(), root_).lexically_normal().generic_string();
        {
            std::lock_guard lock(cacheMutex_);
            if (cache_.contains(key)) {
                continue;
            }
        }

        std::vector<unsigned char> bytes = readFile(entry.path());
        std::lock_guard lock(cacheMutex_);
        cache_.try_emplace(key, std::move(bytes));
    }
}

} // namespace pcolonist
