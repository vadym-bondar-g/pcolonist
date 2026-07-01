#include "pcolonist/gameplay/SurvivalSystem.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

std::string extractString(std::string_view object, std::string_view key) {
    const std::string pattern = "\"" + std::string(key) + "\"";
    const std::size_t keyPosition = object.find(pattern);
    if (keyPosition == std::string_view::npos) {
        return {};
    }
    const std::size_t colon = object.find(':', keyPosition + pattern.size());
    const std::size_t firstQuote = object.find('"', colon + 1);
    if (colon == std::string_view::npos || firstQuote == std::string_view::npos) {
        return {};
    }
    std::size_t secondQuote = firstQuote + 1;
    while (secondQuote < object.size()) {
        if (object[secondQuote] == '"' && object[secondQuote - 1] != '\\') {
            return std::string(object.substr(firstQuote + 1, secondQuote - firstQuote - 1));
        }
        ++secondQuote;
    }
    return {};
}

std::vector<float> extractNumberArray(std::string_view object, std::string_view key) {
    const std::string pattern = "\"" + std::string(key) + "\"";
    const std::size_t keyPosition = object.find(pattern);
    if (keyPosition == std::string_view::npos) {
        return {};
    }
    const std::size_t open = object.find('[', keyPosition + pattern.size());
    const std::size_t close = object.find(']', open + 1);
    if (open == std::string_view::npos || close == std::string_view::npos) {
        return {};
    }
    std::vector<float> values;
    std::size_t cursor = open + 1;
    while (cursor < close) {
        while (cursor < close && (std::isspace(static_cast<unsigned char>(object[cursor])) || object[cursor] == ',')) {
            ++cursor;
        }
        if (cursor >= close) {
            break;
        }
        std::size_t end = cursor;
        while (end < close && object[end] != ',') {
            ++end;
        }
        values.push_back(std::stof(std::string(object.substr(cursor, end - cursor))));
        cursor = end + 1;
    }
    return values;
}

std::vector<std::string> extractStringArray(std::string_view object, std::string_view key) {
    const std::string pattern = "\"" + std::string(key) + "\"";
    const std::size_t keyPosition = object.find(pattern);
    if (keyPosition == std::string_view::npos) {
        return {};
    }
    const std::size_t open = object.find('[', keyPosition + pattern.size());
    const std::size_t close = object.find(']', open + 1);
    if (open == std::string_view::npos || close == std::string_view::npos) {
        return {};
    }
    std::vector<std::string> values;
    std::size_t cursor = open + 1;
    while (cursor < close) {
        const std::size_t firstQuote = object.find('"', cursor);
        if (firstQuote == std::string_view::npos || firstQuote >= close) {
            break;
        }
        const std::size_t secondQuote = object.find('"', firstQuote + 1);
        if (secondQuote == std::string_view::npos || secondQuote >= close) {
            break;
        }
        values.emplace_back(object.substr(firstQuote + 1, secondQuote - firstQuote - 1));
        cursor = secondQuote + 1;
    }
    return values;
}

bool containsAnyFoodWord(const std::string& value) {
    return value.find("яйц") != std::string::npos
        || value.find("гриб") != std::string::npos
        || value.find("фрукт") != std::string::npos
        || value.find("ягод") != std::string::npos
        || value.find("водорос") != std::string::npos
        || value.find("рыб") != std::string::npos
        || value.find("мед") != std::string::npos
        || value.find("съедоб") != std::string::npos;
}

pcolonist::LocationResourceYield yieldForResource(const std::string& value) {
    pcolonist::LocationResourceYield yield;
    if (value.find("древес") != std::string::npos
        || value.find("плавник") != std::string::npos
        || value.find("щеп") != std::string::npos) {
        yield.wood += 2;
    }
    if (value.find("крем") != std::string::npos
        || value.find("кам") != std::string::npos
        || value.find("гальк") != std::string::npos
        || value.find("базальт") != std::string::npos) {
        yield.stone += 2;
    }
    if (value.find("волок") != std::string::npos
        || value.find("лиан") != std::string::npos
        || value.find("трост") != std::string::npos) {
        yield.fiber += 2;
    }
    if (value.find("металл") != std::string::npos
        || value.find("желез") != std::string::npos
        || value.find("медь") != std::string::npos
        || value.find("руда") != std::string::npos
        || value.find("провол") != std::string::npos
        || value.find("скоба") != std::string::npos
        || value.find("крюк") != std::string::npos) {
        yield.metal += 1;
    }
    return yield;
}

} // namespace

