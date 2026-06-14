# pcolonist

OpenGL 3.3 engine skeleton with an ECS, typed events and an extensible frame
pipeline.

## Structure

```text
include/pcolonist/
  core/           application, events and frame pipeline
  ecs/            registry and shared components
  assets/         asset paths, OBJ loader and resource cache
  render/         camera, mesh, shader and renderer
  gameplay/       physical player and enemy AI
  world/          dynamic weather state
  platform/       keyboard and mouse input
  physics/        rigid bodies, box colliders and gravity
  animation/      transform animation system
  audio/          audio source state and playback lifecycle
  scripting/      startup command-script interpreter
  serialization/  scene text serialization
  ui/             runtime statistics UI
  memory/         per-frame arena allocator
```

Implementations mirror this layout under `src/`. Runtime content is stored
under `assets/maps`, `assets/models`, `assets/scripts` and `assets/shaders`.

The generated world is a large mysterious island with an arrival camp,
forests, coastal rocks, an eastern marsh, a western standing-stone circle, a
sunken northern temple and a ruined watchtower above the eastern bay. Rebuild
the deterministic map and scenery placement with:

```bash
python3 tools/generate_island.py
```

## Frame pipeline

Every frame executes ordered tasks:

1. `PollEvents`
2. `DispatchEvents`
3. `Input`
4. `Update`: physics, animation and audio
5. `Render`: world and diagnostic UI
6. `Present`

## Systems

- `Registry` stores entities and typed components.
- `PhysicsSystem` integrates velocity, gravity and ground collisions.
- `Player` converts FPS controls into rigid-body movement and jumping.
- `EnemySystem` makes enemy entities detect and pursue the player.
- Static AABB obstacles block the player and enemies, with horizontal swept
  collision preventing fast bodies from passing through narrow tree trunks.
- Static AABB obstacles are indexed in a spatial grid after world loading, so
  dynamic bodies only test nearby collision candidates.
- Terrain collision uses a spatial grid instead of scanning every map
  triangle for every dynamic body.
- Ground contacts sample the center and corners of a collider, retain the
  surface normal, reject overly steep slopes and sweep vertical movement to
  reduce tunneling.
- Player movement is projected onto the current ground plane and short
  downward gaps are snapped to the terrain for stable movement over triangle
  seams and descending slopes.
- Physics advances at a fixed 120 Hz with bounded catch-up steps, keeping
  collisions, gravity and buoyancy stable across different rendering FPS.
- `TerrainCollider` provides indexed surface-height, normal and downward
  raycast queries.
- `MeshLoader` dispatches model formats; OBJ loading triangulates faces and
  calculates vertex normals.
- `Skybox` renders a procedural weather-dependent sky backdrop.
- `ShaderLibrary` compiles and validates scene and sky programs during engine
  startup, before the first frame.
- CMake validates and links GLSL programs during every build when
  `glslangValidator` is installed.
- Shader uniform locations are cached after their first lookup.
- The sky pass renders atmospheric horizon scattering, broad sun glow,
  raymarched volumetric clouds with parallax and internal lighting, storm
  lightning and spherical twinkling stars.
- A full 180-second day/night cycle moves the sun and moon across the sky,
  transitions through sunrise and sunset, and switches scene lighting between
  warm sunlight and cool moonlight.
- The world tracks consecutive day numbers while the sun follows a moving
  east-to-west arc across each full day.
- Directional sunlight, ambient light, specular highlights, rim lighting,
  tone mapping, gamma correction and 4x MSAA improve scene rendering.
- The post-process pass applies FXAA edge smoothing, subtle sharpening,
  multi-scale bloom, depth-based aerial perspective, ACES-style tone mapping,
  adaptive day/night color grading, dithering and vignette.
- Procedural surface variation prevents large untextured meshes from looking
  completely flat. Water uses layered waves, stronger Fresnel reflections and
  animated highlights.
- The procedural sky uses multi-octave illuminated clouds in addition to its
  sun, moon, stars and horizon haze.
