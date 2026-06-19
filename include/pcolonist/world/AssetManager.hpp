#pragma once

#include "pcolonist/render/Mesh.hpp"

#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace pcolonist {

class AssetSystem;
class JobSystem;

class AssetManager {
public:
    explicit AssetManager(const AssetSystem& assets);

    [[nodiscard]] std::shared_ptr<Mesh> loadMesh(const std::filesystem::path& relativePath);
    [[nodiscard]] std::future<std::shared_ptr<Mesh>> loadMeshAsync(
        const std::filesystem::path& relativePath,
        JobSystem& jobs);
    void clearExpired();

private:
    const AssetSystem& assets_;
    std::mutex mutex_;
    std::unordered_map<std::string, std::weak_ptr<Mesh>> meshes_;
};

} // namespace pcolonist
