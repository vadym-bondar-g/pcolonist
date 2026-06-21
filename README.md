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

The executable accepts a small launch configuration layer:

```bash
pcolonist --help
pcolonist --assets assets --window-size 1600x900 --no-vsync
pcolonist --load pcolonist.save
pcolonist --validate-assets --assets assets
```

`--validate-assets` checks required runtime files without opening a window.

The generated world is a large survival-adventure island inspired by Jules
Verne's *The Mysterious Island*. It has a dominant dormant volcano, Grant Lake,
Mercy River, waterfall shelves, a sheltered natural harbor, a high Granite
House cliff and plateau, hidden stone grottos, dense temperate forest and
palm-lined beaches. A western standing-stone circle, sunken northern temple
and ruined watchtower preserve the project's mysterious atmosphere. Rebuild
the deterministic map and scenery placement with:

```bash
python3 tools/generate_island.py
```

For faster iteration, limit the generator to one output group or redirect the
generated assets into a scratch directory:

```bash
python3 tools/generate_island.py --only map --seed 1847 --grid 241
python3 tools/generate_island.py --only debug --grid 61 --output-dir /tmp/pcolonist-island
python3 tools/generate_island.py --only scene
```

`--only map` writes the terrain, split meshes, LODs, internal water and
landmarks. `--only debug` writes traversal/material/biome debug maps and the
terrain report. `--only scene` writes only `assets/scripts/models.scene`.
Every run also writes `assets/maps/generation_quality.json` with output-file,
landmark, river, traversal and decoration-placement checks, plus
`assets/maps/debug_preview.html` with inline previews of generated debug maps.
The generator caches repeated terrain, river, biome and moisture samples during
each run, so scene placement and debug-map iteration avoid recalculating the
same points.
The scene generation step also emits `assets/scripts/gameplay_goals.json`, a
Russian action and objective catalog for survival tasks, resource gathering,
crafting, exploration and final island goals.

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
  raymarched volumetric clouds with parallax and internal lighting, high
  cirrus streaks, low storm shelf clouds, richer twilight bands, moon phases,
  faint constellation lines, storm lightning and spherical twinkling stars. The sun and moon discs use
  analytic ray-sphere intersections in the sky fragment pass. Approximate
  Rayleigh/Mie scattering, a procedural Milky Way and lunar surface variation
  add depth without additional render passes.
- The scene shader adds material-specific noise for bark, leaves, cloth, lichen,
  water, lava and the campfire flame.
- A full 180-second day/night cycle moves the sun and moon across the sky,
  transitions through sunrise and sunset, and switches scene lighting between
  warm sunlight and cool moonlight.
- The world tracks consecutive day numbers while the sun follows a moving
  east-to-west arc across each full day.
- Directional sunlight, ambient light, specular highlights, rim lighting,
  tone mapping, gamma correction and 4x MSAA improve scene rendering.
- The post-process pass applies FXAA edge smoothing, subtle sharpening,
  multi-scale bloom, depth-based aerial perspective, ACES-style tone mapping,
  adaptive day/night color grading, dithering and vignette. Linear-depth
  screen-space occlusion and distance-aware sharpening preserve nearby detail
  without producing halos on the sky.
- Procedural surface variation prevents large untextured meshes from looking
  completely flat. Water uses layered waves, stronger Fresnel reflections and
  animated highlights.
- The procedural sky uses multi-octave illuminated clouds in addition to its
  sun, moon, stars and horizon haze.
- A 2048x2048 directional shadow map with PCF filtering adds soft dynamic
  shadows from the current sun or moon direction.
- Near and far stabilized shadow cascades preserve detail around the player
  while covering the larger island. Their overlap is blended to hide the
  transition between cascade resolutions.
- Terrain colors form height-dependent wet sand, beach, low grass, forest
  soil, high grass and mountain-rock layers. Procedural micro-normals, grains,
  stone veins, grass fibers, moving cloud shadows, wet shoreline shading,
  slope and noise soften their boundaries.
- Repeated meshes are rendered in instanced batches and small distant objects
  are culled to reduce draw calls and vertex work.
- The scene renders into an HDR framebuffer and passes through exposure tone
  mapping, bloom, color grading and vignette post-processing.
- `WeatherSystem` cycles through clear, cloudy and storm conditions, exposing
  per-weather sky parameters for cloud wind, haze, storm strength, star
  visibility and the procedural moon phase.
- The developer panel can cycle sky quality through `OFF`, `LOW`, `MED` and
  `HIGH`; lower modes reduce cloud raymarch steps and self-shadowing to isolate
  sky-related FPS drops.
- A dense animated water mesh surrounds the playable map, with crest foam,
  breaking shoreline foam, crossed wave normals, shallow-water tinting,
  turbidity, caustic shimmer and restrained Fresnel reflections. Underwater
  rendering adds caustics and suspended particles.
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
- Use `frame_counter on` in a script to show the in-scene FPS counter, or
  `frame_counter off` to hide it again.
- `spawn_collider px py pz halfX halfY halfZ` creates an invisible static
  collision volume. The generated island uses these volumes for three
  walkable stone grottos with open entrances, walls and ceilings.
