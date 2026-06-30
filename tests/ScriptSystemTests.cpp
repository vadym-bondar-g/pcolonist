#include "pcolonist/assets/AssetSystem.hpp"
#include "pcolonist/assets/ResourceManager.hpp"
#include "pcolonist/core/JobSystem.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"
#include "pcolonist/scripting/ScriptSystem.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path()
              / ("pcolonist-script-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const {
        return path_;
    }

private:
    std::filesystem::path path_;
};

template <typename Function>
void requireThrows(Function&& function, std::string_view message) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return;
    }
    throw std::runtime_error(std::string(message));
}

void executeScript(const std::filesystem::path& root, std::string_view script) {
    std::filesystem::create_directories(root / "scripts");
    std::ofstream(root / "scripts" / "test.script") << script;

    const pcolonist::AssetSystem assets(root);
    pcolonist::Registry registry;
    pcolonist::PhysicsSystem physics;
    pcolonist::ResourceManager resources;
    pcolonist::JobSystem jobs;
    pcolonist::ScriptSystem scripts;
    static_cast<void>(scripts.execute(assets, "scripts/test.script", registry, physics, resources, jobs));
}

} // namespace

void runScriptSystemTests() {
    TemporaryDirectory temporary;
    const std::filesystem::path root = temporary.path() / "assets";

    executeScript(root, "spawn_collider 0 1 0 1 1 1\n");

    requireThrows(
        [&root] {
            executeScript(root, "spawn_collider 0 1 0 -1 1 1\n");
        },
        "ScriptSystem must reject negative collider half extents");

    requireThrows(
        [&root] {
            executeScript(root, "spawn_body 0 1 0 nan 0 0\n");
        },
        "ScriptSystem must reject non-finite body velocity");

    requireThrows(
        [&root] {
            executeScript(root, "model rock models/rock.obj\nspawn_model rock 0 1 0 -1 1 1 1 1 1\n");
        },
        "ScriptSystem must reject negative model scale before loading models");
}
