#pragma once

#include <filesystem>
#include <string_view>

namespace pcolonist {

class AssetSystem;
class JobSystem;
class PhysicsSystem;
class Registry;
class ResourceManager;

class ScriptSystem {
public:
    void execute(
        const AssetSystem& assets,
        const std::filesystem::path& script,
        Registry& registry,
        PhysicsSystem& physics,
        ResourceManager& resources,
        JobSystem& jobs) const;
};

} // namespace pcolonist