- The OBJ loader supports triangulation, negative indices, exported vertex
  normals, split face normals, optional vertex colors and diffuse `Kd` colors
  from MTL materials. UV-mapped PNG diffuse textures referenced by `map_Kd`
  are supported. MTL `Ns`, `Ks` and `Ke` values control roughness, specular
  strength and emissive lighting.
- `SceneSerializer` serializes transforms and rigid bodies and validates a
  round-trip during startup.
- `FrameArena` provides temporary per-frame memory.
- `UiSystem` displays FPS and entity counts in the window title, with an
  optional scene-controlled FPS counter in the HUD.
- The screen HUD renders a crosshair, status panel, cursor hint and a
  clickable fullscreen button.
- `F11` or the top-right HUD button switches between windowed and fullscreen
  modes while preserving the previous window position and size.
- `~` opens a developer testing panel with respawn, landmark teleports,
  weather and time-of-day controls, plus shadow and bloom toggles.
- Use `NEXT LANDMARK` in that panel to cycle through Granite House, the natural
  harbor, Grant Lake, Mercy River and the hidden grottos.
- Original low-poly tree and rock OBJ models are included under
  `assets/models` and placed as physical scenery.
- A generated coastal palm model forms a tropical shoreline belt around the
  harbor and southern beaches, while denser temperate trees remain inland.
- Deterministic smooth higher-poly layered tree, bush and rock models use
  dense radial and vertical geometry and can be rebuilt with
  `python3 tools/generate_nature_models.py`. Imported Kenney props can be
  refined with UV-preserving beveled geometry by running
  `blender --background --python tools/refine_imported_models.py`.
- Object materials receive procedural bark fibers, foliage variation, stone
  pores and fine cloth grain in addition to their MTL colors and PNG textures.
- The island uses a denser terrain mesh, multi-scale procedural ground detail
  and understory bushes to improve silhouettes and forest depth. Its generated
  height field combines broad ridges, valleys, walkable terraces, deterministic
  fractal detail, erosion channels, coastal rock shelves and a weathered
  volcanic rim. Alternating triangle diagonals avoid a visible directional
  grid. The expanded island is centered around a volcanic cone with an animated
  magma crater, and is divided into a northern plateau, inland lake, river
  valley, eastern bay, marsh and southern peninsula.
- Mercy River is traced downhill across the generated base terrain and receives
  a descending carved channel profile before the final mesh is emitted. Grant
  Lake has a level basin and both waters are emitted as a separate render mesh.
- The river system includes configurable tributaries, wet banks, a flattened
  floodplain and local waterfall shelves, so the drainage reads as a connected
  terrain feature instead of a single straight strip.
- Irregular granite needles and coastal rock stacks break up the height-field
  silhouette around exposed shores and mountain approaches.
- The shoreline is shaped by configurable coves, headlands, reef fields and
  small offshore islets instead of a single oval outline.
- Hidden grottos have configurable entrance direction, chamber size and carved
  terrain approaches, so their entrances sit in the island surface rather than
  only being placed as separate props.
- The island generator exposes grouped tuning blocks near the top of
  `tools/generate_island.py` for terrain scale, water levels, river width, the
  volcano profile, shoreline features, Granite House, grottos and rock-spire
  placement. Start there when adjusting the island model shape.
- Biome rules are also configurable there. Beach, wetland, coastal jungle,
  temperate forest, highland and volcanic biomes drive terrain vertex colors
  and vegetation density.
- Decoration placement uses configurable rules for forests, understory, coastal
  rocks, palms and mushrooms. Each rule controls count, allowed biomes, height,
  slope, moisture and avoidance ranges.
- The generator writes explicit OBJ vertex normals and two lighter terrain LOD
  meshes, `assets/maps/demo_map_lod1.obj` and `assets/maps/demo_map_lod2.obj`,
  so terrain chunks use the full mesh nearby, LOD1 at mid range and LOD2 in the
  distance.
- It also emits split OBJ layers for downstream tools:
  `assets/maps/demo_map_terrain.obj`, `assets/maps/demo_map_structures.obj` and
  `assets/maps/demo_map_rocks.obj`, while keeping `demo_map.obj` as the
  combined runtime-compatible mesh.
- The same run exports `assets/maps/walkability.pgm`, a grayscale traversal
  mask where dark pixels are blocked/steep/wet and bright pixels are easier to
  cross.
- The generator also writes `assets/maps/heightmap.pgm`,
  `assets/maps/biomes.ppm`, `assets/maps/slope.pgm`,
  `assets/maps/moisture.pgm` and `assets/maps/terrain_report.json`. These
  debug outputs make terrain height, biome distribution, slope, moisture,
  walkability stats and decoration tuning values visible without opening the
  full OBJ mesh.
- Generated OBJ maps include planar UV coordinates, so external tools can
  apply terrain textures without reconstructing UVs. The generator also emits
  grayscale material masks for `sand`, `grass`, `rock`, `volcanic` and
  `wetness` under `assets/maps/material_*.pgm`.
