#include "pcolonist/world/WaterWaves.hpp"

#include <glm/geometric.hpp>

#include <array>
#include <cmath>

namespace pcolonist {

namespace {

struct Wave {
    glm::vec2 direction;
    float amplitude;
    float frequency;
    float speed;
};

constexpr std::array waves = {
    Wave{{0.940376F, 0.340136F}, 0.16F, 0.18F, 1.05F},
    Wave{{-0.28F, 0.96F}, 0.09F, 0.31F, 0.72F},
    Wave{{0.707107F, -0.707107F}, 0.045F, 0.57F, 1.42F},
    Wave{{-0.819232F, -0.573462F}, 0.022F, 0.93F, 1.86F},
    Wave{{0.196116F, -0.980581F}, 0.012F, 1.41F, 2.24F},
};

} // namespace

WaterWaveSample WaterWaves::sample(glm::vec2 position, float time) {
    WaterWaveSample sample;
    glm::vec2 gradient{0.0F};
    for (const Wave& wave : waves) {
        const float phase = glm::dot(wave.direction, position) * wave.frequency + time * wave.speed;
        const float sine = std::sin(phase);
        const float cosine = std::cos(phase);
        sample.height += wave.amplitude * sine;
        sample.verticalVelocity += wave.amplitude * wave.speed * cosine;
        gradient += wave.direction * (wave.amplitude * wave.frequency * cosine);
        sample.horizontalVelocity += wave.direction * (wave.amplitude * wave.speed * sine);
    }
    sample.normal = glm::normalize(glm::vec3{-gradient.x, 1.0F, -gradient.y});
    return sample;
}

} // namespace pcolonist
