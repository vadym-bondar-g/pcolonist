#include "pcolonist/core/Application.hpp"

#include "pcolonist/animation/AnimationSystem.hpp"
#include "pcolonist/assets/MeshLoader.hpp"
#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/gameplay/Enemy.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"
#include "pcolonist/render/MeshFactory.hpp"
#include "pcolonist/render/Renderer.hpp"
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
#include <unordered_map>
#include <vector>

namespace {

constexpr int windowWidth = 1280;
constexpr int windowHeight = 720;
constexpr float maximumDeltaTime = 0.1F;
const glm::vec3 arrivalCamp{-32.0F, 3.0F, 77.0F};
const glm::vec3 grantLake{31.0F, 4.0F, -24.0F};
const glm::vec3 mercyRiverMouth{104.0F, 1.0F, 25.0F};
const glm::vec3 naturalHarbor{91.0F, 1.0F, 14.5F};
const glm::vec3 graniteHouse{-104.0F, 7.0F, -10.0F};
const glm::vec3 standingStones{-82.0F, 2.0F, 38.0F};
const glm::vec3 watchtower{91.0F, 2.0F, 18.0F};

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
    : input_(events_),
      assets_(PCOLONIST_ASSET_DIR),
      assetManager_(assets_) {
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("GLFW initialization failed");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    window_ = glfwCreateWindow(windowWidth, windowHeight, "pcolonist", nullptr, nullptr);
    if (window_ == nullptr) {
        glfwTerminate();
        throw std::runtime_error("Window creation failed");
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);
    glfwSetWindowUserPointer(window_, this);
    glfwSetKeyCallback(window_, keyCallback);
    glfwSetCursorPosCallback(window_, mouseCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetFramebufferSizeCallback(window_, framebufferCallback);
    glfwSetWindowCloseCallback(window_, closeCallback);

    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        throw std::runtime_error("OpenGL function loading failed");
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    renderer_ = std::make_unique<Renderer>();
    ui_.initialize();
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
}

Application::~Application() {
    ui_.shutdown();
    renderer_.reset();
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

void Application::run() {
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
        if (menuOpen_) {
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
            debugPanelOpen_);
        ui_.updateTitle(window_, registry_, audio_, context.deltaTime, menuOpen_);
    });
    pipeline_.add(FrameStage::Present, "present frame", [this](FrameContext&) {
        glfwSwapBuffers(window_);
    });
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
        return MeshFactory::gridPlane(640.0F, 224, {0.02F, 0.24F, 0.42F});
    });
    const auto lavaMesh = resources_.load<Mesh>("builtin/lava", [] {
        return MeshFactory::disc(1.0F, 96, {1.0F, 0.12F, 0.005F});
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
    registry_.emplace<Transform>(water, Transform{{0.0F, -0.45F, 0.0F}});
    registry_.emplace<MeshRenderer>(water, waterMesh);
    registry_.emplace<WaterSurface>(water, WaterSurface{{320.0F, 320.0F}});

    const Entity internalWater = registry_.create();
    registry_.emplace<Transform>(internalWater);
    registry_.emplace<MeshRenderer>(internalWater, internalWaterMesh);
    registry_.emplace<WaterSurface>(internalWater, WaterSurface{{0.0F, 0.0F}, false});

    const std::array<Transform, 22> lavaPools = {
        Transform{{0.0F, 13.43F, 0.0F}, {}, {17.8F, 1.0F, 14.6F}},
        Transform{{0.1F, 13.46F, 0.3F}, {0.0F, 0.18F, 0.0F}, {9.2F, 1.0F, 7.0F}},
        Transform{{-6.6F, 13.40F, 2.9F}, {0.0F, 0.32F, 0.0F}, {7.8F, 1.0F, 4.2F}},
        Transform{{6.4F, 13.39F, -3.3F}, {0.0F, -0.54F, 0.0F}, {7.4F, 1.0F, 3.8F}},
        Transform{{2.0F, 13.38F, 7.3F}, {0.0F, 1.02F, 0.0F}, {6.7F, 1.0F, 3.0F}},
        Transform{{-2.3F, 13.37F, -7.1F}, {0.0F, -1.15F, 0.0F}, {6.2F, 1.0F, 2.8F}},
        Transform{{-10.2F, 13.34F, -1.7F}, {0.0F, 1.36F, 0.0F}, {5.1F, 1.0F, 1.8F}},
        Transform{{10.0F, 13.33F, 1.9F}, {0.0F, -1.28F, 0.0F}, {4.9F, 1.0F, 1.7F}},
        Transform{{-7.8F, 13.32F, 7.0F}, {0.0F, 0.78F, 0.0F}, {4.8F, 1.0F, 1.8F}},
        Transform{{8.2F, 13.31F, -7.0F}, {0.0F, -0.92F, 0.0F}, {4.6F, 1.0F, 1.7F}},
        Transform{{-0.5F, 13.30F, 10.2F}, {0.0F, 0.05F, 0.0F}, {5.0F, 1.0F, 1.5F}},
        Transform{{0.8F, 13.29F, -10.0F}, {0.0F, 0.12F, 0.0F}, {4.8F, 1.0F, 1.45F}},
        Transform{{-12.0F, 13.28F, 3.8F}, {0.0F, 0.35F, 0.0F}, {3.0F, 1.0F, 1.15F}},
        Transform{{0.0F, 31.28F, 15.2F}, {0.0F, 0.08F, 0.0F}, {8.8F, 1.0F, 3.2F}},
        Transform{{-3.8F, 31.18F, 16.8F}, {0.0F, -0.42F, 0.0F}, {4.2F, 1.0F, 1.4F}},
        Transform{{3.9F, 31.16F, 16.7F}, {0.0F, 0.46F, 0.0F}, {4.1F, 1.0F, 1.35F}},
        Transform{{0.3F, 31.10F, 18.6F}, {0.0F, 0.02F, 0.0F}, {5.6F, 1.0F, 1.2F}},
        Transform{{-5.8F, 31.02F, 13.4F}, {0.0F, 0.82F, 0.0F}, {2.9F, 1.0F, 0.95F}},
        Transform{{5.6F, 31.00F, 13.2F}, {0.0F, -0.78F, 0.0F}, {2.8F, 1.0F, 0.92F}},
        Transform{{-1.4F, 30.92F, 12.0F}, {0.0F, 1.08F, 0.0F}, {3.8F, 1.0F, 1.05F}},
        Transform{{1.7F, 30.90F, 11.7F}, {0.0F, -1.02F, 0.0F}, {3.6F, 1.0F, 1.0F}},
        Transform{{0.1F, 31.34F, 15.6F}, {0.0F, 0.21F, 0.0F}, {4.2F, 1.0F, 1.7F}},
    };
    for (const Transform& transform : lavaPools) {
        const Entity lava = registry_.create();
        registry_.emplace<Transform>(lava, transform);
        registry_.emplace<MeshRenderer>(lava, lavaMesh);
        registry_.emplace<LavaSurface>(lava);
    }

    createCampfireFire({-26.48F, 0.86F, 75.48F}, 0.52F);

    player_.create(registry_, {-25.0F, 2.0F, 75.0F});
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

void Application::toggleMenu() {
    menuOpen_ = !menuOpen_;
    if (menuOpen_) {
        inventoryOpen_ = false;
        debugPanelOpen_ = false;
        stopPlayerMotion();
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
