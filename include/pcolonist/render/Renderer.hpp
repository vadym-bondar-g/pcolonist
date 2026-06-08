#pragma once

#include "pcolonist/render/ShaderLibrary.hpp"
#include "pcolonist/render/Skybox.hpp"

#include <glm/mat4x4.hpp>

#include <cstddef>
#include <unordered_map>

namespace pcolonist {

class Camera;
class Mesh;
class Registry;
class WeatherSystem;

class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void resize(int width, int height);
    void render(const Camera& camera, Registry& registry, const WeatherSystem& weather);
    void setShadowsEnabled(bool enabled);
    void setBloomEnabled(bool enabled);
    [[nodiscard]] bool shadowsEnabled() const;
    [[nodiscard]] bool bloomEnabled() const;

private:
    struct GpuMesh {
        unsigned int vertexArray = 0;
        unsigned int vertexBuffer = 0;
        unsigned int indexBuffer = 0;
        std::size_t indexCount = 0;
    };

    const GpuMesh& upload(const Mesh& mesh);
    void createRenderTargets();
    void releaseRenderTargets();
    void createShadowMap();
    void renderShadowMap(const Camera& camera, Registry& registry, const WeatherSystem& weather);
    void postProcess(const WeatherSystem& weather);

    ShaderLibrary shaders_;
    Skybox skybox_;
    std::unordered_map<const Mesh*, GpuMesh> meshes_;
    unsigned int hdrFramebuffer_ = 0;
    unsigned int hdrColor_ = 0;
    unsigned int hdrDepth_ = 0;
    unsigned int shadowFramebuffer_ = 0;
    unsigned int shadowDepth_ = 0;
    unsigned int screenVertexArray_ = 0;
    glm::mat4 lightSpaceMatrix_{1.0F};
    int width_ = 1280;
    int height_ = 720;
    bool shadowsEnabled_ = true;
    bool bloomEnabled_ = true;
};

} // namespace pcolonist
