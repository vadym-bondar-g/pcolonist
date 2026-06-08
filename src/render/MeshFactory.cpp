#include "pcolonist/render/MeshFactory.hpp"

#include <glm/geometric.hpp>

#include <array>

namespace pcolonist {

Mesh MeshFactory::cube(glm::vec3 color) {
    constexpr std::array<glm::vec3, 8> positions = {
        glm::vec3{-0.5F, -0.5F, -0.5F}, glm::vec3{0.5F, -0.5F, -0.5F},
        glm::vec3{0.5F, 0.5F, -0.5F}, glm::vec3{-0.5F, 0.5F, -0.5F},
        glm::vec3{-0.5F, -0.5F, 0.5F}, glm::vec3{0.5F, -0.5F, 0.5F},
        glm::vec3{0.5F, 0.5F, 0.5F}, glm::vec3{-0.5F, 0.5F, 0.5F},
    };
    constexpr std::array<std::uint32_t, 36> indices = {
        0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
        0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5,
        0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2,
    };

    Mesh mesh;
    mesh.indices.assign(indices.begin(), indices.end());
    for (const glm::vec3& position : positions) {
        mesh.vertices.push_back({position, color, glm::normalize(position)});
    }
    return mesh;
}

Mesh MeshFactory::plane(float size, glm::vec3 color) {
    const float half = size * 0.5F;
    return {
        {
            {{-half, 0.0F, -half}, color, {0.0F, 1.0F, 0.0F}},
            {{half, 0.0F, -half}, color, {0.0F, 1.0F, 0.0F}},
            {{half, 0.0F, half}, color, {0.0F, 1.0F, 0.0F}},
            {{-half, 0.0F, half}, color, {0.0F, 1.0F, 0.0F}},
        },
        {0, 1, 2, 0, 2, 3},
    };
}

} // namespace pcolonist
