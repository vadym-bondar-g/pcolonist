#pragma once

#include "pcolonist/world/Chunk.hpp"

#include <glm/vec3.hpp>

#include <future>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace pcolonist {

class AssetManager;
class JobSystem;
class Registry;

class ChunkManager {
public:
    struct Config {
        float chunkSize = 128.0F;
        int loadRadius = 3;
        int unloadRadius = 4;
        int lod0Radius = 1;
        int lod1Radius = 2;
    };

    ChunkManager();
    explicit ChunkManager(Config config);

    void loadInitial(glm::vec3 playerPosition, Registry& registry, AssetManager& assets);
    void update(glm::vec3 playerPosition, Registry& registry, AssetManager& assets, JobSystem& jobs);
    void unloadAll(Registry& registry);

    [[nodiscard]] ChunkKey chunkFor(glm::vec3 position) const;
    [[nodiscard]] std::size_t activeChunkCount() const;
    [[nodiscard]] std::size_t pendingChunkCount() const;

private:
    struct PendingChunk {
        TerrainTile terrain;
        std::future<std::shared_ptr<Mesh>> mesh;
    };

    [[nodiscard]] ChunkId desiredId(ChunkKey key, ChunkKey center) const;
    [[nodiscard]] TerrainTile terrainTile(ChunkId id) const;
    [[nodiscard]] bool shouldKeep(ChunkId id, ChunkKey center) const;
    void requestMissingChunks(ChunkKey center, AssetManager& assets, JobSystem& jobs);
    void integrateReadyChunks(Registry& registry);
    void unloadDistantChunks(ChunkKey center, Registry& registry);
    void spawnChunk(Chunk chunk, Registry& registry);

    Config config_;
    std::unordered_map<ChunkId, Chunk, ChunkIdHash> active_;
    std::unordered_map<ChunkId, PendingChunk, ChunkIdHash> pending_;
    std::unordered_set<ChunkId, ChunkIdHash> missing_;
};

} // namespace pcolonist
