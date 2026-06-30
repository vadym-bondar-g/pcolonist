#include "pcolonist/render/Renderer.hpp"

#include "pcolonist/assets/AssetSystem.hpp"
#include "pcolonist/core/ApplicationConfig.hpp"
#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/render/Camera.hpp"
#include "pcolonist/render/Mesh.hpp"
#include "pcolonist/render/PngLoader.hpp"
#include "pcolonist/render/Shader.hpp"
#include "pcolonist/world/WeatherSystem.hpp"

#include <glad/gl.h>
#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr int maxFireLights = 4;
constexpr float batchFloatQuantization = 1024.0F;
constexpr std::array<std::string_view, maxFireLights> fireLightPositionUniforms = {
    "fireLightPositions[0]",
    "fireLightPositions[1]",
    "fireLightPositions[2]",
    "fireLightPositions[3]",
};
constexpr std::array<std::string_view, maxFireLights> fireLightColorUniforms = {
    "fireLightColors[0]",
    "fireLightColors[1]",
    "fireLightColors[2]",
    "fireLightColors[3]",
};
constexpr std::array<std::string_view, maxFireLights> fireLightIntensityUniforms = {
    "fireLightIntensities[0]",
    "fireLightIntensities[1]",
    "fireLightIntensities[2]",
    "fireLightIntensities[3]",
};
constexpr std::array<std::string_view, maxFireLights> fireLightFalloffUniforms = {
    "fireLightFalloffs[0]",
    "fireLightFalloffs[1]",
    "fireLightFalloffs[2]",
    "fireLightFalloffs[3]",
};

int quantizedBatchFloat(float value) {
    return std::isfinite(value) ? static_cast<int>(std::lround(value * batchFloatQuantization)) : 0;
}

glm::mat4 modelMatrix(const pcolonist::Transform& transform) {
    glm::mat4 model = glm::translate(glm::mat4(1.0F), transform.position);
    model = glm::rotate(model, transform.rotation.x, {1.0F, 0.0F, 0.0F});
    model = glm::rotate(model, transform.rotation.y, {0.0F, 1.0F, 0.0F});
    model = glm::rotate(model, transform.rotation.z, {0.0F, 0.0F, 1.0F});
    return glm::scale(model, transform.scale);
}

struct FireLightUniform {
    glm::vec3 position{0.0F};
    glm::vec3 color{1.0F, 0.32F, 0.05F};
    float intensity = 0.0F;
    float falloff = 0.72F;
};

struct DrawState {
    bool initialized = false;
    bool blend = false;
    unsigned int blendSource = GL_ONE;
    unsigned int blendDestination = GL_ZERO;
    bool depthMask = true;
};

void applyDrawState(DrawState& state, bool blend, unsigned int blendSource, unsigned int blendDestination, bool depthMask) {
    if (!state.initialized || state.blend != blend) {
        blend ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
        state.blend = blend;
    }
    if (blend && (!state.initialized
        || state.blendSource != blendSource
        || state.blendDestination != blendDestination)) {
        glBlendFunc(blendSource, blendDestination);
        state.blendSource = blendSource;
        state.blendDestination = blendDestination;
    }
    if (!state.initialized || state.depthMask != depthMask) {
        glDepthMask(depthMask ? GL_TRUE : GL_FALSE);
        state.depthMask = depthMask;
    }
    state.initialized = true;
}

struct FrustumPlane {
    glm::vec3 normal{0.0F};
    float distance = 0.0F;
};

using Frustum = std::array<FrustumPlane, 6>;

FrustumPlane normalizedPlane(glm::vec4 plane) {
    const float length = glm::length(glm::vec3{plane});
    return length > 0.0F
        ? FrustumPlane{glm::vec3{plane} / length, plane.w / length}
        : FrustumPlane{};
}

Frustum cameraFrustum(const pcolonist::Camera& camera, float aspectRatio, float farPlane) {
    const glm::mat4 clip = glm::perspective(glm::radians(70.0F), aspectRatio, 0.1F, farPlane)
        * camera.viewMatrix();
    return {
        normalizedPlane({clip[0][3] + clip[0][0], clip[1][3] + clip[1][0], clip[2][3] + clip[2][0], clip[3][3] + clip[3][0]}),
        normalizedPlane({clip[0][3] - clip[0][0], clip[1][3] - clip[1][0], clip[2][3] - clip[2][0], clip[3][3] - clip[3][0]}),
        normalizedPlane({clip[0][3] + clip[0][1], clip[1][3] + clip[1][1], clip[2][3] + clip[2][1], clip[3][3] + clip[3][1]}),
        normalizedPlane({clip[0][3] - clip[0][1], clip[1][3] - clip[1][1], clip[2][3] - clip[2][1], clip[3][3] - clip[3][1]}),
        normalizedPlane({clip[0][3] + clip[0][2], clip[1][3] + clip[1][2], clip[2][3] + clip[2][2], clip[3][3] + clip[3][2]}),
        normalizedPlane({clip[0][3] - clip[0][2], clip[1][3] - clip[1][2], clip[2][3] - clip[2][2], clip[3][3] - clip[3][2]}),
    };
}

bool sphereInFrustum(const Frustum& frustum, glm::vec3 center, float radius) {
    return std::all_of(frustum.begin(), frustum.end(), [center, radius](const FrustumPlane& plane) {
        return glm::dot(plane.normal, center) + plane.distance >= -radius;
    });
}

bool sphereBehindCamera(glm::vec3 cameraPosition, glm::vec3 cameraFront, glm::vec3 center, float radius) {
    return glm::dot(center - cameraPosition, cameraFront) < -radius;
}

