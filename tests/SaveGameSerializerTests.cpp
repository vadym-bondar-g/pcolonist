#include "pcolonist/serialization/SaveGameSerializer.hpp"

#include <filesystem>
#include <optional>
#include <stdexcept>

void runSaveGameSerializerTests() {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "pcolonist-save-serializer-test.save";

    pcolonist::SaveGameSnapshot snapshot;
    snapshot.playerPosition = {1.0F, 2.0F, 3.0F};
    snapshot.playerVelocity = {0.25F, -0.5F, 1.0F};
    snapshot.inventory.selectedSlot = 2;
    snapshot.inventory.wood = 5;
    snapshot.inventory.stone = 7;
    snapshot.inventory.tools[1] = true;
    snapshot.survival.status.health = 72.0F;
    snapshot.survival.status.sick = true;
    snapshot.weather.weather = pcolonist::WeatherType::Storm;
    snapshot.weather.time = 123.0F;
    snapshot.discovery.discovered = {true, false, true};
    snapshot.discovery.clueClaimed = {false, true, false};
    snapshot.resources.push_back({42, 2, 9});
    snapshot.enemies.push_back({77, {4.0F, 5.0F, 6.0F}, {-1.0F, 0.0F, 1.0F}});
    snapshot.fireLit = true;
    snapshot.hasResourceState = true;
    snapshot.hasEnemyState = true;
    snapshot.nextLandmark = 3;

    pcolonist::SaveGameSerializer::save(path, snapshot);
    const std::optional<pcolonist::SaveGameSnapshot> loaded = pcolonist::SaveGameSerializer::load(path);
    std::filesystem::remove(path);

    if (!loaded || loaded->playerPosition != snapshot.playerPosition || loaded->playerVelocity != snapshot.playerVelocity) {
        throw std::runtime_error("SaveGameSerializer must preserve player transform state");
    }
    if (loaded->inventory.selectedSlot != 2 || loaded->inventory.wood != 5 || !loaded->inventory.tools[1]) {
        throw std::runtime_error("SaveGameSerializer must preserve inventory state");
    }
    if (loaded->weather.weather != pcolonist::WeatherType::Storm || loaded->weather.time != 123.0F) {
        throw std::runtime_error("SaveGameSerializer must preserve weather state");
    }
    if (!loaded->fireLit || loaded->nextLandmark != 3 || !loaded->hasResourceState || !loaded->hasEnemyState) {
        throw std::runtime_error("SaveGameSerializer must preserve flags");
    }
    if (loaded->resources.size() != 1 || loaded->resources.front().entity != 42 || loaded->resources.front().health != 2) {
        throw std::runtime_error("SaveGameSerializer must preserve resource node state");
    }
    if (loaded->enemies.size() != 1 || loaded->enemies.front().entity != 77
        || loaded->enemies.front().position != snapshot.enemies.front().position) {
        throw std::runtime_error("SaveGameSerializer must preserve enemy state");
    }
}
