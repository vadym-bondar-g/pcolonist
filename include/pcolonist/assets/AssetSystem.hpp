#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace pcolonist {

class AssetSystem {
public:
    explicit AssetSystem(std::filesystem::path root);

    [[nodiscard]] std::filesystem::path resolve(const std::filesystem::path& relativePath) const;
    [[nodiscard]] const std::vector<unsigned char>& readBinary(const std::filesystem::path& relativePath) const;
    [[nodiscard]] std::string readText(const std::filesystem::path& relativePath) const;
    void preloadAll() const;

private:
    std::filesystem::path root_;
    mutable std::mutex cacheMutex_;
    mutable std::unordered_map<std::string, std::vector<unsigned char>> cache_;
};

} // namespace pcolonist
