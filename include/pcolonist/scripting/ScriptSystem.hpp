#pragma once

#include "pcolonist/ecs/Registry.hpp"

#include <filesystem>
#include <string_view>
#include <vector>

namespace pcolonist {

class AssetSystem;
class JobSystem;
class PhysicsSystem;
class Registry;
class ResourceManager;

class ScriptSystem {
public:
    [[nodiscard]] bool frameCounterVisible() const;

    std::vector<Entity> execute(
        const AssetSystem& assets,
        const std::filesystem::path& script,
        Registry& registry,
        PhysicsSystem& physics,
        ResourceManager& resources,
        JobSystem& jobs);

private:
    bool frameCounterVisible_ = false;
};

} // namespace pcolonist
