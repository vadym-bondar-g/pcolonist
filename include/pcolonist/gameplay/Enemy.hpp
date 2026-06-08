#pragma once

#include "pcolonist/ecs/Registry.hpp"

#include <glm/vec3.hpp>

#include <memory>

namespace pcolonist {

class Mesh;
class Registry;

struct EnemyComponent {
    float speed = 1.5F;
    float detectionRadius = 14.0F;
};

class Enemy {
public:
    static Entity create(Registry& registry, glm::vec3 position, const std::shared_ptr<Mesh>& mesh);
};

class EnemySystem {
public:
    void update(Registry& registry, Entity target, float deltaTime) const;
};

} // namespace pcolonist