float meshRadius(const pcolonist::Mesh& mesh) {
    float radius = 0.0F;
    for (const pcolonist::Vertex& vertex : mesh.vertices) {
        radius = std::max(radius, glm::length(vertex.position));
    }
    return radius;
}

float cachedMeshRadius(const pcolonist::Mesh& mesh, std::unordered_map<const pcolonist::Mesh*, float>& cache) {
    if (const auto iterator = cache.find(&mesh); iterator != cache.end()) {
        return iterator->second;
    }
    return cache.emplace(&mesh, meshRadius(mesh)).first->second;
}

std::array<FireLightUniform, maxFireLights> collectFireLights(pcolonist::Registry& registry, int& count) {
    std::array<FireLightUniform, maxFireLights> lights{};
    count = 0;
    registry.each<pcolonist::Transform, pcolonist::FireLight>(
        [&lights, &count](pcolonist::Entity, const pcolonist::Transform& transform, const pcolonist::FireLight& light) {
            if (count >= maxFireLights) {
                return;
            }
            lights[static_cast<std::size_t>(count)] = FireLightUniform{
                transform.position,
                light.color,
                light.intensity,
                light.falloff,
            };
            ++count;
        });
    return lights;
}

} // namespace

namespace pcolonist {

Renderer::GlHandle::GlHandle(Kind kind, unsigned int id)
    : kind_(kind),
      id_(id) {}

Renderer::GlHandle::~GlHandle() {
    destroy();
}

Renderer::GlHandle::GlHandle(GlHandle&& other) noexcept
    : kind_(other.kind_),
      id_(std::exchange(other.id_, 0)) {}

Renderer::GlHandle& Renderer::GlHandle::operator=(GlHandle&& other) noexcept {
    if (this != &other) {
        destroy();
        kind_ = other.kind_;
        id_ = std::exchange(other.id_, 0);
    }
    return *this;
}

void Renderer::GlHandle::reset(Kind kind, unsigned int id) {
    destroy();
    kind_ = kind;
    id_ = id;
}

unsigned int Renderer::GlHandle::get() const {
    return id_;
}

bool Renderer::GlHandle::valid() const {
    return id_ != 0;
}

Renderer::GlHandle::operator unsigned int() const {
    return id_;
}

void Renderer::GlHandle::destroy() {
    if (id_ == 0) {
        return;
    }
    switch (kind_) {
    case Kind::Buffer:
        glDeleteBuffers(1, &id_);
        break;
    case Kind::Texture:
        glDeleteTextures(1, &id_);
        break;
    case Kind::Framebuffer:
        glDeleteFramebuffers(1, &id_);
        break;
    case Kind::VertexArray:
        glDeleteVertexArrays(1, &id_);
        break;
    }
    id_ = 0;
}

Renderer::RenderQualityConfig Renderer::qualityConfig(GraphicsQuality quality) {
    switch (quality) {
    case GraphicsQuality::Low:
        return {{32.0F, 128.0F}, 520.0F, 64.0F, 520.0F, 32.0F, 320.0F, 48.0F, 1024, 0};
    case GraphicsQuality::Medium:
        return {{40.0F, 160.0F}, 640.0F, 64.0F, 640.0F, 32.0F, 420.0F, 66.0F, 1536, 1};
    case GraphicsQuality::High:
        return {{48.0F, 190.0F}, 760.0F, 64.0F, 760.0F, 32.0F, 520.0F, 82.0F, 2048, 2};
    case GraphicsQuality::Cinematic:
        return {{64.0F, 240.0F}, 900.0F, 96.0F, 900.0F, 48.0F, 680.0F, 110.0F, 3072, 2};
    }
    return {};
}

bool Renderer::BatchKey::operator==(const BatchKey& other) const {
    return mesh == other.mesh
        && water == other.water
        && waterKind == other.waterKind
        && waterFlowDirectionX == other.waterFlowDirectionX
        && waterFlowDirectionY == other.waterFlowDirectionY
        && waterFoamStrength == other.waterFoamStrength
        && terrain == other.terrain
        && lava == other.lava
        && fire == other.fire
        && smoke == other.smoke
        && grass == other.grass;
}

std::size_t Renderer::BatchKeyHash::operator()(const BatchKey& key) const {
    std::size_t hash = std::hash<const Mesh*>{}(key.mesh);
    const auto mix = [&hash](bool value) {
        hash ^= std::hash<int>{}(value ? 1 : 0) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    };
    mix(key.water);
    hash ^= std::hash<int>{}(key.waterKind) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    hash ^= std::hash<int>{}(key.waterFlowDirectionX) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    hash ^= std::hash<int>{}(key.waterFlowDirectionY) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    hash ^= std::hash<int>{}(key.waterFoamStrength) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
    mix(key.terrain);
    mix(key.lava);
    mix(key.fire);
    mix(key.smoke);
    mix(key.grass);
    return hash;
}

Renderer::Renderer()
    : Renderer(PCOLONIST_ASSET_DIR) {}

Renderer::Renderer(const AssetSystem& assets)
    : Renderer(assets.resolve("."), &assets) {}

Renderer::Renderer(std::filesystem::path assetRoot)
    : Renderer(std::move(assetRoot), nullptr) {}

Renderer::Renderer(std::filesystem::path assetRoot, const AssetSystem* assets)
    : shaders_(assetRoot / "shaders"),
      assetRoot_(std::move(assetRoot)),
      assets_(assets) {
    shaders_.preload();
    unsigned int screenVertexArray = 0;
    glGenVertexArrays(1, &screenVertexArray);
    screenVertexArray_.reset(GlHandle::Kind::VertexArray, screenVertexArray);
    createShadowMap();
    createRenderTargets();
    loadTerrainTextures();
}

Renderer::~Renderer() {
    releaseRenderTargets();
    releaseShadowMap();
    meshes_.clear();
    textures_.clear();
    meshRadii_.clear();
    modelMatrices_.clear();
    renderScratch_.items.clear();
    renderScratch_.batches.clear();
    renderScratch_.drawOrder.clear();
    renderScratch_.transparentOrder.clear();
    renderScratch_.batchIndices.clear();
    renderScratch_.items.shrink_to_fit();
    renderScratch_.batches.shrink_to_fit();
    renderScratch_.drawOrder.shrink_to_fit();
    renderScratch_.transparentOrder.shrink_to_fit();
}

void Renderer::resize(int width, int height) {
    width_ = width;
    height_ = height;
    releaseRenderTargets();
    createRenderTargets();
    glViewport(0, 0, width, height);
}

void Renderer::preloadResources(Registry& registry) {
    registry.each<MeshRenderer>([this](Entity, const MeshRenderer& renderer) {
        if (!renderer.mesh) {
            return;
        }
        upload(*renderer.mesh);
        for (const MeshDraw& draw : renderer.mesh->draws) {
            if (!draw.diffuseTexture.empty()) {
                static_cast<void>(loadTexture(draw.diffuseTexture));
            }
        }
    });
}

void Renderer::collectRenderItems(
    Registry& registry,
    const Camera& camera,
    float aspectRatio,
    float terrainDistance,
    float objectDistance) {
    renderScratch_.items.clear();
    const glm::vec3& cameraPosition = camera.position();
    const glm::vec3& cameraFront = camera.front();
    const Frustum frustum = cameraFrustum(camera, aspectRatio, qualityConfig_.cameraFarPlane);
    registry.each<Transform, MeshRenderer>(
        [this, &registry, cameraPosition, cameraFront, &frustum, terrainDistance, objectDistance](
            Entity entity,
            const Transform& transform,
            const MeshRenderer& renderer) {
            if (!renderer.mesh) {
                return;
            }
            const bool water = registry.has<WaterSurface>(entity);
            const bool terrain = registry.has<TerrainSurface>(entity);
            const bool lava = registry.has<LavaSurface>(entity);
            const bool fire = registry.has<FireSurface>(entity);
            const bool smoke = registry.has<SmokeSurface>(entity);
            const bool grass = registry.has<GrassSurface>(entity);
            if ((!debugOptions_.showTerrain && terrain)
                || (!debugOptions_.showWater && water)
                || (!debugOptions_.showLava && lava)
                || (!debugOptions_.showFire && (fire || smoke))
                || (!debugOptions_.showResources && registry.has<ResourceNode>(entity))
                || (!debugOptions_.showObjects && !terrain && !water && !lava && !fire && !smoke && !grass)) {
                return;
            }

            const glm::vec3 cameraDelta = transform.position - cameraPosition;
            const float distanceSquared = glm::dot(cameraDelta, cameraDelta);
            const float scale = std::max({transform.scale.x, transform.scale.y, transform.scale.z});
            const float grassDistance = qualityConfig_.grassDrawDistance + scale * 3.0F;
            if (grass && distanceSquared > grassDistance * grassDistance) {
                return;
            }
            glm::vec3 cullCenter = transform.position;
            float cullRadius = cachedMeshRadius(*renderer.mesh, meshRadii_) * scale;
            float chunkDistance = 0.0F;
            float chunkRadius = 0.0F;
            float terrainLodMinDistance = 0.0F;
            bool terrainChunk = false;
            bool terrainLod = false;
            int waterKind = 0;
            glm::vec2 waterFlowDirection{0.0F};
            float waterFoamStrength = 1.0F;
            if (water) {
                const WaterSurface& surface = registry.get<WaterSurface>(entity);
                waterKind = static_cast<int>(surface.kind);
                waterFlowDirection = surface.flowDirection;
                waterFoamStrength = surface.foamStrength;
            }
            if (registry.has<TerrainChunk>(entity)) {
                const TerrainChunk& chunk = registry.get<TerrainChunk>(entity);
                chunkDistance = glm::distance(glm::vec2{cameraPosition.x, cameraPosition.z}, chunk.center);
                if (chunkDistance > chunk.radius + terrainDistance) {
                    return;
                }
                if (registry.has<TerrainLod>(entity)) {
                    const TerrainLod& lod = registry.get<TerrainLod>(entity);
                    if (chunkDistance < lod.minDistance || chunkDistance >= lod.maxDistance) {
                        return;
                    }
                    terrainLodMinDistance = lod.minDistance;
                    terrainLod = true;
                }
                cullRadius = chunk.radius;
                chunkRadius = chunk.radius;
                cullCenter = {chunk.center.x, transform.position.y, chunk.center.y};
                terrainChunk = true;
            }
            const bool keepOffscreenWater = water && waterKind == static_cast<int>(WaterKind::Ocean);
            if (sphereBehindCamera(cameraPosition, cameraFront, cullCenter, cullRadius) && !keepOffscreenWater) {
                return;
            }
            if (!sphereInFrustum(frustum, cullCenter, cullRadius) && !keepOffscreenWater) {
                return;
            }

            const float objectDistanceLimit = objectDistance + scale * 10.0F;
            if (!terrain && !water && distanceSquared > objectDistanceLimit * objectDistanceLimit) {
                return;
            }

            renderScratch_.items.push_back({
                renderer.mesh.get(),
                cachedModelMatrix(entity, transform),
                distanceSquared,
                cullRadius,
                scale * 10.0F,
                chunkDistance,
                chunkRadius,
                terrainLodMinDistance,
                waterKind,
                waterFlowDirection,
                waterFoamStrength,
                water,
                terrain,
                lava,
                fire,
                smoke,
                grass,
                terrainChunk,
                terrainLod,
            });
        });
}

std::size_t Renderer::buildBatches(
    float terrainDistance,
    float objectDistance,
    bool shadows,
    float minimumShadowRadius) {
    for (std::size_t index = 0; index < renderScratch_.batchCount; ++index) {
        renderScratch_.batches[index].models.clear();
    }
    renderScratch_.batchCount = 0;
    renderScratch_.batchIndices.clear();
    renderScratch_.batchIndices.reserve(renderScratch_.items.size());

    for (const RenderItem& item : renderScratch_.items) {
        if (shadows && (item.water || item.lava || item.fire || item.smoke || item.grass)) {
            continue;
        }
        if (shadows && item.cullRadius < minimumShadowRadius) {
            continue;
        }
        if (item.terrainChunk && item.chunkDistance > item.chunkRadius + terrainDistance) {
            continue;
        }
        if (shadows && item.terrainLod && item.terrainLodMinDistance > 0.0F) {
            continue;
        }
        const float objectDistanceLimit = objectDistance + item.objectDistancePadding;
        if (!item.terrain && !item.water && item.distanceSquared > objectDistanceLimit * objectDistanceLimit) {
            continue;
        }

        const BatchKey key{
            item.mesh,
            item.water,
            item.waterKind,
            quantizedBatchFloat(item.waterFlowDirection.x),
            quantizedBatchFloat(item.waterFlowDirection.y),
            quantizedBatchFloat(item.waterFoamStrength),
            item.terrain,
            item.lava,
            item.fire,
            item.smoke,
            item.grass,
        };
        const auto [batchIndex, inserted] = renderScratch_.batchIndices.emplace(key, renderScratch_.batchCount);
        if (inserted) {
            if (renderScratch_.batchCount == renderScratch_.batches.size()) {
                renderScratch_.batches.emplace_back();
            }
            RenderBatch& batch = renderScratch_.batches[renderScratch_.batchCount];
            batch.mesh = key.mesh;
            batch.water = item.water;
            batch.waterKind = item.waterKind;
            batch.waterFlowDirection = item.waterFlowDirection;
            batch.waterFoamStrength = item.waterFoamStrength;
            batch.sortDistanceSquared = item.distanceSquared;
            batch.terrain = item.terrain;
            batch.lava = item.lava;
            batch.fire = item.fire;
            batch.smoke = item.smoke;
            batch.grass = item.grass;
            ++renderScratch_.batchCount;
        }
        renderScratch_.batches[batchIndex->second].sortDistanceSquared =
            std::max(renderScratch_.batches[batchIndex->second].sortDistanceSquared, item.distanceSquared);
        renderScratch_.batches[batchIndex->second].models.push_back(item.model);
    }
    return renderScratch_.batchCount;
}

void Renderer::render(const Camera& camera, Registry& registry, const WeatherSystem& weather) {
    if (width_ <= 0 || height_ <= 0) {
        return;
    }
    const float aspectRatio = height_ > 0 ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0F;
    const float maximumShadowTerrainDistance = shadowsEnabled_ ? qualityConfig_.shadowRanges.back() + 46.0F : 0.0F;
    const float maximumShadowObjectDistance = shadowsEnabled_ ? qualityConfig_.shadowRanges.back() + 32.0F : 0.0F;
    collectRenderItems(
        registry,
        camera,
        aspectRatio,
        std::max(debugOptions_.terrainDrawDistance, maximumShadowTerrainDistance),
        std::max(debugOptions_.objectDrawDistance, maximumShadowObjectDistance));
    renderShadowMap(camera, weather);

    glBindFramebuffer(GL_FRAMEBUFFER, hdrFramebuffer_);
    glViewport(0, 0, width_, height_);
    skybox_.render(shaders_.get("sky"), camera, weather, aspectRatio, skyQuality_);

    Shader& shader = shaders_.get("scene");
    shader.use();
    shader.setMat4("projection", glm::perspective(glm::radians(70.0F), aspectRatio, 0.1F, qualityConfig_.cameraFarPlane));
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
    shader.setVec2("windDirection", weather.cloudWind());
    shader.setFloat("windStrength", 0.68F + weather.cloudiness() * 0.28F + weather.stormStrength() * 0.56F);
    shader.setInt("sceneQuality", qualityConfig_.sceneQuality);
    shader.setInt("shadowMapNear", 3);
    shader.setInt("shadowMapFar", 4);
    shader.setInt("diffuseTexture", 0);
    shader.setInt("terrainEarthTexture", 0);
    shader.setInt("terrainSandTexture", 1);
    shader.setInt("terrainBasaltTexture", 2);
    shader.setInt("shadowsEnabled", shadowsEnabled_ ? 1 : 0);
    int fireLightCount = 0;
    const auto fireLights = collectFireLights(registry, fireLightCount);
    shader.setInt("fireLightCount", fireLightCount);
    if (fireLightCount > 0) {
        for (int index = 0; index < fireLightCount; ++index) {
            const std::size_t lightIndex = static_cast<std::size_t>(index);
            shader.setVec3(fireLightPositionUniforms[lightIndex], fireLights[lightIndex].position);
            shader.setVec3(fireLightColorUniforms[lightIndex], fireLights[lightIndex].color);
            shader.setFloat(fireLightIntensityUniforms[lightIndex], fireLights[lightIndex].intensity);
            shader.setFloat(fireLightFalloffUniforms[lightIndex], fireLights[lightIndex].falloff);
        }
    }
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, shadowDepths_[0]);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, shadowDepths_[1]);

    const std::size_t batchCount = buildBatches(
        debugOptions_.terrainDrawDistance,
        debugOptions_.objectDrawDistance,
        false,
        0.0F);
    const auto isTransparent = [](const RenderBatch& batch) {
        return batch.water || batch.lava || batch.fire || batch.smoke;
    };
    renderScratch_.drawOrder.clear();
    renderScratch_.transparentOrder.clear();
    renderScratch_.drawOrder.reserve(batchCount);
    renderScratch_.transparentOrder.reserve(batchCount);
    for (std::size_t batchIndex = 0; batchIndex < batchCount; ++batchIndex) {
        if (isTransparent(renderScratch_.batches[batchIndex])) {
            renderScratch_.transparentOrder.push_back(batchIndex);
        } else {
            renderScratch_.drawOrder.push_back(batchIndex);
        }
    }
    std::sort(
        renderScratch_.drawOrder.begin(),
        renderScratch_.drawOrder.end(),
        [this](std::size_t leftIndex, std::size_t rightIndex) {
            const RenderBatch& left = renderScratch_.batches[leftIndex];
            const RenderBatch& right = renderScratch_.batches[rightIndex];
            const auto bucket = [](const RenderBatch& batch) {
                if (batch.terrain) {
                    return 0;
                }
                if (batch.grass) {
                    return 1;
                }
                return 2;
            };
            const int leftBucket = bucket(left);
            const int rightBucket = bucket(right);
            if (leftBucket != rightBucket) {
                return leftBucket < rightBucket;
            }
            if (left.mesh != right.mesh) {
                return std::less<const Mesh*>{}(left.mesh, right.mesh);
            }
            if (left.waterKind != right.waterKind) {
                return left.waterKind < right.waterKind;
            }
            if (left.waterFlowDirection.x != right.waterFlowDirection.x) {
                return left.waterFlowDirection.x < right.waterFlowDirection.x;
            }
            if (left.waterFlowDirection.y != right.waterFlowDirection.y) {
                return left.waterFlowDirection.y < right.waterFlowDirection.y;
            }
            return left.waterFoamStrength < right.waterFoamStrength;
        });
    std::sort(
        renderScratch_.transparentOrder.begin(),
        renderScratch_.transparentOrder.end(),
        [this](std::size_t leftIndex, std::size_t rightIndex) {
            const RenderBatch& left = renderScratch_.batches[leftIndex];
            const RenderBatch& right = renderScratch_.batches[rightIndex];
            const auto order = [](const RenderBatch& batch) {
                if (batch.smoke) {
                    return 4;
                }
                if (batch.fire) {
                    return 3;
                }
                return 2;
            };
            const int leftOrder = order(left);
            const int rightOrder = order(right);
            if (leftOrder != rightOrder) {
                return leftOrder < rightOrder;
            }
            return left.sortDistanceSquared > right.sortDistanceSquared;
        });
    renderScratch_.drawOrder.insert(
        renderScratch_.drawOrder.end(),
        renderScratch_.transparentOrder.begin(),
        renderScratch_.transparentOrder.end());
    if (debugOptions_.wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    DrawState drawState;
    struct BatchUniformState {
        float water = -1.0F;
        int waterKind = -1;
        float waterFlowDirectionX = -1.0F;
        float waterFlowDirectionY = -1.0F;
        float waterFoamStrength = -1.0F;
        float terrain = -1.0F;
        float lava = -1.0F;
        float fire = -1.0F;
        float smoke = -1.0F;
        float grass = -1.0F;
        int hasDiffuseTexture = -1;
        float roughness = -1.0F;
        float specularStrength = -1.0F;
        float emissiveColorR = -1.0F;
        float emissiveColorG = -1.0F;
        float emissiveColorB = -1.0F;
    };
    BatchUniformState batchState;
    unsigned int lastVertexArray = 0;
    unsigned int lastTerrainTexture0 = 0;
    unsigned int lastTerrainTexture1 = 0;
    unsigned int lastTerrainTexture2 = 0;
    unsigned int lastDiffuseTexture = 0;
    const auto sameDrawState = [](const MeshDraw& left, const MeshDraw& right) {
        return left.firstIndex + left.indexCount == right.firstIndex
            && left.diffuseTexture == right.diffuseTexture
            && left.roughness == right.roughness
            && left.specularStrength == right.specularStrength
            && left.emissive == right.emissive;
    };
    for (const std::size_t batchIndex : renderScratch_.drawOrder) {
        const RenderBatch& batch = renderScratch_.batches[batchIndex];
        GpuMesh& gpuMesh = upload(*batch.mesh);
        const float water = batch.water ? 1.0F : 0.0F;
        if (batchState.water != water) {
            batchState.water = water;
            shader.setFloat("water", water);
        }
        if (batchState.waterKind != batch.waterKind) {
            batchState.waterKind = batch.waterKind;
            shader.setInt("waterKind", batch.waterKind);
        }
        if (batchState.waterFlowDirectionX != batch.waterFlowDirection.x
            || batchState.waterFlowDirectionY != batch.waterFlowDirection.y) {
            batchState.waterFlowDirectionX = batch.waterFlowDirection.x;
            batchState.waterFlowDirectionY = batch.waterFlowDirection.y;
            shader.setVec2("waterFlowDirection", batch.waterFlowDirection);
        }
        if (batchState.waterFoamStrength != batch.waterFoamStrength) {
            batchState.waterFoamStrength = batch.waterFoamStrength;
            shader.setFloat("waterFoamStrength", batch.waterFoamStrength);
        }
        const float terrain = batch.terrain ? 1.0F : 0.0F;
        if (batchState.terrain != terrain) {
            batchState.terrain = terrain;
            shader.setFloat("terrain", terrain);
        }
        const float lava = batch.lava ? 1.0F : 0.0F;
        if (batchState.lava != lava) {
            batchState.lava = lava;
            shader.setFloat("lava", lava);
        }
        const float fire = batch.fire ? 1.0F : 0.0F;
        if (batchState.fire != fire) {
            batchState.fire = fire;
            shader.setFloat("fire", fire);
        }
        const float smoke = batch.smoke ? 1.0F : 0.0F;
        if (batchState.smoke != smoke) {
            batchState.smoke = smoke;
            shader.setFloat("smoke", smoke);
        }
        const float grass = batch.grass ? 1.0F : 0.0F;
        if (batchState.grass != grass) {
            batchState.grass = grass;
            shader.setFloat("grass", grass);
        }
        if (batch.fire) {
            applyDrawState(drawState, true, GL_SRC_ALPHA, GL_ONE, false);
        } else if (batch.water || batch.lava) {
            applyDrawState(drawState, true, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, false);
        } else if (batch.smoke) {
            applyDrawState(drawState, true, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, false);
        } else {
            applyDrawState(drawState, false, GL_ONE, GL_ZERO, true);
        }
        if (batch.terrain) {
            if (lastTerrainTexture0 != terrainEarthTexture_) {
                lastTerrainTexture0 = terrainEarthTexture_;
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, terrainEarthTexture_);
            }
            if (lastTerrainTexture1 != terrainSandTexture_) {
                lastTerrainTexture1 = terrainSandTexture_;
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, terrainSandTexture_);
            }
            if (lastTerrainTexture2 != terrainBasaltTexture_) {
                lastTerrainTexture2 = terrainBasaltTexture_;
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, terrainBasaltTexture_);
            }
        }
        if (lastVertexArray != gpuMesh.vertexArray.get()) {
            lastVertexArray = gpuMesh.vertexArray.get();
            glBindVertexArray(gpuMesh.vertexArray);
        }
        uploadInstances(gpuMesh, batch.models);
        if (batch.mesh->draws.empty()) {
            if (batchState.hasDiffuseTexture != 0) {
                batchState.hasDiffuseTexture = 0;
                shader.setInt("hasDiffuseTexture", 0);
            }
            if (batchState.roughness != 0.72F) {
                batchState.roughness = 0.72F;
                shader.setFloat("roughness", 0.72F);
            }
            if (batchState.specularStrength != 0.2F) {
                batchState.specularStrength = 0.2F;
                shader.setFloat("specularStrength", 0.2F);
            }
            if (batchState.emissiveColorR != 0.0F || batchState.emissiveColorG != 0.0F || batchState.emissiveColorB != 0.0F) {
                batchState.emissiveColorR = 0.0F;
                batchState.emissiveColorG = 0.0F;
                batchState.emissiveColorB = 0.0F;
                shader.setVec3("emissiveColor", {0.0F, 0.0F, 0.0F});
            }
            glDrawElementsInstanced(
                GL_TRIANGLES, static_cast<int>(gpuMesh.indexCount), GL_UNSIGNED_INT, nullptr,
                static_cast<int>(batch.models.size()));
            continue;
        }
        for (std::size_t drawIndex = 0; drawIndex < batch.mesh->draws.size();) {
            const MeshDraw& draw = batch.mesh->draws[drawIndex];
            std::size_t mergedIndexCount = draw.indexCount;
            std::size_t nextDrawIndex = drawIndex + 1;
            while (nextDrawIndex < batch.mesh->draws.size()
                && sameDrawState(batch.mesh->draws[nextDrawIndex - 1], batch.mesh->draws[nextDrawIndex])) {
                mergedIndexCount += batch.mesh->draws[nextDrawIndex].indexCount;
                ++nextDrawIndex;
            }
            const bool textured = !draw.diffuseTexture.empty();
            const int hasTexture = textured ? 1 : 0;
            if (batchState.hasDiffuseTexture != hasTexture) {
                batchState.hasDiffuseTexture = hasTexture;
                shader.setInt("hasDiffuseTexture", hasTexture);
            }
            if (batchState.roughness != draw.roughness) {
                batchState.roughness = draw.roughness;
                shader.setFloat("roughness", draw.roughness);
            }
            if (batchState.specularStrength != draw.specularStrength) {
                batchState.specularStrength = draw.specularStrength;
                shader.setFloat("specularStrength", draw.specularStrength);
            }
            if (batchState.emissiveColorR != draw.emissive.x
                || batchState.emissiveColorG != draw.emissive.y
                || batchState.emissiveColorB != draw.emissive.z) {
                batchState.emissiveColorR = draw.emissive.x;
                batchState.emissiveColorG = draw.emissive.y;
                batchState.emissiveColorB = draw.emissive.z;
                shader.setVec3("emissiveColor", draw.emissive);
            }
            if (textured) {
                const unsigned int texture = loadTexture(draw.diffuseTexture);
                if (lastDiffuseTexture != texture) {
                    lastDiffuseTexture = texture;
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, texture);
                }
            }
            glDrawElementsInstanced(
                GL_TRIANGLES,
                static_cast<int>(mergedIndexCount),
                GL_UNSIGNED_INT,
                reinterpret_cast<void*>(draw.firstIndex * sizeof(std::uint32_t)),
                static_cast<int>(batch.models.size()));
            drawIndex = nextDrawIndex;
        }
    }
    applyDrawState(drawState, false, GL_ONE, GL_ZERO, true);
    if (debugOptions_.wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    bool underwater = false;
    registry.each<Transform, WaterSurface>(
        [&underwater, &camera](Entity, const Transform& transform, const WaterSurface& surface) {
            const glm::vec2 distance = glm::abs(glm::vec2{
                camera.position().x - transform.position.x,
                camera.position().z - transform.position.z,
            });
            underwater = underwater
                || (surface.affectsPhysics
                    && distance.x <= surface.halfExtents.x * transform.scale.x
                    && distance.y <= surface.halfExtents.y * transform.scale.z
                    && camera.position().y < transform.position.y);
        });
    postProcess(weather, underwater);
}

