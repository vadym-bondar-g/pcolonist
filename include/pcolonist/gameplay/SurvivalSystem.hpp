#pragma once

#include "pcolonist/assets/AssetSystem.hpp"
#include "pcolonist/gameplay/PlayerVitals.hpp"
#include "pcolonist/world/WeatherSystem.hpp"

#include <glm/vec3.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace pcolonist {

struct SurvivalStatus {
    float health = 100.0F;
    float thirst = 100.0F;
    float hunger = 100.0F;
    float fatigue = 0.0F;
    float bodyTemperature = 37.0F;
    bool sick = false;
    bool dehydrated = false;
    bool starving = false;
    bool exhausted = false;
    bool hypothermia = false;
    bool hyperthermia = false;
    std::string biomeName = "Открытая местность";
    std::string warning = "Состояние стабильное";
};

struct LocationResourceYield {
    int wood = 0;
    int stone = 0;
    int fiber = 0;
    int metal = 0;

    [[nodiscard]] bool any() const {
        return wood > 0 || stone > 0 || fiber > 0 || metal > 0;
    }
};

struct SurvivalSnapshot {
    SurvivalStatus status;
    float sicknessRisk = 0.0F;
    float forageCooldown = 0.0F;
    float materialCooldown = 0.0F;
};

class SurvivalSystem {
public:
    void loadLocations(const AssetSystem& assets, const std::filesystem::path& relativePath = "scripts/island_locations_100.json");
    void update(glm::vec3 playerPosition, const WeatherSystem& weather, float deltaTime);

    void drinkCleanWater(int units = 1);
    void eatForagedFood(int nutrition = 24);
    void restNearFire(float seconds);

    [[nodiscard]] bool canForageFood() const;
    [[nodiscard]] bool forageFood();
    [[nodiscard]] bool canGatherMaterials() const;
    [[nodiscard]] LocationResourceYield gatherMaterials();
    [[nodiscard]] const SurvivalStatus& status() const;
    [[nodiscard]] const std::string& currentLocationName() const;
    [[nodiscard]] SurvivalSnapshot snapshot() const;
    void applySnapshot(const SurvivalSnapshot& snapshot);

private:
    struct Location {
        std::string name;
        std::string biome;
        glm::vec3 position{0.0F};
        std::vector<std::string> resources;
    };

    void updateCurrentLocation(glm::vec3 playerPosition);
    [[nodiscard]] float biomeThirstModifier() const;
    [[nodiscard]] float biomeHungerModifier() const;
    [[nodiscard]] float biomeFatigueModifier() const;
    [[nodiscard]] float biomeTargetTemperature(const WeatherSystem& weather) const;
    [[nodiscard]] bool sicknessExposure() const;
    void syncStatusFromVitals();
    void syncVitalsFromStatus(float sicknessRisk);
    void updateWarnings();

    std::vector<Location> locations_;
    PlayerVitals vitals_;
    SurvivalStatus status_;
    std::string currentLocationName_ = "Открытая местность";
    const Location* currentLocation_ = nullptr;
    float forageCooldown_ = 0.0F;
    float materialCooldown_ = 0.0F;
};

} // namespace pcolonist
