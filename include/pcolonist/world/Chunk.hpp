#pragma once

#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/render/Mesh.hpp"

#include <glm/vec2.hpp>

#include <filesystem>
#include <memory>
#include <vector>

namespace pcolonist {

struct ChunkKey {
    int x = 0;
    int z = 0;

    bool operator==(const ChunkKey&) const = default;
};

struct ChunkId {
    ChunkKey key;
    int lod = 0;

    bool operator==(const ChunkId&) const = default;
};

struct ChunkKeyHash {
    std::size_t operator()(const ChunkKey& key) const noexcept;
};

struct ChunkIdHash {
    std::size_t operator()(const ChunkId& id) const noexcept;
};

struct TerrainTile {
    ChunkId id;
    std::filesystem::path meshPath;
    glm::vec2 center{0.0F};
    float size = 64.0F;
};

struct Chunk {
    TerrainTile terrain;
    std::shared_ptr<Mesh> terrainMesh;
    std::vector<Entity> entities;
};

} // namespace pcolonist