void Renderer::releaseUnusedMeshes(Registry& registry) {
    std::unordered_set<const Mesh*> liveMeshes;
    std::unordered_set<Entity> liveMeshEntities;
    registry.each<Transform, MeshRenderer>(
        [&liveMeshes, &liveMeshEntities](Entity entity, const Transform&, const MeshRenderer& renderer) {
            if (renderer.mesh) {
                liveMeshes.insert(renderer.mesh.get());
                liveMeshEntities.insert(entity);
            }
        });

    for (auto iterator = meshes_.begin(); iterator != meshes_.end();) {
        if (liveMeshes.contains(iterator->first)) {
            ++iterator;
            continue;
        }
        meshRadii_.erase(iterator->first);
        iterator = meshes_.erase(iterator);
    }
    for (auto iterator = meshRadii_.begin(); iterator != meshRadii_.end();) {
        if (liveMeshes.contains(iterator->first)) {
            ++iterator;
            continue;
        }
        iterator = meshRadii_.erase(iterator);
    }
    for (auto iterator = modelMatrices_.begin(); iterator != modelMatrices_.end();) {
        if (liveMeshEntities.contains(iterator->first)) {
            ++iterator;
            continue;
        }
        iterator = modelMatrices_.erase(iterator);
    }
}

void Renderer::setShadowsEnabled(bool enabled) {
    shadowsEnabled_ = enabled;
}