- Navigation points are written to `assets/maps/landmarks.json`, including the
  camp, harbor, lake, river endpoints, waterfall shelves, volcano, ruins,
  grottos, tributary sources and offshore islets.
- Vegetation placement uses local elevation, slope, moisture and distance to
  water, keeping steep dry ridges exposed and concentrating forest and
  understory in sheltered wet valleys.
- Terrain rendering is split into seamless 64-unit chunks. The complete
  collider remains resident, while chunk GPU meshes are uploaded lazily as
  they enter the camera's streaming radius.
- Terrain uses seamless triplanar earth, sand and basalt textures blended by
  height, slope and proximity to the volcano. Procedural fine detail is layered
  over the textures to reduce visible repetition. Noisy material boundaries,
  large-scale color variation and reflective wet shoreline sand make the
  transitions less uniform.
- Selected camp, vegetation and prop models from Kenney's CC0 3D Nature Pack
  are included under `assets/models/kenney`; the original license is kept next
  to the models. The imported source is
  [Kenney 3D Nature Pack](https://opengameart.org/content/3d-nature-pack).
- `AudioSystem` manages sources and playback state. A decoder/output backend
  such as miniaudio or OpenAL is the next layer needed for audible output.

## Controls

- `W`, `A`, `S`, `D`: move
- `~`: open the developer testing panel
- Mouse: look around
- `Space`: jump or swim upward
- `Left Shift`: dive while swimming
- `F1`: release/capture mouse
- `F11`: toggle fullscreen
- `1`-`5`: select a tool slot
- Left mouse button: use the selected tool; the axe damages nearby targeted
  trees and adds wood after three hits
- `E`: perform the nearby survival action; collect water at Grant Lake or
  Mercy River, collect stone near rocky areas, or light the campfire at the
  arrival camp after collecting wood and stone
- `Tab`: open/close inventory
- `Escape`: open/close pause and settings menu
- Top-right HUD button: toggle fullscreen when the cursor is released

The main menu pauses gameplay and opens as a standalone splash screen. It has
separate Play, Load Game, Settings and Exit entries. Load Game currently opens
a save-slot screen and reports that no saves exist until persistence is added.
Settings opens a separate page for Fullscreen, VSync, FPS Limit, Shadows and
Bloom. The limiter supports `30`, `60`, `120`, `144` and `UNLIMITED`.

UI text uses a built-in 5x7 bitmap font by default, so the runtime does not
need external font libraries. TTF rendering can be enabled with
`PCOLONIST_ENABLE_FREETYPE_UI=ON`; then the runtime prefers
`assets/fonts/ComicSansMS.ttf`, `assets/fonts/NotoSans-Regular.ttf` and common
system Sans fonts. The bitmap font remains as a fallback.

The HUD uses a strict graphite visual system with flat surfaces, compact
square controls, thin dividers and a single cold accent for active states.
It displays in-world time, day/night state and current weather. Panels
use shadows, borders and subtle gradients for better readability.

The bottom hotbar contains five selectable tool slots. The axe starts in slot
one. Trees loaded as `tree` or `oak` resource nodes can be chopped for wood,
which is displayed in the inventory opened with `Tab`.

Dependencies (`GLFW`, `GLAD`, `GLM`, `zlib` and `libpng`) are downloaded and
built by CMake through `FetchContent`.

## Tests

Configure a debug build and run the automated tests with:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

## Windows build

Install Visual Studio 2022 with the Desktop development with C++ workload,
CMake and Git, or let the build script bootstrap them through `winget`:

```bat
build-windows.bat Release
build-windows.bat Release --bootstrap
build-windows.bat --bootstrap-only
build-windows.bat Debug --clean --no-package
```

Use `Debug` instead of `Release` for a debug build. CMake downloads and builds
all library dependencies automatically. Runtime assets are copied next to the
executable. The build script runs tests before creating a portable
`dist\pcolonist-windows-x64-<configuration>.zip` archive.
Bootstrap mode installs Git, CMake and Visual Studio Build Tools when missing.
It requires `winget` and may request administrator permission from Windows.

Every script invocation writes its complete output to
`build\logs\build-windows.log`. The log is overwritten by the next invocation
and its path is printed after success or failure.

When changing dependency versions or recovering from an old dependency build,
run `build-windows.bat Release --clean` to regenerate the CMake dependency
cache.

Windows build options:

- `--clean` removes the previous Windows build before configuring.
- `--no-tests` skips automated tests and does not build their target.
- `--no-package` skips portable ZIP creation.
- `--bootstrap-only` installs dependencies without starting a build.
- `--help` prints all supported options.

Portable packages contain only `pcolonist.exe`, runtime DLLs, assets and the
README instead of the complete Visual Studio output directory. A matching
`.sha256` file is generated for archive verification.

The renderer currently uses OpenGL 3.3. Its sky pass performs software
raymarching and analytic ray intersections, but it does not use NVIDIA RTX
hardware. Hardware ray tracing requires a separate Vulkan Ray Tracing or
DirectX 12 DXR renderer.

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
