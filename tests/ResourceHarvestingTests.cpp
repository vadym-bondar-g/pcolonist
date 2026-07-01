#include "pcolonist/gameplay/ResourceHarvestingSystem.hpp"

#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"

#include <stdexcept>

namespace {

pcolonist::Entity createResource(
    pcolonist::Registry& registry,
    pcolonist::ResourceNode resource,
    glm::vec3 position = {0.0F, 0.0F, -3.0F}) {
    const pcolonist::Entity entity = registry.create();
    registry.emplace<pcolonist::Transform>(entity, pcolonist::Transform{position});
    registry.emplace<pcolonist::BoxCollider>(entity, pcolonist::BoxCollider{{0.5F, 1.0F, 0.5F}, true});
    registry.emplace<pcolonist::ResourceNode>(entity, resource);
    return entity;
}

} // namespace

void runResourceHarvestingTests() {
    pcolonist::Registry registry;
    const pcolonist::Entity tree = createResource(
        registry,
        pcolonist::ResourceNode{2, 2, 4, 0, 1, 0, pcolonist::ResourceKind::Tree});

    pcolonist::ResourceHarvestingSystem harvesting;
    const pcolonist::HarvestRequest wrongTool{{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, 5.0F, pcolonist::Tool::Pickaxe};
    const pcolonist::HarvestResult wrongToolResult = harvesting.harvest(registry, wrongTool);
    if (!wrongToolResult.hit || wrongToolResult.harvested || registry.get<pcolonist::ResourceNode>(tree).health != 2) {
        throw std::runtime_error("ResourceHarvestingSystem must reject incompatible tools without damaging resources");
    }

    const pcolonist::HarvestRequest axe{{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, 5.0F, pcolonist::Tool::Axe};
    const pcolonist::HarvestResult firstHit = harvesting.harvest(registry, axe);
    if (!firstHit.hit || !firstHit.harvested || firstHit.depleted || registry.get<pcolonist::ResourceNode>(tree).health != 1) {
        throw std::runtime_error("ResourceHarvestingSystem must damage compatible resource nodes");
    }
    const pcolonist::HarvestResult secondHit = harvesting.harvest(registry, axe);
    if (!secondHit.depleted || secondHit.yield.wood != 4 || secondHit.yield.fiber != 1) {
        throw std::runtime_error("ResourceHarvestingSystem must return yield when a node is depleted");
    }

    pcolonist::Registry rockRegistry;
    createResource(
        rockRegistry,
        pcolonist::ResourceNode{2, 2, 0, 3, 0, 1, pcolonist::ResourceKind::Ore});
    const pcolonist::HarvestRequest pickaxe{{0.0F, 0.0F, 0.0F}, {0.0F, 0.0F, -1.0F}, 5.0F, pcolonist::Tool::Pickaxe};
    const pcolonist::HarvestResult oreHit = harvesting.harvest(rockRegistry, pickaxe);
    if (!oreHit.depleted || oreHit.yield.stone != 3 || oreHit.yield.metal != 1) {
        throw std::runtime_error("ResourceHarvestingSystem must support pickaxe resource nodes");
    }

    const pcolonist::HarvestResult miss = harvesting.harvest(
        rockRegistry,
        pcolonist::HarvestRequest{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, 5.0F, pcolonist::Tool::Pickaxe});
    if (miss.hit || miss.harvested) {
        throw std::runtime_error("ResourceHarvestingSystem must report misses");
    }
}
