#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace pcolonist {

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec3 normal{0.0F, 1.0F, 0.0F};
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

} // namespace pcolonist
