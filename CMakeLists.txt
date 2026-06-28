cmake_minimum_required(VERSION 3.24)
project(pcolonist VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW documentation")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW tests")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW examples")
set(GLM_BUILD_TESTS OFF CACHE BOOL "Build GLM tests")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build dependencies as static libraries")
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "Build zlib examples")
set(PNG_SHARED OFF CACHE BOOL "Build shared libpng")
set(PNG_STATIC ON CACHE BOOL "Build static libpng")
set(PNG_TESTS OFF CACHE BOOL "Build libpng tests")
set(PNG_TOOLS OFF CACHE BOOL "Build libpng tools")
set(SKIP_INSTALL_ALL ON CACHE BOOL "Skip dependency install rules")

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR cmake
)

FetchContent_Declare(
    ZLIB
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1
    GIT_SHALLOW TRUE
    OVERRIDE_FIND_PACKAGE
)

FetchContent_Declare(
    libpng
    GIT_REPOSITORY https://github.com/pnggroup/libpng.git
    GIT_TAG v1.6.44
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glfw glm glad ZLIB)
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()

# Libpng's generated pnglibconf.h can detect a different zlib.h than the one
# propagated by ZLIB::ZLIB on Windows. Use libpng's supported prebuilt config
# so PNG_ZLIB_VERNUM does not depend on the host's include path or AWK.
set(AWK "" CACHE FILEPATH "Disable libpng AWK config generation" FORCE)
FetchContent_MakeAvailable(libpng)
if(NOT TARGET PNG::PNG)
    add_library(PNG::PNG ALIAS png_static)
endif()

glad_add_library(glad_gl_core_33 REPRODUCIBLE API gl:core=3.3)
find_package(Threads REQUIRED)
option(PCOLONIST_ENABLE_FREETYPE_UI "Render UI text with FreeType TTF fonts" OFF)
if(PCOLONIST_ENABLE_FREETYPE_UI)
    find_package(Freetype REQUIRED)
endif()

