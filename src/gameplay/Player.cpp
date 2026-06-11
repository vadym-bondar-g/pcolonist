#include "pcolonist/gameplay/Player.hpp"

#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"
#include "pcolonist/platform/Input.hpp"
#include "pcolonist/render/Camera.hpp"

#include <GLFW/glfw3.h>
#include <glm/geometric.hpp>

namespace pcolonist {

Entity Player::create(Registry& registry, glm::vec3 position) {
    entity_ = registry.create();
    registry.emplace<Transform>(entity_, Transform{position});
    registry.emplace<RigidBody>(entity_);
    registry.emplace<BoxCollider>(entity_, BoxCollider{{0.35F, 0.9F, 0.35F}, false});
    return entity_;
}

void Player::update(Registry& registry, const Input& input, Camera& camera, float deltaTime) {
    static_cast<void>(deltaTime);
    if (!registry.alive(entity_)) {
        return;
    }

    RigidBody& body = registry.get<RigidBody>(entity_);
    glm::vec3 movement{0.0F};
    movement += input.keyDown(GLFW_KEY_W) ? camera.horizontalFront() : glm::vec3{};
    movement -= input.keyDown(GLFW_KEY_S) ? camera.horizontalFront() : glm::vec3{};
    movement += input.keyDown(GLFW_KEY_D) ? camera.horizontalRight() : glm::vec3{};
    movement -= input.keyDown(GLFW_KEY_A) ? camera.horizontalRight() : glm::vec3{};
    if (glm::length(movement) > 0.0F) {
        movement = glm::normalize(movement) * (body.inWater ? swimSpeed_ : moveSpeed_);
    }
    body.velocity.x = movement.x;
    body.velocity.z = movement.z;
    if (body.inWater) {
        body.velocity.y = input.keyDown(GLFW_KEY_SPACE)
            ? swimVerticalSpeed_
            : input.keyDown(GLFW_KEY_LEFT_SHIFT) ? -swimVerticalSpeed_ : body.velocity.y;
    } else if (input.keyDown(GLFW_KEY_SPACE) && body.grounded) {
        body.velocity.y = jumpSpeed_;
        body.grounded = false;
    }
    syncCamera(registry, camera);
}

void Player::syncCamera(Registry& registry, Camera& camera) const {
    if (registry.alive(entity_)) {
        camera.setPosition(registry.get<Transform>(entity_).position + glm::vec3{0.0F, 0.7F, 0.0F});
    }
}

Entity Player::entity() const {
    return entity_;
}

} // namespace pcolonist
