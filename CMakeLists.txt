cmake_minimum_required(VERSION 3.24)
project(pcolonist VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW documentation")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW tests")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW examples")
set(GLM_BUILD_TESTS OFF CACHE BOOL "Build GLM tests")

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
find_package(PNG REQUIRED)

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
    src/gameplay/Inventory.cpp
    src/gameplay/Player.cpp
    src/memory/FrameArena.cpp
    src/physics/PhysicsSystem.cpp
    src/physics/StaticColliderGrid.cpp
    src/physics/TerrainCollider.cpp
    src/platform/Input.cpp
    src/render/Camera.cpp
    src/render/MeshFactory.cpp
    src/render/PngLoader.cpp
    src/render/Renderer.cpp
    src/render/Shader.cpp
    src/render/ShaderLibrary.cpp
    src/render/Skybox.cpp
    src/scripting/ScriptSystem.cpp
    src/serialization/SceneSerializer.cpp
    src/ui/UiSystem.cpp
    src/world/WeatherSystem.cpp
    src/world/WaterWaves.cpp
)

target_include_directories(pcolonist PRIVATE include)
target_link_libraries(pcolonist PRIVATE glfw glad_gl_core_33 glm::glm PNG::PNG Threads::Threads)
target_compile_features(pcolonist PRIVATE cxx_std_20)
set_target_properties(pcolonist PROPERTIES CXX_EXTENSIONS OFF)

target_compile_definitions(pcolonist PRIVATE
    PCOLONIST_ASSET_DIR="assets"
)

add_custom_command(TARGET pcolonist POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:pcolonist>/assets"
    COMMENT "Copying runtime assets"
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
        src/assets/Assets.cpp
        src/assets/ObjLoader.cpp
        src/gameplay/Inventory.cpp
        src/physics/PhysicsSystem.cpp
        src/physics/StaticColliderGrid.cpp
        src/physics/TerrainCollider.cpp
        src/world/WaterWaves.cpp
        src/world/WeatherSystem.cpp
    )
    target_include_directories(pcolonist_tests PRIVATE include)
    target_link_libraries(pcolonist_tests PRIVATE glm::glm)
    target_compile_features(pcolonist_tests PRIVATE cxx_std_20)
    set_target_properties(pcolonist_tests PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(pcolonist_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(pcolonist_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME core_systems COMMAND pcolonist_tests)
endif()
