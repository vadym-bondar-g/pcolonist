#include "pcolonist/core/Application.hpp"

#include "pcolonist/animation/AnimationSystem.hpp"
#include "pcolonist/assets/MeshLoader.hpp"
#include "pcolonist/ecs/Components.hpp"
#include "pcolonist/gameplay/Enemy.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"
#include "pcolonist/render/MeshFactory.hpp"
#include "pcolonist/render/Renderer.hpp"
#include "pcolonist/serialization/SceneSerializer.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
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
constexpr float terrainChunkSize = 64.0F;
const glm::vec3 arrivalCamp{-32.0F, 3.0F, 77.0F};
const glm::vec3 grantLake{31.0F, 4.0F, -24.0F};
const glm::vec3 mercyRiverMouth{104.0F, 1.0F, 25.0F};
const glm::vec3 naturalHarbor{104.0F, 1.0F, 25.0F};
const glm::vec3 graniteHouse{-104.0F, 7.0F, -10.0F};
const glm::vec3 standingStones{-82.0F, 2.0F, 38.0F};
const glm::vec3 watchtower{91.0F, 2.0F, 18.0F};

struct ChunkKey {
    int x = 0;
    int z = 0;

    bool operator==(const ChunkKey&) const = default;
};

struct ChunkKeyHash {
    std::size_t operator()(const ChunkKey& key) const noexcept {
        return static_cast<std::size_t>(static_cast<unsigned int>(key.x)) * 0x9e3779b1U
            ^ static_cast<std::size_t>(static_cast<unsigned int>(key.z));
    }
};

