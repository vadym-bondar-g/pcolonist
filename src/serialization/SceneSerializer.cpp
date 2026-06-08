#include "pcolonist/serialization/SceneSerializer.hpp"

#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"

#include <sstream>
#include <stdexcept>
#include <string>

namespace pcolonist {

std::string SceneSerializer::serialize(Registry& registry) {
    std::ostringstream output;
    output << "pcolonist_scene 1\n";
    registry.each<Transform>([&output, &registry](Entity entity, const Transform& transform) {
        output << "entity " << transform.position.x << ' ' << transform.position.y << ' ' << transform.position.z
               << ' ' << transform.rotation.x << ' ' << transform.rotation.y << ' ' << transform.rotation.z
               << ' ' << transform.scale.x << ' ' << transform.scale.y << ' ' << transform.scale.z;
        if (registry.has<RigidBody>(entity)) {
            const RigidBody& body = registry.get<RigidBody>(entity);
            output << " body " << body.velocity.x << ' ' << body.velocity.y << ' ' << body.velocity.z;
        }
        output << '\n';
    });
    return output.str();
}

void SceneSerializer::deserialize(std::string_view data, Registry& registry) {
    std::istringstream input{std::string(data)};
    std::string line;
    std::getline(input, line);
    if (line != "pcolonist_scene 1") {
        throw std::runtime_error("Unsupported scene format");
    }

    while (std::getline(input, line)) {
        std::istringstream row(line);
        std::string command;
        row >> command;
        if (command.empty() || command.starts_with('#')) {
            continue;
        }
        if (command != "entity") {
            throw std::runtime_error("Invalid scene entity");
        }

        Transform transform;
        if (!(row >> transform.position.x >> transform.position.y >> transform.position.z
              >> transform.rotation.x >> transform.rotation.y >> transform.rotation.z
              >> transform.scale.x >> transform.scale.y >> transform.scale.z)) {
            throw std::runtime_error("Invalid scene transform");
        }

        const Entity entity = registry.create();
        registry.emplace<Transform>(entity, transform);

        std::string component;
        if (row >> component && component == "body") {
            RigidBody body;
            if (!(row >> body.velocity.x >> body.velocity.y >> body.velocity.z)) {
                throw std::runtime_error("Invalid scene rigid body");
            }
            registry.emplace<RigidBody>(entity, body);
            registry.emplace<BoxCollider>(entity);
        }
    }
}

} // namespace pcolonist
