#pragma once

#include "pcolonist/render/Mesh.hpp"

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

struct WaterSurface {};

} // namespace pcolonist
