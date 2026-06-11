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

#include <algorithm>
#include <array>
#include <stdexcept>

namespace {

constexpr int windowWidth = 1280;
constexpr int windowHeight = 720;
constexpr float maximumDeltaTime = 0.1F;

pcolonist::KeyAction toKeyAction(int action) {
    if (action == GLFW_PRESS) {
        return pcolonist::KeyAction::Press;
    }
    if (action == GLFW_REPEAT) {
        return pcolonist::KeyAction::Repeat;
    }
    return pcolonist::KeyAction::Release;
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
            toggleMenu();
        }
    });
    events_.subscribe<MouseButtonEvent>([this](const MouseButtonEvent& event) {
        if (event.button != GLFW_MOUSE_BUTTON_LEFT || event.action != KeyAction::Press) {
            return;
        }
        if (menuOpen_) {
            handleUiAction(ui_.menuActionAt(event.x, event.y));
        } else if (ui_.fullscreenButtonContains(event.x, event.y)) {
            toggleFullscreen();
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
        if (!menuOpen_) {
            player_.update(registry_, input_, camera_, context.deltaTime);
        }
    });
    pipeline_.add(FrameStage::Update, "update enemies", [this](FrameContext& context) {
        if (!menuOpen_) {
            enemies_.update(registry_, player_.entity(), context.deltaTime);
        }
    });
    pipeline_.add(FrameStage::Update, "update physics", [this](FrameContext& context) {
        if (!menuOpen_) {
            physics_.update(registry_, context.deltaTime);
            player_.syncCamera(registry_, camera_);
        }
    });
    pipeline_.add(FrameStage::Update, "update animations", [this](FrameContext& context) {
        if (!menuOpen_) {
            animations_.update(registry_, context.deltaTime);
        }
    });
    pipeline_.add(FrameStage::Update, "parallel independent systems", [this](FrameContext& context) {
        auto audioUpdate = jobs_.submit([this, deltaTime = context.deltaTime] {
            audio_.update(deltaTime);
        });
        auto weatherUpdate = jobs_.submit([this, deltaTime = context.deltaTime] {
            if (!menuOpen_) {
                weather_.update(deltaTime);
            }
        });
        audioUpdate.get();
        weatherUpdate.get();
    });
    pipeline_.add(FrameStage::Render, "render scene", [this](FrameContext& context) {
        static_cast<void>(context);
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
            weather_);
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

    const Entity mapEntity = registry_.create();
    registry_.emplace<Transform>(mapEntity);
    registry_.emplace<MeshRenderer>(mapEntity, map);
    registry_.emplace<TerrainSurface>(mapEntity);
}

void Application::createWorld() {
    const auto obstacleMesh = resources_.load<Mesh>("builtin/obstacle", [] {
        return MeshFactory::cube({0.48F, 0.36F, 0.22F});
    });
    const auto enemyMesh = resources_.load<Mesh>("builtin/enemy", [] {
        return MeshFactory::cube({0.75F, 0.12F, 0.1F});
    });
    const auto waterMesh = resources_.load<Mesh>("builtin/water", [] {
        return MeshFactory::plane(320.0F, {0.02F, 0.24F, 0.42F});
    });

    const std::array<Transform, 8> obstacles = {
        Transform{{-8.0F, 3.0F, -48.0F}, {}, {2.0F, 6.0F, 2.0F}},
        Transform{{8.0F, 3.0F, -48.0F}, {}, {2.0F, 6.0F, 2.0F}},
        Transform{{-8.0F, 3.0F, -36.0F}, {}, {2.0F, 6.0F, 2.0F}},
        Transform{{8.0F, 3.0F, -36.0F}, {}, {2.0F, 6.0F, 2.0F}},
        Transform{{0.0F, 2.6F, -42.0F}, {}, {1.2F, 5.2F, 1.2F}},
        Transform{{45.0F, 2.5F, 10.0F}, {}, {8.0F, 5.0F, 8.0F}},
        Transform{{-43.0F, 0.35F, 19.0F}, {}, {8.0F, 0.7F, 8.0F}},
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
    registry_.emplace<WaterSurface>(water);

    player_.create(registry_, {0.0F, 2.0F, 18.0F});
    Enemy::create(registry_, {-14.0F, 1.0F, -22.0F}, enemyMesh);
    Enemy::create(registry_, {20.0F, 1.0F, -30.0F}, enemyMesh);
    Enemy::create(registry_, {38.0F, 1.0F, 25.0F}, enemyMesh);
    Enemy::create(registry_, {-35.0F, 1.0F, 12.0F}, enemyMesh);
}

void Application::initializeSystems() {
    scripts_.execute(assets_, "scripts/startup.script", registry_, physics_, resources_, jobs_);
    scripts_.execute(assets_, "scripts/models.scene", registry_, physics_, resources_, jobs_);

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
    case UiAction::Quit:
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
        break;
    case UiAction::None:
        break;
    }
}

void Application::toggleMenu() {
    menuOpen_ = !menuOpen_;
    input_.setCursorCaptured(!menuOpen_);
    updateCursorMode();
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
    if (menuOpen_) {
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
