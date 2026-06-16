#include "pcolonist/physics/PhysicsSystem.hpp"

#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/world/WaterWaves.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace pcolonist {

namespace {

std::optional<TerrainHit> terrainSurface(Registry& registry, glm::vec2 point, float maximumHeight) {
    std::optional<TerrainHit> result;
    registry.each<TerrainCollider>(
        [&result, point, maximumHeight](Entity, const TerrainCollider& collider) {
            if (const auto hit = collider.surfaceAt(point, maximumHeight);
                hit && (!result || hit->height > result->height)) {
                result = hit;
            }
        });
    return result;
}

struct WaterHit {
    float level = 0.0F;
    WaterWaveSample wave;
};

std::optional<WaterHit> waterSurface(Registry& registry, glm::vec2 point, float time) {
    std::optional<WaterHit> result;
    registry.each<Transform, WaterSurface>(
        [&result, point, time](Entity, const Transform& transform, const WaterSurface& surface) {
            if (!surface.affectsPhysics) {
                return;
            }
            const glm::vec2 half = surface.halfExtents * glm::vec2{transform.scale.x, transform.scale.z};
            if (std::abs(point.x - transform.position.x) <= half.x
                && std::abs(point.y - transform.position.z) <= half.y) {
                const WaterWaveSample wave = WaterWaves::sample(point, time);
                result = WaterHit{transform.position.y + wave.height, wave};
            }
        });
    return result;
}

std::optional<float> horizontalSweep(
    glm::vec3 start,
    glm::vec3 end,
    glm::vec3 half,
    const StaticBox& obstacle) {
    const float bodyBottom = std::min(start.y, end.y) - half.y;
    const float bodyTop = std::max(start.y, end.y) + half.y;
    if (bodyTop <= obstacle.center.y - obstacle.halfExtents.y
        || bodyBottom >= obstacle.center.y + obstacle.halfExtents.y) {
        return std::nullopt;
    }

    const glm::vec2 expandedHalf{
        obstacle.halfExtents.x + half.x,
        obstacle.halfExtents.z + half.z,
    };
    const glm::vec2 minimum{obstacle.center.x - expandedHalf.x, obstacle.center.z - expandedHalf.y};
    const glm::vec2 maximum{obstacle.center.x + expandedHalf.x, obstacle.center.z + expandedHalf.y};
    const glm::vec2 origin{start.x, start.z};
    const glm::vec2 movement{end.x - start.x, end.z - start.z};
    if (origin.x > minimum.x && origin.x < maximum.x
        && origin.y > minimum.y && origin.y < maximum.y) {
        return std::nullopt;
    }

    float entry = 0.0F;
    float exit = 1.0F;
    for (int axis = 0; axis < 2; ++axis) {
        if (std::abs(movement[axis]) < 0.000001F) {
            if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis]) {
                return std::nullopt;
            }
            continue;
        }
        float near = (minimum[axis] - origin[axis]) / movement[axis];
        float far = (maximum[axis] - origin[axis]) / movement[axis];
        if (near > far) {
            std::swap(near, far);
        }
        entry = std::max(entry, near);
        exit = std::min(exit, far);
        if (entry > exit) {
            return std::nullopt;
        }
    }
    return entry >= 0.0F && entry <= 1.0F ? std::optional<float>{entry} : std::nullopt;
}

} // namespace

