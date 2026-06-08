cmake_minimum_required(VERSION 3.24)
project(pcolonist VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
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

FetchContent_MakeAvailable(glfw glm glad)
glad_add_library(glad_gl_core_33 REPRODUCIBLE API gl:core=3.3)
find_package(Threads REQUIRED)

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
    src/gameplay/Enemy.cpp
    src/gameplay/Player.cpp
    src/memory/FrameArena.cpp
    src/physics/PhysicsSystem.cpp
    src/platform/Input.cpp
    src/render/Camera.cpp
    src/render/MeshFactory.cpp
    src/render/Renderer.cpp
    src/render/Shader.cpp
    src/render/ShaderLibrary.cpp
    src/render/Skybox.cpp
    src/scripting/ScriptSystem.cpp
    src/serialization/SceneSerializer.cpp
    src/ui/UiSystem.cpp
    src/world/WeatherSystem.cpp
)

target_include_directories(pcolonist PRIVATE include)
target_link_libraries(pcolonist PRIVATE glfw glad_gl_core_33 glm::glm Threads::Threads)
target_compile_definitions(pcolonist PRIVATE
    PCOLONIST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

find_program(GLSLANG_VALIDATOR glslangValidator)
if(GLSLANG_VALIDATOR)
    add_custom_target(validate_shaders ALL
        COMMAND ${GLSLANG_VALIDATOR} -l
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/basic.vert
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/basic.frag
        COMMAND ${GLSLANG_VALIDATOR} -l
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/sky.vert
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/sky.frag
        COMMAND ${GLSLANG_VALIDATOR} -l
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/ui.vert
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/ui.frag
        COMMAND ${GLSLANG_VALIDATOR} -l
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/shadow.vert
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/shadow.frag
        COMMAND ${GLSLANG_VALIDATOR} -l
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/post.vert
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/post.frag
        COMMENT "Validating GLSL shader programs"
        VERBATIM
    )
    add_dependencies(pcolonist validate_shaders)
endif()

if(MSVC)
    target_compile_options(pcolonist PRIVATE /W4 /permissive-)
else()
    target_compile_options(pcolonist PRIVATE -Wall -Wextra -Wpedantic)
endif()
