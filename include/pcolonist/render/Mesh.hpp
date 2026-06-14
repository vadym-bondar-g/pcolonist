#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace pcolonist {

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec3 normal{0.0F, 1.0F, 0.0F};
    glm::vec2 textureCoordinate{0.0F};
};

struct MeshDraw {
    std::size_t firstIndex = 0;
    std::size_t indexCount = 0;
    std::filesystem::path diffuseTexture;
    float roughness = 0.72F;
    float specularStrength = 0.2F;
    glm::vec3 emissive{0.0F};
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<MeshDraw> draws;
};

} // namespace pcolonist
