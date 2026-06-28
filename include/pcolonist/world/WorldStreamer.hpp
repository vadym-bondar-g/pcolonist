#pragma once

#include "pcolonist/world/ChunkManager.hpp"

#include <glm/vec3.hpp>

namespace pcolonist {

class AssetManager;
class AssetSystem;
class JobSystem;
class PhysicsSystem;
class Registry;
class ResourceManager;
class ScriptSystem;

class WorldStreamer {
public:
    explicit WorldStreamer(AssetManager& assets);
    WorldStreamer(AssetManager& assets, ChunkManager::Config config);

    void loadInitial(
        glm::vec3 playerPosition,
        Registry& registry,
        const AssetSystem& assetSystem,
        PhysicsSystem& physics,
        ResourceManager& resources,
        ScriptSystem& scripts,
        JobSystem& jobs);
    void update(
        glm::vec3 playerPosition,
        Registry& registry,
        const AssetSystem& assetSystem,
        PhysicsSystem& physics,
        ResourceManager& resources,
        ScriptSystem& scripts,
        JobSystem& jobs);
    void unloadAll(Registry& registry);

    [[nodiscard]] std::size_t activeChunkCount() const;
    [[nodiscard]] std::size_t pendingChunkCount() const;
    [[nodiscard]] ChunkManager::RuntimeStats stats(glm::vec3 playerPosition) const;

private:
    AssetManager& assets_;
    ChunkManager chunks_;
};

} // namespace pcolonist
