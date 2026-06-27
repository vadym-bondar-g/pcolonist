#include "pcolonist/scripting/ScriptSystem.hpp"

#include "pcolonist/assets/AssetSystem.hpp"
#include "pcolonist/assets/MeshLoader.hpp"
#include "pcolonist/assets/ResourceManager.hpp"
#include "pcolonist/core/JobSystem.hpp"
#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"

#include <sstream>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace pcolonist {

bool ScriptSystem::frameCounterVisible() const {
    return frameCounterVisible_;
}

std::vector<Entity> ScriptSystem::execute(
    const AssetSystem& assets,
    const std::filesystem::path& script,
    Registry& registry,
    PhysicsSystem& physics,
    ResourceManager& resources,
    JobSystem& jobs) {
    struct ModelEntry {
        std::string path;
        std::optional<std::future<Mesh>> pending;
        std::shared_ptr<Mesh> loaded;
    };

    std::istringstream input(assets.readText(script));
    std::unordered_map<std::string, ModelEntry> models;
    const auto resolveModel = [&assets, &jobs, &resources](ModelEntry& model) {
        if (!model.loaded) {
            model.loaded = resources.find<Mesh>(model.path);
        }
        if (!model.loaded && !model.pending) {
            model.pending = jobs.submit([&assets, path = model.path] {
                return MeshLoader::load(assets, path);
            });
        }
        if (!model.loaded) {
            Mesh mesh = model.pending->get();
            model.loaded = resources.load<Mesh>(model.path, [mesh = std::move(mesh)]() mutable {
                return std::move(mesh);
            });
        }
        return model.loaded;
    };

    std::string line;
    std::vector<Entity> spawned;
    while (std::getline(input, line)) {
        std::istringstream row(line);
        std::string command;
        row >> command;
        if (command.empty() || command.starts_with('#')) {
            continue;
        }

        if (command == "gravity") {
            glm::vec3 gravity;
            if (!(row >> gravity.x >> gravity.y >> gravity.z)) {
                throw std::runtime_error("Script gravity expects x y z");
            }
            physics.setGravity(gravity);
            continue;
        }

        if (command == "frame_counter") {
            std::string value;
            if (!(row >> value) || (value != "on" && value != "off")) {
                throw std::runtime_error("Script frame_counter expects on or off");
            }
            frameCounterVisible_ = value == "on";
            continue;
        }

        if (command == "spawn_body") {
            Transform transform;
            RigidBody body;
            if (!(row >> transform.position.x >> transform.position.y >> transform.position.z
                  >> body.velocity.x >> body.velocity.y >> body.velocity.z)) {
                throw std::runtime_error("Script spawn_body expects position and velocity");
            }
            const Entity entity = registry.create();
            registry.emplace<Transform>(entity, transform);
            registry.emplace<RigidBody>(entity, body);
            registry.emplace<BoxCollider>(entity);
            spawned.push_back(entity);
            continue;
        }

        if (command == "spawn_collider") {
            Transform transform;
            BoxCollider collider;
            collider.isStatic = true;
            if (!(row >> transform.position.x >> transform.position.y >> transform.position.z
                  >> collider.halfExtents.x >> collider.halfExtents.y >> collider.halfExtents.z)) {
                throw std::runtime_error("Script spawn_collider expects position and half extents");
            }
            const Entity entity = registry.create();
            registry.emplace<Transform>(entity, transform);
            registry.emplace<BoxCollider>(entity, collider);
            spawned.push_back(entity);
            continue;
        }

        if (command == "model") {
            std::string id;
            std::string path;
            if (!(row >> id >> path)) {
                throw std::runtime_error("Script model expects id and asset path");
            }
            if (models.contains(id)) {
                throw std::runtime_error("Script model id is already declared: " + id);
            }
            models.emplace(id, ModelEntry{path, std::nullopt, {}});
            continue;
        }

        if (command == "spawn_model") {
            std::string id;
            Transform transform;
            BoxCollider collider;
            collider.isStatic = true;
            if (!(row >> id
                  >> transform.position.x >> transform.position.y >> transform.position.z
                  >> transform.scale.x >> transform.scale.y >> transform.scale.z
                  >> collider.halfExtents.x >> collider.halfExtents.y >> collider.halfExtents.z)) {
                throw std::runtime_error("Script spawn_model expects id, position, scale and collider");
            }
            row >> transform.rotation.y;
            auto iterator = models.find(id);
            if (iterator == models.end()) {
                throw std::runtime_error("Script model id is not loaded: " + id);
            }
            const std::shared_ptr<Mesh> model = resolveModel(iterator->second);
            const Entity entity = registry.create();
            registry.emplace<Transform>(entity, transform);
            registry.emplace<MeshRenderer>(entity, model);
            registry.emplace<BoxCollider>(entity, collider);
            if (id == "tree" || id == "oak") {
                registry.emplace<ResourceNode>(entity);
            }
            spawned.push_back(entity);
            continue;
        }

        if (command == "spawn_decor") {
            std::string id;
            Transform transform;
            if (!(row >> id
                  >> transform.position.x >> transform.position.y >> transform.position.z
                  >> transform.scale.x >> transform.scale.y >> transform.scale.z)) {
                throw std::runtime_error("Script spawn_decor expects id, position and scale");
            }
            row >> transform.rotation.y;
            auto iterator = models.find(id);
            if (iterator == models.end()) {
                throw std::runtime_error("Script model id is not loaded: " + id);
            }
            const Entity entity = registry.create();
            registry.emplace<Transform>(entity, transform);
            registry.emplace<MeshRenderer>(entity, resolveModel(iterator->second));
            spawned.push_back(entity);
            continue;
        }

        throw std::runtime_error("Unknown script command: " + command);
    }

    return spawned;
}

} // namespace pcolonist
