#pragma once

#include "pcolonist/assets/AssetSystem.hpp"
#include "pcolonist/gameplay/Inventory.hpp"

#include <glm/vec3.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace pcolonist {

struct DiscoveryStatus {
    int discoveredLocations = 0;
    int totalLocations = 0;
    int storyClues = 0;
    int totalStoryClues = 0;
    int secretsFound = 0;
    int totalSecrets = 0;
    bool blocked = false;
    std::string currentLocation = "Неизведанная местность";
    std::string lastMessage = "Исследуйте остров";
    std::string clueMessage = "Следов пока нет";
};

struct DiscoverySnapshot {
    std::vector<bool> discovered;
    std::vector<bool> clueClaimed;
};

class DiscoverySystem {
public:
    void loadLocations(const AssetSystem& assets, const std::filesystem::path& relativePath = "scripts/island_locations_100.json");
    void update(glm::vec3 playerPosition, const Inventory& inventory);

    [[nodiscard]] const DiscoveryStatus& status() const;
    [[nodiscard]] bool hasDiscoveredCurrentLocation() const;
    [[nodiscard]] DiscoverySnapshot snapshot() const;
    void applySnapshot(const DiscoverySnapshot& snapshot);

private:
    struct Location {
        std::string name;
        std::string biome;
        glm::vec3 position{0.0F};
        std::vector<std::string> storyClues;
        std::vector<std::string> requiredTools;
        bool discovered = false;
        bool clueClaimed = false;
    };

    [[nodiscard]] bool hasRequiredTools(const Location& location, const Inventory& inventory) const;
    [[nodiscard]] std::string missingToolLabel(const Location& location, const Inventory& inventory) const;
    void recount();

    std::vector<Location> locations_;
    DiscoveryStatus status_;
    const Location* currentLocation_ = nullptr;
};

} // namespace pcolonist
