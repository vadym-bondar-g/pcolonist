#include "pcolonist/core/Application.hpp"

#include "pcolonist/animation/AnimationSystem.hpp"
#include "pcolonist/assets/MeshLoader.hpp"
#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/gameplay/Enemy.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"
#include "pcolonist/render/MeshFactory.hpp"
#include "pcolonist/render/Renderer.hpp"
#include "pcolonist/serialization/SaveGameSerializer.hpp"
#include "pcolonist/serialization/SceneSerializer.hpp"
#include "pcolonist/world/WorldStreamer.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr float maximumDeltaTime = 0.1F;
const glm::vec3 arrivalCamp{-32.0F, 3.0F, 77.0F};
const glm::vec3 grantLake{31.0F, 4.0F, -24.0F};
const glm::vec3 mercyRiverMouth{104.0F, 1.0F, 25.0F};
const glm::vec3 naturalHarbor{91.0F, 1.0F, 14.5F};
const glm::vec3 graniteHouse{-104.0F, 7.0F, -10.0F};
const glm::vec3 standingStones{-82.0F, 2.0F, 38.0F};
const glm::vec3 watchtower{91.0F, 2.0F, 18.0F};
const glm::vec3 craterLavaCenter{0.4F, 13.24F, -0.2F};
const glm::vec3 craterLavaOverflow{2.8F, 13.12F, 10.8F};

pcolonist::KeyAction toKeyAction(int action) {
    if (action == GLFW_PRESS) {
        return pcolonist::KeyAction::Press;
    }
    if (action == GLFW_REPEAT) {
        return pcolonist::KeyAction::Repeat;
    }
    return pcolonist::KeyAction::Release;
}

std::optional<float> rayBoxDistance(
    glm::vec3 origin,
    glm::vec3 direction,
    glm::vec3 center,
    glm::vec3 halfExtents,
    float maximumDistance) {
    float near = 0.0F;
    float far = maximumDistance;
    for (int axis = 0; axis < 3; ++axis) {
        const float minimum = center[axis] - halfExtents[axis];
        const float maximum = center[axis] + halfExtents[axis];
        if (std::abs(direction[axis]) < 0.00001F) {
            if (origin[axis] < minimum || origin[axis] > maximum) {
                return std::nullopt;
            }
            continue;
        }
        const float inverse = 1.0F / direction[axis];
        float first = (minimum - origin[axis]) * inverse;
        float second = (maximum - origin[axis]) * inverse;
        if (first > second) {
            std::swap(first, second);
        }
        near = std::max(near, first);
        far = std::min(far, second);
        if (near > far) {
            return std::nullopt;
        }
    }
    return near <= maximumDistance ? std::optional<float>{near} : std::nullopt;
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
    renderer_ = std::make_unique<Renderer>(config_.assetRoot);
    renderer_->setShadowsEnabled(config_.shadows);
    renderer_->setBloomEnabled(config_.bloom);
    renderer_->setGraphicsQuality(config_.graphicsQuality);
    renderer_->setSkyQuality(config_.skyQuality);
    frameLimiter_.setLimit(config_.frameLimit);
    ui_.initialize(config_.assetRoot);
    ui_.setLanguage(config_.language);
    survival_.loadLocations(assets_);
    discovery_.loadLocations(assets_);
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

    if (config_.loadGame && !loadGame()) {
        throw std::runtime_error("Cannot load save file: " + config_.savePath.string());
    }
}

