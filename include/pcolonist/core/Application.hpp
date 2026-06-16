#pragma once

#include "pcolonist/animation/AnimationSystem.hpp"
#include "pcolonist/assets/AssetSystem.hpp"
#include "pcolonist/assets/ResourceManager.hpp"
#include "pcolonist/audio/AudioSystem.hpp"
#include "pcolonist/core/EventBus.hpp"
#include "pcolonist/core/FixedTimestep.hpp"
#include "pcolonist/core/FrameLimiter.hpp"
#include "pcolonist/core/FramePipeline.hpp"
#include "pcolonist/core/JobSystem.hpp"
#include "pcolonist/ecs/Registry.hpp"
#include "pcolonist/gameplay/Enemy.hpp"
#include "pcolonist/gameplay/Inventory.hpp"
#include "pcolonist/gameplay/Player.hpp"
#include "pcolonist/memory/FrameArena.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"
#include "pcolonist/platform/Input.hpp"
#include "pcolonist/render/Camera.hpp"
#include "pcolonist/scripting/ScriptSystem.hpp"
#include "pcolonist/ui/UiSystem.hpp"
#include "pcolonist/world/WeatherSystem.hpp"

#include <memory>
#include <string>

struct GLFWwindow;

namespace pcolonist {

class Renderer;

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

private:
    void registerEventHandlers();
    void buildPipeline();
    void loadMap();
    void createWorld();
    void initializeSystems();
    void handleUiAction(UiAction action);
    void toggleMenu();
    void toggleInventory();
    void toggleDebugPanel();
    void toggleFullscreen();
    void teleportPlayer(glm::vec3 position);
    void useSelectedTool();
    void updateCursorMode();

    static void keyCallback(GLFWwindow* window, int key, int scanCode, int action, int modifiers);
    static void mouseCallback(GLFWwindow* window, double xPosition, double yPosition);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int modifiers);
    static void framebufferCallback(GLFWwindow* window, int width, int height);
    static void closeCallback(GLFWwindow* window);

    GLFWwindow* window_ = nullptr;
    EventBus events_;
    FramePipeline pipeline_;
    FrameLimiter frameLimiter_;
    Input input_;
    Camera camera_;
    AssetSystem assets_;
    JobSystem jobs_;
    ResourceManager resources_;
    Registry registry_;
    Player player_;
    EnemySystem enemies_;
    Inventory inventory_;
    WeatherSystem weather_;
    PhysicsSystem physics_;
    AnimationSystem animations_;
    AudioSystem audio_;
    ScriptSystem scripts_;
    UiSystem ui_;
    FrameArena frameArena_;
    std::string sceneSnapshot_;
    std::unique_ptr<Renderer> renderer_;
    bool firstMouseEvent_ = true;
    bool cursorCaptured_ = false;
    bool fullscreen_ = false;
    bool menuOpen_ = false;
    bool inventoryOpen_ = false;
    bool debugPanelOpen_ = false;
    bool vsync_ = true;
    std::size_t nextLandmark_ = 0;
    int windowedX_ = 100;
    int windowedY_ = 100;
    int windowedWidth_ = 1280;
    int windowedHeight_ = 720;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
    FixedTimestep physicsTimestep_;
    float physicsTime_ = 0.0F;
};

} // namespace pcolonist