void Renderer::setBloomEnabled(bool enabled) {
    bloomEnabled_ = enabled;
}

void Renderer::setGraphicsQuality(GraphicsQuality quality) {
    const RenderQualityConfig nextConfig = qualityConfig(quality);
    const bool recreateShadowMap = nextConfig.shadowSize != qualityConfig_.shadowSize;
    qualityConfig_ = nextConfig;
    setDebugOptions(debugOptions_);
    if (recreateShadowMap) {
        releaseShadowMap();
        createShadowMap();
    }
}

void Renderer::setSkyQuality(SkyQuality quality) {
    skyQuality_ = quality;
}

void Renderer::setDebugOptions(RendererDebugOptions options) {
    options.terrainDrawDistance = std::clamp(
        options.terrainDrawDistance,
        qualityConfig_.terrainDrawDistanceMinimum,
        qualityConfig_.terrainDrawDistanceMaximum);
    options.objectDrawDistance = std::clamp(
        options.objectDrawDistance,
        qualityConfig_.objectDrawDistanceMinimum,
        qualityConfig_.objectDrawDistanceMaximum);
    debugOptions_ = options;
}

void Renderer::cycleSkyQuality() {
    switch (skyQuality_) {
    case SkyQuality::Off:
        skyQuality_ = SkyQuality::Low;
        break;
    case SkyQuality::Low:
        skyQuality_ = SkyQuality::Medium;
        break;
    case SkyQuality::Medium:
        skyQuality_ = SkyQuality::High;
        break;
    case SkyQuality::High:
        skyQuality_ = SkyQuality::Off;
        break;
    }
}

