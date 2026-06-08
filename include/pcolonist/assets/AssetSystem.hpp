#pragma once

#include <filesystem>
#include <string>

namespace pcolonist {

class AssetSystem {
public:
    explicit AssetSystem(std::filesystem::path root);

    [[nodiscard]] std::filesystem::path resolve(const std::filesystem::path& relativePath) const;
    [[nodiscard]] std::string readText(const std::filesystem::path& relativePath) const;

private:
    std::filesystem::path root_;
};

} // namespace pcolonist