- A 2048x2048 directional shadow map with PCF filtering adds soft dynamic
  shadows from the current sun or moon direction.
- Near and far stabilized shadow cascades preserve detail around the player
  while covering the larger island.
- Terrain colors form height-dependent wet sand, beach, low grass, forest
  soil, high grass and mountain-rock layers. Procedural micro-normals, grains,
  stone veins, grass fibers, moving cloud shadows, wet shoreline shading,
  slope and noise soften their boundaries.
- Repeated meshes are rendered in instanced batches and small distant objects
  are culled to reduce draw calls and vertex work.
- The scene renders into an HDR framebuffer and passes through exposure tone
  mapping, bloom, color grading and vignette post-processing.
- `WeatherSystem` cycles through clear, cloudy and storm conditions.
- A dense animated water mesh surrounds the playable map, with crest foam,
  breaking shoreline foam, fine normal ripples, distance tinting and Fresnel
  reflections. Underwater rendering adds caustics and suspended particles.
- Water uses a shared multi-wave model for rendering and physics. Moving
  crests change buoyancy, surface normals and apply vertical and horizontal
  impulses to floating bodies.
- `AnimationSystem` updates rotation and bob animations.
- `ScriptSystem` executes commands from `assets/scripts/startup.script`.
- Models and their placement are configured in `assets/scripts/models.scene`.
  Use `model <id> <asset-path>` to load through `MeshLoader` and
  `spawn_model <id> px py pz sx sy sz hx hy hz` to create a static ECS model
  with a box collider. Use `spawn_decor <id> px py pz sx sy sz` for visual
  vegetation and props that should not block movement. Both spawn commands
  accept an optional final yaw angle in radians.
- The OBJ loader supports triangulation, negative indices, exported vertex
  normals, split face normals, optional vertex colors and diffuse `Kd` colors
  from MTL materials. UV-mapped PNG diffuse textures referenced by `map_Kd`
  are supported. MTL `Ns`, `Ks` and `Ke` values control roughness, specular
  strength and emissive lighting.
- `SceneSerializer` serializes transforms and rigid bodies and validates a
  round-trip during startup.
- `FrameArena` provides temporary per-frame memory.
- `UiSystem` displays FPS, entity and audio counts in the window title.
- The screen HUD renders a crosshair, status panel, cursor hint and a
  clickable fullscreen button.
- `F11` or the top-right HUD button switches between windowed and fullscreen
  modes while preserving the previous window position and size.
- Original low-poly tree and rock OBJ models are included under
  `assets/models` and placed as physical scenery.
- Deterministic smooth higher-poly layered tree, bush and rock models use
  dense radial and vertical geometry and can be rebuilt with
  `python3 tools/generate_nature_models.py`.
- The island uses a denser terrain mesh, multi-scale procedural ground detail
  and understory bushes to improve silhouettes and forest depth.