void Application::shutdown() {
    initialized_ = false;
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
        if (event.key == GLFW_KEY_ESCAPE && event.action == KeyAction::Press) {
            if (debugPanelOpen_) {
                toggleDebugPanel();
            } else {
                toggleMenu();
            }
            return;
        }
        if (menuOpen_ && event.action == KeyAction::Press) {
            const UiAction action = ui_.menuKeyAction(event.key);
            if (action != UiAction::None) {
                handleUiAction(action);
            }
            return;
        }
        if (event.key == GLFW_KEY_TAB && event.action == KeyAction::Press && !menuOpen_ && !debugPanelOpen_) {
            toggleInventory();
        }
        if (event.key == GLFW_KEY_GRAVE_ACCENT && event.action == KeyAction::Press) {
            toggleDebugPanel();
        }
        if (event.key == GLFW_KEY_E && event.action == KeyAction::Press && !menuOpen_ && !inventoryOpen_ && !debugPanelOpen_) {
            useContextAction();
        }
        if (event.key == GLFW_KEY_C && event.action == KeyAction::Press && !menuOpen_ && !debugPanelOpen_) {
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
        if (!menuOpen_ && !inventoryOpen_ && !debugPanelOpen_) {
            player_.update(registry_, input_, camera_, context.deltaTime);
        }
    });
    pipeline_.add(FrameStage::Update, "stream terrain chunks", [this](FrameContext&) {
        if (worldStreamer_ == nullptr || menuOpen_) {
            return;
        }
        worldStreamer_->update(playerPosition(), registry_, jobs_);
        renderer_->releaseUnusedMeshes(registry_);
    });
    pipeline_.add(FrameStage::Update, "update enemies", [this](FrameContext& context) {
        if (!menuOpen_ && !inventoryOpen_ && !debugPanelOpen_) {
            enemies_.update(registry_, player_.entity(), context.deltaTime);
        }
    });
    pipeline_.add(FrameStage::Update, "update physics", [this](FrameContext& context) {
        if (!menuOpen_ && !inventoryOpen_ && !debugPanelOpen_) {
            physicsTimestep_.advance(context.deltaTime, [this](float step) {
                physicsTime_ += step;
                physics_.setTime(physicsTime_);
                physics_.update(registry_, step);
            });
            player_.syncCamera(registry_, camera_);
        }
    });
    pipeline_.add(FrameStage::Update, "update animations", [this](FrameContext& context) {
        if (!menuOpen_ && !inventoryOpen_ && !debugPanelOpen_) {
            animations_.update(registry_, context.deltaTime);
        }
    });
    pipeline_.add(FrameStage::Update, "parallel independent systems", [this](FrameContext& context) {
        auto audioUpdate = jobs_.submit([this, deltaTime = context.deltaTime] {
            audio_.update(deltaTime);
        });
        auto weatherUpdate = jobs_.submit([this, deltaTime = context.deltaTime] {
            if (!menuOpen_ && !inventoryOpen_ && !debugPanelOpen_) {
                weather_.update(deltaTime);
            }
        });
        audioUpdate.get();
        weatherUpdate.get();
    });
    pipeline_.add(FrameStage::Update, "update survival", [this](FrameContext& context) {
        if (!menuOpen_ && !inventoryOpen_ && !debugPanelOpen_) {
            survival_.update(playerPosition(), weather_, context.deltaTime);
        }
    });
    pipeline_.add(FrameStage::Update, "update discoveries", [this](FrameContext&) {
        if (!menuOpen_ && !inventoryOpen_ && !debugPanelOpen_) {
            discovery_.update(playerPosition(), inventory_);
        }
    });
    pipeline_.add(FrameStage::Render, "render scene", [this](FrameContext& context) {
        static_cast<void>(context);
        if (menuOpen_ && !gameStarted_) {
            return;
        }
        renderer_->render(camera_, registry_, weather_);
    });
    pipeline_.add(FrameStage::Render, "render UI", [this](FrameContext& context) {
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
    });
    pipeline_.add(FrameStage::Present, "present frame", [this](FrameContext&) {
        glfwSwapBuffers(window_);
    });
}

void Application::resetWorldState() {
    registry_ = Registry{};
    worldStreamer_.reset();
    physicsTimestep_ = FixedTimestep{};
    loadMap();
    createWorld();
    scripts_.execute(assets_, "scripts/startup.script", registry_, physics_, resources_, jobs_);
    scripts_.execute(assets_, "scripts/models.scene", registry_, physics_, resources_, jobs_);
    ui_.setFrameCounterVisible(scripts_.frameCounterVisible());
    physics_.rebuildStaticIndex(registry_);
}

