#pragma once

#include "pcolonist/world/Chunk.hpp"

#include <glm/vec3.hpp>

#include <future>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace pcolonist {

class AssetManager;
class AssetSystem;
class JobSystem;
class PhysicsSystem;
class Registry;
class ResourceManager;
class ScriptSystem;

class ChunkManager {
public:
    struct Config {
        float chunkSize = 128.0F;
        int loadRadius = 2;
        int unloadRadius = 3;
        int lod0Radius = 1;
        int lod1Radius = 2;
    };

    struct RuntimeStats {
        ChunkKey center{};
        float chunkSize = 0.0F;
        int loadRadius = 0;
        int unloadRadius = 0;
        int lod0Radius = 0;
        int lod1Radius = 0;
        std::size_t activeChunks = 0;
        std::size_t pendingChunks = 0;
        std::size_t missingChunks = 0;
        std::size_t lod0Chunks = 0;
        std::size_t lod1Chunks = 0;
        std::size_t lod2Chunks = 0;
        std::size_t terrainEntities = 0;
        std::size_t sceneEntities = 0;
    };

    ChunkManager();
    explicit ChunkManager(Config config);

    void loadInitial(
        glm::vec3 playerPosition,
        Registry& registry,
        AssetManager& assets,
        const AssetSystem& assetSystem,
        PhysicsSystem& physics,
        ResourceManager& resources,
        ScriptSystem& scripts,
        JobSystem& jobs);
    [[nodiscard]] bool update(
        glm::vec3 playerPosition,
        Registry& registry,
        AssetManager& assets,
        const AssetSystem& assetSystem,
        PhysicsSystem& physics,
        ResourceManager& resources,
        ScriptSystem& scripts,
        JobSystem& jobs);
    void unloadAll(Registry& registry);

    [[nodiscard]] ChunkKey chunkFor(glm::vec3 position) const;
    [[nodiscard]] std::size_t activeChunkCount() const;
    [[nodiscard]] std::size_t pendingChunkCount() const;
    [[nodiscard]] RuntimeStats stats(glm::vec3 playerPosition) const;

private:
    struct PendingChunk {
        TerrainTile terrain;
        std::future<std::shared_ptr<Mesh>> mesh;
    };

    [[nodiscard]] ChunkId desiredId(ChunkKey key, ChunkKey center) const;
    [[nodiscard]] TerrainTile terrainTile(ChunkId id) const;
    [[nodiscard]] bool shouldKeep(ChunkId id, ChunkKey center) const;
    [[nodiscard]] bool shouldTrackRequest(ChunkId id, ChunkKey center) const;
    void pruneStaleRequests(ChunkKey center);
    void requestMissingChunks(ChunkKey center, AssetManager& assets, JobSystem& jobs);
    void integrateReadyChunks(
        ChunkKey center,
        Registry& registry,
        const AssetSystem& assetSystem,
        PhysicsSystem& physics,
        ResourceManager& resources,
        ScriptSystem& scripts,
        JobSystem& jobs);
    bool unloadDistantChunks(ChunkKey center, Registry& registry, bool& unloadedChunks);
    void spawnChunk(
        Chunk chunk,
        Registry& registry,
        const AssetSystem& assetSystem,
        PhysicsSystem& physics,
        ResourceManager& resources,
        ScriptSystem& scripts,
        JobSystem& jobs);

    Config config_;
    std::unordered_map<ChunkId, Chunk, ChunkIdHash> active_;
    std::unordered_map<ChunkId, PendingChunk, ChunkIdHash> pending_;
    std::unordered_set<ChunkId, ChunkIdHash> missing_;
};

} // namespace pcolonist
