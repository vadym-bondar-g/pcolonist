#include "pcolonist/physics/PhysicsSystem.hpp"

#include "pcolonist/ecs/Components.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

std::optional<float> triangleHeight(
    glm::vec2 point,
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& c) {
    const glm::vec2 ab{b.x - a.x, b.z - a.z};
    const glm::vec2 ac{c.x - a.x, c.z - a.z};
    const glm::vec2 ap{point.x - a.x, point.y - a.z};
    const float denominator = ab.x * ac.y - ac.x * ab.y;
    if (std::abs(denominator) < 0.00001F) {
        return std::nullopt;
    }
    const float u = (ap.x * ac.y - ac.x * ap.y) / denominator;
    const float v = (ab.x * ap.y - ap.x * ab.y) / denominator;
    if (u < -0.001F || v < -0.001F || u + v > 1.001F) {
        return std::nullopt;
    }
    return a.y + (b.y - a.y) * u + (c.y - a.y) * v;
}

} // namespace

namespace pcolonist {

TerrainCollider::TerrainCollider(const Mesh& mesh, const Transform& transform, float cellSize)
    : cellSize_(std::max(cellSize, 0.1F)) {
    triangles_.reserve(mesh.indices.size() / 3);
    const auto worldPosition = [&transform](const glm::vec3& local) {
        return transform.position + local * transform.scale;
    };
    for (std::size_t index = 0; index + 2 < mesh.indices.size(); index += 3) {
        Triangle triangle{
            worldPosition(mesh.vertices[mesh.indices[index]].position),
            worldPosition(mesh.vertices[mesh.indices[index + 1]].position),
            worldPosition(mesh.vertices[mesh.indices[index + 2]].position),
            {},
        };
        const glm::vec3 cross = glm::cross(triangle.b - triangle.a, triangle.c - triangle.a);
        if (glm::length(cross) < 0.00001F) {
            continue;
        }
        triangle.normal = glm::normalize(cross);
        if (triangle.normal.y < 0.0F) {
            triangle.normal = -triangle.normal;
        }
        const std::size_t triangleIndex = triangles_.size();
        triangles_.push_back(triangle);

        const float minimumX = std::min({triangle.a.x, triangle.b.x, triangle.c.x});
        const float maximumX = std::max({triangle.a.x, triangle.b.x, triangle.c.x});
        const float minimumZ = std::min({triangle.a.z, triangle.b.z, triangle.c.z});
        const float maximumZ = std::max({triangle.a.z, triangle.b.z, triangle.c.z});
        const Cell minimum = cellAt({minimumX, minimumZ});
        const Cell maximum = cellAt({maximumX, maximumZ});
        for (int x = minimum.x; x <= maximum.x; ++x) {
            for (int z = minimum.z; z <= maximum.z; ++z) {
                cells_[{x, z}].push_back(triangleIndex);
            }
        }
    }
}

std::optional<TerrainHit> TerrainCollider::surfaceAt(glm::vec2 point, float maximumHeight) const {
    const auto cell = cells_.find(cellAt(point));
    if (cell == cells_.end()) {
        return std::nullopt;
    }
    std::optional<TerrainHit> result;
    for (const std::size_t index : cell->second) {
        const Triangle& triangle = triangles_[index];
        if (const auto height = triangleHeight(point, triangle.a, triangle.b, triangle.c);
            height && *height <= maximumHeight && (!result || *height > result->height)) {
            result = TerrainHit{*height, triangle.normal};
        }
    }
    return result;
}

std::optional<TerrainHit> TerrainCollider::raycastDown(glm::vec3 origin, float distance) const {
    const auto hit = surfaceAt({origin.x, origin.z}, origin.y);
    return hit && origin.y - hit->height <= distance ? hit : std::nullopt;
}

std::size_t TerrainCollider::CellHash::operator()(const Cell& cell) const noexcept {
    const auto x = static_cast<std::size_t>(static_cast<unsigned int>(cell.x));
    const auto z = static_cast<std::size_t>(static_cast<unsigned int>(cell.z));
    return x * 0x9e3779b1U ^ z;
}

TerrainCollider::Cell TerrainCollider::cellAt(glm::vec2 point) const {
    return {
        static_cast<int>(std::floor(point.x / cellSize_)),
        static_cast<int>(std::floor(point.y / cellSize_)),
    };
}

} // namespace pcolonist
