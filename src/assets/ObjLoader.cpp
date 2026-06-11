#include "pcolonist/assets/ObjLoader.hpp"

#include "pcolonist/assets/AssetSystem.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include <charconv>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

struct ObjReference {
    std::uint32_t position = 0;
    int textureCoordinate = -1;
    int normal = -1;
    std::uint32_t material = 0;

    bool operator==(const ObjReference&) const = default;
};

struct ObjReferenceHash {
    std::size_t operator()(const ObjReference& reference) const noexcept {
        return static_cast<std::size_t>(reference.position) * 31U
            ^ static_cast<std::size_t>(reference.textureCoordinate + 1) * 67U
            ^ static_cast<std::size_t>(reference.normal + 1) * 131U
            ^ static_cast<std::size_t>(reference.material) * 8191U;
    }
};

int parseIndex(std::string_view token, std::size_t count, std::string_view label) {
    int objIndex = 0;
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), objIndex);
    if (error != std::errc{} || end != token.data() + token.size() || objIndex == 0) {
        throw std::runtime_error("Invalid OBJ " + std::string(label) + " index");
    }

    const int resolved = objIndex > 0 ? objIndex - 1 : static_cast<int>(count) + objIndex;
    if (resolved < 0 || resolved >= static_cast<int>(count)) {
        throw std::runtime_error("OBJ " + std::string(label) + " index is out of range");
    }
    return resolved;
}

ObjReference parseReference(
    std::string_view token,
    std::size_t positionCount,
    std::size_t textureCoordinateCount,
    std::size_t normalCount,
    std::uint32_t material) {
    const std::size_t firstSlash = token.find('/');
    const std::string_view position = token.substr(0, firstSlash);
    ObjReference reference;
    reference.position = static_cast<std::uint32_t>(parseIndex(position, positionCount, "position"));
    reference.material = material;
    if (firstSlash == std::string_view::npos) {
        return reference;
    }

    const std::size_t secondSlash = token.find('/', firstSlash + 1);
    const std::string_view textureCoordinate = token.substr(
        firstSlash + 1,
        secondSlash == std::string_view::npos ? std::string_view::npos : secondSlash - firstSlash - 1);
    if (!textureCoordinate.empty()) {
        reference.textureCoordinate = parseIndex(textureCoordinate, textureCoordinateCount, "texture coordinate");
    }
    if (secondSlash != std::string_view::npos && secondSlash + 1 < token.size()) {
        reference.normal = parseIndex(token.substr(secondSlash + 1), normalCount, "normal");
    }
    return reference;
}

glm::vec3 colorFromPosition(const glm::vec3& position) {
    return glm::clamp(glm::vec3{0.42F, 0.55F, 0.32F} + position * 0.025F, 0.15F, 0.9F);
}

struct ObjMaterial {
    glm::vec3 color{1.0F};
    std::filesystem::path diffuseTexture;
};

std::unordered_map<std::string, ObjMaterial> loadMaterials(
    const pcolonist::AssetSystem& assets,
    const std::filesystem::path& objPath,
    const std::string& library) {
    std::unordered_map<std::string, ObjMaterial> materials;
    std::istringstream input(assets.readText(objPath.parent_path() / library));
    std::string current;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream row(line);
        std::string command;
        row >> command;
        if (command == "newmtl") {
            row >> current;
            materials.try_emplace(current);
        } else if (command == "Kd" && !current.empty()) {
            glm::vec3 color;
            if (row >> color.r >> color.g >> color.b) {
                materials[current].color = glm::clamp(color, 0.0F, 1.0F);
            }
        } else if (command == "map_Kd" && !current.empty()) {
            std::string texture;
            while (row >> texture) {
                const std::filesystem::path texturePath = texture;
                if (texturePath.is_absolute()
                    || (texturePath.extension() != ".png" && texturePath.extension() != ".PNG")) {
                    throw std::runtime_error("OBJ diffuse textures must be relative PNG files");
                }
                materials[current].diffuseTexture = (objPath.parent_path() / texturePath).lexically_normal();
            }
        }
    }
    return materials;
}

} // namespace

