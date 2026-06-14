#pragma once

#include "pcolonist/render/Mesh.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <memory>

namespace pcolonist {

struct Transform {
    glm::vec3 position{0.0F};
    glm::vec3 rotation{0.0F};
    glm::vec3 scale{1.0F};
};

struct MeshRenderer {
    std::shared_ptr<Mesh> mesh;
};

struct WaterSurface {
    glm::vec2 halfExtents{320.0F};
};
struct TerrainSurface {};
struct TerrainChunk {
    glm::vec2 center{0.0F};
    float radius = 0.0F;
};
struct LavaSurface {};

struct ResourceNode {
    int health = 3;
    int woodYield = 5;
};

} // namespace pcolonist
