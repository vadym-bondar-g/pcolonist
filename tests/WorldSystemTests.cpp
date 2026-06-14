#include "pcolonist/core/FixedTimestep.hpp"
#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"
#include "pcolonist/world/WaterWaves.hpp"
#include "pcolonist/world/WeatherSystem.hpp"

#include <glm/geometric.hpp>

#include <cmath>
#include <memory>
#include <stdexcept>

void runWorldSystemTests() {
    pcolonist::FixedTimestep fixed;
    int smallFrameSteps = 0;
    for (int frame = 0; frame < 60; ++frame) {
        fixed.advance(1.0F / 60.0F, [&smallFrameSteps](float) {
            ++smallFrameSteps;
        });
    }
    if (smallFrameSteps != 120) {
        throw std::runtime_error("Fixed timestep must produce stable physics steps");
    }

    pcolonist::WeatherSystem weather;
    const glm::vec3 sunrise = weather.sunDirection();
    weather.update(45.0F);
    const glm::vec3 noon = weather.sunDirection();
    if (noon.y <= sunrise.y || weather.daylight() < 0.9F) {
        throw std::runtime_error("Sun must rise and reach daylight during the day cycle");
    }
    weather.update(90.0F);
    if (weather.dayNumber() != 2U || weather.dayProgress() > 0.001F) {
        throw std::runtime_error("Weather system must advance to the next day");
    }
    weather.setDayProgress(0.5F);
    if (weather.daylight() < 0.99F || std::abs(weather.dayProgress() - 0.5F) > 0.001F) {
        throw std::runtime_error("Developer controls must be able to set noon");
    }
    weather.setWeather(pcolonist::WeatherType::Storm);
    weather.update(1.0F);
    if (weather.weatherName() != "STORM") {
        throw std::runtime_error("Manually selected weather must persist");
    }

    const pcolonist::WaterWaveSample first = pcolonist::WaterWaves::sample({4.0F, -3.0F}, 1.0F);
    const pcolonist::WaterWaveSample later = pcolonist::WaterWaves::sample({4.0F, -3.0F}, 3.0F);
    if (std::abs(first.height - later.height) < 0.001F
        || std::abs(glm::length(first.normal) - 1.0F) > 0.001F
        || glm::length(first.horizontalVelocity - later.horizontalVelocity) < 0.001F) {
        throw std::runtime_error("Water waves must move and provide normalized physical surface data");
    }

    pcolonist::Registry registry;
    const pcolonist::Entity water = registry.create();
    registry.emplace<pcolonist::Transform>(water, pcolonist::Transform{{0.0F, 0.0F, 0.0F}});
    registry.emplace<pcolonist::WaterSurface>(water, pcolonist::WaterSurface{{10.0F, 10.0F}});
    const pcolonist::Entity floating = registry.create();
    registry.emplace<pcolonist::Transform>(floating, pcolonist::Transform{{4.0F, 0.0F, -3.0F}});
    registry.emplace<pcolonist::RigidBody>(
        floating,
        pcolonist::RigidBody{{0.0F, 0.0F, 0.0F}, 1.0F, false});
    registry.emplace<pcolonist::BoxCollider>(floating, pcolonist::BoxCollider{{0.5F, 0.5F, 0.5F}});
    pcolonist::PhysicsSystem physics;
    physics.setTime(1.0F);
    physics.update(registry, 0.1F);
    const pcolonist::RigidBody& body = registry.get<pcolonist::RigidBody>(floating);
    if (!body.inWater || glm::length(body.velocity) < 0.001F) {
        throw std::runtime_error("Physical waves must affect floating bodies");
    }
}
#include "pcolonist/core/FixedTimestep.hpp"