namespace pcolonist {

Mesh ObjLoader::load(const AssetSystem& assets, const std::filesystem::path& relativePath) {
    std::istringstream input(assets.readText(relativePath));
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> colors;
    std::vector<glm::vec2> textureCoordinates;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> materialColors{glm::vec3{1.0F}};
    std::vector<std::filesystem::path> materialTextures(1);
    std::vector<std::vector<std::uint32_t>> materialDrawIndices(1);
    std::unordered_map<std::string, std::uint32_t> materialLookup;
    std::vector<bool> explicitNormals;
    std::unordered_map<ObjReference, std::uint32_t, ObjReferenceHash> vertices;
    Mesh mesh;
    std::uint32_t currentMaterial = 0;
    std::string line;

    const auto meshIndex = [&](const ObjReference& reference) {
        if (const auto iterator = vertices.find(reference); iterator != vertices.end()) {
            return iterator->second;
        }
        const glm::vec3 normal = reference.normal >= 0 ? normals[reference.normal] : glm::vec3{0.0F};
        const std::uint32_t index = static_cast<std::uint32_t>(mesh.vertices.size());
        const glm::vec3 color = reference.material == 0
            ? colors[reference.position]
            : materialColors[reference.material];
        const glm::vec2 textureCoordinate = reference.textureCoordinate >= 0
            ? textureCoordinates[reference.textureCoordinate]
            : glm::vec2{0.0F};
        mesh.vertices.push_back({positions[reference.position], color, normal, textureCoordinate});
        explicitNormals.push_back(reference.normal >= 0);
        vertices.emplace(reference, index);
        return index;
    };

    while (std::getline(input, line)) {
        std::istringstream lineStream(line);
        std::string command;
        lineStream >> command;

        if (command == "mtllib") {
            std::string library;
            if (lineStream >> library) {
                for (const auto& [name, material] : loadMaterials(assets, relativePath, library)) {
                    const std::uint32_t index = static_cast<std::uint32_t>(materialColors.size());
                    materialLookup[name] = index;
                    materialColors.push_back(material.color);
                    materialTextures.push_back(material.diffuseTexture);
                    materialDrawIndices.emplace_back();
                }
            }
        } else if (command == "usemtl") {
            std::string material;
            lineStream >> material;
            const auto iterator = materialLookup.find(material);
            currentMaterial = iterator == materialLookup.end() ? 0 : iterator->second;
        } else if (command == "v") {
            glm::vec3 position;
            if (!(lineStream >> position.x >> position.y >> position.z)) {
                throw std::runtime_error("Invalid OBJ vertex in " + relativePath.string());
            }
            std::vector<float> extras;
            float value = 0.0F;
            while (lineStream >> value) {
                extras.push_back(value);
            }
            positions.push_back(position);
            colors.push_back(extras.size() >= 3
                ? glm::clamp(glm::vec3{extras[extras.size() - 3], extras[extras.size() - 2], extras.back()}, 0.0F, 1.0F)
                : colorFromPosition(position));
        } else if (command == "vt") {
            glm::vec2 textureCoordinate;
            if (!(lineStream >> textureCoordinate.x >> textureCoordinate.y)) {
                throw std::runtime_error("Invalid OBJ texture coordinate in " + relativePath.string());
            }
            textureCoordinates.push_back(textureCoordinate);
        } else if (command == "vn") {
            glm::vec3 normal;
            if (!(lineStream >> normal.x >> normal.y >> normal.z)) {
                throw std::runtime_error("Invalid OBJ normal in " + relativePath.string());
            }
            normals.push_back(glm::length(normal) > 0.0F ? glm::normalize(normal) : glm::vec3{0.0F, 1.0F, 0.0F});
        } else if (command == "f") {
            std::vector<std::uint32_t> face;
            std::string token;
            while (lineStream >> token) {
                face.push_back(meshIndex(parseReference(
                    token,
                    positions.size(),
                    textureCoordinates.size(),
                    normals.size(),
                    currentMaterial)));
            }
            if (face.size() < 3) {
                throw std::runtime_error("OBJ face has fewer than three vertices");
            }
            for (std::size_t index = 1; index + 1 < face.size(); ++index) {
                mesh.indices.insert(mesh.indices.end(), {face[0], face[index], face[index + 1]});
                materialDrawIndices[currentMaterial].insert(
                    materialDrawIndices[currentMaterial].end(),
                    {face[0], face[index], face[index + 1]});
            }
        }
    }

    if (mesh.vertices.empty() || mesh.indices.empty()) {
        throw std::runtime_error("OBJ contains no renderable geometry: " + relativePath.string());
    }

    for (std::size_t index = 0; index < mesh.indices.size(); index += 3) {
        Vertex& a = mesh.vertices[mesh.indices[index]];
        Vertex& b = mesh.vertices[mesh.indices[index + 1]];
        Vertex& c = mesh.vertices[mesh.indices[index + 2]];
        const glm::vec3 normal = glm::cross(b.position - a.position, c.position - a.position);
        if (!explicitNormals[mesh.indices[index]]) {
            a.normal += normal;
        }
        if (!explicitNormals[mesh.indices[index + 1]]) {
            b.normal += normal;
        }
        if (!explicitNormals[mesh.indices[index + 2]]) {
            c.normal += normal;
        }
    }
    for (Vertex& vertex : mesh.vertices) {
        vertex.normal = glm::length(vertex.normal) > 0.0F ? glm::normalize(vertex.normal) : glm::vec3{0.0F, 1.0F, 0.0F};
    }

    mesh.indices.clear();
    for (std::size_t material = 0; material < materialDrawIndices.size(); ++material) {
        const std::vector<std::uint32_t>& indices = materialDrawIndices[material];
        if (indices.empty()) {
            continue;
        }
        mesh.draws.push_back({mesh.indices.size(), indices.size(), materialTextures[material]});
        mesh.indices.insert(mesh.indices.end(), indices.begin(), indices.end());
    }
    return mesh;
}

} // namespace pcolonist
