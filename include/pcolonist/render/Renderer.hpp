#pragma once

#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/render/ShaderLibrary.hpp"
#include "pcolonist/render/Skybox.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace pcolonist {

class Camera;
class Mesh;
class Registry;
class WeatherSystem;
enum class GraphicsQuality;

struct RendererDebugOptions {
    float terrainDrawDistance = 340.0F;
    float objectDrawDistance = 260.0F;
    bool showTerrain = true;
    bool showObjects = true;
    bool showResources = true;
    bool showWater = true;
    bool showLava = true;
    bool showFire = true;
    bool wireframe = false;
};

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
    void setDebugOptions(RendererDebugOptions options);
    void cycleSkyQuality();
    [[nodiscard]] bool shadowsEnabled() const;
    [[nodiscard]] bool bloomEnabled() const;
    [[nodiscard]] SkyQuality skyQuality() const;
    [[nodiscard]] const char* skyQualityName() const;
    [[nodiscard]] const RendererDebugOptions& debugOptions() const;

private:
    struct GlHandle {
        enum class Kind {
            Buffer,
            Texture,
            Framebuffer,
            VertexArray,
        };

        GlHandle() = default;
        GlHandle(Kind kind, unsigned int id);
        ~GlHandle();

        GlHandle(const GlHandle&) = delete;
        GlHandle& operator=(const GlHandle&) = delete;
        GlHandle(GlHandle&& other) noexcept;
        GlHandle& operator=(GlHandle&& other) noexcept;

        void reset(Kind kind, unsigned int id = 0);
        [[nodiscard]] unsigned int get() const;
        [[nodiscard]] bool valid() const;
        operator unsigned int() const;

    private:
        void destroy();

        Kind kind_ = Kind::Buffer;
        unsigned int id_ = 0;
    };

    struct GpuMesh {
        GlHandle vertexArray;
        GlHandle vertexBuffer;
        GlHandle indexBuffer;
        GlHandle instanceBuffer;
        std::size_t indexCount = 0;
        std::size_t instanceCapacity = 0;
    };

    struct RenderItem {
        const Mesh* mesh = nullptr;
        glm::mat4 model{1.0F};
        float distanceSquared = 0.0F;
        float cullRadius = 0.0F;
        float objectDistancePadding = 0.0F;
        float chunkDistance = 0.0F;
        float chunkRadius = 0.0F;
        float terrainLodMinDistance = 0.0F;
        int waterKind = 0;
        glm::vec2 waterFlowDirection{0.0F};
        float waterFoamStrength = 1.0F;
        bool water = false;
        bool terrain = false;
        bool lava = false;
        bool fire = false;
        bool smoke = false;
        bool grass = false;
        bool terrainChunk = false;
        bool terrainLod = false;
    };

    struct RenderBatch {
        const Mesh* mesh = nullptr;
        std::vector<glm::mat4> models;
        bool water = false;
        int waterKind = 0;
        glm::vec2 waterFlowDirection{0.0F};
        float waterFoamStrength = 1.0F;
        float sortDistanceSquared = 0.0F;
        bool terrain = false;
        bool lava = false;
        bool fire = false;
        bool smoke = false;
        bool grass = false;
    };

    struct BatchKey {
        const Mesh* mesh = nullptr;
        bool water = false;
        int waterKind = 0;
        int waterFlowDirectionX = 0;
        int waterFlowDirectionY = 0;
        int waterFoamStrength = 0;
        bool terrain = false;
        bool lava = false;
        bool fire = false;
        bool smoke = false;
        bool grass = false;

        [[nodiscard]] bool operator==(const BatchKey& other) const;
    };

    struct BatchKeyHash {
        [[nodiscard]] std::size_t operator()(const BatchKey& key) const;
    };

    struct RenderScratch {
        std::vector<RenderItem> items;
        std::vector<RenderBatch> batches;
        std::vector<std::size_t> drawOrder;
        std::vector<std::size_t> transparentOrder;
        std::unordered_map<BatchKey, std::size_t, BatchKeyHash> batchIndices;
        std::size_t batchCount = 0;
    };

    struct CachedModelMatrix {
        glm::vec3 position{0.0F};
        glm::vec3 rotation{0.0F};
        glm::vec3 scale{1.0F};
        glm::mat4 model{1.0F};
    };

    struct RenderQualityConfig {
        std::array<float, 2> shadowRanges{48.0F, 190.0F};
        float cameraFarPlane = 760.0F;
        float terrainDrawDistanceMinimum = 64.0F;
        float terrainDrawDistanceMaximum = 760.0F;
        float objectDrawDistanceMinimum = 32.0F;
        float objectDrawDistanceMaximum = 520.0F;
        float grassDrawDistance = 82.0F;
        int shadowSize = 2048;
        int sceneQuality = 2;
    };

    [[nodiscard]] static RenderQualityConfig qualityConfig(GraphicsQuality quality);
    [[nodiscard]] const glm::mat4& cachedModelMatrix(Entity entity, const Transform& transform);
    GpuMesh& upload(const Mesh& mesh);
    void uploadInstances(GpuMesh& gpuMesh, const std::vector<glm::mat4>& models);
    unsigned int loadTexture(const std::filesystem::path& path);
    void loadTerrainTextures();
    void createRenderTargets();
    void releaseRenderTargets();
    void createShadowMap();
    void releaseShadowMap();
    void collectRenderItems(Registry& registry, const Camera& camera, float aspectRatio, float terrainDistance, float objectDistance);
    std::size_t buildBatches(float terrainDistance, float objectDistance, bool shadows, float minimumShadowRadius);
    void renderShadowMap(const Camera& camera, const WeatherSystem& weather);
    void postProcess(const WeatherSystem& weather, bool underwater);

    ShaderLibrary shaders_;
    Skybox skybox_;
    std::filesystem::path assetRoot_;
    std::unordered_map<const Mesh*, GpuMesh> meshes_;
    std::unordered_map<const Mesh*, float> meshRadii_;
    std::unordered_map<Entity, CachedModelMatrix> modelMatrices_;
    std::unordered_map<std::filesystem::path, GlHandle> textures_;
    RenderScratch renderScratch_;
    RenderQualityConfig qualityConfig_;
    GlHandle hdrFramebuffer_;
    GlHandle hdrColor_;
    GlHandle hdrDepth_;
    std::array<GlHandle, 2> shadowFramebuffers_;
    std::array<GlHandle, 2> shadowDepths_;
    GlHandle screenVertexArray_;
    std::array<glm::mat4, 2> lightSpaceMatrices_{glm::mat4{1.0F}, glm::mat4{1.0F}};
    int width_ = 1280;
    int height_ = 720;
    bool shadowsEnabled_ = true;
    bool bloomEnabled_ = true;
    SkyQuality skyQuality_ = SkyQuality::Medium;
    RendererDebugOptions debugOptions_;
    unsigned int terrainEarthTexture_ = 0;
    unsigned int terrainSandTexture_ = 0;
    unsigned int terrainBasaltTexture_ = 0;
};

} // namespace pcolonist
