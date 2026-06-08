#include "pcolonist/animation/AnimationSystem.hpp"

#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/ecs/Registry.hpp"

#include <cmath>

namespace pcolonist {

void AnimationSystem::update(Registry& registry, float deltaTime) const {
    registry.each<Transform, Animation>(
        [deltaTime](Entity, Transform& transform, Animation& animation) {
            animation.elapsed += deltaTime;
            transform.rotation += animation.rotationSpeed * deltaTime;
            if (animation.bobAmplitude != 0.0F) {
                transform.position.y = animation.baseHeight
                    + std::sin(animation.elapsed * animation.bobFrequency) * animation.bobAmplitude;
            }
        });
}

} // namespace pcolonist
