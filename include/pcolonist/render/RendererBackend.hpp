#pragma once

namespace pcolonist {

class Camera;
class Registry;
class WeatherSystem;
enum class GraphicsQuality;
enum class SkyQuality;

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

class RendererBackend {
public:
    virtual ~RendererBackend() = default;

    virtual void resize(int width, int height) = 0;
    virtual void preloadResources(Registry& registry) = 0;
    virtual void render(const Camera& camera, Registry& registry, const WeatherSystem& weather) = 0;
    virtual void releaseUnusedMeshes(Registry& registry) = 0;
    virtual void setShadowsEnabled(bool enabled) = 0;
    virtual void setBloomEnabled(bool enabled) = 0;
    virtual void setGraphicsQuality(GraphicsQuality quality) = 0;
    virtual void setSkyQuality(SkyQuality quality) = 0;
    virtual void setDebugOptions(RendererDebugOptions options) = 0;
    virtual void cycleSkyQuality() = 0;
    [[nodiscard]] virtual bool shadowsEnabled() const = 0;
    [[nodiscard]] virtual bool bloomEnabled() const = 0;
    [[nodiscard]] virtual SkyQuality skyQuality() const = 0;
    [[nodiscard]] virtual const char* skyQualityName() const = 0;
    [[nodiscard]] virtual const RendererDebugOptions& debugOptions() const = 0;
};

} // namespace pcolonist
