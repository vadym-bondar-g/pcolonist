#include "pcolonist/gameplay/BuildingPlacementSystem.hpp"

#include "pcolonist/render/Mesh.hpp"

#include <stdexcept>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

pcolonist::Mesh flatTerrainMesh() {
    pcolonist::Mesh mesh;
    mesh.vertices = {
        {{-8.0F, 0.0F, -8.0F}, {0.0F, 1.0F, 0.0F}},
        {{8.0F, 0.0F, -8.0F}, {0.0F, 1.0F, 0.0F}},
        {{8.0F, 0.0F, 8.0F}, {0.0F, 1.0F, 0.0F}},
        {{-8.0F, 0.0F, 8.0F}, {0.0F, 1.0F, 0.0F}},
    };
    mesh.indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

void addTerrain(pcolonist::Registry& registry) {
    const pcolonist::Mesh terrainMesh = flatTerrainMesh();
    const pcolonist::Entity terrain = registry.create();
    registry.emplace<pcolonist::TerrainCollider>(terrain, terrainMesh, pcolonist::Transform{}, 4.0F);
}

} // namespace

void runBuildingPlacementTests() {
    {
        pcolonist::Registry registry;
        addTerrain(registry);
        const pcolonist::BuildingPlacementSystem placement;

        const pcolonist::BuildingPlacementResult result = placement.preview(
            registry,
            pcolonist::BuildingPlacementRequest{
                pcolonist::BuildingKind::Campfire,
                {1.2F, 1.0F, 0.0F},
                {0.0F, 0.0F, 0.0F},
                0.25F,
            });

        require(result.canPlace, "BuildingPlacementSystem must allow a clear supported campfire placement");
        require(result.transform.position.y > 0.3F && result.transform.position.y < 0.4F, "Campfire must snap to terrain height");
        require(result.collider.isStatic, "Placed building colliders must be static");
    }

    {
        pcolonist::Registry registry;
        addTerrain(registry);
        const pcolonist::Entity blocker = registry.create();
        registry.emplace<pcolonist::Transform>(blocker, pcolonist::Transform{{1.2F, 0.35F, 0.0F}});
        registry.emplace<pcolonist::BoxCollider>(blocker, pcolonist::BoxCollider{{0.6F, 0.4F, 0.6F}, true});

        const pcolonist::BuildingPlacementResult result = pcolonist::BuildingPlacementSystem{}.preview(
            registry,
            pcolonist::BuildingPlacementRequest{
                pcolonist::BuildingKind::Campfire,
                {1.2F, 1.0F, 0.0F},
                {0.0F, 0.0F, 0.0F},
                0.0F,
            });

        require(!result.canPlace, "BuildingPlacementSystem must reject occupied placement space");
    }

    {
        pcolonist::Registry registry;
        const pcolonist::BuildingPlacementResult result = pcolonist::BuildingPlacementSystem{}.preview(
            registry,
            pcolonist::BuildingPlacementRequest{
                pcolonist::BuildingKind::Campfire,
                {8.0F, 0.0F, 0.0F},
                {0.0F, 0.0F, 0.0F},
                0.0F,
            });

        require(!result.canPlace, "BuildingPlacementSystem must enforce placement reach");
    }

    {
        const pcolonist::BuildingDefinition& campfire =
            pcolonist::BuildingPlacementSystem::definition(pcolonist::BuildingKind::Campfire);
        require(campfire.cost.wood == 1 && campfire.cost.stone == 1, "Campfire cost must match starter crafting rules");
    }
}
