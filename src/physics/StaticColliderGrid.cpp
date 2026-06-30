#include "pcolonist/physics/PhysicsSystem.hpp"

#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/ecs/Registry.hpp"

#include <glm/common.hpp>

#include <algorithm>
#include <cmath>

namespace pcolonist {

StaticColliderGrid::StaticColliderGrid(float cellSize)
    : cellSize_(std::max(cellSize, 0.1F)) {}

void StaticColliderGrid::rebuild(Registry& registry) {
    boxes_.clear();
    cells_.clear();
    registry.each<Transform, BoxCollider>(
        [this](Entity, const Transform& transform, const BoxCollider& collider) {
            if (collider.isStatic && validPhysicsPoint(transform.position)) {
                const glm::vec3 halfExtents = sanitizedColliderHalfExtents(collider, transform);
                if (halfExtents != glm::vec3{}) {
                    boxes_.push_back({transform.position, halfExtents});
                }
            }
        });

    for (std::size_t index = 0; index < boxes_.size(); ++index) {
        const StaticBox& box = boxes_[index];
        const Cell minimum = cellAt({box.center.x - box.halfExtents.x, box.center.z - box.halfExtents.z});
        const Cell maximum = cellAt({box.center.x + box.halfExtents.x, box.center.z + box.halfExtents.z});
        for (int x = minimum.x; x <= maximum.x; ++x) {
            for (int z = minimum.z; z <= maximum.z; ++z) {
                cells_[{x, z}].push_back(index);
            }
        }
    }
}

std::vector<const StaticBox*> StaticColliderGrid::query(glm::vec3 center, glm::vec3 halfExtents) const {
    std::vector<const StaticBox*> result;
    if (!validPhysicsPoint(center) || !validPhysicsPoint(halfExtents)) {
        return result;
    }
    halfExtents = glm::abs(halfExtents);
    const Cell minimum = cellAt({center.x - halfExtents.x, center.z - halfExtents.z});
    const Cell maximum = cellAt({center.x + halfExtents.x, center.z + halfExtents.z});
    std::vector<bool> visited(boxes_.size(), false);
    for (int x = minimum.x; x <= maximum.x; ++x) {
        for (int z = minimum.z; z <= maximum.z; ++z) {
            const auto cell = cells_.find({x, z});
            if (cell == cells_.end()) {
                continue;
            }
            for (const std::size_t index : cell->second) {
                if (!visited[index]) {
                    visited[index] = true;
                    result.push_back(&boxes_[index]);
                }
            }
        }
    }
    return result;
}

std::size_t StaticColliderGrid::size() const {
    return boxes_.size();
}

std::size_t StaticColliderGrid::CellHash::operator()(const Cell& cell) const noexcept {
    const auto x = static_cast<std::size_t>(static_cast<unsigned int>(cell.x));
    const auto z = static_cast<std::size_t>(static_cast<unsigned int>(cell.z));
    return x * 0x9e3779b1U ^ z;
}

StaticColliderGrid::Cell StaticColliderGrid::cellAt(glm::vec2 point) const {
    return {
        static_cast<int>(std::floor(point.x / cellSize_)),
        static_cast<int>(std::floor(point.y / cellSize_)),
    };
}

} // namespace pcolonist