option(PCOLONIST_ENABLE_IMGUI_DEBUG "Enable Dear ImGui debug overlay when external/imgui is available" ON)
set(PCOLONIST_HAS_IMGUI_DEBUG OFF)
if(PCOLONIST_ENABLE_IMGUI_DEBUG AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui.cpp")
    set(PCOLONIST_HAS_IMGUI_DEBUG ON)
    add_library(imgui STATIC
        external/imgui/imgui.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp
        external/imgui/backends/imgui_impl_glfw.cpp
        external/imgui/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(imgui PUBLIC
        external/imgui
        external/imgui/backends
    )
    target_link_libraries(imgui PUBLIC glfw)
endif()

add_executable(pcolonist
    main.cpp
    src/animation/AnimationSystem.cpp
    src/assets/Assets.cpp
    src/assets/MeshLoader.cpp
    src/assets/ObjLoader.cpp
    src/audio/AudioSystem.cpp
    src/core/Application.cpp
    src/core/FrameLimiter.cpp
    src/core/FramePipeline.cpp
    src/core/JobSystem.cpp
    src/core/RuntimeOptions.cpp
    src/gameplay/Enemy.cpp
    src/gameplay/CraftingSystem.cpp
    src/gameplay/DiscoverySystem.cpp
    src/gameplay/Inventory.cpp
    src/gameplay/Player.cpp
    src/gameplay/SurvivalSystem.cpp
    src/memory/FrameArena.cpp
    src/physics/PhysicsSystem.cpp
    src/physics/StaticColliderGrid.cpp
    src/physics/TerrainCollider.cpp
    src/platform/Input.cpp
    src/render/Camera.cpp
    src/render/MeshFactory.cpp
    src/render/PngLoader.cpp
    src/render/Renderer.cpp
    src/render/RendererTargets.cpp
    src/render/Shader.cpp
    src/render/ShaderLibrary.cpp
    src/render/Skybox.cpp
    src/scripting/ScriptSystem.cpp
    src/serialization/SaveGameSerializer.cpp
    src/serialization/SceneSerializer.cpp
    src/ui/DebugUi.cpp
    src/ui/MainMenu.cpp
    src/ui/PauseMenu.cpp
    src/ui/UiSystem.cpp
    src/world/WeatherSystem.cpp
    src/world/WaterWaves.cpp
    src/world/AssetManager.cpp
    src/world/Chunk.cpp
    src/world/ChunkManager.cpp
    src/world/WorldStreamer.cpp
)

target_include_directories(pcolonist PRIVATE include)
target_link_libraries(pcolonist PRIVATE glfw glad_gl_core_33 glm::glm PNG::PNG Threads::Threads)
if(PCOLONIST_ENABLE_FREETYPE_UI)
    target_link_libraries(pcolonist PRIVATE Freetype::Freetype)
    target_compile_definitions(pcolonist PRIVATE PCOLONIST_ENABLE_FREETYPE_UI=1)
endif()
if(PCOLONIST_HAS_IMGUI_DEBUG)
    target_link_libraries(pcolonist PRIVATE imgui)
    target_compile_definitions(pcolonist PRIVATE PCOLONIST_ENABLE_IMGUI_DEBUG=1)
endif()
target_compile_features(pcolonist PRIVATE cxx_std_20)
set_target_properties(pcolonist PROPERTIES CXX_EXTENSIONS OFF)

target_compile_definitions(pcolonist PRIVATE
    PCOLONIST_ASSET_DIR="assets"
)

add_custom_target(pcolonist_runtime_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:pcolonist>/assets"
    COMMENT "Synchronizing runtime assets"
    VERBATIM
)
add_dependencies(pcolonist_runtime_assets pcolonist)
add_custom_command(TARGET pcolonist POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:pcolonist>/assets"
    COMMENT "Synchronizing runtime assets next to pcolonist"
    VERBATIM
)

set(PCOLONIST_SHADER_PROGRAMS
    basic
    sky
    ui
    shadow
    post
)
set(PCOLONIST_SHADER_FILES)
foreach(program IN LISTS PCOLONIST_SHADER_PROGRAMS)
    list(APPEND PCOLONIST_SHADER_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/${program}.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/${program}.frag"
    )
endforeach()

option(PCOLONIST_VALIDATE_SHADERS "Validate GLSL shaders during the build" ON)
if(PCOLONIST_VALIDATE_SHADERS)
    find_program(GLSLANG_VALIDATOR glslangValidator)
endif()

if(PCOLONIST_VALIDATE_SHADERS AND GLSLANG_VALIDATOR)
    set(shader_validation_stamp "${CMAKE_CURRENT_BINARY_DIR}/shaders.validated")
    set(shader_validation_commands)
    foreach(program IN LISTS PCOLONIST_SHADER_PROGRAMS)
        list(APPEND shader_validation_commands
            COMMAND "${GLSLANG_VALIDATOR}" -l
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/${program}.vert"
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/${program}.frag"
        )
    endforeach()

    add_custom_command(
        OUTPUT "${shader_validation_stamp}"
        ${shader_validation_commands}
        COMMAND ${CMAKE_COMMAND} -E touch "${shader_validation_stamp}"
        DEPENDS ${PCOLONIST_SHADER_FILES}
        COMMENT "Validating GLSL shader programs"
        VERBATIM
    )
    add_custom_target(validate_shaders DEPENDS "${shader_validation_stamp}")
    add_dependencies(pcolonist validate_shaders)
endif()

if(MSVC)
    target_compile_options(pcolonist PRIVATE /W4 /permissive-)
else()
    target_compile_options(pcolonist PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(pcolonist_tests
        tests/AssetSystemTests.cpp
        tests/ObjLoaderTests.cpp
        tests/InventoryTests.cpp
        tests/TerrainColliderTests.cpp
        tests/WorldSystemTests.cpp
        tests/PngLoaderTests.cpp
        tests/SaveGameSerializerTests.cpp
        src/assets/Assets.cpp
        src/assets/ObjLoader.cpp
        src/gameplay/Inventory.cpp
        src/physics/PhysicsSystem.cpp
        src/physics/StaticColliderGrid.cpp
        src/physics/TerrainCollider.cpp
        src/render/PngLoader.cpp
        src/serialization/SaveGameSerializer.cpp
        src/world/WaterWaves.cpp
        src/world/WeatherSystem.cpp
    )
    target_include_directories(pcolonist_tests PRIVATE include)
    target_link_libraries(pcolonist_tests PRIVATE glm::glm PNG::PNG)
    target_compile_features(pcolonist_tests PRIVATE cxx_std_20)
    set_target_properties(pcolonist_tests PROPERTIES CXX_EXTENSIONS OFF)
    target_compile_definitions(pcolonist_tests PRIVATE
        PCOLONIST_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    )

    if(MSVC)
        target_compile_options(pcolonist_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(pcolonist_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME core_systems COMMAND pcolonist_tests)
endif()
