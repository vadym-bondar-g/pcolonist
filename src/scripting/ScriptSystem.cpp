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
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace pcolonist {

void ScriptSystem::execute(
    const AssetSystem& assets,
    const std::filesystem::path& script,
    Registry& registry,
    PhysicsSystem& physics,
    ResourceManager& resources,
    JobSystem& jobs) const {
    struct ModelEntry {
        std::string path;
        std::future<Mesh> pending;
        std::shared_ptr<Mesh> loaded;
    };

    std::istringstream input(assets.readText(script));
    std::unordered_map<std::string, ModelEntry> models;
    const auto resolveModel = [&resources](ModelEntry& model) {
        if (!model.loaded) {
            Mesh mesh = model.pending.get();
            model.loaded = resources.load<Mesh>(model.path, [mesh = std::move(mesh)]() mutable {
                return std::move(mesh);
            });
        }
        return model.loaded;
    };

    std::string line;
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
            models.emplace(id, ModelEntry{path, jobs.submit([&assets, path] {
                return MeshLoader::load(assets, path);
            }), {}});
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
            auto iterator = models.find(id);
            if (iterator == models.end()) {
                throw std::runtime_error("Script model id is not loaded: " + id);
            }
            const std::shared_ptr<Mesh> model = resolveModel(iterator->second);
            const Entity entity = registry.create();
            registry.emplace<Transform>(entity, transform);
            registry.emplace<MeshRenderer>(entity, model);
            registry.emplace<BoxCollider>(entity, collider);
            continue;
        }

        throw std::runtime_error("Unknown script command: " + command);
    }

    for (auto& [id, model] : models) {
        static_cast<void>(id);
        resolveModel(model);
    }
}

} // namespace pcolonist
