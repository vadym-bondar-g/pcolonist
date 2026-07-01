#include "pcolonist/core/Application.hpp"

#include "pcolonist/animation/AnimationSystem.hpp"
#include "pcolonist/assets/MeshLoader.hpp"
#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/gameplay/Enemy.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"
#include "pcolonist/render/MeshFactory.hpp"
#include "pcolonist/render/Renderer.hpp"
#include "pcolonist/render/VulkanSupport.hpp"
#include "pcolonist/serialization/SaveGameSerializer.hpp"
#include "pcolonist/serialization/SceneSerializer.hpp"
#include "pcolonist/world/WorldStreamer.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr float maximumDeltaTime = 0.1F;

struct WaterDefinition {
    glm::vec3 position{0.0F};
    glm::vec2 halfExtents{0.0F};
    bool affectsPhysics = true;
    pcolonist::WaterKind kind = pcolonist::WaterKind::Ocean;
    glm::vec2 flowDirection{0.0F};
    float foamStrength = 1.0F;
};

struct LavaDefinition {
    std::string mesh;
    glm::vec3 position{0.0F};
    glm::vec3 rotation{0.0F};
    glm::vec3 scale{1.0F};
    bool hasFireLight = false;
    glm::vec3 fireColor{1.0F, 0.16F, 0.025F};
    float fireIntensity = 1.0F;
    float fireFalloff = 0.72F;
};

pcolonist::KeyAction toKeyAction(int action) {
    if (action == GLFW_PRESS) {
        return pcolonist::KeyAction::Press;
    }
    if (action == GLFW_REPEAT) {
        return pcolonist::KeyAction::Repeat;
    }
    return pcolonist::KeyAction::Release;
}

std::size_t matchingBrace(std::string_view text, std::size_t open) {
    int depth = 0;
    for (std::size_t index = open; index < text.size(); ++index) {
        if (text[index] == '{') {
            ++depth;
        } else if (text[index] == '}') {
            --depth;
            if (depth == 0) {
                return index;
            }
        }
    }
    return std::string_view::npos;
}

std::pair<std::size_t, std::size_t> objectRange(std::string_view text, std::string_view key, std::size_t start = 0) {
    const std::string quoted = "\"" + std::string(key) + "\"";
    const std::size_t keyPosition = text.find(quoted, start);
    if (keyPosition == std::string_view::npos) {
        return {std::string_view::npos, std::string_view::npos};
    }
    const std::size_t open = text.find('{', keyPosition + quoted.size());
    if (open == std::string_view::npos) {
        return {std::string_view::npos, std::string_view::npos};
    }
    return {open, matchingBrace(text, open)};
}

float numberField(std::string_view text, std::string_view key, std::size_t start, std::size_t end, float fallback = 0.0F) {
    const std::string quoted = "\"" + std::string(key) + "\"";
    const std::size_t keyPosition = text.find(quoted, start);
    if (keyPosition == std::string_view::npos || keyPosition > end) {
        return fallback;
    }
    const std::size_t colon = text.find(':', keyPosition + quoted.size());
    if (colon == std::string_view::npos || colon > end) {
        return fallback;
    }
    const std::size_t first = text.find_first_of("-0123456789", colon + 1);
    if (first == std::string_view::npos || first > end) {
        return fallback;
    }
    const std::size_t last = text.find_first_not_of("-0123456789.eE+", first);
    float value = fallback;
    const char* begin = text.data() + first;
    const char* finish = text.data() + std::min(last, end + 1);
    std::from_chars(begin, finish, value);
    return value;
}

bool boolField(std::string_view text, std::string_view key, std::size_t start, std::size_t end, bool fallback) {
    const std::string quoted = "\"" + std::string(key) + "\"";
    const std::size_t keyPosition = text.find(quoted, start);
    if (keyPosition == std::string_view::npos || keyPosition > end) {
        return fallback;
    }
    const std::size_t colon = text.find(':', keyPosition + quoted.size());
    if (colon == std::string_view::npos || colon > end) {
        return fallback;
    }
    const std::size_t first = text.find_first_not_of(" \t\r\n", colon + 1);
    if (first == std::string_view::npos || first > end) {
        return fallback;
    }
    return text.substr(first, 4) == "true" ? true : text.substr(first, 5) == "false" ? false : fallback;
}

std::string stringField(std::string_view text, std::string_view key, std::size_t start, std::size_t end) {
    const std::string quoted = "\"" + std::string(key) + "\"";
    const std::size_t keyPosition = text.find(quoted, start);
    if (keyPosition == std::string_view::npos || keyPosition > end) {
        return {};
    }
    const std::size_t colon = text.find(':', keyPosition + quoted.size());
    const std::size_t open = text.find('"', colon + 1);
    if (colon == std::string_view::npos || open == std::string_view::npos || open > end) {
        return {};
    }
    const std::size_t close = text.find('"', open + 1);
    if (close == std::string_view::npos || close > end) {
        return {};
    }
    return std::string{text.substr(open + 1, close - open - 1)};
}

glm::vec3 vec3Field(std::string_view text, std::string_view key, std::size_t start, std::size_t end, glm::vec3 fallback = {}) {
    const auto [open, close] = objectRange(text, key, start);
    if (open == std::string_view::npos || close > end) {
        return fallback;
    }
    return {
        numberField(text, "x", open, close, fallback.x),
        numberField(text, "y", open, close, fallback.y),
        numberField(text, "z", open, close, fallback.z),
    };
}

glm::vec2 vec2Field(std::string_view text, std::string_view key, std::size_t start, std::size_t end, glm::vec2 fallback = {}) {
    const auto [open, close] = objectRange(text, key, start);
    if (open == std::string_view::npos || close > end) {
        return fallback;
    }
    return {
        numberField(text, "x", open, close, fallback.x),
        numberField(text, "z", open, close, fallback.y),
    };
}

pcolonist::WaterKind waterKind(std::string_view text, std::size_t start, std::size_t end) {
    return stringField(text, "kind", start, end) == "inland" ? pcolonist::WaterKind::Inland : pcolonist::WaterKind::Ocean;
}

std::unordered_map<std::string, glm::vec3> parseLandmarks(std::string_view text) {
    std::unordered_map<std::string, glm::vec3> landmarks;
    std::size_t cursor = 0;
    while (true) {
        const std::size_t nameKey = text.find("\"name\"", cursor);
        if (nameKey == std::string_view::npos) {
            break;
        }
        const std::size_t entryEnd = text.find("\n    }", nameKey);
        const std::string name = stringField(text, "name", nameKey, entryEnd == std::string_view::npos ? text.size() - 1 : entryEnd);
        const glm::vec3 position = vec3Field(
            text,
            "position",
            nameKey,
            entryEnd == std::string_view::npos ? text.size() - 1 : entryEnd);
        if (!name.empty()) {
            landmarks.emplace(name, position);
        }
        cursor = nameKey + 6;
    }
    return landmarks;
}

std::vector<std::string> parseStringArray(std::string_view text, std::string_view key) {
    std::vector<std::string> values;
    const std::string quoted = "\"" + std::string(key) + "\"";
    const std::size_t keyPosition = text.find(quoted);
    if (keyPosition == std::string_view::npos) {
        return values;
    }
    const std::size_t open = text.find('[', keyPosition + quoted.size());
    const std::size_t close = text.find(']', open + 1);
    if (open == std::string_view::npos || close == std::string_view::npos) {
        return values;
    }
    std::size_t cursor = open + 1;
    while (cursor < close) {
        const std::size_t valueOpen = text.find('"', cursor);
        if (valueOpen == std::string_view::npos || valueOpen > close) {
            break;
        }
        const std::size_t valueClose = text.find('"', valueOpen + 1);
        if (valueClose == std::string_view::npos || valueClose > close) {
            break;
        }
        values.emplace_back(text.substr(valueOpen + 1, valueClose - valueOpen - 1));
        cursor = valueClose + 1;
    }
    return values;
}

