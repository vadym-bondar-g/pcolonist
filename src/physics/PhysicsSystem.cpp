#include "pcolonist/physics/PhysicsSystem.hpp"

#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/ecs/Registry.hpp"

#include <glm/common.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace pcolonist {

namespace {

std::optional<float> triangleHeight(
    glm::vec2 point,
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& c) {
    const glm::vec2 ab{b.x - a.x, b.z - a.z};
    const glm::vec2 ac{c.x - a.x, c.z - a.z};
    const glm::vec2 ap{point.x - a.x, point.y - a.z};
    const float denominator = ab.x * ac.y - ac.x * ab.y;
    if (std::abs(denominator) < 0.00001F) {
        return std::nullopt;
    }
    const float u = (ap.x * ac.y - ac.x * ap.y) / denominator;
    const float v = (ab.x * ap.y - ap.x * ab.y) / denominator;
    if (u < -0.001F || v < -0.001F || u + v > 1.001F) {
        return std::nullopt;
    }
    return a.y + (b.y - a.y) * u + (c.y - a.y) * v;
}

std::optional<float> terrainHeight(Registry& registry, glm::vec2 point, float maximumHeight) {
    std::optional<float> result;
    registry.each<Transform, MeshRenderer, TerrainSurface>(
        [&result, point, maximumHeight](Entity, const Transform& transform, const MeshRenderer& renderer, const TerrainSurface&) {
            if (!renderer.mesh) {
                return;
            }
            const Mesh& mesh = *renderer.mesh;
            for (std::size_t index = 0; index + 2 < mesh.indices.size(); index += 3) {
                const auto worldPosition = [&transform](const glm::vec3& local) {
                    return transform.position + local * transform.scale;
                };
                const glm::vec3 a = worldPosition(mesh.vertices[mesh.indices[index]].position);
                const glm::vec3 b = worldPosition(mesh.vertices[mesh.indices[index + 1]].position);
                const glm::vec3 c = worldPosition(mesh.vertices[mesh.indices[index + 2]].position);
                if (const auto height = triangleHeight(point, a, b, c);
                    height && *height <= maximumHeight && (!result || *height > *result)) {
                    result = *height;
                }
            }
        });
    return result;
}

std::optional<float> waterLevel(Registry& registry, glm::vec2 point) {
    std::optional<float> result;
    registry.each<Transform, MeshRenderer, WaterSurface>(
        [&result, point](Entity, const Transform& transform, const MeshRenderer& renderer, const WaterSurface&) {
            if (!renderer.mesh || renderer.mesh->vertices.empty()) {
                return;
            }
            float minimumX = std::numeric_limits<float>::max();
            float maximumX = std::numeric_limits<float>::lowest();
            float minimumZ = std::numeric_limits<float>::max();
            float maximumZ = std::numeric_limits<float>::lowest();
            for (const Vertex& vertex : renderer.mesh->vertices) {
                const glm::vec3 world = transform.position + vertex.position * transform.scale;
                minimumX = std::min(minimumX, world.x);
                maximumX = std::max(maximumX, world.x);
                minimumZ = std::min(minimumZ, world.z);
                maximumZ = std::max(maximumZ, world.z);
            }
            if (point.x >= minimumX && point.x <= maximumX && point.y >= minimumZ && point.y <= maximumZ) {
                result = transform.position.y;
            }
        });
    return result;
}

} // namespace

void PhysicsSystem::update(Registry& registry, float deltaTime) const {
    registry.each<Transform, RigidBody, BoxCollider>(
        [this, deltaTime, &registry](Entity entity, Transform& transform, RigidBody& body, const BoxCollider& collider) {
            if (body.mass <= 0.0F || collider.isStatic) {
                return;
            }
            const glm::vec3 half = collider.halfExtents * transform.scale;
            const std::optional<float> water = waterLevel(registry, {transform.position.x, transform.position.z});
            const float bottom = transform.position.y - half.y;
            const float top = transform.position.y + half.y;
            const float submerged = water
                ? glm::clamp((*water - bottom) / std::max(top - bottom, 0.001F), 0.0F, 1.0F)
                : 0.0F;
            body.inWater = submerged > 0.05F;
            if (body.useGravity) {
                body.velocity += gravity_ * (1.0F - submerged * 0.82F) * deltaTime;
            }
            if (body.inWater) {
                const float damping = std::exp(-2.2F * submerged * deltaTime);
                body.velocity *= damping;
                body.velocity.y += submerged * 2.2F * deltaTime;
            }
            transform.position += body.velocity * deltaTime;

            const float lowestPoint = transform.position.y - half.y;
            const std::optional<float> terrain = terrainHeight(
                registry,
                {transform.position.x, transform.position.z},
                lowestPoint + 0.8F);
            if (terrain && lowestPoint < *terrain) {
                transform.position.y += *terrain - lowestPoint;
                body.velocity.y = 0.0F;
                body.grounded = true;
            } else {
                body.grounded = false;
            }

            registry.each<Transform, BoxCollider>(
                [entity, &transform, &body, half](Entity other, const Transform& obstacle, const BoxCollider& otherCollider) {
                    if (other == entity || !otherCollider.isStatic) {
                        return;
                    }
                    const glm::vec3 otherHalf = otherCollider.halfExtents * obstacle.scale;
                    const glm::vec3 delta = transform.position - obstacle.position;
                    const glm::vec3 overlap = half + otherHalf - glm::abs(delta);
                    if (overlap.x <= 0.0F || overlap.y <= 0.0F || overlap.z <= 0.0F) {
                        return;
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
                });
        });
}

void PhysicsSystem::setGravity(glm::vec3 gravity) {
    gravity_ = gravity;
}

const glm::vec3& PhysicsSystem::gravity() const {
    return gravity_;
}

} // namespace pcolonist
