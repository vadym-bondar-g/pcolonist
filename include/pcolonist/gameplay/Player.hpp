#pragma once

#include "pcolonist/ecs/Registry.hpp"

#include <glm/vec3.hpp>

namespace pcolonist {

class Camera;
class Input;
class Registry;

class Player {
public:
    Entity create(Registry& registry, glm::vec3 position);
    void update(Registry& registry, const Input& input, Camera& camera, float deltaTime);
    void syncCamera(Registry& registry, Camera& camera) const;
    [[nodiscard]] Entity entity() const;

private:
    Entity entity_ = nullEntity;
    float moveSpeed_ = 6.0F;
    float jumpSpeed_ = 6.0F;
    float swimSpeed_ = 3.5F;
    float swimVerticalSpeed_ = 3.0F;
};

} // namespace pcolonist