WaterDefinition parseWaterDefinition(std::string_view text, std::string_view key) {
    const auto [open, close] = objectRange(text, key);
    if (open == std::string_view::npos) {
        throw std::runtime_error("Missing water config: " + std::string(key));
    }
    glm::vec2 flow = vec2Field(text, "flow_direction", open, close, {0.0F, 0.0F});
    if (glm::length(flow) > 0.001F) {
        flow = glm::normalize(flow);
    }
    return {
        vec3Field(text, "position", open, close),
        vec2Field(text, "half_extents", open, close),
        boolField(text, "affects_physics", open, close, true),
        waterKind(text, open, close),
        flow,
        numberField(text, "foam_strength", open, close, 1.0F),
    };
}

LavaDefinition parseLavaDefinition(std::string_view text, std::string_view key) {
    const auto [open, close] = objectRange(text, key);
    if (open == std::string_view::npos) {
        throw std::runtime_error("Missing lava config: " + std::string(key));
    }
    LavaDefinition definition;
    definition.mesh = stringField(text, "mesh", open, close);
    definition.position = vec3Field(text, "position", open, close);
    definition.rotation = vec3Field(text, "rotation", open, close);
    definition.scale = vec3Field(text, "scale", open, close, {1.0F, 1.0F, 1.0F});
    const auto [fireOpen, fireClose] = objectRange(text, "fire_light", open);
    definition.hasFireLight = fireOpen != std::string_view::npos && fireClose <= close;
    if (definition.hasFireLight) {
        definition.fireColor = vec3Field(text, "color", fireOpen, fireClose, definition.fireColor);
        definition.fireIntensity = numberField(text, "intensity", fireOpen, fireClose, definition.fireIntensity);
        definition.fireFalloff = numberField(text, "falloff", fireOpen, fireClose, definition.fireFalloff);
    }
    return definition;
}

float deterministicNoise(int x, int z, int salt = 0) {
    std::uint32_t value = static_cast<std::uint32_t>(x) * 374761393U
        + static_cast<std::uint32_t>(z) * 668265263U
        + static_cast<std::uint32_t>(salt) * 362437U;
    value = (value ^ (value >> 13U)) * 1274126177U;
    value ^= value >> 16U;
    return static_cast<float>(value & 0x00ffffffU) / static_cast<float>(0x01000000U);
}

std::optional<pcolonist::TerrainHit> terrainSurfaceAt(pcolonist::Registry& registry, glm::vec2 point, float maximumHeight) {
    std::optional<pcolonist::TerrainHit> result;
    registry.each<pcolonist::TerrainCollider>(
        [&result, point, maximumHeight](pcolonist::Entity, const pcolonist::TerrainCollider& collider) {
            if (const auto hit = collider.surfaceAt(point, maximumHeight);
                hit && (!result || hit->height > result->height)) {
                result = hit;
            }
        });
    return result;
}

} // namespace

