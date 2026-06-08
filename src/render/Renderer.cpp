#include "pcolonist/render/Renderer.hpp"

#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/render/Camera.hpp"
#include "pcolonist/render/Mesh.hpp"
#include "pcolonist/world/WeatherSystem.hpp"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cstddef>
#include <filesystem>
#include <stdexcept>

namespace {

constexpr int shadowSize = 2048;

glm::mat4 modelMatrix(const pcolonist::Transform& transform) {
    glm::mat4 model = glm::translate(glm::mat4(1.0F), transform.position);
    model = glm::rotate(model, transform.rotation.x, {1.0F, 0.0F, 0.0F});
    model = glm::rotate(model, transform.rotation.y, {0.0F, 1.0F, 0.0F});
    model = glm::rotate(model, transform.rotation.z, {0.0F, 0.0F, 1.0F});
    return glm::scale(model, transform.scale);
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
    glDeleteTextures(1, &shadowDepth_);
    glDeleteFramebuffers(1, &shadowFramebuffer_);
    glDeleteVertexArrays(1, &screenVertexArray_);
    for (const auto& [mesh, gpuMesh] : meshes_) {
        static_cast<void>(mesh);
        glDeleteBuffers(1, &gpuMesh.indexBuffer);
        glDeleteBuffers(1, &gpuMesh.vertexBuffer);
        glDeleteVertexArrays(1, &gpuMesh.vertexArray);
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
    shader.setMat4("projection", glm::perspective(glm::radians(70.0F), aspectRatio, 0.1F, 250.0F));
    shader.setMat4("view", camera.viewMatrix());
    shader.setMat4("lightSpaceMatrix", lightSpaceMatrix_);
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
    shader.setFloat("time", weather.time());
    shader.setInt("shadowMap", 3);
    shader.setInt("shadowsEnabled", shadowsEnabled_ ? 1 : 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, shadowDepth_);

    registry.each<Transform, MeshRenderer>(
        [this, &registry, &shader](Entity entity, const Transform& transform, const MeshRenderer& renderer) {
            if (!renderer.mesh) {
                return;
            }
            const GpuMesh& gpuMesh = upload(*renderer.mesh);
            shader.setMat4("model", modelMatrix(transform));
            shader.setFloat("water", registry.has<WaterSurface>(entity) ? 1.0F : 0.0F);
            glBindVertexArray(gpuMesh.vertexArray);
            glDrawElements(GL_TRIANGLES, static_cast<int>(gpuMesh.indexCount), GL_UNSIGNED_INT, nullptr);
        });

    postProcess(weather);
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

    glGenRenderbuffers(1, &hdrDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, hdrDepth_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width_, height_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, hdrDepth_);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("HDR framebuffer is incomplete");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::releaseRenderTargets() {
    glDeleteRenderbuffers(1, &hdrDepth_);
    glDeleteTextures(1, &hdrColor_);
    glDeleteFramebuffers(1, &hdrFramebuffer_);
    hdrDepth_ = 0;
    hdrColor_ = 0;
    hdrFramebuffer_ = 0;
}

void Renderer::createShadowMap() {
    glGenFramebuffers(1, &shadowFramebuffer_);
    glGenTextures(1, &shadowDepth_);
    glBindTexture(GL_TEXTURE_2D, shadowDepth_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowSize, shadowSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    constexpr float border[] = {1.0F, 1.0F, 1.0F, 1.0F};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepth_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("Shadow framebuffer is incomplete");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::renderShadowMap(const Camera& camera, Registry& registry, const WeatherSystem& weather) {
    const glm::vec3 lightDirection = weather.daylight() > 0.05F ? weather.sunDirection() : weather.moonDirection();
    const glm::vec3 center = camera.position();
    const glm::mat4 lightProjection = glm::ortho(-42.0F, 42.0F, -42.0F, 42.0F, 1.0F, 120.0F);
    const glm::mat4 lightView = glm::lookAt(center + lightDirection * 55.0F, center, {0.0F, 1.0F, 0.0F});
    lightSpaceMatrix_ = lightProjection * lightView;
    if (!shadowsEnabled_) {
        return;
    }

    glViewport(0, 0, shadowSize, shadowSize);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffer_);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    Shader& shader = shaders_.get("shadow");
    shader.use();
    shader.setMat4("lightSpaceMatrix", lightSpaceMatrix_);
    registry.each<Transform, MeshRenderer>(
        [this, &registry, &shader](Entity entity, const Transform& transform, const MeshRenderer& renderer) {
            if (!renderer.mesh || registry.has<WaterSurface>(entity)) {
                return;
            }
            const GpuMesh& gpuMesh = upload(*renderer.mesh);
            shader.setMat4("model", modelMatrix(transform));
            glBindVertexArray(gpuMesh.vertexArray);
            glDrawElements(GL_TRIANGLES, static_cast<int>(gpuMesh.indexCount), GL_UNSIGNED_INT, nullptr);
        });
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::postProcess(const WeatherSystem& weather) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    Shader& shader = shaders_.get("post");
    shader.use();
    shader.setInt("hdrScene", 0);
    shader.setInt("bloomEnabled", bloomEnabled_ ? 1 : 0);
    shader.setFloat("exposure", 1.08F + weather.daylight() * 0.12F);
    shader.setFloat("daylight", weather.daylight());
    shader.setFloat("time", weather.time());
    shader.setVec3("resolution", {static_cast<float>(width_), static_cast<float>(height_), 0.0F});
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrColor_);
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
    return meshes_.emplace(&mesh, gpuMesh).first->second;
}

} // namespace pcolonist