namespace pcolonist {

void SurvivalSystem::loadLocations(const AssetSystem& assets, const std::filesystem::path& relativePath) {
    locations_.clear();

    std::string content;
    try {
        content = assets.readText(relativePath);
    } catch (const std::exception&) {
        return;
    }

    int depth = 0;
    std::size_t objectStart = std::string::npos;
    for (std::size_t index = 0; index < content.size(); ++index) {
        if (content[index] == '{') {
            if (depth == 0) {
                objectStart = index;
            }
            ++depth;
        } else if (content[index] == '}') {
            --depth;
            if (depth == 0 && objectStart != std::string::npos) {
                const std::string_view object{content.data() + objectStart, index - objectStart + 1};
                Location location;
                location.name = extractString(object, "name");
                location.biome = extractString(object, "biome");
                const auto position = extractNumberArray(object, "position");
                if (position.size() >= 3) {
                    location.position = {position[0], position[1], position[2]};
                }
                location.resources = extractStringArray(object, "resources");
                if (!location.name.empty() && !location.biome.empty()) {
                    locations_.push_back(std::move(location));
                }
                objectStart = std::string::npos;
            }
        }
    }
}

void SurvivalSystem::update(glm::vec3 playerPosition, const WeatherSystem& weather, float deltaTime) {
    updateCurrentLocation(playerPosition);
    forageCooldown_ = std::max(forageCooldown_ - deltaTime, 0.0F);
    materialCooldown_ = std::max(materialCooldown_ - deltaTime, 0.0F);

    vitals_.update(PlayerVitalsContext{
        deltaTime,
        biomeThirstModifier(),
        biomeHungerModifier(),
        biomeFatigueModifier(),
        biomeTargetTemperature(weather),
        weather.stormStrength(),
        sicknessExposure(),
    });
    syncStatusFromVitals();
    updateWarnings();
}

void SurvivalSystem::drinkCleanWater(int units) {
    vitals_.drinkCleanWater(units);
    syncStatusFromVitals();
    updateWarnings();
}

void SurvivalSystem::eatForagedFood(int nutrition) {
    vitals_.eat(static_cast<float>(std::max(nutrition, 0)));
    syncStatusFromVitals();
    updateWarnings();
}

void SurvivalSystem::restNearFire(float seconds) {
    vitals_.restNearHeat(seconds);
    syncStatusFromVitals();
    updateWarnings();
}

bool SurvivalSystem::canForageFood() const {
    if (currentLocation_ == nullptr || forageCooldown_ > 0.0F) {
        return false;
    }
    return std::any_of(currentLocation_->resources.begin(), currentLocation_->resources.end(), containsAnyFoodWord);
}

bool SurvivalSystem::forageFood() {
    if (!canForageFood()) {
        return false;
    }
    eatForagedFood(22);
    forageCooldown_ = 35.0F;
    return true;
}

bool SurvivalSystem::canGatherMaterials() const {
    if (currentLocation_ == nullptr || materialCooldown_ > 0.0F) {
        return false;
    }
    return std::any_of(currentLocation_->resources.begin(), currentLocation_->resources.end(), [](const std::string& resource) {
        return yieldForResource(resource).any();
    });
}

LocationResourceYield SurvivalSystem::gatherMaterials() {
    LocationResourceYield total;
    if (!canGatherMaterials()) {
        return total;
    }
    for (const std::string& resource : currentLocation_->resources) {
        const LocationResourceYield yield = yieldForResource(resource);
        total.wood += yield.wood;
        total.stone += yield.stone;
        total.fiber += yield.fiber;
        total.metal += yield.metal;
    }
    total.wood = std::min(total.wood, 4);
    total.stone = std::min(total.stone, 4);
    total.fiber = std::min(total.fiber, 4);
    total.metal = std::min(total.metal, 2);
    materialCooldown_ = 28.0F;
    return total;
}

const SurvivalStatus& SurvivalSystem::status() const {
    return status_;
}

const std::string& SurvivalSystem::currentLocationName() const {
    return currentLocationName_;
}

SurvivalSnapshot SurvivalSystem::snapshot() const {
    return {status_, vitals_.state().sicknessRisk, forageCooldown_, materialCooldown_};
}

void SurvivalSystem::applySnapshot(const SurvivalSnapshot& snapshot) {
    status_ = snapshot.status;
    syncVitalsFromStatus(snapshot.sicknessRisk);
    syncStatusFromVitals();
    forageCooldown_ = std::max(snapshot.forageCooldown, 0.0F);
    materialCooldown_ = std::max(snapshot.materialCooldown, 0.0F);
    currentLocationName_ = status_.biomeName;
    currentLocation_ = nullptr;
    updateWarnings();
}

void SurvivalSystem::updateCurrentLocation(glm::vec3 playerPosition) {
    const Location* nearest = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    for (const Location& location : locations_) {
        const float distance = glm::length(glm::vec2{playerPosition.x - location.position.x, playerPosition.z - location.position.z});
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearest = &location;
        }
    }

