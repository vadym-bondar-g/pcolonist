#pragma once

#include <glm/vec3.hpp>

namespace pcolonist {

class Registry;

struct RigidBody {
    glm::vec3 velocity{0.0F};
    float mass = 1.0F;
    bool useGravity = true;
    bool grounded = false;
};

struct BoxCollider {
    glm::vec3 halfExtents{0.5F};
    bool isStatic = false;
};

class PhysicsSystem {
public:
    void update(Registry& registry, float deltaTime) const;
    void setGravity(glm::vec3 gravity);
    [[nodiscard]] const glm::vec3& gravity() const;

private:
    glm::vec3 gravity_{0.0F, -9.81F, 0.0F};
};

} // namespace pcolonist
