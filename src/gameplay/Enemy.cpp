#include "pcolonist/gameplay/Enemy.hpp"

#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"

#include <glm/geometric.hpp>

#include <cmath>

namespace pcolonist {

Entity Enemy::create(Registry& registry, glm::vec3 position, const std::shared_ptr<Mesh>& mesh) {
    const Entity entity = registry.create();
    registry.emplace<Transform>(entity, Transform{position, {}, {0.8F, 1.6F, 0.8F}});
    registry.emplace<MeshRenderer>(entity, mesh);
    registry.emplace<RigidBody>(entity);
    registry.emplace<BoxCollider>(entity, BoxCollider{{0.4F, 0.8F, 0.4F}, false});
    registry.emplace<EnemyComponent>(entity);
    return entity;
}

void EnemySystem::update(Registry& registry, Entity target, float deltaTime) const {
    static_cast<void>(deltaTime);
    if (!registry.alive(target) || !registry.has<Transform>(target)) {
        return;
    }
    const glm::vec3 targetPosition = registry.get<Transform>(target).position;
    registry.each<Transform, RigidBody, EnemyComponent>(
        [&targetPosition](Entity, Transform& transform, RigidBody& body, const EnemyComponent& enemy) {
            glm::vec3 direction = targetPosition - transform.position;
            direction.y = 0.0F;
            const float distance = glm::length(direction);
            if (distance > 0.2F && distance < enemy.detectionRadius) {
                direction /= distance;
                body.velocity.x = direction.x * enemy.speed;
                body.velocity.z = direction.z * enemy.speed;
                transform.rotation.y = std::atan2(direction.x, direction.z);
            } else {
                body.velocity.x = 0.0F;
                body.velocity.z = 0.0F;
            }
        });
}

} // namespace pcolonist