- Selected camp, vegetation and prop models from Kenney's CC0 3D Nature Pack
  are included under `assets/models/kenney`; the original license is kept next
  to the models. The imported source is
  [Kenney 3D Nature Pack](https://opengameart.org/content/3d-nature-pack).
- `AudioSystem` manages sources and playback state. A decoder/output backend
  such as miniaudio or OpenAL is the next layer needed for audible output.

## Controls

- `W`, `A`, `S`, `D`: move
- Mouse: look around
- `Space`: jump or swim upward
- `Left Shift`: dive while swimming
- `F1`: release/capture mouse
- `F11`: toggle fullscreen
- `1`-`5`: select a tool slot
- Left mouse button: use the selected tool; the axe damages nearby targeted
  trees and adds wood after three hits
- `Tab`: open/close inventory
- `Escape`: open/close pause and settings menu
- Top-right HUD button: toggle fullscreen when the cursor is released

The settings menu pauses gameplay and provides Resume, Fullscreen, VSync,
FPS Limit, Shadows, Bloom and Quit buttons. The limiter supports `30`, `60`,
`120`, `144` and `UNLIMITED`.

UI text uses a built-in 5x7 bitmap font, so it renders consistently without
external font files.

The HUD displays in-world time, day/night state and current weather. Panels
use shadows, borders and subtle gradients for better readability.

The bottom hotbar contains five selectable tool slots. The axe starts in slot
one. Trees loaded as `tree` or `oak` resource nodes can be chopped for wood,
which is displayed in the inventory opened with `Tab`.

Dependencies (`GLFW`, `GLAD`, `GLM`) are downloaded by CMake through
`FetchContent`. PNG texture decoding uses the system `libpng` development
package.

## Tests

Configure a debug build and run the automated tests with:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

## Windows build

Install Visual Studio 2022 with the Desktop development with C++ workload,
CMake and [vcpkg](https://github.com/microsoft/vcpkg). Set `VCPKG_ROOT` to the
vcpkg directory, then build with:

```bat
build-windows.bat Release
```

Use `Debug` instead of `Release` for a debug build. The vcpkg manifest installs
`libpng` automatically. The executable is written to
`build\windows\<configuration>\pcolonist.exe`.

## Free model sources

Prefer assets explicitly licensed as `CC0`. They can be used and redistributed
in commercial projects without mandatory attribution.

- [Kenney Nature Kit](https://opengameart.org/content/nature-kit): 330+ CC0
  low-poly trees, rocks, terrain pieces, plants and camping props. This is the
  best visual and performance match for the current project.
- [OpenGameArt CC0 3D Nature collection](https://opengameart.org/content/cc0-3d-nature):
  a curated collection of CC0 rocks, vegetation, terrain and cave assets.
- [Poly Haven models](https://polyhaven.com/models): high-quality realistic
  CC0 models. Use a low-resolution variant and simplify it before importing.
  Poly Haven confirms that all of its assets are
  [CC0](https://polyhaven.com/license).
- [CG 3D](https://cg3d.org/): public-domain and CC0 models available in
  several formats, including OBJ.

To add a model, export one object as triangulated OBJ, place it under
`assets/models`, then declare and spawn it in `assets/scripts/models.scene`:

```text
model campfire models/campfire.obj
spawn_model campfire 4 0 -6 1 1 1 0.7 0.8 0.7
```

Keep models low-poly, apply transforms before export, use Y-up coordinates,
and verify the license of every individual download. MTL diffuse `Kd` colors
and PNG diffuse textures are supported.

### Importing custom models

Use the import script to validate, copy and register a custom model:

```bash
python3 tools/import_model.py ~/Models/watchtower.obj --id watchtower
```

The script copies the OBJ and its referenced MTL and PNG files into
`assets/models/custom/watchtower/` and adds its `model` declaration to
`assets/scripts/models.scene`. To place one instance immediately, provide its
position, scale and collider half extents:

```bash
python3 tools/import_model.py ~/Models/watchtower.obj --id watchtower \
  --spawn 5 0 -8 1 1 1 1.5 3 1.5
```

Custom models must meet these requirements:

- The format must be Wavefront OBJ. Other mesh formats are not supported yet.
- The OBJ must contain vertex (`v`) and face (`f`) records.
- Keep the model low-poly. Triangles are recommended; polygons are
  triangulated by the loader.
- Use Y-up coordinates, place the origin sensibly, and apply rotation and
  scale before exporting.
- Vertex normals (`vn`) are optional; the loader calculates missing normals.
- Vertex colors are supported. Otherwise, the loader uses `Kd` diffuse colors
  from a referenced MTL file or generates colors from vertex positions.
- UV coordinates (`vt`), PNG diffuse textures referenced by MTL `map_Kd`, and
  scalar MTL lighting properties are supported. Other image formats and
  normal/PBR texture maps are not supported yet.
- Keep referenced MTL and PNG files next to the OBJ. Filenames and referenced
  paths must not contain whitespace.
- Collider half extents in `spawn_model` are not calculated automatically and
  must be chosen to fit the model.