bool Renderer::shadowsEnabled() const {
    return shadowsEnabled_;
}

bool Renderer::bloomEnabled() const {
    return bloomEnabled_;
}

SkyQuality Renderer::skyQuality() const {
    return skyQuality_;
}

const char* Renderer::skyQualityName() const {
    switch (skyQuality_) {
    case SkyQuality::Off:
        return "OFF";
    case SkyQuality::Low:
        return "LOW";
    case SkyQuality::Medium:
        return "MED";
    case SkyQuality::High:
        return "HIGH";
    }
    return "UNKNOWN";
}

const RendererDebugOptions& Renderer::debugOptions() const {
    return debugOptions_;
}

const glm::mat4& Renderer::cachedModelMatrix(Entity entity, const Transform& transform) {
    CachedModelMatrix& cached = modelMatrices_[entity];
    if (cached.position != transform.position
        || cached.rotation != transform.rotation
        || cached.scale != transform.scale) {
        cached.position = transform.position;
        cached.rotation = transform.rotation;
        cached.scale = transform.scale;
        cached.model = modelMatrix(transform);
    }
    return cached.model;
}

void Renderer::renderShadowMap(const Camera& camera, const WeatherSystem& weather) {
    const glm::vec3 lightDirection = weather.daylight() > 0.05F ? weather.sunDirection() : weather.moonDirection();
    const glm::vec3 center = camera.position();
    for (std::size_t cascade = 0; cascade < qualityConfig_.shadowRanges.size(); ++cascade) {
        const float range = qualityConfig_.shadowRanges[cascade];
        const float texelSize = range * 2.0F / static_cast<float>(qualityConfig_.shadowSize);
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

    glViewport(0, 0, qualityConfig_.shadowSize, qualityConfig_.shadowSize);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    Shader& shader = shaders_.get("shadow");
    shader.use();
    for (std::size_t cascade = 0; cascade < shadowFramebuffers_.size(); ++cascade) {
        const float range = qualityConfig_.shadowRanges[cascade];
        const float terrainShadowDistance = range + (cascade == 0 ? 28.0F : 46.0F);
        const float objectShadowDistance = range + (cascade == 0 ? 22.0F : 32.0F);
        const float minimumShadowRadius = cascade == 0 ? 0.0F : 1.35F;
        const std::size_t batchCount = buildBatches(
            terrainShadowDistance,
            objectShadowDistance,
            true,
            minimumShadowRadius);
        renderScratch_.drawOrder.clear();
        renderScratch_.drawOrder.reserve(batchCount);
        for (std::size_t batchIndex = 0; batchIndex < batchCount; ++batchIndex) {
            renderScratch_.drawOrder.push_back(batchIndex);
        }
        std::sort(
            renderScratch_.drawOrder.begin(),
            renderScratch_.drawOrder.end(),
            [this](std::size_t leftIndex, std::size_t rightIndex) {
                const RenderBatch& left = renderScratch_.batches[leftIndex];
                const RenderBatch& right = renderScratch_.batches[rightIndex];
                if (left.mesh != right.mesh) {
                    return std::less<const Mesh*>{}(left.mesh, right.mesh);
                }
                return left.terrain > right.terrain;
            });
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFramebuffers_[cascade]);
        glClear(GL_DEPTH_BUFFER_BIT);
        shader.setMat4("lightSpaceMatrix", lightSpaceMatrices_[cascade]);
        unsigned int lastVertexArray = 0;
        for (const std::size_t batchIndex : renderScratch_.drawOrder) {
            const RenderBatch& batch = renderScratch_.batches[batchIndex];
            GpuMesh& gpuMesh = upload(*batch.mesh);
            if (lastVertexArray != gpuMesh.vertexArray.get()) {
                lastVertexArray = gpuMesh.vertexArray.get();
                glBindVertexArray(gpuMesh.vertexArray);
            }
            uploadInstances(gpuMesh, batch.models);
            glDrawElementsInstanced(
                GL_TRIANGLES, static_cast<int>(gpuMesh.indexCount), GL_UNSIGNED_INT, nullptr,
                static_cast<int>(batch.models.size()));
        }
    }
    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::uploadInstances(GpuMesh& gpuMesh, const std::vector<glm::mat4>& models) {
    glBindBuffer(GL_ARRAY_BUFFER, gpuMesh.instanceBuffer);
    if (models.empty()) {
        return;
    }
    const std::size_t byteCount = models.size() * sizeof(glm::mat4);
    if (models.size() > gpuMesh.instanceCapacity) {
        const std::size_t nextCapacity = std::max(models.size(), std::max<std::size_t>(1, gpuMesh.instanceCapacity * 2));
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(nextCapacity * sizeof(glm::mat4)),
            nullptr,
            GL_STREAM_DRAW);
        gpuMesh.instanceCapacity = nextCapacity;
    }
    void* mapped = glMapBufferRange(
        GL_ARRAY_BUFFER,
        0,
        static_cast<GLsizeiptr>(byteCount),
        GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
    if (mapped) {
        std::memcpy(mapped, models.data(), byteCount);
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }
}

void Renderer::loadTerrainTextures() {
    terrainEarthTexture_ = loadTexture("textures/terrain/earth.png");
    terrainSandTexture_ = loadTexture("textures/terrain/sand.png");
    terrainBasaltTexture_ = loadTexture("textures/terrain/basalt.png");
}

Renderer::GpuMesh& Renderer::upload(const Mesh& mesh) {
    if (const auto iterator = meshes_.find(&mesh); iterator != meshes_.end()) {
        return iterator->second;
    }
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        throw std::runtime_error("Cannot upload empty mesh");
    }
    GpuMesh gpuMesh;
    gpuMesh.indexCount = mesh.indices.size();
    unsigned int vertexArray = 0;
    unsigned int vertexBuffer = 0;
    unsigned int indexBuffer = 0;
    unsigned int instanceBuffer = 0;
    glGenVertexArrays(1, &vertexArray);
    glGenBuffers(1, &vertexBuffer);
    glGenBuffers(1, &indexBuffer);
    glGenBuffers(1, &instanceBuffer);
    gpuMesh.vertexArray.reset(GlHandle::Kind::VertexArray, vertexArray);
    gpuMesh.vertexBuffer.reset(GlHandle::Kind::Buffer, vertexBuffer);
    gpuMesh.indexBuffer.reset(GlHandle::Kind::Buffer, indexBuffer);
    gpuMesh.instanceBuffer.reset(GlHandle::Kind::Buffer, instanceBuffer);
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
    return meshes_.emplace(&mesh, std::move(gpuMesh)).first->second;
}

unsigned int Renderer::loadTexture(const std::filesystem::path& path) {
    if (const auto iterator = textures_.find(path); iterator != textures_.end()) {
        return iterator->second.get();
    }

    const std::filesystem::path fullPath = assetRoot_ / path;
    Image image;
    try {
        image = assets_ != nullptr
            ? PngLoader::load(path.generic_string(), assets_->readBinary(path))
            : PngLoader::load(fullPath);
    } catch (const std::exception& exception) {
        throw std::runtime_error(
            "Failed to load texture '" + path.generic_string()
            + "' at '" + fullPath.generic_string()
            + "': " + exception.what());
    }
    unsigned int texture = 0;
    glGenTextures(1, &texture);
    GlHandle textureHandle{GlHandle::Kind::Texture, texture};
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
    return textures_.emplace(path, std::move(textureHandle)).first->second.get();
}

} // namespace pcolonist