    if (nearest != nullptr && nearestDistance <= 42.0F) {
        currentLocation_ = nearest;
        currentLocationName_ = nearest->name;
        status_.biomeName = nearest->biome;
    } else {
        currentLocation_ = nullptr;
        currentLocationName_ = "Открытая местность";
        status_.biomeName = "Открытая местность";
    }
}

float SurvivalSystem::biomeThirstModifier() const {
    if (status_.biomeName == "Вулкан") {
        return 2.15F;
    }
    if (status_.biomeName == "Болота") {
        return 1.35F;
    }
    if (status_.biomeName == "Горы") {
        return 1.2F;
    }
    if (status_.biomeName == "Тропический лес") {
        return 0.9F;
    }
    return 1.0F;
}

float SurvivalSystem::biomeHungerModifier() const {
    if (status_.biomeName == "Горы" || status_.biomeName == "Пещеры") {
        return 1.28F;
    }
    if (status_.biomeName == "Болота") {
        return 1.18F;
    }
    return 1.0F;
}

float SurvivalSystem::biomeFatigueModifier() const {
    if (status_.biomeName == "Горы") {
        return 1.75F;
    }
    if (status_.biomeName == "Пещеры") {
        return 1.45F;
    }
    if (status_.biomeName == "Болота") {
        return 1.35F;
    }
    return 1.0F;
}

float SurvivalSystem::biomeTargetTemperature(const WeatherSystem& weather) const {
    float target = 37.0F;
    if (status_.biomeName == "Вулкан") {
        target = 39.3F;
    } else if (status_.biomeName == "Пещеры") {
        target = 35.7F;
    } else if (status_.biomeName == "Горы") {
        target = 35.9F;
    } else if (status_.biomeName == "Болота") {
        target = 36.4F;
    } else if (status_.biomeName == "Тропический лес") {
        target = 36.8F;
    }
    target -= weather.nightFactor() * 0.75F;
    target -= weather.stormStrength() * 0.55F;
    return target;
}

bool SurvivalSystem::sicknessExposure() const {
    return status_.biomeName == "Болота" || status_.biomeName == "Пещеры";
}

void SurvivalSystem::syncStatusFromVitals() {
    const PlayerVitalsState& vitals = vitals_.state();
    status_.health = vitals.health;
    status_.thirst = vitals.thirst;
    status_.hunger = vitals.hunger;
    status_.fatigue = vitals.fatigue;
    status_.bodyTemperature = vitals.bodyTemperature;
    status_.sick = vitals.sick;
    status_.dehydrated = vitals.dehydrated;
    status_.starving = vitals.starving;
    status_.exhausted = vitals.exhausted;
    status_.hypothermia = vitals.hypothermia;
    status_.hyperthermia = vitals.hyperthermia;
}

void SurvivalSystem::syncVitalsFromStatus(float sicknessRisk) {
    vitals_.applyState(PlayerVitalsState{
        status_.health,
        status_.thirst,
        status_.hunger,
        status_.fatigue,
        status_.bodyTemperature,
        sicknessRisk,
        status_.sick,
        status_.dehydrated,
        status_.starving,
        status_.exhausted,
        status_.hypothermia,
        status_.hyperthermia,
    });
}

void SurvivalSystem::updateWarnings() {
    if (status_.health < 25.0F) {
        status_.warning = "Критическое состояние";
    } else if (status_.dehydrated || status_.thirst < 18.0F) {
        status_.warning = "Нужна вода";
    } else if (status_.starving || status_.hunger < 18.0F) {
        status_.warning = "Нужна еда";
    } else if (status_.hypothermia) {
        status_.warning = "Переохлаждение";
    } else if (status_.hyperthermia) {
        status_.warning = "Перегрев";
    } else if (status_.exhausted || status_.fatigue > 82.0F) {
        status_.warning = "Нужно отдохнуть";
    } else if (status_.sick || vitals_.state().sicknessRisk > 65.0F) {
        status_.warning = "Риск болезни";
    } else if (canForageFood()) {
        status_.warning = "Рядом съедобные ресурсы";
    } else {
        status_.warning = "Состояние стабильное";
    }
}

} // namespace pcolonist
