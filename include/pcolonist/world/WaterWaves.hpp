#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace pcolonist {

struct WaterWaveSample {
    float height = 0.0F;
    float verticalVelocity = 0.0F;
    glm::vec3 normal{0.0F, 1.0F, 0.0F};
    glm::vec2 horizontalVelocity{0.0F};
};

class WaterWaves {
public:
    [[nodiscard]] static WaterWaveSample sample(glm::vec2 position, float time);
};

} // namespace pcolonist
