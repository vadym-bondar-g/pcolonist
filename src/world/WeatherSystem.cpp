#include "pcolonist/world/WeatherSystem.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace pcolonist {

namespace {
constexpr float dayDuration = 180.0F;
constexpr float pi = 3.14159265358979323846F;
}

void WeatherSystem::update(float deltaTime) {
    time_ += static_cast<double>(deltaTime);
    if (automaticWeather_) {
        const int phase = static_cast<int>(time_ / 60.0F) % 3;
        weather_ = phase == 0 ? WeatherType::Clear : phase == 1 ? WeatherType::Cloudy : WeatherType::Storm;
    }
}

void WeatherSystem::setWeather(WeatherType weather) {
    weather_ = weather;
    automaticWeather_ = false;
}

void WeatherSystem::setDayProgress(float progress) {
    const double day = std::floor(time_ / dayDuration);
    time_ = (day + static_cast<double>(glm::clamp(progress, 0.0F, 0.9999F))) * dayDuration;
}

glm::vec3 WeatherSystem::skyColor() const {
    const float light = daylight();
    const glm::vec3 clear = glm::mix(glm::vec3{0.003F, 0.008F, 0.035F}, glm::vec3{0.22F, 0.52F, 0.92F}, light);
    if (weather_ == WeatherType::Storm) {
        return clear * 0.3F;
    }
    if (weather_ == WeatherType::Cloudy) {
        return clear * 0.65F;
    }
    return clear;
}

glm::vec3 WeatherSystem::fogColor() const {
    const float light = daylight();
    const glm::vec3 clearHaze = glm::mix(glm::vec3{0.035F, 0.055F, 0.085F}, glm::vec3{0.46F, 0.62F, 0.78F}, light);
    if (weather_ == WeatherType::Storm) {
        return glm::mix(glm::vec3{0.10F, 0.12F, 0.14F}, glm::vec3{0.24F, 0.27F, 0.30F}, light);
    }
    if (weather_ == WeatherType::Cloudy) {
        return glm::mix(clearHaze, glm::vec3{0.43F, 0.49F, 0.54F}, 0.5F);
    }
    return clearHaze;
}

glm::vec3 WeatherSystem::horizonColor() const {
    const float twilight = 1.0F - std::abs(daylight() * 2.0F - 1.0F);
    const glm::vec3 mist = glm::mix(skyColor() * 0.68F, fogColor(), 0.58F + cloudiness() * 0.24F);
    const glm::vec3 warmBand = glm::mix(glm::vec3{1.0F, 0.34F, 0.08F}, glm::vec3{0.92F, 0.22F, 0.20F}, cloudiness());
    return glm::mix(mist, warmBand, twilight * (0.62F - cloudiness() * 0.18F));
}

glm::vec3 WeatherSystem::sunDirection() const {
    const float angle = dayProgress() * 2.0F * pi - pi * 0.5F;
    return glm::normalize(glm::vec3{
        std::cos(angle),
        std::sin(angle),
        std::cos(angle + 0.35F) * 0.32F,
    });
}

glm::vec3 WeatherSystem::sunColor() const {
    const glm::vec3 warm{1.0F, 0.55F, 0.28F};
    const glm::vec3 noon{1.0F, 0.94F, 0.78F};
    return glm::mix(warm, noon, daylight()) * (1.0F - cloudiness() * 0.55F);
}

glm::vec3 WeatherSystem::moonDirection() const {
    return -sunDirection();
}

glm::vec3 WeatherSystem::moonColor() const {
    return glm::vec3{0.38F, 0.52F, 0.82F} * nightFactor() * (1.0F - cloudiness() * 0.35F);
}

glm::vec3 WeatherSystem::ambientColor() const {
    return skyColor() * (0.14F + daylight() * 0.28F) + moonColor() * 0.12F;
}

float WeatherSystem::fogDensity() const {
    const float dawnDuskMist = (1.0F - std::abs(daylight() * 2.0F - 1.0F)) * 0.0018F;
    const float base = weather_ == WeatherType::Storm ? 0.0125F : weather_ == WeatherType::Cloudy ? 0.0075F : 0.0038F;
    return base + dawnDuskMist;
}

float WeatherSystem::cloudiness() const {
    return weather_ == WeatherType::Storm ? 0.9F : weather_ == WeatherType::Cloudy ? 0.55F : 0.15F;
}

float WeatherSystem::stormStrength() const {
    return weather_ == WeatherType::Storm ? 1.0F : weather_ == WeatherType::Cloudy ? 0.22F : 0.0F;
}

float WeatherSystem::hazeAmount() const {
    const float twilightMist = (1.0F - std::abs(daylight() * 2.0F - 1.0F)) * 0.25F;
    const float weatherHaze = weather_ == WeatherType::Storm ? 0.82F : weather_ == WeatherType::Cloudy ? 0.48F : 0.18F;
    return glm::clamp(weatherHaze + twilightMist, 0.0F, 1.0F);
}

float WeatherSystem::starVisibility() const {
    return nightFactor() * (1.0F - cloudiness() * 0.72F) * (1.0F - hazeAmount() * 0.42F);
}

float WeatherSystem::moonPhase() const {
    const float moonCycle = static_cast<float>(std::fmod(time_ / dayDuration + 0.35, 8.0) / 8.0);
    return std::sin(moonCycle * 2.0F * pi);
}

glm::vec2 WeatherSystem::cloudWind() const {
    if (weather_ == WeatherType::Storm) {
        return {0.028F, -0.013F};
    }
    if (weather_ == WeatherType::Cloudy) {
        return {0.015F, 0.004F};
    }
    return {0.006F, 0.002F};
}

float WeatherSystem::daylight() const {
    const float elevation = sunDirection().y;
    return glm::smoothstep(-0.12F, 0.18F, elevation);
}

float WeatherSystem::nightFactor() const {
    return 1.0F - daylight();
}

float WeatherSystem::dayProgress() const {
    return static_cast<float>(std::fmod(time_, static_cast<double>(dayDuration)) / dayDuration);
}

std::uint32_t WeatherSystem::dayNumber() const {
    return static_cast<std::uint32_t>(time_ / dayDuration) + 1U;
}

std::string_view WeatherSystem::weatherName() const {
    switch (weather_) {
    case WeatherType::Clear: return "CLEAR";
    case WeatherType::Cloudy: return "CLOUDY";
    case WeatherType::Storm: return "STORM";
    }
    return "UNKNOWN";
}

float WeatherSystem::time() const {
    return static_cast<float>(time_);
}

} // namespace pcolonist