void PhysicsSystem::update(Registry& registry, float deltaTime) const {
    registry.each<Transform, RigidBody, BoxCollider>(
        [this, deltaTime, &registry](Entity entity, Transform& transform, RigidBody& body, const BoxCollider& collider) {
            if (body.mass <= 0.0F || collider.isStatic) {
                return;
            }
            const glm::vec3 half = collider.halfExtents * transform.scale;
            const std::optional<WaterHit> water = waterSurface(
                registry,
                {transform.position.x, transform.position.z},
                time_);
            const float bottom = transform.position.y - half.y;
            const float top = transform.position.y + half.y;
            const float submerged = water
                ? glm::clamp((water->level - bottom) / std::max(top - bottom, 0.001F), 0.0F, 1.0F)
                : 0.0F;
            body.inWater = submerged > 0.05F;
            if (body.useGravity) {
                body.velocity += gravity_ * (1.0F - submerged * 0.82F) * deltaTime;
            }
            if (body.inWater) {
                const float damping = std::exp(-2.2F * submerged * deltaTime);
                body.velocity *= damping;
                body.velocity.y += submerged * 2.2F * deltaTime;
                body.velocity.y += water->wave.verticalVelocity * submerged * 0.65F * deltaTime;
                body.velocity.x += water->wave.horizontalVelocity.x * submerged * 0.35F * deltaTime;
                body.velocity.z += water->wave.horizontalVelocity.y * submerged * 0.35F * deltaTime;
            }
            const bool wasGrounded = body.grounded;
            const glm::vec3 previousPosition = transform.position;
            transform.position += body.velocity * deltaTime;

            const float lowestPoint = transform.position.y - half.y;
            const float previousLowestPoint = previousPosition.y - half.y;
            const float sweepTop = std::max(previousLowestPoint, lowestPoint) + 0.8F;
            const std::array<glm::vec2, 5> samples = {
                glm::vec2{0.0F, 0.0F},
                glm::vec2{-half.x * 0.75F, -half.z * 0.75F},
                glm::vec2{half.x * 0.75F, -half.z * 0.75F},
                glm::vec2{-half.x * 0.75F, half.z * 0.75F},
                glm::vec2{half.x * 0.75F, half.z * 0.75F},
            };
            std::optional<TerrainHit> terrain;
            for (const glm::vec2 offset : samples) {
                const auto hit = terrainSurface(
                    registry,
                    {transform.position.x + offset.x, transform.position.z + offset.y},
                    sweepTop);
                if (hit && (!terrain || hit->height > terrain->height)) {
                    terrain = hit;
                }
            }
            constexpr float maximumWalkSlope = 0.62F;
            constexpr float groundSnapDistance = 0.28F;
            const bool penetratingTerrain = terrain && lowestPoint < terrain->height;
            const bool snapToTerrain = terrain
                && wasGrounded
                && body.velocity.y <= 0.0F
                && lowestPoint - terrain->height <= groundSnapDistance;
            if ((penetratingTerrain || snapToTerrain) && terrain->normal.y >= maximumWalkSlope) {
                transform.position.y += terrain->height - lowestPoint;
                body.velocity -= terrain->normal * std::min(glm::dot(body.velocity, terrain->normal), 0.0F);
                body.grounded = true;
                body.groundNormal = terrain->normal;
            } else {
                body.grounded = false;
                body.groundNormal = {0.0F, 1.0F, 0.0F};
                if (penetratingTerrain) {
                    const glm::vec3 downhill = glm::normalize(glm::vec3{
                        terrain->normal.x,
                        -std::sqrt(terrain->normal.x * terrain->normal.x + terrain->normal.z * terrain->normal.z),
                        terrain->normal.z,
                    });
                    body.velocity += downhill * 5.0F * deltaTime;
                    transform.position.x = previousPosition.x;
                    transform.position.z = previousPosition.z;
                }
            }

            const glm::vec3 horizontalDelta{
                transform.position.x - previousPosition.x,
                0.0F,
                transform.position.z - previousPosition.z,
            };
            const glm::vec3 sweepCenter = previousPosition + horizontalDelta * 0.5F;
            const glm::vec3 sweepHalf = half + glm::abs(horizontalDelta) * 0.5F;
            float earliestHit = std::numeric_limits<float>::infinity();
            for (const StaticBox* obstacle : staticColliders_.query(sweepCenter, sweepHalf)) {
                if (const auto hit = horizontalSweep(previousPosition, transform.position, half, *obstacle)) {
                    earliestHit = std::min(earliestHit, *hit);
                }
            }
            if (std::isfinite(earliestHit)) {
                const float stop = std::max(0.0F, earliestHit - 0.001F);
                transform.position.x = previousPosition.x + horizontalDelta.x * stop;
                transform.position.z = previousPosition.z + horizontalDelta.z * stop;
                body.velocity.x = 0.0F;
                body.velocity.z = 0.0F;
            }

            static_cast<void>(entity);
            for (const StaticBox* obstacle : staticColliders_.query(transform.position, half)) {
                    const glm::vec3 delta = transform.position - obstacle->center;
                    const glm::vec3 overlap = half + obstacle->halfExtents - glm::abs(delta);
                    if (overlap.x <= 0.0F || overlap.y <= 0.0F || overlap.z <= 0.0F) {
                        continue;
                    }

                    if (overlap.x < overlap.y && overlap.x < overlap.z) {
                        transform.position.x += std::copysign(overlap.x, delta.x);
                        body.velocity.x = 0.0F;
                    } else if (overlap.z < overlap.y) {
                        transform.position.z += std::copysign(overlap.z, delta.z);
                        body.velocity.z = 0.0F;
                    } else {
                        transform.position.y += std::copysign(overlap.y, delta.y);
                        body.velocity.y = 0.0F;
                        body.grounded = delta.y > 0.0F;
                    }
            }
        });
}

void PhysicsSystem::setGravity(glm::vec3 gravity) {
    gravity_ = gravity;
}

void PhysicsSystem::setTime(float time) {
    time_ = time;
}

void PhysicsSystem::rebuildStaticIndex(Registry& registry) {
    staticColliders_.rebuild(registry);
}

const glm::vec3& PhysicsSystem::gravity() const {
    return gravity_;
}

const StaticColliderGrid& PhysicsSystem::staticColliders() const {
    return staticColliders_;
}

} // namespace pcolonist