void Application::loadMap() {
    const bool chunkAssetsAvailable = std::filesystem::exists(assets_.resolve("maps/chunks/terrain_lod0_-1_1.obj"));

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

    if (chunkAssetsAvailable) {
        worldStreamer_ = std::make_unique<WorldStreamer>(assetManager_);
        worldStreamer_->loadInitial(arrivalCamp, registry_);
        return;
    }

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
        return MeshFactory::gridPlane(1920.0F, 288, {0.02F, 0.24F, 0.42F});
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
    registry_.emplace<Transform>(water, Transform{{0.0F, 0.08F, 0.0F}});
    registry_.emplace<MeshRenderer>(water, waterMesh);
    registry_.emplace<WaterSurface>(water, WaterSurface{{960.0F, 960.0F}});

    const Entity internalWater = registry_.create();
    registry_.emplace<Transform>(internalWater);
    registry_.emplace<MeshRenderer>(internalWater, internalWaterMesh);
    registry_.emplace<WaterSurface>(internalWater, WaterSurface{{0.0F, 0.0F}, false});

    const Entity craterLake = registry_.create();
    registry_.emplace<Transform>(craterLake, Transform{craterLavaCenter, {0.0F, 0.16F, 0.0F}, {13.9F, 1.0F, 11.6F}});
    registry_.emplace<MeshRenderer>(craterLake, lavaMesh);
    registry_.emplace<LavaSurface>(craterLake);
    registry_.emplace<FireLight>(craterLake, FireLight{{1.0F, 0.16F, 0.025F}, 4.8F, 0.055F});

    const Entity innerCurrent = registry_.create();
    registry_.emplace<Transform>(innerCurrent, Transform{{-3.7F, 13.31F, 3.6F}, {0.0F, 0.72F, 0.0F}, {8.4F, 1.0F, 2.7F}});
    registry_.emplace<MeshRenderer>(innerCurrent, lavaTongueMesh);
    registry_.emplace<LavaSurface>(innerCurrent);

    const Entity overflow = registry_.create();
    registry_.emplace<Transform>(overflow, Transform{craterLavaOverflow, {0.0F, 1.48F, 0.0F}, {7.8F, 1.0F, 1.95F}});
    registry_.emplace<MeshRenderer>(overflow, lavaTongueMesh);
    registry_.emplace<LavaSurface>(overflow);

    createCampfireFire({-26.48F, 0.86F, 75.48F}, 0.52F);

    player_.create(registry_, arrivalCamp);
    Enemy::create(registry_, {-14.0F, 1.0F, -22.0F}, enemyMesh);
    Enemy::create(registry_, {45.0F, 3.0F, -45.0F}, enemyMesh);
    Enemy::create(registry_, {-60.0F, 4.0F, 55.0F}, enemyMesh);
    Enemy::create(registry_, {-35.0F, 1.0F, 12.0F}, enemyMesh);
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
    scripts_.execute(assets_, "scripts/models.scene", registry_, physics_, resources_, jobs_);
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
        teleportPlayer({-25.0F, 2.0F, 75.0F});
        break;
    case UiAction::TeleportVolcano:
        teleportPlayer({0.0F, 34.0F, 18.0F});
        break;
    case UiAction::TeleportNextLandmark: {
        constexpr std::array landmarks = {
            glm::vec3{-104.0F, 11.0F, -2.0F},
            glm::vec3{91.0F, 3.0F, 14.5F},
            glm::vec3{31.0F, 10.0F, -24.0F},
            glm::vec3{56.0F, 4.0F, -11.0F},
            glm::vec3{-72.0F, 4.0F, -61.0F},
            glm::vec3{65.0F, 2.0F, 63.0F},
            glm::vec3{-91.0F, 3.0F, 60.0F},
        };
        teleportPlayer(landmarks[nextLandmark_ % landmarks.size()]);
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
    teleportPlayer(arrivalCamp);
    gameStarted_ = true;
    menuOpen_ = false;
    inventoryOpen_ = false;
    debugPanelOpen_ = false;
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
    craftMessage_ = "СОХРАНЕНИЕ ЗАГРУЖЕНО";
    gameStarted_ = true;
    menuOpen_ = false;
    inventoryOpen_ = false;
    debugPanelOpen_ = false;
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
        stopPlayerMotion();
        ui_.resetMenuAnimation();
    }
    input_.setCursorCaptured(!menuOpen_ && !inventoryOpen_);
    updateCursorMode();
}

void Application::toggleInventory() {
    inventoryOpen_ = !inventoryOpen_;
    if (inventoryOpen_) {
        debugPanelOpen_ = false;
        stopPlayerMotion();
    }
    input_.setCursorCaptured(!inventoryOpen_);
    updateCursorMode();
}

void Application::toggleDebugPanel() {
    debugPanelOpen_ = !debugPanelOpen_;
    if (debugPanelOpen_) {
        menuOpen_ = false;
        inventoryOpen_ = false;
        stopPlayerMotion();
    }
    input_.setCursorCaptured(!debugPanelOpen_);
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
    if (inventory_.selectedTool() != Tool::Axe) {
        return;
    }

    constexpr float reach = 4.5F;
    Entity target = nullEntity;
    float nearest = reach;
    registry_.each<Transform, ResourceNode, BoxCollider>(
        [this, &target, &nearest](
            Entity entity,
            const Transform& transform,
            const ResourceNode&,
            const BoxCollider& collider) {
            const auto distance = rayBoxDistance(
                camera_.position(),
                camera_.front(),
                transform.position,
                collider.halfExtents * transform.scale,
                nearest);
            if (distance) {
                target = entity;
                nearest = *distance;
            }
        });
    if (target == nullEntity) {
        return;
    }

    ResourceNode& resource = registry_.get<ResourceNode>(target);
    --resource.health;
    if (resource.health <= 0) {
        inventory_.addWood(resource.woodYield);
        craftMessage_ = crafting_.nextHint(inventory_, nearCraftStation());
        registry_.destroy(target);
        physics_.rebuildStaticIndex(registry_);
    }
}

