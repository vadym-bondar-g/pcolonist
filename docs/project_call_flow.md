# Project Call Flow

This document describes the main call flow from application startup to rendering, with focus on where `RendererTargets.cpp` fits into the pipeline.

## Entry Point

`main.cpp`

1. `main()`
2. `runApplication(...)`
3. `parseRuntimeOptions(...)`
4. If launch mode is `Run`:
   - create `Application`
   - call `application.initialize()`
   - call `application.run()`

Key file: `main.cpp`

## Initialization

`Application::initialize()`

Main order:

1. `glfwInit()`
2. Create GLFW window
3. Create OpenGL context
4. `gladLoadGL(...)`
5. `glEnable(GL_DEPTH_TEST)`
6. `assets_.preloadAll()`
7. Create `Renderer`
8. Initialize UI and debug UI
9. Load world metadata
10. `loadMap()`
11. `createWorld()`
12. `initializeSystems()`
13. `renderer_->resize(width, height)`
14. `buildPipeline()`

Key file: `src/core/Application.cpp`

## Renderer Creation

`Renderer::Renderer(...)`

When the renderer is created:

1. `ShaderLibrary` receives the shader directory path.
2. `shaders_.preload()` compiles and validates shaders.
3. `screenVertexArray_` is created.
4. `createShadowMap()` is called.
5. `createRenderTargets()` is called.
6. `loadTerrainTextures()` is called.

This is where `src/render/RendererTargets.cpp` enters the flow:

- `createRenderTargets()` creates the HDR framebuffer.
- `createShadowMap()` creates shadow map framebuffers.
- `postProcess()` renders the final screen from the HDR texture into the default framebuffer.

Key files:

- `src/render/Renderer.cpp`
- `src/render/RendererTargets.cpp`

## Frame Loop

`Application::run()`

Each frame:

1. `frameLimiter_.beginFrame()`
2. Calculate `deltaTime`
3. `frameArena_.reset()`
4. `pipeline_.execute(context)`
5. Increment frame number
6. `frameLimiter_.endFrame()`

Key file: `src/core/Application.cpp`

## Frame Pipeline

`Application::buildPipeline()`

The frame pipeline runs these stages:

1. `PollEvents`
   - `glfwPollEvents()`

2. `DispatchEvents`
   - `events_.dispatch()`
   - `updateCursorMode()`

3. `Input`
   - `player_.update(...)`

4. `Update`
   - `worldStreamer_->update(...)`
   - if chunks changed:
     - `renderer_->preloadResources(registry_)`
     - `renderer_->releaseUnusedMeshes(registry_)`

5. `Update`
   - enemy update

6. `Update`
   - physics fixed timestep

7. `Update`
   - animation update

8. `Update`
   - audio and weather jobs

9. `Update`
   - survival and discovery systems

10. `Render`
    - `renderer_->render(camera_, registry_, weather_)`

11. `Render UI`
    - `ui_.render(...)`
    - `debugUi_.render(...)`
    - `trackPerformance(...)`

12. `Present`
    - `glfwSwapBuffers(window_)`

Key file: `src/core/Application.cpp`

## Renderer Frame

`Renderer::render(...)`

Main render order:

1. Check `width_` and `height_`.
2. `collectRenderItems(...)`
   - walks the `Registry`
   - gathers visible `MeshRenderer` components
   - performs frustum and distance culling
   - computes cached model matrices

3. `renderShadowMap(...)`
   - if shadows are enabled, renders shadow depth for two cascades

4. `glBindFramebuffer(GL_FRAMEBUFFER, hdrFramebuffer_)`
   - switches rendering into the HDR render target created in `RendererTargets.cpp`

5. `skybox_.render(...)`

6. Configure scene shader uniforms.

7. `buildBatches(...)`
   - groups render items by mesh and material flags

8. Main draw loop:
   - `upload(mesh)` if the mesh is not yet on the GPU
   - `uploadInstances(...)`
   - bind textures
   - `glDrawElementsInstanced(...)`

9. Detect underwater state.

10. `postProcess(weather, underwater)`
    - implemented in `RendererTargets.cpp`
    - reads `hdrColor_` and `hdrDepth_`
    - renders a fullscreen triangle into the default framebuffer

Key file: `src/render/Renderer.cpp`

## RendererTargets.cpp

`src/render/RendererTargets.cpp` owns render target and post-processing infrastructure. It does not decide which world objects are drawn. That decision belongs mostly to `Renderer.cpp`.

### `createRenderTargets()`

Creates the main HDR render target:

- `hdrFramebuffer_`
- `hdrColor_` texture using `GL_RGBA16F`
- `hdrDepth_` texture using `GL_DEPTH_COMPONENT24`

Then attaches color/depth textures to the framebuffer and checks `GL_FRAMEBUFFER_COMPLETE`.

Called from:

- `Renderer` constructor
- `Renderer::resize(...)`

### `releaseRenderTargets()`

Releases HDR framebuffer, color texture, and depth texture through `GlHandle`.

Called from:

- `Renderer::~Renderer()`
- `Renderer::resize(...)`

### `createShadowMap()`

Creates shadow-map resources:

- two shadow framebuffers
- two depth textures
- texture size based on `qualityConfig_.shadowSize`

Called from:

- `Renderer` constructor
- `Renderer::setGraphicsQuality(...)`, when shadow size changes

### `releaseShadowMap()`

Releases shadow depth textures and shadow framebuffers.

Called from:

- `Renderer::~Renderer()`
- `createShadowMap()`
- `Renderer::setGraphicsQuality(...)`

### `postProcess(...)`

Final composition step:

1. Bind default framebuffer.
2. Set viewport to window size.
3. Disable depth test.
4. Use the `post` shader.
5. Bind:
   - `hdrColor_` as `hdrScene`
   - `hdrDepth_` as `sceneDepth`
6. Set weather, bloom, underwater, and resolution uniforms.
7. Draw fullscreen triangle.
8. Re-enable depth test.

Called from:

- end of `Renderer::render(...)`

## Short Diagram

```text
main()
  runApplication()
    Application::initialize()
      GLFW/GLAD init
      AssetSystem::preloadAll()
      Renderer::Renderer()
        ShaderLibrary::preload()
        Renderer::createShadowMap()       <- RendererTargets.cpp
        Renderer::createRenderTargets()   <- RendererTargets.cpp
        Renderer::loadTerrainTextures()
      loadMap()
      createWorld()
      buildPipeline()

    Application::run()
      pipeline_.execute()
        input/update/physics/animations
        Renderer::render()
          collectRenderItems()
          renderShadowMap()
            uses shadow targets from RendererTargets.cpp
          bind hdrFramebuffer_
          skybox render
          scene render
          postProcess()                   <- RendererTargets.cpp
        ui render
        glfwSwapBuffers()
```

## Main Takeaway

`RendererTargets.cpp` is the render target layer:

- it creates framebuffer and texture targets,
- manages HDR and shadow-map GPU resources,
- runs final post-processing.

It does not decide what entities are rendered. Entity selection, culling, batching, mesh upload, and draw calls live mainly in `src/render/Renderer.cpp`.
