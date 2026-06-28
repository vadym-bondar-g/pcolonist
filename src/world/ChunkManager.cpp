#include "pcolonist/world/ChunkManager.hpp"

#include "pcolonist/assets/AssetSystem.hpp"
#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"
#include "pcolonist/scripting/ScriptSystem.hpp"
#include "pcolonist/world/AssetManager.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <stdexcept>

namespace {

int chebyshevDistance(pcolonist::ChunkKey a, pcolonist::ChunkKey b) {
    return std::max(std::abs(a.x - b.x), std::abs(a.z - b.z));
}

bool ready(std::future<std::shared_ptr<pcolonist::Mesh>>& future) {
    using namespace std::chrono_literals;
    return future.wait_for(0ms) == std::future_status::ready;
}

std::filesystem::path sceneChunkPath(pcolonist::ChunkKey key) {
    return "scripts/chunks/scene_" + std::to_string(key.x) + "_" + std::to_string(key.z) + ".scene";
}

} // namespace

namespace pcolonist {

ChunkManager::ChunkManager()
    : ChunkManager(Config{}) {}

ChunkManager::ChunkManager(Config config)
    : config_(config) {}

void ChunkManager::loadInitial(
    glm::vec3 playerPosition,
    Registry& registry,
    AssetManager& assets,
    const AssetSystem& assetSystem,
    PhysicsSystem& physics,
    ResourceManager& resources,
    ScriptSystem& scripts,
    JobSystem& jobs) {
    const ChunkKey center = chunkFor(playerPosition);
    for (int z = center.z - config_.loadRadius; z <= center.z + config_.loadRadius; ++z) {
        for (int x = center.x - config_.loadRadius; x <= center.x + config_.loadRadius; ++x) {
            const ChunkId id = desiredId({x, z}, center);
            if (active_.contains(id) || missing_.contains(id)) {
                continue;
            }
            TerrainTile tile = terrainTile(id);
            try {
                Chunk chunk{tile, assets.loadMesh(tile.meshPath), {}};
                spawnChunk(std::move(chunk), registry, assetSystem, physics, resources, scripts, jobs);
            } catch (const std::exception&) {
                missing_.insert(id);
            }
        }
    }
}

void ChunkManager::update(
    glm::vec3 playerPosition,
    Registry& registry,
    AssetManager& assets,
    const AssetSystem& assetSystem,
    PhysicsSystem& physics,
    ResourceManager& resources,
    ScriptSystem& scripts,
    JobSystem& jobs) {
    const ChunkKey center = chunkFor(playerPosition);
    const bool unloadedStaticColliders = unloadDistantChunks(center, registry);
    if (unloadedStaticColliders) {
        physics.rebuildStaticIndex(registry);
    }
    integrateReadyChunks(registry, assetSystem, physics, resources, scripts, jobs);
    requestMissingChunks(center, assets, jobs);
    assets.clearExpired();
}

void ChunkManager::unloadAll(Registry& registry) {
    for (auto& [id, chunk] : active_) {
        static_cast<void>(id);
        for (Entity entity : chunk.entities) {
            registry.destroy(entity);
        }
    }
    active_.clear();
    pending_.clear();
}

ChunkKey ChunkManager::chunkFor(glm::vec3 position) const {
    return {
        static_cast<int>(std::floor(position.x / config_.chunkSize)),
        static_cast<int>(std::floor(position.z / config_.chunkSize)),
    };
}

std::size_t ChunkManager::activeChunkCount() const {
    return active_.size();
}

std::size_t ChunkManager::pendingChunkCount() const {
    return pending_.size();
}

ChunkManager::RuntimeStats ChunkManager::stats(glm::vec3 playerPosition) const {
    RuntimeStats result;
    result.center = chunkFor(playerPosition);
    result.chunkSize = config_.chunkSize;
    result.loadRadius = config_.loadRadius;
    result.unloadRadius = config_.unloadRadius;
    result.lod0Radius = config_.lod0Radius;
    result.lod1Radius = config_.lod1Radius;
    result.activeChunks = active_.size();
    result.pendingChunks = pending_.size();
    result.missingChunks = missing_.size();
    for (const auto& [id, chunk] : active_) {
        switch (id.lod) {
        case 0:
            ++result.lod0Chunks;
            break;
        case 1:
            ++result.lod1Chunks;
            break;
        default:
            ++result.lod2Chunks;
            break;
        }
        result.terrainEntities += chunk.terrainMesh ? 1U : 0U;
        result.sceneEntities += chunk.entities.size() > 0U ? chunk.entities.size() - 1U : 0U;
    }
    return result;
}

ChunkId ChunkManager::desiredId(ChunkKey key, ChunkKey center) const {
    const int distance = chebyshevDistance(key, center);
    const int lod = distance <= config_.lod0Radius ? 0 : distance <= config_.lod1Radius ? 1 : 2;
    return {key, lod};
}

TerrainTile ChunkManager::terrainTile(ChunkId id) const {
    return {
        id,
        "maps/chunks/terrain_lod" + std::to_string(id.lod) + "_" + std::to_string(id.key.x) + "_"
            + std::to_string(id.key.z) + ".obj",
        {
            (static_cast<float>(id.key.x) + 0.5F) * config_.chunkSize,
            (static_cast<float>(id.key.z) + 0.5F) * config_.chunkSize,
        },
        config_.chunkSize,
    };
}

bool ChunkManager::shouldKeep(ChunkId id, ChunkKey center) const {
    if (chebyshevDistance(id.key, center) > config_.unloadRadius) {
        return false;
    }
    const ChunkId replacement = desiredId(id.key, center);
    if (replacement.lod == id.lod) {
        return true;
    }
    return !active_.contains(replacement);
}

void ChunkManager::requestMissingChunks(ChunkKey center, AssetManager& assets, JobSystem& jobs) {
    for (int z = center.z - config_.loadRadius; z <= center.z + config_.loadRadius; ++z) {
        for (int x = center.x - config_.loadRadius; x <= center.x + config_.loadRadius; ++x) {
            const ChunkId id = desiredId({x, z}, center);
            if (active_.contains(id) || pending_.contains(id) || missing_.contains(id)) {
                continue;
            }
            TerrainTile tile = terrainTile(id);
            pending_.emplace(id, PendingChunk{tile, assets.loadMeshAsync(tile.meshPath, jobs)});
        }
    }
}

void ChunkManager::integrateReadyChunks(
    Registry& registry,
    const AssetSystem& assetSystem,
    PhysicsSystem& physics,
    ResourceManager& resources,
    ScriptSystem& scripts,
    JobSystem& jobs) {
    for (auto iterator = pending_.begin(); iterator != pending_.end();) {
        if (!ready(iterator->second.mesh)) {
            ++iterator;
            continue;
        }

        try {
            Chunk chunk{iterator->second.terrain, iterator->second.mesh.get(), {}};
            spawnChunk(std::move(chunk), registry, assetSystem, physics, resources, scripts, jobs);
        } catch (const std::exception&) {
            missing_.insert(iterator->first);
        }
        iterator = pending_.erase(iterator);
    }
}

bool ChunkManager::unloadDistantChunks(ChunkKey center, Registry& registry) {
    bool unloadedStaticColliders = false;
    for (auto iterator = active_.begin(); iterator != active_.end();) {
        if (shouldKeep(iterator->first, center)) {
            ++iterator;
            continue;
        }
        for (Entity entity : iterator->second.entities) {
            unloadedStaticColliders = unloadedStaticColliders || registry.has<BoxCollider>(entity);
            registry.destroy(entity);
        }
        iterator = active_.erase(iterator);
    }
    return unloadedStaticColliders;
}

void ChunkManager::spawnChunk(
    Chunk chunk,
    Registry& registry,
    const AssetSystem& assetSystem,
    PhysicsSystem& physics,
    ResourceManager& resources,
    ScriptSystem& scripts,
    JobSystem& jobs) {
    if (!chunk.terrainMesh || chunk.terrainMesh->vertices.empty() || chunk.terrainMesh->indices.empty()) {
        throw std::runtime_error("Cannot spawn empty terrain chunk");
    }

    const Entity terrain = registry.create();
    const Transform& transform = registry.emplace<Transform>(terrain);
    registry.emplace<MeshRenderer>(terrain, chunk.terrainMesh);
    registry.emplace<TerrainSurface>(terrain);
    registry.emplace<TerrainChunk>(terrain, chunk.terrain.center, chunk.terrain.size * 0.78F);
    registry.emplace<TerrainCollider>(terrain, *chunk.terrainMesh, transform);
    chunk.entities.push_back(terrain);

    const std::filesystem::path scenePath = sceneChunkPath(chunk.terrain.id.key);
    if (std::filesystem::exists(assetSystem.resolve(scenePath))) {
        std::vector<Entity> sceneEntities = scripts.execute(assetSystem, scenePath, registry, physics, resources, jobs);
        chunk.entities.insert(chunk.entities.end(), sceneEntities.begin(), sceneEntities.end());
        physics.rebuildStaticIndex(registry);
    }
    active_.emplace(chunk.terrain.id, std::move(chunk));
}

} // namespace pcolonist
