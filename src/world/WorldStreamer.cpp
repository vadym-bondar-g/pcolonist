#include "pcolonist/world/WorldStreamer.hpp"

#include "pcolonist/world/AssetManager.hpp"

namespace pcolonist {

WorldStreamer::WorldStreamer(AssetManager& assets)
    : WorldStreamer(assets, ChunkManager::Config{}) {}

WorldStreamer::WorldStreamer(AssetManager& assets, ChunkManager::Config config)
    : assets_(assets),
      chunks_(config) {}

void WorldStreamer::loadInitial(glm::vec3 playerPosition, Registry& registry) {
    chunks_.loadInitial(playerPosition, registry, assets_);
}

void WorldStreamer::update(glm::vec3 playerPosition, Registry& registry, JobSystem& jobs) {
    chunks_.update(playerPosition, registry, assets_, jobs);
}

void WorldStreamer::unloadAll(Registry& registry) {
    chunks_.unloadAll(registry);
}

std::size_t WorldStreamer::activeChunkCount() const {
    return chunks_.activeChunkCount();
}

std::size_t WorldStreamer::pendingChunkCount() const {
    return chunks_.pendingChunkCount();
}

} // namespace pcolonist
