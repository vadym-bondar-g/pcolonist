#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <string_view>

namespace pcolonist {

enum class WeatherType {
    Clear,
    Cloudy,
    Storm,
};

class WeatherSystem {
public:
    void update(float deltaTime);
    void setWeather(WeatherType weather);

    [[nodiscard]] glm::vec3 skyColor() const;
    [[nodiscard]] glm::vec3 horizonColor() const;
    [[nodiscard]] glm::vec3 fogColor() const;
    [[nodiscard]] glm::vec3 sunDirection() const;
    [[nodiscard]] glm::vec3 sunColor() const;
    [[nodiscard]] glm::vec3 moonDirection() const;
    [[nodiscard]] glm::vec3 moonColor() const;
    [[nodiscard]] glm::vec3 ambientColor() const;
    [[nodiscard]] float fogDensity() const;
    [[nodiscard]] float cloudiness() const;
    [[nodiscard]] float daylight() const;
    [[nodiscard]] float nightFactor() const;
    [[nodiscard]] float dayProgress() const;
    [[nodiscard]] std::uint32_t dayNumber() const;
    [[nodiscard]] std::string_view weatherName() const;
    [[nodiscard]] float time() const;

private:
    WeatherType weather_ = WeatherType::Clear;
    double time_ = 45.0;
};

} // namespace pcolonist
