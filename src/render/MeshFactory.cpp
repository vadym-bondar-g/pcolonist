#include "pcolonist/render/MeshFactory.hpp"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace pcolonist {

Mesh MeshFactory::cube(glm::vec3 color) {
    constexpr std::size_t subdivisions = 5;
    constexpr float innerExtent = 0.42F;
    constexpr float bevelRadius = 0.08F;
    constexpr std::array<glm::vec3, 6> normals = {
        glm::vec3{1.0F, 0.0F, 0.0F}, glm::vec3{-1.0F, 0.0F, 0.0F},
        glm::vec3{0.0F, 1.0F, 0.0F}, glm::vec3{0.0F, -1.0F, 0.0F},
        glm::vec3{0.0F, 0.0F, 1.0F}, glm::vec3{0.0F, 0.0F, -1.0F},
    };
    constexpr std::array<glm::vec3, 6> horizontal = {
        glm::vec3{0.0F, 0.0F, -1.0F}, glm::vec3{0.0F, 0.0F, 1.0F},
        glm::vec3{1.0F, 0.0F, 0.0F}, glm::vec3{1.0F, 0.0F, 0.0F},
        glm::vec3{1.0F, 0.0F, 0.0F}, glm::vec3{-1.0F, 0.0F, 0.0F},
    };
    constexpr std::array<glm::vec3, 6> vertical = {
        glm::vec3{0.0F, 1.0F, 0.0F}, glm::vec3{0.0F, 1.0F, 0.0F},
        glm::vec3{0.0F, 0.0F, -1.0F}, glm::vec3{0.0F, 0.0F, 1.0F},
        glm::vec3{0.0F, 1.0F, 0.0F}, glm::vec3{0.0F, 1.0F, 0.0F},
    };

    Mesh mesh;
    constexpr std::size_t side = subdivisions + 1;
    mesh.vertices.reserve(normals.size() * side * side);
    mesh.indices.reserve(normals.size() * subdivisions * subdivisions * 6);
    for (std::size_t face = 0; face < normals.size(); ++face) {
        const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
        for (std::size_t y = 0; y <= subdivisions; ++y) {
            for (std::size_t x = 0; x <= subdivisions; ++x) {
                const glm::vec2 uv{
                    static_cast<float>(x) / static_cast<float>(subdivisions),
                    static_cast<float>(y) / static_cast<float>(subdivisions),
                };
                const glm::vec3 cubePosition = normals[face] * 0.5F
                    + horizontal[face] * (uv.x - 0.5F)
                    + vertical[face] * (uv.y - 0.5F);
                const glm::vec3 inner = glm::clamp(cubePosition, -innerExtent, innerExtent);
                const glm::vec3 offset = cubePosition - inner;
                const glm::vec3 vertexNormal = glm::length(offset) > 0.0001F
                    ? glm::normalize(offset)
                    : normals[face];
                mesh.vertices.push_back({inner + vertexNormal * bevelRadius, color, vertexNormal, uv});
            }
        }
        for (std::size_t y = 0; y < subdivisions; ++y) {
            for (std::size_t x = 0; x < subdivisions; ++x) {
                const std::uint32_t a = base + static_cast<std::uint32_t>(y * side + x);
                const std::uint32_t b = a + 1;
                const std::uint32_t c = base + static_cast<std::uint32_t>((y + 1) * side + x);
                const std::uint32_t d = c + 1;
                mesh.indices.insert(mesh.indices.end(), {a, b, d, a, d, c});
            }
        }
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

Mesh MeshFactory::disc(float radius, std::size_t segments, glm::vec3 color) {
    segments = std::max<std::size_t>(segments, 3);
    Mesh mesh;
    mesh.vertices.push_back({{0.0F, 0.0F, 0.0F}, color, {0.0F, 1.0F, 0.0F}, {0.5F, 0.5F}});
    for (std::size_t index = 0; index < segments; ++index) {
        const float angle = static_cast<float>(index) / static_cast<float>(segments) * 6.28318530718F;
        const glm::vec2 radial{std::cos(angle), std::sin(angle)};
        mesh.vertices.push_back({
            {radial.x * radius, 0.0F, radial.y * radius},
            color,
            {0.0F, 1.0F, 0.0F},
            radial * 0.5F + 0.5F,
        });
    }
    for (std::size_t index = 0; index < segments; ++index) {
        mesh.indices.insert(mesh.indices.end(), {
            0,
            static_cast<std::uint32_t>(index + 1),
            static_cast<std::uint32_t>((index + 1) % segments + 1),
        });
    }
    return mesh;
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
