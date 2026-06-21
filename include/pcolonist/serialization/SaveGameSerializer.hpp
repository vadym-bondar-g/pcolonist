#pragma once

#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/gameplay/DiscoverySystem.hpp"
#include "pcolonist/gameplay/Inventory.hpp"
#include "pcolonist/gameplay/SurvivalSystem.hpp"
#include "pcolonist/world/WeatherSystem.hpp"

#include <glm/vec3.hpp>

#include <filesystem>
#include <optional>
#include <vector>

namespace pcolonist {

struct SavedResourceNode {
    Entity entity = nullEntity;
    int health = 0;
    int woodYield = 0;
};

struct SavedEnemy {
    Entity entity = nullEntity;
    glm::vec3 position{0.0F};
    glm::vec3 velocity{0.0F};
};

struct SaveGameSnapshot {
    glm::vec3 playerPosition{0.0F};
    glm::vec3 playerVelocity{0.0F};
    InventorySnapshot inventory;
    SurvivalSnapshot survival;
    DiscoverySnapshot discovery;
    WeatherSnapshot weather;
    std::vector<SavedResourceNode> resources;
    std::vector<SavedEnemy> enemies;
    bool fireLit = false;
    bool hasResourceState = false;
    bool hasEnemyState = false;
    std::size_t nextLandmark = 0;
};

class SaveGameSerializer {
public:
    static void save(const std::filesystem::path& path, const SaveGameSnapshot& snapshot);
    [[nodiscard]] static std::optional<SaveGameSnapshot> load(const std::filesystem::path& path);
};

} // namespace pcolonist
