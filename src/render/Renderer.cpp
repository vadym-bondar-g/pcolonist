#include "pcolonist/render/Renderer.hpp"

#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/render/Camera.hpp"
#include "pcolonist/render/Mesh.hpp"
#include "pcolonist/render/PngLoader.hpp"
#include "pcolonist/world/WeatherSystem.hpp"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace {

constexpr int shadowSize = 2048;
constexpr std::array<float, 2> shadowRanges = {48.0F, 190.0F};

glm::mat4 modelMatrix(const pcolonist::Transform& transform) {
    glm::mat4 model = glm::translate(glm::mat4(1.0F), transform.position);
    model = glm::rotate(model, transform.rotation.x, {1.0F, 0.0F, 0.0F});
    model = glm::rotate(model, transform.rotation.y, {0.0F, 1.0F, 0.0F});
    model = glm::rotate(model, transform.rotation.z, {0.0F, 0.0F, 1.0F});
    return glm::scale(model, transform.scale);
}

struct RenderBatch {
    const pcolonist::Mesh* mesh = nullptr;
    std::vector<glm::mat4> models;
    bool water = false;
    bool terrain = false;
    bool lava = false;
};

std::vector<RenderBatch> collectBatches(
    pcolonist::Registry& registry,
    const glm::vec3& cameraPosition,
    bool shadows) {
    std::vector<RenderBatch> batches;
    registry.each<pcolonist::Transform, pcolonist::MeshRenderer>(
        [&batches, &registry, cameraPosition, shadows](
            pcolonist::Entity entity,
            const pcolonist::Transform& transform,
            const pcolonist::MeshRenderer& renderer) {
            if (!renderer.mesh
                || (shadows && (registry.has<pcolonist::WaterSurface>(entity)
                    || registry.has<pcolonist::LavaSurface>(entity)))) {
                return;
            }
            const bool water = registry.has<pcolonist::WaterSurface>(entity);
            const bool terrain = registry.has<pcolonist::TerrainSurface>(entity);
            const bool lava = registry.has<pcolonist::LavaSurface>(entity);
            if (registry.has<pcolonist::TerrainChunk>(entity)) {
                const pcolonist::TerrainChunk& chunk = registry.get<pcolonist::TerrainChunk>(entity);
                if (glm::distance(glm::vec2{cameraPosition.x, cameraPosition.z}, chunk.center) > chunk.radius + 210.0F) {
                    return;
                }
            }
            const float distance = glm::length(transform.position - cameraPosition);
            const float scale = std::max({transform.scale.x, transform.scale.y, transform.scale.z});
            if (!terrain && !water && distance > 190.0F + scale * 8.0F) {
                return;
            }
            auto batch = std::find_if(batches.begin(), batches.end(), [&](const RenderBatch& candidate) {
                return candidate.mesh == renderer.mesh.get()
                    && candidate.water == water
                    && candidate.terrain == terrain
                    && candidate.lava == lava;
            });
            if (batch == batches.end()) {
                batches.push_back({renderer.mesh.get(), {}, water, terrain, lava});
                batch = std::prev(batches.end());
            }
            batch->models.push_back(modelMatrix(transform));
        });
    return batches;
}

} // namespace