glm::vec3 Application::playerPosition() const {
    if (!registry_.alive(player_.entity())) {
        return {};
    }
    return registry_.get<Transform>(player_.entity()).position;
}

void Application::useContextAction() {
    const glm::vec3 position = playerPosition();
    const auto near = [&position](glm::vec3 point, float radius) {
        return glm::length(glm::vec2{position.x - point.x, position.z - point.z}) <= radius;
    };
    if (near(arrivalCamp, 14.0F) && !fireLit_ && inventory_.wood() > 0 && inventory_.stone() > 0) {
        if (inventory_.spendWood(1) && inventory_.spendStone(1)) {
            fireLit_ = true;
            craftMessage_ = crafting_.nextHint(inventory_, nearCraftStation());
        }
        return;
    }
    if (near(grantLake, 24.0F) || near(mercyRiverMouth, 24.0F)) {
        inventory_.addWater(1);
        survival_.drinkCleanWater(1);
        return;
    }
    if (near(arrivalCamp, 12.0F) && fireLit_) {
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
    if (near(naturalHarbor, 42.0F) || glm::length(glm::vec2{position.x, position.z}) < 56.0F) {
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
    return (fireLit_ && near(arrivalCamp, 16.0F))
        || near(graniteHouse, 24.0F)
        || near(watchtower, 20.0F);
}

ObjectiveHudState Application::objectiveHudState() const {
    const glm::vec3 position = playerPosition();
    const auto near = [&position](glm::vec3 point, float radius) {
        return glm::length(glm::vec2{position.x - point.x, position.z - point.z}) <= radius;
    };
    std::string_view hint = "E: НЕТ ДЕЙСТВИЯ";
    if (near(arrivalCamp, 14.0F) && !fireLit_) {
        hint = inventory_.wood() > 0 && inventory_.stone() > 0 ? "E: КОСТЕР" : "НУЖНЫ ДЕРЕВО/КАМЕНЬ";
    } else if (near(grantLake, 24.0F) || near(mercyRiverMouth, 24.0F)) {
        hint = "E: ПИТЬ / НАБРАТЬ ВОДУ";
    } else if (near(arrivalCamp, 12.0F) && fireLit_) {
        hint = "E: ОТДОХНУТЬ У КОСТРА";
    } else if (survival_.canForageFood()) {
        hint = "E: СОБРАТЬ ЕДУ";
    } else if (survival_.canGatherMaterials()) {
        hint = "E: СОБРАТЬ МАТЕРИАЛЫ";
    } else if (discovery_.status().blocked) {
        hint = discovery_.status().lastMessage;
    } else if (inventory_.water() > 0 && survival_.status().thirst < 86.0F) {
        hint = "E: ВЫПИТЬ ВОДУ";
    } else if (near(naturalHarbor, 42.0F) || glm::length(glm::vec2{position.x, position.z}) < 56.0F) {
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
        near(graniteHouse, 22.0F),
        near(standingStones, 20.0F) || near(watchtower, 18.0F),
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
    if (menuOpen_ || inventoryOpen_ || debugPanelOpen_) {
        input_.setCursorCaptured(false);
    }
    if (cursorCaptured_ == input_.cursorCaptured()) {
        return;
    }
    cursorCaptured_ = input_.cursorCaptured();
    firstMouseEvent_ = true;
    glfwSetInputMode(window_, GLFW_CURSOR, cursorCaptured_ ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void Application::keyCallback(GLFWwindow* window, int key, int, int action, int) {
    auto* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
    application->events_.enqueue(KeyEvent{key, toKeyAction(action)});
}

void Application::mouseCallback(GLFWwindow* window, double xPosition, double yPosition) {
    auto* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
    application->events_.enqueue(MouseMovedEvent{xPosition, yPosition});
}

void Application::mouseButtonCallback(GLFWwindow* window, int button, int action, int) {
    auto* application = static_cast<Application*>(glfwGetWindowUserPointer(window));
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