namespace pcolonist {

Application::Application()
    : Application(ApplicationConfig{}) {}

Application::Application(ApplicationConfig config)
    : config_(std::move(config)),
      input_(events_),
      assets_(config_.assetRoot),
      assetManager_(assets_) {}

Application::~Application() {
    shutdown();
}

void Application::initialize() {
    if (initialized_) {
        return;
    }

    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("GLFW initialization failed");
    }
    glfwInitialized_ = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    window_ = glfwCreateWindow(config_.windowWidth, config_.windowHeight, "pcolonist", nullptr, nullptr);
    if (window_ == nullptr) {
        shutdown();
        throw std::runtime_error("Window creation failed");
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(config_.vsync ? 1 : 0);
    glfwSetWindowUserPointer(window_, this);
    glfwSetKeyCallback(window_, keyCallback);
    glfwSetCursorPosCallback(window_, mouseCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetFramebufferSizeCallback(window_, framebufferCallback);
    glfwSetWindowCloseCallback(window_, closeCallback);

    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0) {
        shutdown();
        throw std::runtime_error("OpenGL function loading failed");
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    const VulkanProbeResult vulkan = probeVulkanSupport();
    std::clog << "pcolonist: vulkan="
              << (vulkan.compiled ? (vulkan.available ? "available" : "unavailable") : "disabled")
              << " devices=" << vulkan.devices.size() << '\n';
    initializePerformanceLog();
    writePerformanceEvent("asset_preload_begin");
    assets_.preloadAll();
    writePerformanceEvent("asset_preload_end");
    renderer_ = std::make_unique<Renderer>(assets_);
    renderer_->setShadowsEnabled(config_.shadows);
    renderer_->setBloomEnabled(config_.bloom);
    renderer_->setGraphicsQuality(config_.graphicsQuality);
    renderer_->setSkyQuality(config_.skyQuality);
    frameLimiter_.setLimit(config_.frameLimit);
    debugUi_.initialize(window_);
    ui_.initialize(config_.assetRoot);
    ui_.setLanguage(config_.language);
    survival_.loadLocations(assets_);
    discovery_.loadLocations(assets_);
    loadWorldMetadata();
    loadMap();
    createWorld();
    initializeSystems();

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    renderer_->resize(width, height);
    ui_.resize(width, height);

    registerEventHandlers();
    buildPipeline();
    updateCursorMode();
    vsync_ = config_.vsync;
    initialized_ = true;
    writePerformanceEvent("initialize_complete");

    if (config_.loadGame && !loadGame()) {
        throw std::runtime_error("Cannot load save file: " + config_.savePath.string());
    }
}

void Application::shutdown() {
    writePerformanceEvent("shutdown");
    shutdownPerformanceLog();
    initialized_ = false;
    debugUi_.shutdown();
    ui_.shutdown();
    renderer_.reset();
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    if (glfwInitialized_) {
        glfwTerminate();
        glfwInitialized_ = false;
    }
}

void Application::run() {
    if (!initialized_) {
        throw std::runtime_error("Application must be initialized before run");
    }

    FrameContext context;
    double previousTime = glfwGetTime();

    while (glfwWindowShouldClose(window_) == GLFW_FALSE) {
        frameLimiter_.beginFrame();
        context.totalTime = glfwGetTime();
        context.deltaTime = std::min(static_cast<float>(context.totalTime - previousTime), maximumDeltaTime);
        previousTime = context.totalTime;
        frameArena_.reset();
        pipeline_.execute(context);
        ++context.frameNumber;
        frameLimiter_.endFrame();
    }
}

void Application::registerEventHandlers() {
    events_.subscribe<MouseMovedEvent>([this](const MouseMovedEvent& event) {
        ui_.setPointerPosition(event.x, event.y);
        if (!input_.cursorCaptured()) {
            return;
        }
        if (firstMouseEvent_) {
            lastMouseX_ = event.x;
            lastMouseY_ = event.y;
            firstMouseEvent_ = false;
        }

        camera_.look(
            static_cast<float>(event.x - lastMouseX_),
            static_cast<float>(lastMouseY_ - event.y));
        lastMouseX_ = event.x;
        lastMouseY_ = event.y;
    });

    events_.subscribe<WindowResizedEvent>([this](const WindowResizedEvent& event) {
        renderer_->resize(event.width, event.height);
        ui_.resize(event.width, event.height);
    });
    events_.subscribe<KeyEvent>([this](const KeyEvent& event) {
        if (event.key == GLFW_KEY_F11 && event.action == KeyAction::Press) {
            toggleFullscreen();
        }
        if (event.key == GLFW_KEY_F3 && event.action == KeyAction::Press) {
            toggleDebugOverlay();
            return;
        }
        if (event.key == GLFW_KEY_ESCAPE && event.action == KeyAction::Press) {
            if (debugUi_.open()) {
                debugUi_.close();
                updateCursorMode();
            } else if (debugPanelOpen_) {
                toggleDebugPanel();
            } else {
                toggleMenu();
            }
            return;
        }
        if (debugUi_.open()) {
            return;
        }
        if (menuOpen_ && event.action == KeyAction::Press) {
            const UiAction action = ui_.menuKeyAction(event.key);
            if (action != UiAction::None) {
                handleUiAction(action);
            }
            return;
        }
        if (event.key == GLFW_KEY_TAB && event.action == KeyAction::Press && !menuOpen_ && !debugPanelOpen_ && !debugUi_.open()) {
            toggleInventory();
        }
        if (event.key == GLFW_KEY_GRAVE_ACCENT && event.action == KeyAction::Press) {
            toggleDebugPanel();
        }
        if (event.key == GLFW_KEY_E && event.action == KeyAction::Press && !menuOpen_ && !inventoryOpen_ && !debugPanelOpen_ && !debugUi_.open()) {
            useContextAction();
        }
        if (event.key == GLFW_KEY_C && event.action == KeyAction::Press && !menuOpen_ && !debugPanelOpen_ && !debugUi_.open()) {
            craftNextItem();
        }
        if (event.action == KeyAction::Press && event.key >= GLFW_KEY_1 && event.key <= GLFW_KEY_5) {
            inventory_.select(static_cast<std::size_t>(event.key - GLFW_KEY_1));
        }
    });
    events_.subscribe<MouseButtonEvent>([this](const MouseButtonEvent& event) {
        if (event.button != GLFW_MOUSE_BUTTON_LEFT || event.action != KeyAction::Press) {
            return;
        }
        if (debugUi_.open()) {
            return;
        }
        if (debugPanelOpen_) {
            handleUiAction(ui_.debugActionAt(event.x, event.y));
        } else if (menuOpen_) {
            handleUiAction(ui_.menuActionAt(event.x, event.y));
        } else if (inventoryOpen_) {
            return;
        } else if (ui_.fullscreenButtonContains(event.x, event.y)) {
            toggleFullscreen();
        } else if (input_.cursorCaptured()) {
            useSelectedTool();
        }
    });
}

void Application::buildPipeline() {
    pipeline_.add(FrameStage::PollEvents, "poll GLFW events", [](FrameContext&) {
        glfwPollEvents();
    });
    pipeline_.add(FrameStage::DispatchEvents, "dispatch engine events", [this](FrameContext&) {
        events_.dispatch();
        updateCursorMode();
        if (input_.closeRequested()) {
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
    });
    pipeline_.add(FrameStage::Input, "update physical player", [this](FrameContext& context) {
        if (!menuOpen_ && !inventoryOpen_ && !debugPanelOpen_ && !debugUi_.open()) {
            player_.update(registry_, input_, camera_, context.deltaTime);
        }
    });
    pipeline_.add(FrameStage::Update, "stream terrain chunks", [this](FrameContext&) {
        if (worldStreamer_ == nullptr || menuOpen_) {
            return;
        }
        if (worldStreamer_->update(playerPosition(), registry_, assets_, physics_, resources_, scripts_, jobs_)) {
            renderer_->preloadResources(registry_);
            renderer_->releaseUnusedMeshes(registry_);
        }
    });
    pipeline_.add(FrameStage::Update, "update enemies", [this](FrameContext& context) {
        if (!menuOpen_ && !inventoryOpen_ && !debugPanelOpen_ && !debugUi_.open()) {
            enemies_.update(registry_, player_.entity(), context.deltaTime);
        }
    });
    pipeline_.add(FrameStage::Update, "update physics", [this](FrameContext& context) {
        if (!menuOpen_ && !inventoryOpen_ && !debugPanelOpen_ && !debugUi_.open()) {
            physicsTimestep_.advance(context.deltaTime, [this](float step) {
                physicsTime_ += step;
                physics_.setTime(physicsTime_);
                physics_.update(registry_, step);
            });
            player_.syncCamera(registry_, camera_);
        }
    });
    pipeline_.add(FrameStage::Update, "update animations", [this](FrameContext& context) {
        if (!menuOpen_ && !inventoryOpen_ && !debugPanelOpen_ && !debugUi_.open()) {
            animations_.update(registry_, context.deltaTime);
        }
    });
    pipeline_.add(FrameStage::Update, "parallel independent systems", [this](FrameContext& context) {
        auto audioUpdate = jobs_.submit([this, deltaTime = context.deltaTime] {
            audio_.update(deltaTime);
        });
        auto weatherUpdate = jobs_.submit([this, deltaTime = context.deltaTime] {
            if (!menuOpen_ && !inventoryOpen_ && !debugPanelOpen_ && !debugUi_.open()) {
                weather_.update(deltaTime);
            }
        });
        audioUpdate.get();
        weatherUpdate.get();
    });
    pipeline_.add(FrameStage::Update, "update survival", [this](FrameContext& context) {
        if (!menuOpen_ && !inventoryOpen_ && !debugPanelOpen_ && !debugUi_.open()) {
            survival_.update(playerPosition(), weather_, context.deltaTime);
        }
    });
    pipeline_.add(FrameStage::Update, "update discoveries", [this](FrameContext&) {
        if (!menuOpen_ && !inventoryOpen_ && !debugPanelOpen_ && !debugUi_.open()) {
            discovery_.update(playerPosition(), inventory_);
        }
    });
    pipeline_.add(FrameStage::Render, "render scene", [this](FrameContext& context) {
        static_cast<void>(context);
        if (menuOpen_ && !gameStarted_) {
            return;
        }
        renderer_->setDebugOptions(debugUi_.rendererDebugOptions());
        renderer_->render(camera_, registry_, weather_);
    });
    pipeline_.add(FrameStage::Render, "render UI", [this](FrameContext& context) {
        DebugUiStats stats = debugUiStats(context.deltaTime, context.totalTime);
        ui_.render(
            fullscreen_,
            input_.cursorCaptured(),
            menuOpen_,
            vsync_,
            frameLimiter_.limit(),
            renderer_->shadowsEnabled(),
            renderer_->bloomEnabled(),
            renderer_->skyQualityName(),
            weather_,
            inventory_,
            objectiveHudState(),
            inventoryOpen_,
            debugPanelOpen_,
            saveAvailable(),
            gameStarted_);
        ui_.updateTitle(window_, registry_, audio_, context.deltaTime, menuOpen_);
        debugUi_.render(stats);
        trackPerformance(stats);
    });
    pipeline_.add(FrameStage::Present, "present frame", [this](FrameContext&) {
        glfwSwapBuffers(window_);
    });
}

void Application::loadWorldMetadata() {
    landmarks_ = parseLandmarks(assets_.readText("maps/landmarks.json"));
    debugTeleports_.clear();
    const std::string config = assets_.readText("maps/world_config.json");
    for (const std::string& name : parseStringArray(config, "debug_teleports")) {
        debugTeleports_.push_back(landmark(name));
    }
}

void Application::resetWorldState() {
    registry_ = Registry{};
    worldStreamer_.reset();
    physicsTimestep_ = FixedTimestep{};
    loadMap();
    createWorld();
    scripts_.execute(assets_, "scripts/startup.script", registry_, physics_, resources_, jobs_);
    if (worldStreamer_ == nullptr) {
        scripts_.execute(assets_, "scripts/models.scene", registry_, physics_, resources_, jobs_);
    }
    ui_.setFrameCounterVisible(scripts_.frameCounterVisible());
    physics_.rebuildStaticIndex(registry_);
}

void Application::loadMap() {
    const bool chunkAssetsAvailable = std::filesystem::exists(assets_.resolve("maps/chunks/terrain_lod0_-1_1.obj"));
    if (chunkAssetsAvailable) {
        worldStreamer_ = std::make_unique<WorldStreamer>(assetManager_);
        worldStreamer_->loadInitial(landmark("arrival_camp"), registry_, assets_, physics_, resources_, scripts_, jobs_);
        return;
    }

    const auto structures = resources_.load<Mesh>("maps/demo_map_structures.obj", [this] {
        return MeshLoader::load(assets_, "maps/demo_map_structures.obj");
    });
    const auto rocks = resources_.load<Mesh>("maps/demo_map_rocks.obj", [this] {
        return MeshLoader::load(assets_, "maps/demo_map_rocks.obj");
    });

    const Entity structuresEntity = registry_.create();
    registry_.emplace<Transform>(structuresEntity);
    registry_.emplace<MeshRenderer>(structuresEntity, structures);

    const Entity rocksEntity = registry_.create();
    registry_.emplace<Transform>(rocksEntity);
    registry_.emplace<MeshRenderer>(rocksEntity, rocks);

    const auto terrain = resources_.load<Mesh>("maps/demo_map_terrain.obj", [this] {
        return MeshLoader::load(assets_, "maps/demo_map_terrain.obj");
    });
    const Entity terrainEntity = registry_.create();
    const Transform& transform = registry_.emplace<Transform>(terrainEntity);
    registry_.emplace<MeshRenderer>(terrainEntity, terrain);
    registry_.emplace<TerrainSurface>(terrainEntity);
    registry_.emplace<TerrainCollider>(terrainEntity, *terrain, transform);
}

void Application::createWorld() {
    const auto obstacleMesh = resources_.load<Mesh>("builtin/obstacle", [] {
        return MeshFactory::cube({0.48F, 0.36F, 0.22F});
    });
    const auto enemyMesh = resources_.load<Mesh>("builtin/enemy", [] {
        return MeshFactory::cube({0.75F, 0.12F, 0.1F});
    });
    const auto waterMesh = resources_.load<Mesh>("builtin/water", [] {
        return MeshFactory::gridPlane(1920.0F, 160, {0.02F, 0.24F, 0.42F});
    });
    const auto lavaMesh = resources_.load<Mesh>("builtin/lava", [] {
        return MeshFactory::lavaLake(1.0F, 1.0F, 128, 5, {1.0F, 0.12F, 0.005F});
    });
    const auto lavaTongueMesh = resources_.load<Mesh>("builtin/lava_tongue", [] {
        return MeshFactory::lavaLake(1.0F, 0.42F, 96, 4, {1.0F, 0.12F, 0.005F});
    });
    const auto internalWaterMesh = resources_.load<Mesh>("maps/internal_water.obj", [this] {
        return MeshLoader::load(assets_, "maps/internal_water.obj");
    });
    const std::string worldConfig = assets_.readText("maps/world_config.json");
    const WaterDefinition oceanWater = parseWaterDefinition(worldConfig, "ocean");
    const WaterDefinition internalWaterDefinition = parseWaterDefinition(worldConfig, "internal");
    const std::array lavaDefinitions = {
        parseLavaDefinition(worldConfig, "crater_lake"),
        parseLavaDefinition(worldConfig, "inner_current"),
        parseLavaDefinition(worldConfig, "overflow"),
    };

    const std::array<Transform, 8> obstacles = {
        Transform{{-8.0F, 3.0F, -90.0F}, {}, {2.0F, 6.0F, 2.0F}},
        Transform{{8.0F, 3.0F, -90.0F}, {}, {2.0F, 6.0F, 2.0F}},
        Transform{{-8.0F, 3.0F, -78.0F}, {}, {2.0F, 6.0F, 2.0F}},
        Transform{{8.0F, 3.0F, -78.0F}, {}, {2.0F, 6.0F, 2.0F}},
        Transform{{0.0F, 2.6F, -84.0F}, {}, {1.2F, 5.2F, 1.2F}},
        Transform{{91.0F, 2.5F, 18.0F}, {}, {8.0F, 5.0F, 8.0F}},
        Transform{{-82.0F, 0.35F, 38.0F}, {}, {8.0F, 0.7F, 8.0F}},
        Transform{{0.0F, 0.5F, -27.0F}, {}, {7.0F, 1.0F, 1.5F}},
    };
    for (const Transform& transform : obstacles) {
        const Entity entity = registry_.create();
        registry_.emplace<Transform>(entity, transform);
        registry_.emplace<MeshRenderer>(entity, obstacleMesh);
        registry_.emplace<BoxCollider>(entity, BoxCollider{{0.5F, 0.5F, 0.5F}, true});
    }

    const Entity water = registry_.create();
    registry_.emplace<Transform>(water, Transform{oceanWater.position});
    registry_.emplace<MeshRenderer>(water, waterMesh);
    registry_.emplace<WaterSurface>(
        water,
        WaterSurface{
            oceanWater.halfExtents,
            oceanWater.affectsPhysics,
            oceanWater.kind,
            oceanWater.flowDirection,
            oceanWater.foamStrength});

    const Entity internalWater = registry_.create();
    registry_.emplace<Transform>(internalWater, Transform{internalWaterDefinition.position});
    registry_.emplace<MeshRenderer>(internalWater, internalWaterMesh);
    registry_.emplace<WaterSurface>(
        internalWater,
        WaterSurface{
            internalWaterDefinition.halfExtents,
            internalWaterDefinition.affectsPhysics,
            internalWaterDefinition.kind,
            internalWaterDefinition.flowDirection,
            internalWaterDefinition.foamStrength});

    for (const LavaDefinition& definition : lavaDefinitions) {
        const Entity lava = registry_.create();
        registry_.emplace<Transform>(lava, Transform{definition.position, definition.rotation, definition.scale});
        registry_.emplace<MeshRenderer>(lava, definition.mesh == "builtin/lava" ? lavaMesh : lavaTongueMesh);
        registry_.emplace<LavaSurface>(lava);
        if (definition.hasFireLight) {
            registry_.emplace<FireLight>(
                lava,
                FireLight{definition.fireColor, definition.fireIntensity, definition.fireFalloff});
        }
    }

    player_.create(registry_, landmark("arrival_camp"));
    Enemy::create(registry_, {-42.0F, 1.0F, -66.0F}, enemyMesh);
    Enemy::create(registry_, {135.0F, 3.0F, -135.0F}, enemyMesh);
    Enemy::create(registry_, {-180.0F, 4.0F, 165.0F}, enemyMesh);
    Enemy::create(registry_, {-105.0F, 1.0F, 36.0F}, enemyMesh);
    createGrassPatches();
}

void Application::createGrassPatches() {
    const auto grassMesh = resources_.load<Mesh>("builtin/grass_patch", [] {
        return MeshFactory::grassPatch({0.10F, 0.26F, 0.055F}, {0.42F, 0.58F, 0.16F});
    });

    constexpr float spacing = 4.8F;
    constexpr int radius = 62;
    constexpr int maxPatches = 2200;
    int patches = 0;
    const glm::vec3 center = landmark("arrival_camp");
    for (int z = -radius; z <= radius && patches < maxPatches; ++z) {
        for (int x = -radius; x <= radius && patches < maxPatches; ++x) {
            const float jitterX = (deterministicNoise(x, z, 1) - 0.5F) * spacing * 0.82F;
            const float jitterZ = (deterministicNoise(x, z, 2) - 0.5F) * spacing * 0.82F;
            const glm::vec2 point{
                center.x + static_cast<float>(x) * spacing + jitterX,
                center.z + static_cast<float>(z) * spacing + jitterZ,
            };
            if (glm::length(point) < 48.0F || glm::length(point) > 390.0F) {
                continue;
            }
            const float densityNoise = deterministicNoise(x, z, 3);
            const float broadDensity = deterministicNoise(x / 5, z / 5, 4);
            if (densityNoise < 0.30F || broadDensity < 0.18F) {
                continue;
            }
            const auto surface = terrainSurfaceAt(registry_, point, 80.0F);
            if (!surface || surface->height < -0.08F || surface->height > 6.2F || surface->normal.y < 0.78F) {
                continue;
            }

            const float scale = 0.82F + deterministicNoise(x, z, 5) * 0.84F;
            const float yaw = deterministicNoise(x, z, 6) * 6.28318530718F;
            const Entity grass = registry_.create();
            registry_.emplace<Transform>(
                grass,
                Transform{
                    {point.x, surface->height + 0.015F, point.y},
                    {0.0F, yaw, 0.0F},
                    {scale, scale * (0.78F + deterministicNoise(x, z, 7) * 0.34F), scale},
                });
            registry_.emplace<MeshRenderer>(grass, grassMesh);
            registry_.emplace<GrassSurface>(grass);
            ++patches;
        }
    }
}

Entity Application::createCampfireFire(glm::vec3 position, float size) {
    const auto emberMesh = resources_.load<Mesh>("builtin/campfire_embers", [] {
        return MeshFactory::disc(0.52F, 28, {1.0F, 0.16F, 0.018F});
    });
    const auto fireMesh = resources_.load<Mesh>("builtin/campfire_flame", [] {
        return MeshFactory::flame({1.0F, 0.20F, 0.02F}, {1.0F, 0.86F, 0.24F});
    });
    const auto smokeMesh = resources_.load<Mesh>("builtin/campfire_smoke", [] {
        return MeshFactory::flame({0.52F, 0.50F, 0.46F}, {0.72F, 0.70F, 0.66F});
    });

    const Entity fire = registry_.create();
    registry_.emplace<Transform>(fire, Transform{position + glm::vec3{0.0F, 0.22F * size, 0.0F}});
    registry_.emplace<CampfireFire>(fire, CampfireFire{size});
    registry_.emplace<FireLight>(fire, FireLight{{1.0F, 0.31F, 0.05F}, 0.82F * size, 0.82F});

    const Entity embers = registry_.create();
    registry_.emplace<Transform>(
        embers,
        Transform{position + glm::vec3{0.0F, -0.018F * size, 0.0F}, {}, {0.62F * size, 1.0F, 0.62F * size}});
    registry_.emplace<MeshRenderer>(embers, emberMesh);
    registry_.emplace<LavaSurface>(embers);

    createFireLayer(fireMesh, position + glm::vec3{0.0F, -0.015F * size, 0.0F}, {0.70F * size, 0.88F * size, 0.70F * size}, 0.0F, 0.34F, 0.018F * size);
    createFireLayer(fireMesh, position + glm::vec3{0.035F * size, 0.045F * size, -0.025F * size}, {0.44F * size, 0.68F * size, 0.44F * size}, 0.9F, -0.53F, 0.014F * size);
    createFireLayer(fireMesh, position + glm::vec3{-0.035F * size, 0.095F * size, 0.03F * size}, {0.26F * size, 0.48F * size, 0.26F * size}, 1.7F, 0.78F, 0.010F * size);
    createSmokeLayer(smokeMesh, position + glm::vec3{0.02F * size, 0.35F * size, -0.01F * size}, {0.42F * size, 0.72F * size, 0.42F * size}, 0.35F, 0.10F, 0.022F * size);
    createSmokeLayer(smokeMesh, position + glm::vec3{-0.03F * size, 0.55F * size, 0.02F * size}, {0.58F * size, 0.95F * size, 0.58F * size}, 1.25F, -0.08F, 0.028F * size);

    return fire;
}

Entity Application::createFireLayer(
    const std::shared_ptr<Mesh>& mesh,
    glm::vec3 position,
    glm::vec3 scale,
    float yaw,
    float spin,
    float bob) {
    const Entity flame = registry_.create();
    registry_.emplace<Transform>(flame, Transform{position, {0.0F, yaw, 0.0F}, scale});
    registry_.emplace<MeshRenderer>(flame, mesh);
    registry_.emplace<FireSurface>(flame);
    registry_.emplace<Animation>(flame, Animation{{0.0F, spin, 0.0F}, bob, 8.0F + std::abs(spin) * 1.7F, position.y});
    return flame;
}

Entity Application::createSmokeLayer(
    const std::shared_ptr<Mesh>& mesh,
    glm::vec3 position,
    glm::vec3 scale,
    float yaw,
    float spin,
    float bob) {
    const Entity smoke = registry_.create();
    registry_.emplace<Transform>(smoke, Transform{position, {0.0F, yaw, 0.0F}, scale});
    registry_.emplace<MeshRenderer>(smoke, mesh);
    registry_.emplace<SmokeSurface>(smoke);
    registry_.emplace<Animation>(smoke, Animation{{0.0F, spin, 0.0F}, bob, 2.1F + std::abs(spin) * 1.4F, position.y});
    return smoke;
}

void Application::initializeSystems() {
    scripts_.execute(assets_, "scripts/startup.script", registry_, physics_, resources_, jobs_);
    if (worldStreamer_ == nullptr) {
        scripts_.execute(assets_, "scripts/models.scene", registry_, physics_, resources_, jobs_);
    }
    ui_.setFrameCounterVisible(scripts_.frameCounterVisible());
    physics_.rebuildStaticIndex(registry_);
    physicsTime_ = weather_.time();

    const AudioHandle ambient = audio_.createSource("audio/ambient.ogg", true);
    audio_.play(ambient);

    sceneSnapshot_ = SceneSerializer::serialize(registry_);
    Registry validationRegistry;
    SceneSerializer::deserialize(sceneSnapshot_, validationRegistry);
}

void Application::handleUiAction(UiAction action) {
    switch (action) {
    case UiAction::Resume:
        toggleMenu();
        break;
    case UiAction::ContinueGame:
        if (gameStarted_) {
            toggleMenu();
        } else {
            static_cast<void>(loadGame());
        }
        break;
    case UiAction::NewGame:
        startNewGame();
        break;
    case UiAction::SaveGame:
        saveGame();
        break;
    case UiAction::LoadGame:
        static_cast<void>(loadGame());
        break;
    case UiAction::MainMenu:
        returnToMainMenu();
        break;
    case UiAction::ToggleFullscreen:
        toggleFullscreen();
        break;
    case UiAction::ToggleVsync:
        vsync_ = !vsync_;
        glfwSwapInterval(vsync_ ? 1 : 0);
        break;
    case UiAction::CycleFrameLimit: {
        constexpr std::array limits = {30, 60, 120, 144, 0};
        const auto iterator = std::find(limits.begin(), limits.end(), frameLimiter_.limit());
        const auto next = iterator == limits.end() || std::next(iterator) == limits.end()
            ? limits.begin()
            : std::next(iterator);
        frameLimiter_.setLimit(*next);
        break;
    }
    case UiAction::ToggleShadows:
        renderer_->setShadowsEnabled(!renderer_->shadowsEnabled());
        break;
    case UiAction::ToggleBloom:
        renderer_->setBloomEnabled(!renderer_->bloomEnabled());
        break;
    case UiAction::CycleSkyQuality:
        renderer_->cycleSkyQuality();
        break;
    case UiAction::CycleLanguage:
        ui_.setLanguage(nextLanguage(ui_.language()));
        break;
    case UiAction::RespawnPlayer:
        teleportPlayer(landmark("arrival_camp"));
        break;
    case UiAction::TeleportVolcano:
        teleportPlayer({0.0F, 34.0F, 18.0F});
        break;
    case UiAction::TeleportNextLandmark: {
        if (!debugTeleports_.empty()) {
            teleportPlayer(debugTeleports_[nextLandmark_ % debugTeleports_.size()]);
        }
        ++nextLandmark_;
        break;
    }
    case UiAction::CycleWeather:
        if (weather_.weatherName() == "CLEAR") {
            weather_.setWeather(WeatherType::Cloudy);
        } else if (weather_.weatherName() == "CLOUDY") {
            weather_.setWeather(WeatherType::Storm);
        } else {
            weather_.setWeather(WeatherType::Clear);
        }
        break;
    case UiAction::SetNoon:
        weather_.setDayProgress(0.5F);
        break;
    case UiAction::SetNight:
        weather_.setDayProgress(0.0F);
        break;
    case UiAction::Quit:
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
        break;
    case UiAction::None:
        break;
    }
}

void Application::startNewGame() {
    resetWorldState();
    inventory_ = Inventory{};
    survival_ = SurvivalSystem{};
    survival_.loadLocations(assets_);
    discovery_ = DiscoverySystem{};
    discovery_.loadLocations(assets_);
    weather_ = WeatherSystem{};
    fireLit_ = false;
    nextLandmark_ = 0;
    physicsTime_ = weather_.time();
    craftMessage_ = crafting_.nextHint(inventory_, nearCraftStation());
    teleportPlayer(landmark("arrival_camp"));
    renderer_->preloadResources(registry_);
    gameStarted_ = true;
    menuOpen_ = false;
    inventoryOpen_ = false;
    debugPanelOpen_ = false;
    debugUi_.close();
    input_.setCursorCaptured(true);
    updateCursorMode();
}

void Application::saveGame() {
    if (!gameStarted_ || !registry_.alive(player_.entity())) {
        return;
    }

    const Transform& transform = registry_.get<Transform>(player_.entity());
    const RigidBody& body = registry_.get<RigidBody>(player_.entity());
    SaveGameSnapshot snapshot;
    snapshot.playerPosition = transform.position;
    snapshot.playerVelocity = body.velocity;
    snapshot.inventory = inventory_.snapshot();
    snapshot.survival = survival_.snapshot();
    snapshot.discovery = discovery_.snapshot();
    snapshot.weather = weather_.snapshot();
    snapshot.fireLit = fireLit_;
    snapshot.nextLandmark = nextLandmark_;
    snapshot.hasResourceState = true;
    snapshot.hasEnemyState = true;

    registry_.each<ResourceNode>([&snapshot](Entity entity, const ResourceNode& resource) {
        snapshot.resources.push_back({entity, resource.health, resource.woodYield});
    });
    registry_.each<Transform, RigidBody, EnemyComponent>(
        [&snapshot](Entity entity, const Transform& transform, const RigidBody& body, const EnemyComponent&) {
            snapshot.enemies.push_back({entity, transform.position, body.velocity});
        });

    SaveGameSerializer::save(config_.savePath, snapshot);
    craftMessage_ = "ИГРА СОХРАНЕНА";
}

bool Application::loadGame() {
    const std::optional<SaveGameSnapshot> loadedSnapshot = SaveGameSerializer::load(config_.savePath);
    if (!loadedSnapshot) {
        craftMessage_ = "СОХРАНЕНИЕ НЕ НАЙДЕНО";
        return false;
    }
    const SaveGameSnapshot& snapshot = *loadedSnapshot;

    resetWorldState();
    inventory_.applySnapshot(snapshot.inventory);
    survival_.applySnapshot(snapshot.survival);
    discovery_.applySnapshot(snapshot.discovery);
    weather_.applySnapshot(snapshot.weather);
    fireLit_ = snapshot.fireLit;
    nextLandmark_ = snapshot.nextLandmark;
    physicsTime_ = weather_.time();
    if (snapshot.hasResourceState) {
        std::unordered_map<Entity, SavedResourceNode> resourceByEntity;
        resourceByEntity.reserve(snapshot.resources.size());
        for (const SavedResourceNode& resource : snapshot.resources) {
            resourceByEntity.emplace(resource.entity, resource);
        }

        std::vector<Entity> resourcesToDestroy;
        registry_.each<ResourceNode>([&resourceByEntity, &resourcesToDestroy](Entity entity, ResourceNode& resource) {
            const auto iterator = resourceByEntity.find(entity);
            if (iterator == resourceByEntity.end()) {
                resourcesToDestroy.push_back(entity);
                return;
            }
            resource.health = iterator->second.health;
            resource.woodYield = iterator->second.woodYield;
        });
        for (Entity entity : resourcesToDestroy) {
            registry_.destroy(entity);
        }
        physics_.rebuildStaticIndex(registry_);
    }
    if (snapshot.hasEnemyState) {
        std::unordered_map<Entity, SavedEnemy> enemyByEntity;
        enemyByEntity.reserve(snapshot.enemies.size());
        for (const SavedEnemy& enemy : snapshot.enemies) {
            enemyByEntity.emplace(enemy.entity, enemy);
        }

        std::vector<Entity> enemiesToDestroy;
        registry_.each<Transform, RigidBody, EnemyComponent>(
            [&enemyByEntity, &enemiesToDestroy](
                Entity entity,
                Transform& transform,
                RigidBody& body,
                const EnemyComponent&) {
                const auto iterator = enemyByEntity.find(entity);
                if (iterator == enemyByEntity.end()) {
                    enemiesToDestroy.push_back(entity);
                    return;
                }
                transform.position = iterator->second.position;
                body.velocity = iterator->second.velocity;
            });
        for (Entity entity : enemiesToDestroy) {
            registry_.destroy(entity);
        }
    }
    teleportPlayer(snapshot.playerPosition);
    registry_.get<RigidBody>(player_.entity()).velocity = snapshot.playerVelocity;
    renderer_->preloadResources(registry_);
    craftMessage_ = "СОХРАНЕНИЕ ЗАГРУЖЕНО";
    gameStarted_ = true;
    menuOpen_ = false;
    inventoryOpen_ = false;
    debugPanelOpen_ = false;
    debugUi_.close();
    input_.setCursorCaptured(true);
    updateCursorMode();
    return true;
}

bool Application::saveAvailable() const {
    return std::filesystem::exists(config_.savePath);
}

void Application::returnToMainMenu() {
    gameStarted_ = false;
    menuOpen_ = true;
    inventoryOpen_ = false;
    debugPanelOpen_ = false;
    debugUi_.close();
    stopPlayerMotion();
    input_.setCursorCaptured(false);
    ui_.resetMenuAnimation();
    updateCursorMode();
}

void Application::toggleMenu() {
    menuOpen_ = !menuOpen_;
    if (menuOpen_) {
        inventoryOpen_ = false;
        debugPanelOpen_ = false;
        debugUi_.close();
        stopPlayerMotion();
        ui_.resetMenuAnimation();
    }
    input_.setCursorCaptured(!menuOpen_ && !inventoryOpen_ && !debugUi_.open());
    updateCursorMode();
}

void Application::toggleInventory() {
    inventoryOpen_ = !inventoryOpen_;
    if (inventoryOpen_) {
        debugPanelOpen_ = false;
        debugUi_.close();
        stopPlayerMotion();
    }
    input_.setCursorCaptured(!inventoryOpen_ && !debugUi_.open());
    updateCursorMode();
}

void Application::toggleDebugPanel() {
    debugPanelOpen_ = !debugPanelOpen_;
    if (debugPanelOpen_) {
        menuOpen_ = false;
        inventoryOpen_ = false;
        debugUi_.close();
        stopPlayerMotion();
    }
    input_.setCursorCaptured(!debugPanelOpen_);
    updateCursorMode();
}

void Application::toggleDebugOverlay() {
    debugUi_.toggleOpen();
    if (debugUi_.open()) {
        menuOpen_ = false;
        inventoryOpen_ = false;
        debugPanelOpen_ = false;
        stopPlayerMotion();
    }
    input_.setCursorCaptured(!debugUi_.open() && !menuOpen_ && !inventoryOpen_ && !debugPanelOpen_);
    updateCursorMode();
}

void Application::teleportPlayer(glm::vec3 position) {
    if (!registry_.alive(player_.entity())) {
        return;
    }
    registry_.get<Transform>(player_.entity()).position = position;
    registry_.get<RigidBody>(player_.entity()) = RigidBody{};
    player_.syncCamera(registry_, camera_);
}

glm::vec3 Application::landmark(std::string_view name) const {
    const auto iterator = landmarks_.find(std::string{name});
    if (iterator == landmarks_.end()) {
        throw std::runtime_error("Missing generated landmark: " + std::string{name});
    }
    return iterator->second;
}

void Application::stopPlayerMotion() {
    input_.clearMovementKeys();
    if (!registry_.alive(player_.entity())) {
        return;
    }
    RigidBody& body = registry_.get<RigidBody>(player_.entity());
    body.velocity.x = 0.0F;
    body.velocity.z = 0.0F;
    if (body.inWater) {
        body.velocity.y = 0.0F;
    }
}

void Application::useSelectedTool() {
    const HarvestResult result = harvesting_.harvest(
        registry_,
        HarvestRequest{
            camera_.position(),
            camera_.front(),
            4.5F,
            inventory_.selectedTool(),
        });
    if (!result.hit || !result.harvested) {
        if (!result.message.empty()) {
            craftMessage_ = std::string(result.message);
        }
        return;
    }

    if (result.depleted) {
        inventory_.addWood(result.yield.wood);
        inventory_.addStone(result.yield.stone);
        inventory_.addFiber(result.yield.fiber);
        inventory_.addMetal(result.yield.metal);
        craftMessage_ = crafting_.nextHint(inventory_, nearCraftStation());
        registry_.destroy(result.target);
        physics_.rebuildStaticIndex(registry_);
    } else {
        craftMessage_ = std::string(result.message);
    }
}

glm::vec3 Application::playerPosition() const {
    if (!registry_.alive(player_.entity())) {
        return {};
    }
    return registry_.get<Transform>(player_.entity()).position;
}

void Application::initializePerformanceLog() {
    const std::filesystem::path directory = config_.savePath.has_parent_path()
        ? config_.savePath.parent_path()
        : std::filesystem::path{"."};
    const std::filesystem::path path = directory / "pcolonist_fps.log";
    performanceLog_.open(path, std::ios::out | std::ios::app);
    if (!performanceLog_) {
        return;
    }

    performanceLog_
        << "# session_start save_path=" << config_.savePath.generic_string()
        << " asset_root=" << config_.assetRoot.generic_string() << '\n'
        << "type,time,frames,fps,avg_ms,min_ms,max_ms,entities,mesh_renderers,active_chunks,pending_chunks,lod0,lod1,lod2,terrain_draw,object_draw,shadows,bloom,note\n";
    performanceLog_ << std::fixed << std::setprecision(3);
    nextPerformanceLogTime_ = 2.0;
    performanceWindowStart_ = 0.0;
    performanceFrameTimeSum_ = 0.0;
    performanceFrameTimeMin_ = 1000000.0F;
    performanceFrameTimeMax_ = 0.0F;
    performanceFrameCount_ = 0;
}

void Application::shutdownPerformanceLog() {
    if (performanceLog_) {
        performanceLog_.flush();
        performanceLog_.close();
    }
}

void Application::writePerformanceEvent(std::string_view event) {
    if (!performanceLog_) {
        return;
    }

    const double time = glfwInitialized_ ? glfwGetTime() : 0.0;
    performanceLog_
        << "event,"
        << time
        << ",0,0.000,0.000,0.000,0.000,0,0,0,0,0,0,0,0.000,0.000,"
        << (renderer_ != nullptr && renderer_->shadowsEnabled() ? 1 : 0) << ','
        << (renderer_ != nullptr && renderer_->bloomEnabled() ? 1 : 0) << ','
        << event << '\n';
}

void Application::trackPerformance(const DebugUiStats& stats) {
    if (!performanceLog_) {
        return;
    }

    if (performanceFrameCount_ == 0) {
        performanceWindowStart_ = stats.totalTime;
    }
    ++performanceFrameCount_;
    performanceFrameTimeSum_ += stats.deltaTime;
    performanceFrameTimeMin_ = std::min(performanceFrameTimeMin_, stats.deltaTime);
    performanceFrameTimeMax_ = std::max(performanceFrameTimeMax_, stats.deltaTime);

    if (stats.totalTime < nextPerformanceLogTime_) {
        return;
    }

    const double elapsed = std::max(0.001, stats.totalTime - performanceWindowStart_);
    const double fps = static_cast<double>(performanceFrameCount_) / elapsed;
    const double averageMs = performanceFrameTimeSum_ * 1000.0 / static_cast<double>(performanceFrameCount_);
    const double minMs = static_cast<double>(performanceFrameTimeMin_) * 1000.0;
    const double maxMs = static_cast<double>(performanceFrameTimeMax_) * 1000.0;
    const RendererDebugOptions rendererOptions = renderer_ != nullptr ? renderer_->debugOptions() : RendererDebugOptions{};

    std::string note = fps < 30.0 ? "low_fps" : maxMs > 50.0 ? "frame_spike" : "ok";
    if (stats.streaming.pendingChunks > 0) {
        note += ";pending_chunks";
    }
    if (menuOpen_) {
        note += ";menu";
    }

    performanceLog_
        << "sample,"
        << stats.totalTime << ','
        << performanceFrameCount_ << ','
        << fps << ','
        << averageMs << ','
        << minMs << ','
        << maxMs << ','
        << stats.entities << ','
        << stats.meshRenderers << ','
        << stats.streaming.activeChunks << ','
        << stats.streaming.pendingChunks << ','
        << stats.streaming.lod0Chunks << ','
        << stats.streaming.lod1Chunks << ','
        << stats.streaming.lod2Chunks << ','
        << rendererOptions.terrainDrawDistance << ','
        << rendererOptions.objectDrawDistance << ','
        << (stats.shadows ? 1 : 0) << ','
        << (stats.bloom ? 1 : 0) << ','
        << note << '\n';

    nextPerformanceLogTime_ = stats.totalTime + 2.0;
    performanceWindowStart_ = stats.totalTime;
    performanceFrameTimeSum_ = 0.0;
    performanceFrameTimeMin_ = 1000000.0F;
    performanceFrameTimeMax_ = 0.0F;
    performanceFrameCount_ = 0;
}

void Application::useContextAction() {
    const glm::vec3 position = playerPosition();
    const auto near = [&position](glm::vec3 point, float radius) {
        return glm::length(glm::vec2{position.x - point.x, position.z - point.z}) <= radius;
    };
    if (near(landmark("arrival_camp"), 14.0F) && !fireLit_) {
        const BuildingDefinition& campfire = BuildingPlacementSystem::definition(BuildingKind::Campfire);
        if (!inventory_.canAfford(campfire.cost.wood, campfire.cost.stone, campfire.cost.fiber, campfire.cost.metal)) {
            craftMessage_ = "НУЖНЫ ДЕРЕВО/КАМЕНЬ";
            return;
        }

        const glm::vec3 forward = camera_.horizontalFront();
        const glm::vec3 desiredPosition = position + forward * 2.35F;
        const float yaw = std::atan2(forward.x, forward.z);
        const BuildingPlacementResult placement = buildingPlacement_.preview(
            registry_,
            BuildingPlacementRequest{BuildingKind::Campfire, desiredPosition, position, yaw});
        if (!placement.canPlace) {
            craftMessage_ = std::string(placement.reason);
            return;
        }

        if (inventory_.spendResources(campfire.cost.wood, campfire.cost.stone, campfire.cost.fiber, campfire.cost.metal)) {
            const Entity building = registry_.create();
            registry_.emplace<Transform>(building, placement.transform);
            registry_.emplace<BoxCollider>(building, placement.collider);
            registry_.emplace<ConstructedBuilding>(building, ConstructedBuilding{BuildingKind::Campfire, 1});
            const glm::vec3 groundPosition = placement.transform.position - glm::vec3{0.0F, placement.collider.halfExtents.y, 0.0F};
            createCampfireFire(groundPosition, 1.0F);
            physics_.rebuildStaticIndex(registry_);
            fireLit_ = true;
            craftMessage_ = crafting_.nextHint(inventory_, nearCraftStation());
        }
        return;
    }
    if (near(landmark("grant_lake"), 24.0F) || near(landmark("mercy_river_mouth"), 24.0F)) {
        inventory_.addWater(1);
        survival_.drinkCleanWater(1);
        return;
    }
    if (near(landmark("arrival_camp"), 12.0F) && fireLit_) {
        survival_.restNearFire(18.0F);
        return;
    }
    if (survival_.forageFood()) {
        return;
    }
    if (survival_.canGatherMaterials()) {
        LocationResourceYield yield = survival_.gatherMaterials();
        if (inventory_.selectedTool() == Tool::Pickaxe && (yield.stone > 0 || yield.metal > 0)) {
            yield.stone += 2;
            yield.metal += 1;
        }
        inventory_.addWood(yield.wood);
        inventory_.addStone(yield.stone);
        inventory_.addFiber(yield.fiber);
        inventory_.addMetal(yield.metal);
        craftMessage_ = crafting_.nextHint(inventory_, nearCraftStation());
        return;
    }
    if (inventory_.water() > 0 && survival_.status().thirst < 86.0F && inventory_.spendWater(1)) {
        survival_.drinkCleanWater(1);
        return;
    }
    if (near(landmark("natural_harbor"), 42.0F) || glm::length(glm::vec2{position.x, position.z}) < 56.0F) {
        inventory_.addStone(inventory_.selectedTool() == Tool::Pickaxe ? 3 : 1);
        if (inventory_.selectedTool() == Tool::Pickaxe) {
            inventory_.addMetal(1);
        }
        craftMessage_ = crafting_.nextHint(inventory_, nearCraftStation());
    }
}

void Application::craftNextItem() {
    const CraftingResult result = crafting_.craftNextAvailable(inventory_, nearCraftStation());
    craftMessage_ = result.crafted ? result.message : result.message + " | " + crafting_.nextHint(inventory_, nearCraftStation());
}

bool Application::nearCraftStation() const {
    const glm::vec3 position = playerPosition();
    const auto near = [&position](glm::vec3 point, float radius) {
        return glm::length(glm::vec2{position.x - point.x, position.z - point.z}) <= radius;
    };
    return (fireLit_ && near(landmark("arrival_camp"), 16.0F))
        || near(landmark("granite_house"), 24.0F)
        || near(landmark("watchtower"), 20.0F);
}

ObjectiveHudState Application::objectiveHudState() const {
    const glm::vec3 position = playerPosition();
    const auto near = [&position](glm::vec3 point, float radius) {
        return glm::length(glm::vec2{position.x - point.x, position.z - point.z}) <= radius;
    };
    std::string_view hint = "E: НЕТ ДЕЙСТВИЯ";
    if (near(landmark("arrival_camp"), 14.0F) && !fireLit_) {
        hint = inventory_.wood() > 0 && inventory_.stone() > 0 ? "E: КОСТЕР" : "НУЖНЫ ДЕРЕВО/КАМЕНЬ";
    } else if (near(landmark("grant_lake"), 24.0F) || near(landmark("mercy_river_mouth"), 24.0F)) {
        hint = "E: ПИТЬ / НАБРАТЬ ВОДУ";
    } else if (near(landmark("arrival_camp"), 12.0F) && fireLit_) {
        hint = "E: ОТДОХНУТЬ У КОСТРА";
    } else if (survival_.canForageFood()) {
        hint = "E: СОБРАТЬ ЕДУ";
    } else if (survival_.canGatherMaterials()) {
        hint = "E: СОБРАТЬ МАТЕРИАЛЫ";
    } else if (discovery_.status().blocked) {
        hint = discovery_.status().lastMessage;
    } else if (inventory_.water() > 0 && survival_.status().thirst < 86.0F) {
        hint = "E: ВЫПИТЬ ВОДУ";
    } else if (near(landmark("natural_harbor"), 42.0F) || glm::length(glm::vec2{position.x, position.z}) < 56.0F) {
        hint = "E: ДОБЫТЬ КАМЕНЬ";
    }
    const SurvivalStatus& survival = survival_.status();
    const DiscoveryStatus& discovery = discovery_.status();
    return {
        position,
        inventory_.wood() > 0,
        inventory_.stone() > 0,
        inventory_.water() > 0,
        fireLit_,
        near(landmark("granite_house"), 22.0F),
        near(landmark("standing_stones"), 20.0F) || near(landmark("watchtower"), 18.0F),
        hint,
        survival.health,
        survival.thirst,
        survival.hunger,
        survival.fatigue,
        survival.bodyTemperature,
        survival.sick,
        survival.biomeName,
        survival_.currentLocationName(),
        survival.warning,
        craftMessage_,
        discovery.discoveredLocations,
        discovery.totalLocations,
        discovery.storyClues,
        discovery.totalStoryClues,
        discovery.secretsFound,
        discovery.totalSecrets,
        discovery.blocked,
        discovery.lastMessage,
        discovery.clueMessage,
    };
}

DebugUiStats Application::debugUiStats(float deltaTime, double totalTime) {
    DebugUiStats stats;
    stats.deltaTime = deltaTime;
    stats.totalTime = totalTime;
    stats.frameLimit = frameLimiter_.limit();
    stats.vsync = vsync_;
    stats.shadows = renderer_ != nullptr && renderer_->shadowsEnabled();
    stats.bloom = renderer_ != nullptr && renderer_->bloomEnabled();
    stats.playerPosition = playerPosition();
    if (worldStreamer_ != nullptr) {
        stats.streaming = worldStreamer_->stats(stats.playerPosition);
    }
    stats.entities = registry_.size();
    registry_.each<MeshRenderer>([&stats](Entity, const MeshRenderer&) {
        ++stats.meshRenderers;
    });
    registry_.each<TerrainSurface>([&stats](Entity, const TerrainSurface&) {
        ++stats.terrainSurfaces;
    });
    registry_.each<TerrainChunk>([&stats](Entity, const TerrainChunk&) {
        ++stats.terrainChunks;
    });
    registry_.each<BoxCollider>([&stats](Entity, const BoxCollider& collider) {
        if (collider.isStatic) {
            ++stats.staticColliders;
        } else {
            ++stats.dynamicColliders;
        }
    });
    registry_.each<ResourceNode>([&stats](Entity, const ResourceNode&) {
        ++stats.resources;
    });
    registry_.each<WaterSurface>([&stats](Entity, const WaterSurface&) {
        ++stats.waterSurfaces;
    });
    registry_.each<LavaSurface>([&stats](Entity, const LavaSurface&) {
        ++stats.lavaSurfaces;
    });
    registry_.each<FireLight>([&stats](Entity, const FireLight&) {
        ++stats.fireLights;
    });
    registry_.each<Animation>([&stats](Entity, const Animation&) {
        ++stats.animated;
    });
    return stats;
}

void Application::toggleFullscreen() {
    if (!fullscreen_) {
        glfwGetWindowPos(window_, &windowedX_, &windowedY_);
        glfwGetWindowSize(window_, &windowedWidth_, &windowedHeight_);
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (mode == nullptr) {
            return;
        }
        glfwSetWindowMonitor(window_, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        fullscreen_ = true;
    } else {
        glfwSetWindowMonitor(
            window_,
            nullptr,
            windowedX_,
            windowedY_,
            windowedWidth_,
            windowedHeight_,
            GLFW_DONT_CARE);
        fullscreen_ = false;
    }
}

void Application::updateCursorMode() {
    if (menuOpen_ || inventoryOpen_ || debugPanelOpen_ || debugUi_.open()) {
        input_.setCursorCaptured(false);
    }
    if (cursorCaptured_ == input_.cursorCaptured()) {
        return;
    }
    cursorCaptured_ = input_.cursorCaptured();
    firstMouseEvent_ = true;
    glfwSetInputMode(window_, GLFW_CURSOR, cursorCaptured_ ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void Application::keyCallback(GLFWwindow* window, int key, int scanCode, int action, int modifiers) {
    auto* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
    application->debugUi_.handleKey(window, key, scanCode, action, modifiers);
    application->events_.enqueue(KeyEvent{key, toKeyAction(action)});
}

void Application::mouseCallback(GLFWwindow* window, double xPosition, double yPosition) {
    auto* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
    application->debugUi_.handleCursorPosition(window, xPosition, yPosition);
    application->events_.enqueue(MouseMovedEvent{xPosition, yPosition});
}

void Application::mouseButtonCallback(GLFWwindow* window, int button, int action, int modifiers) {
    auto* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
    application->debugUi_.handleMouseButton(window, button, action, modifiers);
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    int windowWidthPixels = 0;
    int windowHeightPixels = 0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetWindowSize(window, &windowWidthPixels, &windowHeightPixels);
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    if (windowWidthPixels > 0 && windowHeightPixels > 0) {
        x *= static_cast<double>(framebufferWidth) / static_cast<double>(windowWidthPixels);
        y *= static_cast<double>(framebufferHeight) / static_cast<double>(windowHeightPixels);
    }
    application->events_.enqueue(MouseButtonEvent{button, toKeyAction(action), x, y});
}

void Application::framebufferCallback(GLFWwindow* window, int width, int height) {
    auto* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
    application->events_.enqueue(WindowResizedEvent{width, height});
}

void Application::closeCallback(GLFWwindow* window) {
    auto* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
    application->events_.enqueue(WindowCloseEvent{});
}

} // namespace pcolonist
