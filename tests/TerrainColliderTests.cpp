#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"

#include <cmath>
#include <stdexcept>

void runTerrainColliderTests() {
    pcolonist::Mesh mesh{
        {
            {{0.0F, 0.0F, 0.0F}, {}},
            {{10.0F, 0.0F, 0.0F}, {}},
            {{0.0F, 10.0F, 10.0F}, {}},
        },
        {0, 1, 2},
        {},
    };
    const pcolonist::TerrainCollider collider(mesh, pcolonist::Transform{});
    const auto hit = collider.surfaceAt({2.0F, 2.0F}, 10.0F);
    if (!hit || std::abs(hit->height - 2.0F) > 0.001F || hit->normal.y <= 0.0F) {
        throw std::runtime_error("TerrainCollider must return indexed surface height and upward normal");
    }
    if (collider.surfaceAt({9.0F, 9.0F}, 10.0F)) {
        throw std::runtime_error("TerrainCollider must reject points outside triangles");
    }
    if (!collider.raycastDown({2.0F, 5.0F, 2.0F}, 3.1F)
        || collider.raycastDown({2.0F, 5.0F, 2.0F}, 2.9F)) {
        throw std::runtime_error("TerrainCollider raycast distance is incorrect");
    }

    pcolonist::Registry registry;
    const pcolonist::Entity terrain = registry.create();
    registry.emplace<pcolonist::TerrainCollider>(terrain, mesh, pcolonist::Transform{});
    const pcolonist::Entity bodyEntity = registry.create();
    registry.emplace<pcolonist::Transform>(bodyEntity, pcolonist::Transform{{2.0F, 8.0F, 2.0F}});
    registry.emplace<pcolonist::RigidBody>(
        bodyEntity,
        pcolonist::RigidBody{{0.0F, -100.0F, 0.0F}, 1.0F, false});
    registry.emplace<pcolonist::BoxCollider>(bodyEntity, pcolonist::BoxCollider{{0.5F, 0.5F, 0.5F}});
    pcolonist::PhysicsSystem{}.update(registry, 0.1F);
    const auto& transform = registry.get<pcolonist::Transform>(bodyEntity);
    const auto& body = registry.get<pcolonist::RigidBody>(bodyEntity);
    if (!body.grounded || transform.position.y < 2.49F) {
        throw std::runtime_error("Swept terrain collision must stop fast falling bodies");
    }

    registry.get<pcolonist::Transform>(bodyEntity).position = {2.0F, 2.7F, 2.0F};
    registry.get<pcolonist::RigidBody>(bodyEntity).velocity = {0.0F, 0.0F, 0.0F};
    registry.get<pcolonist::RigidBody>(bodyEntity).grounded = true;
    pcolonist::PhysicsSystem{}.update(registry, 0.016F);
    if (!registry.get<pcolonist::RigidBody>(bodyEntity).grounded
        || std::abs(registry.get<pcolonist::Transform>(bodyEntity).position.y - 2.875F) > 0.001F
        || registry.get<pcolonist::RigidBody>(bodyEntity).groundNormal.y >= 1.0F) {
        throw std::runtime_error("Ground snapping must retain contact while descending");
    }

    const pcolonist::Entity nearObstacle = registry.create();
    registry.emplace<pcolonist::Transform>(nearObstacle, pcolonist::Transform{{20.0F, 1.0F, 20.0F}});
    registry.emplace<pcolonist::BoxCollider>(
        nearObstacle,
        pcolonist::BoxCollider{{1.0F, 1.0F, 1.0F}, true});
    const pcolonist::Entity farObstacle = registry.create();
    registry.emplace<pcolonist::Transform>(farObstacle, pcolonist::Transform{{80.0F, 1.0F, 80.0F}});
    registry.emplace<pcolonist::BoxCollider>(
        farObstacle,
        pcolonist::BoxCollider{{1.0F, 1.0F, 1.0F}, true});
    pcolonist::PhysicsSystem indexedPhysics;
    indexedPhysics.rebuildStaticIndex(registry);
    const auto candidates = indexedPhysics.staticColliders().query({20.0F, 1.0F, 20.0F}, {0.5F, 0.5F, 0.5F});
    if (indexedPhysics.staticColliders().size() != 2 || candidates.size() != 1) {
        throw std::runtime_error("Static collider grid must return only nearby obstacles");
    }

    const pcolonist::Entity collidingBody = registry.create();
    registry.emplace<pcolonist::Transform>(collidingBody, pcolonist::Transform{{20.0F, 1.0F, 20.0F}});
    registry.emplace<pcolonist::RigidBody>(
        collidingBody,
        pcolonist::RigidBody{{0.0F, 0.0F, 0.0F}, 1.0F, false});
    registry.emplace<pcolonist::BoxCollider>(collidingBody, pcolonist::BoxCollider{{0.5F, 0.5F, 0.5F}});
    indexedPhysics.update(registry, 0.016F);
    if (registry.get<pcolonist::Transform>(collidingBody).position == glm::vec3{20.0F, 1.0F, 20.0F}) {
        throw std::runtime_error("Physics system must resolve collisions through the static grid");
    }

    registry.get<pcolonist::Transform>(collidingBody).position = {10.0F, 1.0F, 20.0F};
    registry.get<pcolonist::RigidBody>(collidingBody).velocity = {200.0F, 0.0F, 0.0F};
    indexedPhysics.update(registry, 0.1F);
    if (registry.get<pcolonist::Transform>(collidingBody).position.x >= 18.5F) {
        throw std::runtime_error("Swept static collision must stop fast bodies before obstacles");
    }
}
