#pragma once

#include "pcolonist/render/Mesh.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

namespace pcolonist {

class Registry;
struct Transform;

struct StaticBox {
    glm::vec3 center{0.0F};
    glm::vec3 halfExtents{0.5F};
};

class StaticColliderGrid {
public:
    explicit StaticColliderGrid(float cellSize = 8.0F);

    void rebuild(Registry& registry);
    [[nodiscard]] std::vector<const StaticBox*> query(glm::vec3 center, glm::vec3 halfExtents) const;
    [[nodiscard]] std::size_t size() const;

private:
    struct Cell {
        int x = 0;
        int z = 0;

        bool operator==(const Cell&) const = default;
    };

    struct CellHash {
        std::size_t operator()(const Cell& cell) const noexcept;
    };

    [[nodiscard]] Cell cellAt(glm::vec2 point) const;

    float cellSize_ = 8.0F;
    std::vector<StaticBox> boxes_;
    std::unordered_map<Cell, std::vector<std::size_t>, CellHash> cells_;
};

struct TerrainHit {
    float height = 0.0F;
    glm::vec3 normal{0.0F, 1.0F, 0.0F};
};

class TerrainCollider {
public:
    TerrainCollider() = default;
    TerrainCollider(const Mesh& mesh, const Transform& transform, float cellSize = 4.0F);

    [[nodiscard]] std::optional<TerrainHit> surfaceAt(glm::vec2 point, float maximumHeight) const;
    [[nodiscard]] std::optional<TerrainHit> raycastDown(glm::vec3 origin, float distance) const;

private:
    struct Triangle {
        glm::vec3 a;
        glm::vec3 b;
        glm::vec3 c;
        glm::vec3 normal;
    };

    struct Cell {
        int x = 0;
        int z = 0;

        bool operator==(const Cell&) const = default;
    };

    struct CellHash {
        std::size_t operator()(const Cell& cell) const noexcept;
    };

    [[nodiscard]] Cell cellAt(glm::vec2 point) const;

    float cellSize_ = 4.0F;
    std::vector<Triangle> triangles_;
    std::unordered_map<Cell, std::vector<std::size_t>, CellHash> cells_;
};

struct RigidBody {
    glm::vec3 velocity{0.0F};
    float mass = 1.0F;
    bool useGravity = true;
    bool grounded = false;
    bool inWater = false;
    glm::vec3 groundNormal{0.0F, 1.0F, 0.0F};
};

struct BoxCollider {
    glm::vec3 halfExtents{0.5F};
    bool isStatic = false;
};

[[nodiscard]] glm::vec3 sanitizedColliderHalfExtents(const BoxCollider& collider, const Transform& transform);
[[nodiscard]] bool validPhysicsPoint(glm::vec3 point);

class PhysicsSystem {
public:
    void update(Registry& registry, float deltaTime) const;
    void setGravity(glm::vec3 gravity);
    void setTime(float time);
    void rebuildStaticIndex(Registry& registry);
    [[nodiscard]] const glm::vec3& gravity() const;
    [[nodiscard]] const StaticColliderGrid& staticColliders() const;

private:
    glm::vec3 gravity_{0.0F, -9.81F, 0.0F};
    float time_ = 0.0F;
    StaticColliderGrid staticColliders_;
};

} // namespace pcolonist
