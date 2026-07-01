#pragma once

#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/gameplay/Inventory.hpp"

#include <glm/vec3.hpp>

#include <string_view>

namespace pcolonist {

struct HarvestYield {
    int wood = 0;
    int stone = 0;
    int fiber = 0;
    int metal = 0;

    [[nodiscard]] bool any() const {
        return wood > 0 || stone > 0 || fiber > 0 || metal > 0;
    }
};

struct HarvestRequest {
    glm::vec3 origin{0.0F};
    glm::vec3 direction{0.0F, 0.0F, -1.0F};
    float reach = 4.5F;
    Tool tool = Tool::Empty;
};

struct HarvestResult {
    bool hit = false;
    bool harvested = false;
    bool depleted = false;
    Entity target = nullEntity;
    float distance = 0.0F;
    HarvestYield yield;
    std::string_view message;
};

class ResourceHarvestingSystem {
public:
    [[nodiscard]] HarvestResult harvest(Registry& registry, const HarvestRequest& request) const;

private:
    [[nodiscard]] bool canHarvest(ResourceKind kind, Tool tool) const;
    [[nodiscard]] int damageFor(ResourceKind kind, Tool tool) const;
    [[nodiscard]] HarvestYield yieldFor(const ResourceNode& resource) const;
};

} // namespace pcolonist
