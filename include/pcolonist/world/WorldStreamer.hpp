#pragma once

#include "pcolonist/world/ChunkManager.hpp"

#include <glm/vec3.hpp>

namespace pcolonist {

class AssetManager;
class JobSystem;
class Registry;

class WorldStreamer {
public:
    explicit WorldStreamer(AssetManager& assets);
    WorldStreamer(AssetManager& assets, ChunkManager::Config config);

    void loadInitial(glm::vec3 playerPosition, Registry& registry);
    void update(glm::vec3 playerPosition, Registry& registry, JobSystem& jobs);
    void unloadAll(Registry& registry);

    [[nodiscard]] std::size_t activeChunkCount() const;
    [[nodiscard]] std::size_t pendingChunkCount() const;

private:
    AssetManager& assets_;
    ChunkManager chunks_;
};

} // namespace pcolonist
