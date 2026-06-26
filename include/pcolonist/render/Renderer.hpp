#pragma once

#include "pcolonist/render/ShaderLibrary.hpp"
#include "pcolonist/render/Skybox.hpp"

#include <glm/mat4x4.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <unordered_map>

namespace pcolonist {

class Camera;
class Mesh;
class Registry;
class WeatherSystem;
enum class GraphicsQuality;

class Renderer {
public:
    Renderer();
    explicit Renderer(std::filesystem::path assetRoot);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void resize(int width, int height);
    void render(const Camera& camera, Registry& registry, const WeatherSystem& weather);
    void releaseUnusedMeshes(Registry& registry);
    void setShadowsEnabled(bool enabled);
    void setBloomEnabled(bool enabled);
    void setGraphicsQuality(GraphicsQuality quality);
    void setSkyQuality(SkyQuality quality);
    void cycleSkyQuality();
    [[nodiscard]] bool shadowsEnabled() const;
    [[nodiscard]] bool bloomEnabled() const;
    [[nodiscard]] SkyQuality skyQuality() const;
    [[nodiscard]] const char* skyQualityName() const;

private:
    struct GpuMesh {
        unsigned int vertexArray = 0;
        unsigned int vertexBuffer = 0;
        unsigned int indexBuffer = 0;
        unsigned int instanceBuffer = 0;
        std::size_t indexCount = 0;
    };

    const GpuMesh& upload(const Mesh& mesh);
    unsigned int loadTexture(const std::filesystem::path& path);
    void createRenderTargets();
    void releaseRenderTargets();
    void createShadowMap();
    void renderShadowMap(const Camera& camera, Registry& registry, const WeatherSystem& weather);
    void postProcess(const WeatherSystem& weather, bool underwater);

    ShaderLibrary shaders_;
    Skybox skybox_;
    std::filesystem::path assetRoot_;
    std::unordered_map<const Mesh*, GpuMesh> meshes_;
    std::unordered_map<const Mesh*, float> meshRadii_;
    std::unordered_map<std::filesystem::path, unsigned int> textures_;
    unsigned int hdrFramebuffer_ = 0;
    unsigned int hdrColor_ = 0;
    unsigned int hdrDepth_ = 0;
    std::array<unsigned int, 2> shadowFramebuffers_{};
    std::array<unsigned int, 2> shadowDepths_{};
    unsigned int screenVertexArray_ = 0;
    std::array<glm::mat4, 2> lightSpaceMatrices_{glm::mat4{1.0F}, glm::mat4{1.0F}};
    int width_ = 1280;
    int height_ = 720;
    bool shadowsEnabled_ = true;
    bool bloomEnabled_ = true;
    int sceneQuality_ = 2;
    SkyQuality skyQuality_ = SkyQuality::Medium;
};

} // namespace pcolonist
