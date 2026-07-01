#pragma once

#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"

#include <glm/vec3.hpp>

#include <string_view>

namespace pcolonist {

struct BuildingCost {
    int wood = 0;
    int stone = 0;
    int fiber = 0;
    int metal = 0;
};

struct BuildingDefinition {
    BuildingKind kind = BuildingKind::Campfire;
    std::string_view displayName;
    BuildingCost cost;
    glm::vec3 colliderHalfExtents{0.5F};
    float maxPlacementDistance = 3.0F;
    float terrainProbeHeight = 6.0F;
    float minimumGroundNormalY = 0.78F;
};

struct BuildingPlacementRequest {
    BuildingKind kind = BuildingKind::Campfire;
    glm::vec3 desiredPosition{0.0F};
    glm::vec3 playerPosition{0.0F};
    float yaw = 0.0F;
};

struct BuildingPlacementResult {
    bool canPlace = false;
    std::string_view reason;
    BuildingDefinition definition;
    Transform transform;
    BoxCollider collider;
};

class BuildingPlacementSystem {
public:
    [[nodiscard]] BuildingPlacementResult preview(Registry& registry, const BuildingPlacementRequest& request) const;
    [[nodiscard]] static const BuildingDefinition& definition(BuildingKind kind);
};

} // namespace pcolonist
