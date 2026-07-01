#pragma once

#include "pcolonist/core/ApplicationConfig.hpp"
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
#include "pcolonist/gameplay/CraftingSystem.hpp"
#include "pcolonist/gameplay/BuildingPlacementSystem.hpp"
#include "pcolonist/gameplay/DiscoverySystem.hpp"
#include "pcolonist/gameplay/Enemy.hpp"
#include "pcolonist/gameplay/Inventory.hpp"
#include "pcolonist/gameplay/Player.hpp"
#include "pcolonist/gameplay/ResourceHarvestingSystem.hpp"
#include "pcolonist/gameplay/SurvivalSystem.hpp"
#include "pcolonist/memory/FrameArena.hpp"
#include "pcolonist/physics/PhysicsSystem.hpp"
#include "pcolonist/platform/Input.hpp"
#include "pcolonist/render/Camera.hpp"
#include "pcolonist/render/RendererBackend.hpp"
#include "pcolonist/scripting/ScriptSystem.hpp"
#include "pcolonist/ui/DebugUi.hpp"
#include "pcolonist/ui/UiSystem.hpp"
#include "pcolonist/world/AssetManager.hpp"
#include "pcolonist/world/WeatherSystem.hpp"
#include "pcolonist/world/WorldStreamer.hpp"

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct GLFWwindow;

namespace pcolonist {

struct Mesh;

class Application {
public:
    Application();
    explicit Application(ApplicationConfig config);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void initialize();
    void run();
    void shutdown();

private:
    void registerEventHandlers();
    void buildPipeline();
    void loadWorldMetadata();
    void resetWorldState();
    void loadMap();
    void createWorld();
    void createGrassPatches();
    Entity createCampfireFire(glm::vec3 position, float size);
    Entity createFireLayer(
        const std::shared_ptr<Mesh>& mesh,
        glm::vec3 position,
        glm::vec3 scale,
        float yaw,
        float spin,
        float bob);
    Entity createSmokeLayer(
        const std::shared_ptr<Mesh>& mesh,
        glm::vec3 position,
        glm::vec3 scale,
        float yaw,
        float spin,
        float bob);
    void initializeSystems();
    void handleUiAction(UiAction action);
    void startNewGame();
    void saveGame();
    bool loadGame();
    [[nodiscard]] bool saveAvailable() const;
    void returnToMainMenu();
    void toggleMenu();
    void toggleInventory();
    void toggleDebugPanel();
    void toggleDebugOverlay();
    void toggleFullscreen();
    void teleportPlayer(glm::vec3 position);
    [[nodiscard]] glm::vec3 landmark(std::string_view name) const;
    void stopPlayerMotion();
    void useSelectedTool();
    void useContextAction();
    void craftNextItem();
    bool nearCraftStation() const;
    glm::vec3 playerPosition() const;
    ObjectiveHudState objectiveHudState() const;
    DebugUiStats debugUiStats(float deltaTime, double totalTime);
    void initializePerformanceLog();
    void shutdownPerformanceLog();
    void writePerformanceEvent(std::string_view event);
    void trackPerformance(const DebugUiStats& stats);
    void updateCursorMode();

    static void keyCallback(GLFWwindow* window, int key, int scanCode, int action, int modifiers);
    static void mouseCallback(GLFWwindow* window, double xPosition, double yPosition);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int modifiers);
    static void framebufferCallback(GLFWwindow* window, int width, int height);
    static void closeCallback(GLFWwindow* window);

    GLFWwindow* window_ = nullptr;
    ApplicationConfig config_;
    EventBus events_;
    FramePipeline pipeline_;
    FrameLimiter frameLimiter_;
    Input input_;
    Camera camera_;
    AssetSystem assets_;
    AssetManager assetManager_;
    JobSystem jobs_;
    ResourceManager resources_;
    Registry registry_;
    Player player_;
    EnemySystem enemies_;
    Inventory inventory_;
    CraftingSystem crafting_;
    BuildingPlacementSystem buildingPlacement_;
    ResourceHarvestingSystem harvesting_;
    DiscoverySystem discovery_;
    SurvivalSystem survival_;
    WeatherSystem weather_;
    PhysicsSystem physics_;
    AnimationSystem animations_;
    AudioSystem audio_;
    ScriptSystem scripts_;
    DebugUi debugUi_;
    UiSystem ui_;
    FrameArena frameArena_;
    std::string sceneSnapshot_;
    std::ofstream performanceLog_;
    std::unordered_map<std::string, glm::vec3> landmarks_;
    std::vector<glm::vec3> debugTeleports_;
    std::unique_ptr<RendererBackend> renderer_;
    std::unique_ptr<WorldStreamer> worldStreamer_;
    bool firstMouseEvent_ = true;
    bool cursorCaptured_ = false;
    bool fullscreen_ = false;
    bool menuOpen_ = true;
    bool inventoryOpen_ = false;
    bool debugPanelOpen_ = false;
    bool fireLit_ = false;
    bool gameStarted_ = false;
    bool vsync_ = true;
    std::string craftMessage_ = "C: КАМЕННЫЙ НОЖ Д:1 К:2 ВЛ:1";
    std::size_t nextLandmark_ = 0;
    int windowedX_ = 100;
    int windowedY_ = 100;
    int windowedWidth_ = 1280;
    int windowedHeight_ = 720;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;
    double nextPerformanceLogTime_ = 0.0;
    double performanceWindowStart_ = 0.0;
    double performanceFrameTimeSum_ = 0.0;
    float performanceFrameTimeMin_ = 1000000.0F;
    float performanceFrameTimeMax_ = 0.0F;
    std::uint64_t performanceFrameCount_ = 0;
    FixedTimestep physicsTimestep_;
    float physicsTime_ = 0.0F;
    bool initialized_ = false;
    bool glfwInitialized_ = false;
};

} // namespace pcolonist
