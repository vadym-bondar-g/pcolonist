#include "pcolonist/world/Chunk.hpp"

#include <cstddef>

namespace pcolonist {

std::size_t ChunkKeyHash::operator()(const ChunkKey& key) const noexcept {
    const auto x = static_cast<std::size_t>(static_cast<unsigned int>(key.x));
    const auto z = static_cast<std::size_t>(static_cast<unsigned int>(key.z));
    return (x * 0x9e3779b1U) ^ (z + 0x85ebca6bU + (x << 6U) + (x >> 2U));
}

std::size_t ChunkIdHash::operator()(const ChunkId& id) const noexcept {
    return ChunkKeyHash{}(id.key) ^ (static_cast<std::size_t>(id.lod) * 0xc2b2ae35U);
}

} // namespace pcolonist
