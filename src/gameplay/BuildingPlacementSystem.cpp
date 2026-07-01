#include "pcolonist/gameplay/BuildingPlacementSystem.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace pcolonist {

namespace {

constexpr std::array definitions{
    BuildingDefinition{
        BuildingKind::Campfire,
        "КОСТЕР",
        BuildingCost{1, 1, 0, 0},
        glm::vec3{0.78F, 0.34F, 0.78F},
        3.4F,
        7.0F,
        0.72F,
    },
    BuildingDefinition{
        BuildingKind::StorageCrate,
        "ЯЩИК",
        BuildingCost{4, 0, 2, 0},
        glm::vec3{0.72F, 0.55F, 0.72F},
        3.2F,
        7.0F,
        0.78F,
    },
    BuildingDefinition{
        BuildingKind::Workbench,
        "ВЕРСТАК",
        BuildingCost{6, 2, 2, 1},
        glm::vec3{1.12F, 0.78F, 0.72F},
        3.0F,
        7.0F,
        0.82F,
    },
};

bool finiteVector(glm::vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float horizontalDistance(glm::vec3 a, glm::vec3 b) {
    return glm::length(glm::vec2{a.x - b.x, a.z - b.z});
}

bool overlaps(glm::vec3 aCenter, glm::vec3 aHalfExtents, glm::vec3 bCenter, glm::vec3 bHalfExtents) {
    return std::abs(aCenter.x - bCenter.x) <= aHalfExtents.x + bHalfExtents.x
        && std::abs(aCenter.y - bCenter.y) <= aHalfExtents.y + bHalfExtents.y
        && std::abs(aCenter.z - bCenter.z) <= aHalfExtents.z + bHalfExtents.z;
}

std::optional<TerrainHit> bestTerrainSurface(Registry& registry, glm::vec3 position, float probeHeight) {
    std::optional<TerrainHit> best;
    registry.each<TerrainCollider>(
        [&best, position, probeHeight](Entity, const TerrainCollider& terrain) {
            const std::optional<TerrainHit> hit = terrain.surfaceAt(
                glm::vec2{position.x, position.z},
                position.y + probeHeight);
            if (!hit) {
                return;
            }
            if (!best || hit->height > best->height) {
                best = *hit;
            }
        });
    return best;
}

} // namespace

BuildingPlacementResult BuildingPlacementSystem::preview(
    Registry& registry,
    const BuildingPlacementRequest& request) const {
    const BuildingDefinition& building = definition(request.kind);
    BuildingPlacementResult result;
    result.definition = building;

    if (!finiteVector(request.desiredPosition) || !finiteVector(request.playerPosition)) {
        result.reason = "НЕКОРРЕКТНАЯ ТОЧКА СТРОИТЕЛЬСТВА";
        return result;
    }
    if (horizontalDistance(request.desiredPosition, request.playerPosition) > building.maxPlacementDistance) {
        result.reason = "СЛИШКОМ ДАЛЕКО";
        return result;
    }
    if (!validPhysicsPoint(request.desiredPosition)) {
        result.reason = "НЕЛЬЗЯ СТРОИТЬ ЗДЕСЬ";
        return result;
    }

    Transform transform{
        request.desiredPosition,
        {0.0F, request.yaw, 0.0F},
        {1.0F, 1.0F, 1.0F},
    };
    if (const std::optional<TerrainHit> surface = bestTerrainSurface(registry, transform.position, building.terrainProbeHeight)) {
        if (surface->normal.y < building.minimumGroundNormalY) {
            result.reason = "НУЖНА БОЛЕЕ РОВНАЯ ПОВЕРХНОСТЬ";
            return result;
        }
        transform.position.y = surface->height + building.colliderHalfExtents.y;
    }

    const BoxCollider collider{building.colliderHalfExtents, true};
    bool blocked = false;
    registry.each<Transform, BoxCollider>(
        [&blocked, &transform, &collider](Entity, const Transform& otherTransform, const BoxCollider& otherCollider) {
            if (blocked) {
                return;
            }
            const glm::vec3 otherHalfExtents = sanitizedColliderHalfExtents(otherCollider, otherTransform);
            if (overlaps(transform.position, collider.halfExtents, otherTransform.position, otherHalfExtents)) {
                blocked = true;
            }
        });
    if (blocked) {
        result.reason = "МЕСТО ЗАНЯТО";
        return result;
    }

    result.canPlace = true;
    result.reason = "МОЖНО СТРОИТЬ";
    result.transform = transform;
    result.collider = collider;
    return result;
}

const BuildingDefinition& BuildingPlacementSystem::definition(BuildingKind kind) {
    const auto iterator = std::find_if(
        definitions.begin(),
        definitions.end(),
        [kind](const BuildingDefinition& definition) {
            return definition.kind == kind;
        });
    return iterator != definitions.end() ? *iterator : definitions.front();
}

} // namespace pcolonist