std::vector<std::pair<pcolonist::TerrainChunk, std::shared_ptr<pcolonist::Mesh>>> splitTerrain(
    const pcolonist::Mesh& source) {
    struct ChunkBuilder {
        pcolonist::Mesh mesh;
        std::unordered_map<std::uint32_t, std::uint32_t> remap;
    };
    std::unordered_map<ChunkKey, ChunkBuilder, ChunkKeyHash> builders;
    for (std::size_t index = 0; index + 2 < source.indices.size(); index += 3) {
        const glm::vec3 center = (
            source.vertices[source.indices[index]].position
            + source.vertices[source.indices[index + 1]].position
            + source.vertices[source.indices[index + 2]].position) / 3.0F;
        const ChunkKey key{
            static_cast<int>(std::floor(center.x / terrainChunkSize)),
            static_cast<int>(std::floor(center.z / terrainChunkSize)),
        };
        ChunkBuilder& builder = builders[key];
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const std::uint32_t sourceIndex = source.indices[index + corner];
            const auto [iterator, inserted] = builder.remap.try_emplace(
                sourceIndex,
                static_cast<std::uint32_t>(builder.mesh.vertices.size()));
            if (inserted) {
                builder.mesh.vertices.push_back(source.vertices[sourceIndex]);
            }
            builder.mesh.indices.push_back(iterator->second);
        }
    }

    std::vector<std::pair<pcolonist::TerrainChunk, std::shared_ptr<pcolonist::Mesh>>> chunks;
    chunks.reserve(builders.size());
    for (auto& [key, builder] : builders) {
        const glm::vec2 center{
            (static_cast<float>(key.x) + 0.5F) * terrainChunkSize,
            (static_cast<float>(key.z) + 0.5F) * terrainChunkSize,
        };
        chunks.emplace_back(
            pcolonist::TerrainChunk{center, terrainChunkSize * 0.78F},
            std::make_shared<pcolonist::Mesh>(std::move(builder.mesh)));
    }
    return chunks;
}

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
      assets_(PCOLONIST_ASSET_DIR) {
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
    pipeline_.add(FrameStage::Update, "update enemies", [this](FrameContext& context) {
        if (!menuOpen_ && !inventoryOpen_) {
            enemies_.update(registry_, player_.entity(), context.deltaTime);
        }
    });
    pipeline_.add(FrameStage::Update, "update physics", [this](FrameContext& context) {
        if (!menuOpen_ && !inventoryOpen_) {
            physicsTimestep_.advance(context.deltaTime, [this](float step) {
                physicsTime_ += step;
                physics_.setTime(physicsTime_);
                physics_.update(registry_, step);
            });
            player_.syncCamera(registry_, camera_);
        }
    });
    pipeline_.add(FrameStage::Update, "update animations", [this](FrameContext& context) {
        if (!menuOpen_ && !inventoryOpen_) {
            animations_.update(registry_, context.deltaTime);
        }
    });
    pipeline_.add(FrameStage::Update, "parallel independent systems", [this](FrameContext& context) {
        auto audioUpdate = jobs_.submit([this, deltaTime = context.deltaTime] {
            audio_.update(deltaTime);
        });
        auto weatherUpdate = jobs_.submit([this, deltaTime = context.deltaTime] {
            if (!menuOpen_ && !inventoryOpen_) {
                weather_.update(deltaTime);
            }
        });
        audioUpdate.get();
        weatherUpdate.get();
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
    const auto map = resources_.load<Mesh>("maps/demo_map.obj", [this] {
        return MeshLoader::load(assets_, "maps/demo_map.obj");
    });
    const auto mapLod1 = resources_.load<Mesh>("maps/demo_map_lod1.obj", [this] {
        return MeshLoader::load(assets_, "maps/demo_map_lod1.obj");
    });
    const auto mapLod2 = resources_.load<Mesh>("maps/demo_map_lod2.obj", [this] {
        return MeshLoader::load(assets_, "maps/demo_map_lod2.obj");
    });

    const Entity mapEntity = registry_.create();
    const Transform& transform = registry_.emplace<Transform>(mapEntity);
    registry_.emplace<TerrainCollider>(mapEntity, *map, transform);

    const auto addTerrainLod = [this](const Mesh& source, TerrainLod lod) {
        for (auto& [chunk, mesh] : splitTerrain(source)) {
            const Entity chunkEntity = registry_.create();
            registry_.emplace<Transform>(chunkEntity);
            registry_.emplace<MeshRenderer>(chunkEntity, std::move(mesh));
            registry_.emplace<TerrainSurface>(chunkEntity);
            registry_.emplace<TerrainChunk>(chunkEntity, chunk);
            registry_.emplace<TerrainLod>(chunkEntity, lod);
        }
    };
    addTerrainLod(*map, TerrainLod{0.0F, 145.0F});
    addTerrainLod(*mapLod1, TerrainLod{145.0F, 280.0F});
    addTerrainLod(*mapLod2, TerrainLod{280.0F, 100000.0F});
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
        return MeshFactory::disc(5.0F, 64, {1.0F, 0.12F, 0.005F});
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

    const Entity lava = registry_.create();
    registry_.emplace<Transform>(lava, Transform{{0.0F, 13.4F, 0.0F}});
    registry_.emplace<MeshRenderer>(lava, lavaMesh);
    registry_.emplace<LavaSurface>(lava);

    player_.create(registry_, {-25.0F, 2.0F, 75.0F});
    Enemy::create(registry_, {-14.0F, 1.0F, -22.0F}, enemyMesh);
    Enemy::create(registry_, {45.0F, 3.0F, -45.0F}, enemyMesh);
    Enemy::create(registry_, {-60.0F, 4.0F, 55.0F}, enemyMesh);
    Enemy::create(registry_, {-35.0F, 1.0F, 12.0F}, enemyMesh);
}

void Application::initializeSystems() {
    scripts_.execute(assets_, "scripts/startup.script", registry_, physics_, resources_, jobs_);
    scripts_.execute(assets_, "scripts/models.scene", registry_, physics_, resources_, jobs_);
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
    case UiAction::RespawnPlayer:
        teleportPlayer({-25.0F, 2.0F, 75.0F});
        break;
    case UiAction::TeleportVolcano:
        teleportPlayer({0.0F, 34.0F, 18.0F});
        break;
    case UiAction::TeleportNextLandmark: {
        constexpr std::array landmarks = {
            glm::vec3{-104.0F, 11.0F, -2.0F},
            glm::vec3{104.0F, 3.0F, 25.0F},
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
    }
    input_.setCursorCaptured(!menuOpen_ && !inventoryOpen_);
    updateCursorMode();
}

void Application::toggleInventory() {
    inventoryOpen_ = !inventoryOpen_;
    if (inventoryOpen_) {
        debugPanelOpen_ = false;
    }
    input_.setCursorCaptured(!inventoryOpen_);
    updateCursorMode();
}

void Application::toggleDebugPanel() {
    debugPanelOpen_ = !debugPanelOpen_;
    if (debugPanelOpen_) {
        menuOpen_ = false;
        inventoryOpen_ = false;
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
        }
        return;
    }
    if (near(grantLake, 24.0F) || near(mercyRiverMouth, 24.0F)) {
        inventory_.addWater(1);
        return;
    }
    if (near(naturalHarbor, 42.0F) || glm::length(glm::vec2{position.x, position.z}) < 56.0F) {
        inventory_.addStone(1);
    }
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
        hint = "E: НАБРАТЬ ВОДУ";
    } else if (near(naturalHarbor, 42.0F) || glm::length(glm::vec2{position.x, position.z}) < 56.0F) {
        hint = "E: ДОБЫТЬ КАМЕНЬ";
    }
    return {
        position,
        inventory_.wood() > 0,
        inventory_.stone() > 0,
        inventory_.water() > 0,
        fireLit_,
        near(graniteHouse, 22.0F),
        near(standingStones, 20.0F) || near(watchtower, 18.0F),
        hint,
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
