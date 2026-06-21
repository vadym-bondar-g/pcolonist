#include "pcolonist/serialization/SaveGameSerializer.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

std::string weatherToken(pcolonist::WeatherType weather) {
    switch (weather) {
    case pcolonist::WeatherType::Clear: return "clear";
    case pcolonist::WeatherType::Cloudy: return "cloudy";
    case pcolonist::WeatherType::Storm: return "storm";
    }
    return "clear";
}

pcolonist::WeatherType parseWeather(std::string_view token) {
    if (token == "cloudy") {
        return pcolonist::WeatherType::Cloudy;
    }
    if (token == "storm") {
        return pcolonist::WeatherType::Storm;
    }
    return pcolonist::WeatherType::Clear;
}

} // namespace

namespace pcolonist {

void SaveGameSerializer::save(const std::filesystem::path& path, const SaveGameSnapshot& snapshot) {
    const std::filesystem::path temporaryPath = path.string() + ".tmp";
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    {
        std::ofstream output(temporaryPath, std::ios::trunc);
        if (!output) {
            throw std::runtime_error("Cannot open save file for writing: " + temporaryPath.string());
        }

        output << "pcolonist_save 1\n";
        output << "player "
               << snapshot.playerPosition.x << ' ' << snapshot.playerPosition.y << ' ' << snapshot.playerPosition.z << ' '
               << snapshot.playerVelocity.x << ' ' << snapshot.playerVelocity.y << ' ' << snapshot.playerVelocity.z << '\n';
        output << "inventory " << snapshot.inventory.selectedSlot << ' ' << snapshot.inventory.wood << ' '
               << snapshot.inventory.stone << ' ' << snapshot.inventory.water << ' ' << snapshot.inventory.fiber << ' '
               << snapshot.inventory.metal;
        for (bool tool : snapshot.inventory.tools) {
            output << ' ' << (tool ? 1 : 0);
        }
        output << '\n';
        output << "survival "
               << snapshot.survival.status.health << ' ' << snapshot.survival.status.thirst << ' '
               << snapshot.survival.status.hunger << ' ' << snapshot.survival.status.fatigue << ' '
               << snapshot.survival.status.bodyTemperature << ' ' << (snapshot.survival.status.sick ? 1 : 0) << ' '
               << snapshot.survival.sicknessRisk << ' ' << snapshot.survival.forageCooldown << ' '
               << snapshot.survival.materialCooldown << '\n';
        output << "weather " << weatherToken(snapshot.weather.weather) << ' ' << snapshot.weather.time << '\n';
        output << "flags " << (snapshot.fireLit ? 1 : 0) << ' ' << snapshot.nextLandmark << '\n';
        output << "discovery " << snapshot.discovery.discovered.size() << '\n';
        for (std::size_t index = 0; index < snapshot.discovery.discovered.size(); ++index) {
            output << (snapshot.discovery.discovered[index] ? 1 : 0) << ' '
                   << (index < snapshot.discovery.clueClaimed.size() && snapshot.discovery.clueClaimed[index] ? 1 : 0)
                   << '\n';
        }
        output << "resources " << snapshot.resources.size() << '\n';
        for (const SavedResourceNode& resource : snapshot.resources) {
            output << resource.entity << ' ' << resource.health << ' ' << resource.woodYield << '\n';
        }
        output << "enemies " << snapshot.enemies.size() << '\n';
        for (const SavedEnemy& enemy : snapshot.enemies) {
            output << enemy.entity << ' '
                   << enemy.position.x << ' ' << enemy.position.y << ' ' << enemy.position.z << ' '
                   << enemy.velocity.x << ' ' << enemy.velocity.y << ' ' << enemy.velocity.z << '\n';
        }
    }

    std::error_code error;
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporaryPath, path, error);
    if (error) {
        throw std::runtime_error("Cannot replace save file: " + error.message());
    }
}

std::optional<SaveGameSnapshot> SaveGameSerializer::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        return std::nullopt;
    }

    std::string header;
    int version = 0;
    input >> header >> version;
    if (header != "pcolonist_save" || version != 1) {
        throw std::runtime_error("Unsupported save format: " + path.string());
    }

    SaveGameSnapshot snapshot;
    std::string command;
    while (input >> command) {
        if (command == "player") {
            input >> snapshot.playerPosition.x >> snapshot.playerPosition.y >> snapshot.playerPosition.z
                  >> snapshot.playerVelocity.x >> snapshot.playerVelocity.y >> snapshot.playerVelocity.z;
        } else if (command == "inventory") {
            input >> snapshot.inventory.selectedSlot >> snapshot.inventory.wood >> snapshot.inventory.stone
                  >> snapshot.inventory.water >> snapshot.inventory.fiber >> snapshot.inventory.metal;
            int unlocked = 0;
            for (bool& tool : snapshot.inventory.tools) {
                input >> unlocked;
                tool = unlocked != 0;
            }
        } else if (command == "survival") {
            int sick = 0;
            input >> snapshot.survival.status.health >> snapshot.survival.status.thirst
                  >> snapshot.survival.status.hunger >> snapshot.survival.status.fatigue
                  >> snapshot.survival.status.bodyTemperature >> sick >> snapshot.survival.sicknessRisk
                  >> snapshot.survival.forageCooldown >> snapshot.survival.materialCooldown;
            snapshot.survival.status.sick = sick != 0;
        } else if (command == "weather") {
            std::string token;
            input >> token >> snapshot.weather.time;
            snapshot.weather.weather = parseWeather(token);
        } else if (command == "flags") {
            int savedFireLit = 0;
            input >> savedFireLit >> snapshot.nextLandmark;
            snapshot.fireLit = savedFireLit != 0;
        } else if (command == "discovery") {
            std::size_t count = 0;
            input >> count;
            snapshot.discovery.discovered.assign(count, false);
            snapshot.discovery.clueClaimed.assign(count, false);
            for (std::size_t index = 0; index < count; ++index) {
                int discovered = 0;
                int clueClaimed = 0;
                input >> discovered >> clueClaimed;
                snapshot.discovery.discovered[index] = discovered != 0;
                snapshot.discovery.clueClaimed[index] = clueClaimed != 0;
            }
        } else if (command == "resources") {
            std::size_t count = 0;
            input >> count;
            snapshot.resources.clear();
            snapshot.resources.reserve(count);
            for (std::size_t index = 0; index < count; ++index) {
                SavedResourceNode resource;
                input >> resource.entity >> resource.health >> resource.woodYield;
                snapshot.resources.push_back(resource);
            }
            snapshot.hasResourceState = true;
        } else if (command == "enemies") {
            std::size_t count = 0;
            input >> count;
            snapshot.enemies.clear();
            snapshot.enemies.reserve(count);
            for (std::size_t index = 0; index < count; ++index) {
                SavedEnemy enemy;
                input >> enemy.entity
                      >> enemy.position.x >> enemy.position.y >> enemy.position.z
                      >> enemy.velocity.x >> enemy.velocity.y >> enemy.velocity.z;
                snapshot.enemies.push_back(enemy);
            }
            snapshot.hasEnemyState = true;
        } else {
            std::string ignored;
            std::getline(input, ignored);
        }
        if (!input) {
            throw std::runtime_error("Corrupt save file: " + path.string());
        }
    }

    return snapshot;
}

} // namespace pcolonist