namespace pcolonist {

Renderer::Renderer()
    : shaders_(std::filesystem::path(PCOLONIST_ASSET_DIR) / "shaders") {
    shaders_.preload();
    glGenVertexArrays(1, &screenVertexArray_);
    createShadowMap();
    createRenderTargets();
}

Renderer::~Renderer() {
    releaseRenderTargets();
    glDeleteTextures(static_cast<int>(shadowDepths_.size()), shadowDepths_.data());
    glDeleteFramebuffers(static_cast<int>(shadowFramebuffers_.size()), shadowFramebuffers_.data());
    glDeleteVertexArrays(1, &screenVertexArray_);
    for (const auto& [mesh, gpuMesh] : meshes_) {
        static_cast<void>(mesh);
        glDeleteBuffers(1, &gpuMesh.instanceBuffer);
        glDeleteBuffers(1, &gpuMesh.indexBuffer);
        glDeleteBuffers(1, &gpuMesh.vertexBuffer);
        glDeleteVertexArrays(1, &gpuMesh.vertexArray);
    }
    for (const auto& [path, texture] : textures_) {
        static_cast<void>(path);
        glDeleteTextures(1, &texture);
    }
}

void Renderer::resize(int width, int height) {
    width_ = width;
    height_ = height;
    releaseRenderTargets();
    createRenderTargets();
    glViewport(0, 0, width, height);
}

void Renderer::render(const Camera& camera, Registry& registry, const WeatherSystem& weather) {
    if (width_ <= 0 || height_ <= 0) {
        return;
    }
    renderShadowMap(camera, registry, weather);

    glBindFramebuffer(GL_FRAMEBUFFER, hdrFramebuffer_);
    glViewport(0, 0, width_, height_);
    const float aspectRatio = height_ > 0 ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0F;
    skybox_.render(shaders_.get("sky"), camera, weather, aspectRatio);

    Shader& shader = shaders_.get("scene");
    shader.use();
    shader.setMat4("projection", glm::perspective(glm::radians(70.0F), aspectRatio, 0.1F, 520.0F));
    shader.setMat4("view", camera.viewMatrix());
    shader.setMat4("lightSpaceMatrices[0]", lightSpaceMatrices_[0]);
    shader.setMat4("lightSpaceMatrices[1]", lightSpaceMatrices_[1]);
    shader.setVec3("cameraPosition", camera.position());
    shader.setVec3("fogColor", weather.fogColor());
    shader.setVec3("sunDirection", weather.sunDirection());
    shader.setVec3("sunColor", weather.sunColor());
    shader.setVec3("moonDirection", weather.moonDirection());
    shader.setVec3("moonColor", weather.moonColor());
    shader.setVec3("ambientColor", weather.ambientColor());
    shader.setFloat("fogDensity", weather.fogDensity());
    shader.setFloat("daylight", weather.daylight());
    shader.setFloat("nightFactor", weather.nightFactor());
    shader.setFloat("cloudiness", weather.cloudiness());
    shader.setFloat("time", weather.time());
    shader.setInt("shadowMapNear", 3);
    shader.setInt("shadowMapFar", 4);
    shader.setInt("diffuseTexture", 0);
    shader.setInt("terrainEarthTexture", 0);
    shader.setInt("terrainSandTexture", 1);
    shader.setInt("terrainBasaltTexture", 2);
    shader.setInt("shadowsEnabled", shadowsEnabled_ ? 1 : 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, shadowDepths_[0]);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, shadowDepths_[1]);

    for (const RenderBatch& batch : collectBatches(registry, camera.position(), false)) {
            const GpuMesh& gpuMesh = upload(*batch.mesh);
            shader.setFloat("water", batch.water ? 1.0F : 0.0F);
            shader.setFloat("terrain", batch.terrain ? 1.0F : 0.0F);
            shader.setFloat("lava", batch.lava ? 1.0F : 0.0F);
            if (batch.terrain) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, loadTexture("textures/terrain/earth.png"));
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, loadTexture("textures/terrain/sand.png"));
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, loadTexture("textures/terrain/basalt.png"));
            }
            glBindVertexArray(gpuMesh.vertexArray);
            glBindBuffer(GL_ARRAY_BUFFER, gpuMesh.instanceBuffer);
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(batch.models.size() * sizeof(glm::mat4)),
                batch.models.data(),
                GL_STREAM_DRAW);
            if (batch.mesh->draws.empty()) {
                shader.setInt("hasDiffuseTexture", 0);
                shader.setFloat("roughness", 0.72F);
                shader.setFloat("specularStrength", 0.2F);
                shader.setVec3("emissiveColor", {0.0F, 0.0F, 0.0F});
                glDrawElementsInstanced(
                    GL_TRIANGLES, static_cast<int>(gpuMesh.indexCount), GL_UNSIGNED_INT, nullptr,
                    static_cast<int>(batch.models.size()));
                continue;
            }
            for (const MeshDraw& draw : batch.mesh->draws) {
                const bool textured = !draw.diffuseTexture.empty();
                shader.setInt("hasDiffuseTexture", textured ? 1 : 0);
                shader.setFloat("roughness", draw.roughness);
                shader.setFloat("specularStrength", draw.specularStrength);
                shader.setVec3("emissiveColor", draw.emissive);
                if (textured) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, loadTexture(draw.diffuseTexture));
                }
                glDrawElementsInstanced(
                    GL_TRIANGLES,
                    static_cast<int>(draw.indexCount),
                    GL_UNSIGNED_INT,
                    reinterpret_cast<void*>(draw.firstIndex * sizeof(std::uint32_t)),
                    static_cast<int>(batch.models.size()));
            }
    }

    bool underwater = false;
    registry.each<Transform, WaterSurface>(
        [&underwater, &camera](Entity, const Transform& transform, const WaterSurface&) {
            underwater = underwater || camera.position().y < transform.position.y;
        });
    postProcess(weather, underwater);
}

