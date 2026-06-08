#pragma once

#include <glm/vec3.hpp>

namespace pcolonist {

class Registry;

struct Animation {
    glm::vec3 rotationSpeed{0.0F, 1.0F, 0.0F};
    float bobAmplitude = 0.0F;
    float bobFrequency = 1.0F;
    float baseHeight = 0.0F;
    float elapsed = 0.0F;
};

class AnimationSystem {
public:
    void update(Registry& registry, float deltaTime) const;
};

} // namespace pcolonist
