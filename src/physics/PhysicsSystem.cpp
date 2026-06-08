#include "pcolonist/physics/PhysicsSystem.hpp"

#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/ecs/Registry.hpp"

#include <glm/common.hpp>

#include <algorithm>
#include <cmath>

namespace pcolonist {

void PhysicsSystem::update(Registry& registry, float deltaTime) const {
    registry.each<Transform, RigidBody, BoxCollider>(
        [this, deltaTime, &registry](Entity entity, Transform& transform, RigidBody& body, const BoxCollider& collider) {
            if (body.mass <= 0.0F || collider.isStatic) {
                return;
            }
            if (body.useGravity) {
                body.velocity += gravity_ * deltaTime;
            }
            transform.position += body.velocity * deltaTime;

            const float lowestPoint = transform.position.y - collider.halfExtents.y * transform.scale.y;
            if (lowestPoint < 0.0F) {
                transform.position.y -= lowestPoint;
                body.velocity.y = 0.0F;
                body.grounded = true;
            } else {
                body.grounded = false;
            }

            registry.each<Transform, BoxCollider>(
                [entity, &transform, &body, &collider](Entity other, const Transform& obstacle, const BoxCollider& otherCollider) {
                    if (other == entity || !otherCollider.isStatic) {
                        return;
                    }
                    const glm::vec3 half = collider.halfExtents * transform.scale;
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