void Renderer::setShadowsEnabled(bool enabled) {
    shadowsEnabled_ = enabled;
}

void Renderer::setBloomEnabled(bool enabled) {
    bloomEnabled_ = enabled;
}

bool Renderer::shadowsEnabled() const {
    return shadowsEnabled_;
}

bool Renderer::bloomEnabled() const {
    return bloomEnabled_;
}

void Renderer::createRenderTargets() {
    if (width_ <= 0 || height_ <= 0) {
        return;
    }
    glGenFramebuffers(1, &hdrFramebuffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFramebuffer_);
    glGenTextures(1, &hdrColor_);
    glBindTexture(GL_TEXTURE_2D, hdrColor_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrColor_, 0);

    glGenTextures(1, &hdrDepth_);
    glBindTexture(GL_TEXTURE_2D, hdrDepth_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width_, height_, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, hdrDepth_, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("HDR framebuffer is incomplete");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::releaseRenderTargets() {
    glDeleteTextures(1, &hdrDepth_);
    glDeleteTextures(1, &hdrColor_);
    glDeleteFramebuffers(1, &hdrFramebuffer_);
    hdrDepth_ = 0;
    hdrColor_ = 0;
    hdrFramebuffer_ = 0;
}

void Renderer::createShadowMap() {
    glGenFramebuffers(static_cast<int>(shadowFramebuffers_.size()), shadowFramebuffers_.data());
    glGenTextures(static_cast<int>(shadowDepths_.size()), shadowDepths_.data());
    for (std::size_t cascade = 0; cascade < shadowDepths_.size(); ++cascade) {
        glBindTexture(GL_TEXTURE_2D, shadowDepths_[cascade]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowSize, shadowSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        constexpr float border[] = {1.0F, 1.0F, 1.0F, 1.0F};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffers_[cascade]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepths_[cascade], 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("Shadow framebuffer is incomplete");
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::renderShadowMap(const Camera& camera, Registry& registry, const WeatherSystem& weather) {
    const glm::vec3 lightDirection = weather.daylight() > 0.05F ? weather.sunDirection() : weather.moonDirection();
    const glm::vec3 center = camera.position();
    for (std::size_t cascade = 0; cascade < shadowRanges.size(); ++cascade) {
        const float range = shadowRanges[cascade];
        const float texelSize = range * 2.0F / static_cast<float>(shadowSize);
        const glm::vec3 stableCenter{
            std::floor(center.x / texelSize) * texelSize,
            center.y,
            std::floor(center.z / texelSize) * texelSize,
        };
        const glm::mat4 lightProjection = glm::ortho(-range, range, -range, range, 1.0F, range * 3.0F);
        const glm::mat4 lightView = glm::lookAt(
            stableCenter + lightDirection * range * 1.4F, stableCenter, {0.0F, 1.0F, 0.0F});
        lightSpaceMatrices_[cascade] = lightProjection * lightView;
    }
    if (!shadowsEnabled_) {
        return;
    }

    glViewport(0, 0, shadowSize, shadowSize);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    Shader& shader = shaders_.get("shadow");
    shader.use();
    const std::vector<RenderBatch> batches = collectBatches(registry, camera.position(), true);
    for (std::size_t cascade = 0; cascade < shadowFramebuffers_.size(); ++cascade) {
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffers_[cascade]);
        glClear(GL_DEPTH_BUFFER_BIT);
        shader.setMat4("lightSpaceMatrix", lightSpaceMatrices_[cascade]);
        for (const RenderBatch& batch : batches) {
            const GpuMesh& gpuMesh = upload(*batch.mesh);
            glBindVertexArray(gpuMesh.vertexArray);
            glBindBuffer(GL_ARRAY_BUFFER, gpuMesh.instanceBuffer);
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(batch.models.size() * sizeof(glm::mat4)),
                batch.models.data(),
                GL_STREAM_DRAW);
            glDrawElementsInstanced(
                GL_TRIANGLES, static_cast<int>(gpuMesh.indexCount), GL_UNSIGNED_INT, nullptr,
                static_cast<int>(batch.models.size()));
        }
    }
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::postProcess(const WeatherSystem& weather, bool underwater) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    Shader& shader = shaders_.get("post");
    shader.use();
    shader.setInt("hdrScene", 0);
    shader.setInt("sceneDepth", 1);
    shader.setInt("bloomEnabled", bloomEnabled_ ? 1 : 0);
    shader.setInt("underwater", underwater ? 1 : 0);
    shader.setFloat("exposure", 1.08F + weather.daylight() * 0.12F + weather.cloudiness() * 0.055F);
    shader.setFloat("daylight", weather.daylight());
    shader.setFloat("cloudiness", weather.cloudiness());
    shader.setFloat("time", weather.time());
    shader.setVec3("resolution", {static_cast<float>(width_), static_cast<float>(height_), 0.0F});
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrColor_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, hdrDepth_);
    glBindVertexArray(screenVertexArray_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glEnable(GL_DEPTH_TEST);
}

const Renderer::GpuMesh& Renderer::upload(const Mesh& mesh) {
    if (const auto iterator = meshes_.find(&mesh); iterator != meshes_.end()) {
        return iterator->second;
    }
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        throw std::runtime_error("Cannot upload empty mesh");
    }
    GpuMesh gpuMesh;
    gpuMesh.indexCount = mesh.indices.size();
    glGenVertexArrays(1, &gpuMesh.vertexArray);
    glGenBuffers(1, &gpuMesh.vertexBuffer);
    glGenBuffers(1, &gpuMesh.indexBuffer);
    glGenBuffers(1, &gpuMesh.instanceBuffer);
    glBindVertexArray(gpuMesh.vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, gpuMesh.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)), mesh.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpuMesh.indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t)), mesh.indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, textureCoordinate)));
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ARRAY_BUFFER, gpuMesh.instanceBuffer);
    for (unsigned int column = 0; column < 4; ++column) {
        glVertexAttribPointer(
            4 + column,
            4,
            GL_FLOAT,
            GL_FALSE,
            sizeof(glm::mat4),
            reinterpret_cast<void*>(sizeof(glm::vec4) * column));
        glEnableVertexAttribArray(4 + column);
        glVertexAttribDivisor(4 + column, 1);
    }
    return meshes_.emplace(&mesh, gpuMesh).first->second;
}

unsigned int Renderer::loadTexture(const std::filesystem::path& path) {
    if (const auto iterator = textures_.find(path); iterator != textures_.end()) {
        return iterator->second;
    }

    const Image image = PngLoader::load(std::filesystem::path(PCOLONIST_ASSET_DIR) / path);
    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_SRGB8_ALPHA8,
        image.width,
        image.height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        image.pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -0.2F);
    return textures_.emplace(path, texture).first->second;
}

} // namespace pcolonist
