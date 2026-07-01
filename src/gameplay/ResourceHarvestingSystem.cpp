#include "pcolonist/gameplay/ResourceHarvestingSystem.hpp"

#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <optional>

namespace {

std::optional<float> rayBoxDistance(
    glm::vec3 origin,
    glm::vec3 direction,
    glm::vec3 center,
    glm::vec3 halfExtents,
    float maximumDistance) {
    float near = 0.0F;
    float far = maximumDistance;
    for (int axis = 0; axis < 3; ++axis) {
        const float minimum = center[axis] - halfExtents[axis];
        const float maximum = center[axis] + halfExtents[axis];
        if (std::abs(direction[axis]) < 0.00001F) {
            if (origin[axis] < minimum || origin[axis] > maximum) {
                return std::nullopt;
            }
            continue;
        }
        const float inverse = 1.0F / direction[axis];
        float first = (minimum - origin[axis]) * inverse;
        float second = (maximum - origin[axis]) * inverse;
        if (first > second) {
            std::swap(first, second);
        }
        near = std::max(near, first);
        far = std::min(far, second);
        if (near > far) {
            return std::nullopt;
        }
    }
    return near <= maximumDistance ? std::optional<float>{near} : std::nullopt;
}

} // namespace

namespace pcolonist {

HarvestResult ResourceHarvestingSystem::harvest(Registry& registry, const HarvestRequest& request) const {
    if (request.reach <= 0.0F || glm::length(request.direction) <= 0.0001F) {
        return {false, false, false, nullEntity, 0.0F, {}, "Нет цели"};
    }

    const glm::vec3 direction = glm::normalize(request.direction);
    HarvestResult result;
    result.distance = request.reach;
    registry.each<Transform, ResourceNode, BoxCollider>(
        [&result, &request, direction](
            Entity entity,
            const Transform& transform,
            const ResourceNode&,
            const BoxCollider& collider) {
            const auto distance = rayBoxDistance(
                request.origin,
                direction,
                transform.position,
                sanitizedColliderHalfExtents(collider, transform),
                result.distance);
            if (distance) {
                result.hit = true;
                result.target = entity;
                result.distance = *distance;
            }
        });

    if (!result.hit) {
        result.message = "Нет ресурса";
        return result;
    }

    ResourceNode& resource = registry.get<ResourceNode>(result.target);
    if (!canHarvest(resource.kind, request.tool)) {
        result.message = "Нужен другой инструмент";
        return result;
    }

    resource.health = std::max(resource.health - damageFor(resource.kind, request.tool), 0);
    result.harvested = true;
    result.message = "Ресурс поврежден";
    if (resource.health <= 0) {
        result.depleted = true;
        result.yield = yieldFor(resource);
        result.message = "Ресурс добыт";
    }
    return result;
}

bool ResourceHarvestingSystem::canHarvest(ResourceKind kind, Tool tool) const {
    switch (kind) {
    case ResourceKind::Tree:
        return tool == Tool::Axe;
    case ResourceKind::Stone:
    case ResourceKind::Ore:
        return tool == Tool::Pickaxe;
    case ResourceKind::Plant:
        return tool == Tool::Knife || tool == Tool::Axe;
    }
    return false;
}

int ResourceHarvestingSystem::damageFor(ResourceKind kind, Tool tool) const {
    if (kind == ResourceKind::Plant && tool == Tool::Knife) {
        return 2;
    }
    if ((kind == ResourceKind::Stone || kind == ResourceKind::Ore) && tool == Tool::Pickaxe) {
        return 2;
    }
    return 1;
}

HarvestYield ResourceHarvestingSystem::yieldFor(const ResourceNode& resource) const {
    return {
        std::max(resource.woodYield, 0),
        std::max(resource.stoneYield, 0),
        std::max(resource.fiberYield, 0),
        std::max(resource.metalYield, 0),
    };
}

} // namespace pcolonist
