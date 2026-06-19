#include "pcolonist/world/AssetManager.hpp"

#include "pcolonist/assets/AssetSystem.hpp"
#include "pcolonist/assets/MeshLoader.hpp"
#include "pcolonist/core/JobSystem.hpp"

#include <algorithm>

namespace pcolonist {

AssetManager::AssetManager(const AssetSystem& assets)
    : assets_(assets) {}

std::shared_ptr<Mesh> AssetManager::loadMesh(const std::filesystem::path& relativePath) {
    const std::string id = relativePath.generic_string();
    {
        std::lock_guard lock(mutex_);
        if (const auto iterator = meshes_.find(id); iterator != meshes_.end()) {
            if (auto mesh = iterator->second.lock()) {
                return mesh;
            }
        }
    }

    auto mesh = std::make_shared<Mesh>(MeshLoader::load(assets_, relativePath));
    {
        std::lock_guard lock(mutex_);
        meshes_[id] = mesh;
    }
    return mesh;
}

std::future<std::shared_ptr<Mesh>> AssetManager::loadMeshAsync(
    const std::filesystem::path& relativePath,
    JobSystem& jobs) {
    return jobs.submit([this, relativePath] {
        return loadMesh(relativePath);
    });
}

void AssetManager::clearExpired() {
    std::lock_guard lock(mutex_);
    for (auto iterator = meshes_.begin(); iterator != meshes_.end();) {
        if (iterator->second.expired()) {
            iterator = meshes_.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

} // namespace pcolonist
