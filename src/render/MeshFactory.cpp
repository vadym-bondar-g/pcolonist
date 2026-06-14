#include "pcolonist/render/MeshFactory.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
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
        {},
    };
}

Mesh MeshFactory::gridPlane(float size, std::size_t subdivisions, glm::vec3 color) {
    subdivisions = std::max<std::size_t>(subdivisions, 1);
    Mesh mesh;
    const std::size_t side = subdivisions + 1;
    mesh.vertices.reserve(side * side);
    mesh.indices.reserve(subdivisions * subdivisions * 6);
    const float half = size * 0.5F;
    for (std::size_t z = 0; z <= subdivisions; ++z) {
        for (std::size_t x = 0; x <= subdivisions; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(subdivisions);
            const float v = static_cast<float>(z) / static_cast<float>(subdivisions);
            mesh.vertices.push_back({
                {u * size - half, 0.0F, v * size - half},
                color,
                {0.0F, 1.0F, 0.0F},
                {u, v},
            });
        }
    }
    for (std::size_t z = 0; z < subdivisions; ++z) {
        for (std::size_t x = 0; x < subdivisions; ++x) {
            const std::uint32_t a = static_cast<std::uint32_t>(z * side + x);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = static_cast<std::uint32_t>((z + 1) * side + x);
            const std::uint32_t d = c + 1;
            mesh.indices.insert(mesh.indices.end(), {a, c, d, a, d, b});
        }
    }
    return mesh;
}

} // namespace pcolonist
