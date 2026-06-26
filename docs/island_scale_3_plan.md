# Island Scale 3.0 Plan

## Goal

Create the first larger island pass at `3.0x` horizontal scale instead of jumping directly to `10.0x`.

The target is a world that feels much larger for traversal and exploration while staying practical for generated asset size, chunk streaming, collision rebuilds, and FPS.

## Why Not 10x First

The current generator uses a roughly `640m x 640m` terrain domain:

- `GRID = 321`
- `SPACING = 2.0`
- terrain span = `(321 - 1) * 2.0 = 640m`
- island radii are about `188m x 160m`
- chunk size is `64m`

A literal `10x` horizontal island makes the island about `100x` larger by area. With the current chunk size and content placement model, that would create too many terrain chunk assets, sparse POI distribution, long empty travel time, and expensive debug/material maps.

The first production target should be `3.0x` horizontal scale:

- full terrain span: about `1920m x 1920m`
- island radii: about `564m x 480m`
- area: about `9x` current playable land
- still small enough to iterate on generated assets locally

## Proposed First Pass Numbers

Use these as the first implementation target:

- `WORLD_SCALE = 3.0`
- `HORIZONTAL_SCALE = 3.0`
- `VERTICAL_SCALE = 1.35`
- `GRID = 481`
- `SPACING = 4.0`
- terrain span = `1920m`
- terrain vertex count = `231,361`, about `2.25x` current grid vertex count
- `chunkSize = 128.0`
- `loadRadius = 3`
- `unloadRadius = 4`
- `lod0Radius = 1`
- `lod1Radius = 2`

This keeps the streaming footprint close to the current design because the chunk size grows with world size.

## Scaling Rules

Scale horizontal positions and horizontal radii by `HORIZONTAL_SCALE`:

- island radii
- shoreline feature positions and radii
- reef and islet positions/radii
- grotto positions, chamber radius, mouth width
- water lake/river/harbor positions and widths
- volcano center, cone radius, crater radius, rim radius, gully distances
- rock spire positions and radii
- landmark positions
- generated scene placements

Scale heights conservatively by `VERTICAL_SCALE`, not by `HORIZONTAL_SCALE`:

- volcano cone height
- crater depth
- rim height
- rock spire height
- landmark vertical offsets
- terrain gaussian height contributions

Do not scale UI, player dimensions, enemy dimensions, item dimensions, or basic physics constants in this pass.

## Content Density

Do not simply multiply decoration counts by `9x`.

First-pass density targets:

- trees: current count * `3.0`
- understory: current count * `3.5`
- coastal rocks: current count * `3.0`
- palms: current count * `2.5`
- mushrooms: current count * `2.0`

This avoids turning the larger island into a dense prop field while still preventing empty terrain.

Add or redistribute POI so travel has rhythm:

- keep the central volcano as the main world anchor
- move existing POI farther apart by scale
- add 3-5 secondary traversal goals between major landmarks
- add 2-3 coastal landmarks visible from long range
- add resource clusters around each travel corridor

## Runtime Changes

The runtime must read the larger generated world as a larger world, not as scaled actor transforms.

Required runtime changes:

- update `ChunkManager::Config::chunkSize` default from `64.0` to `128.0` for scaled assets
- keep load/unload radii initially unchanged
- raise render terrain streaming distance from `340m` to around `520m`
- raise object draw distance from `310m` to around `420m`
- keep shadow far range conservative unless profiling proves it is safe
- update debug teleports after regenerated landmark positions are available

## Generator Changes

Implementation should happen in the generator first:

1. Add scale constants near generator globals:
   - `WORLD_SCALE = 3.0`
   - `HORIZONTAL_SCALE = WORLD_SCALE`
   - `VERTICAL_SCALE = 1.35`

2. Add helpers:
   - `scale_xz(point)`
   - `scale_distance(value)`
   - `scale_height(value)`
   - `scale_config(config)`

3. Apply scaled configs before cached paths are traced:
   - terrain
   - water
   - volcano
   - grottos
   - shoreline features
   - reefs/islets
   - rock spires
   - decoration rule counts

4. Regenerate in a temporary output directory first:
   - `python3 tools/generate_island.py --output-dir /tmp/pcolonist-island-scale3 --grid 481`

5. Review generated reports before replacing repo assets:
   - terrain report
   - generation quality report
   - debug preview
   - chunk file count
   - total generated asset size

## Acceptance Criteria

The scale-3 island is acceptable when:

- generated asset size remains reasonable for the repo
- chunk count does not explode beyond practical streaming
- `--validate-assets` passes
- debug preview shows continuous land, not a sparse enlarged old island
- landmarks are distributed across the island, not clustered near the old center
- player can reach major POI without long empty runs
- volcano crater, lava, lake, river, harbor, and grottos remain coherent
- FPS is stable with `--graphics medium` and playable with `--graphics low`

## Rollout

Commit order should be:

1. generator scale support and temp-output validation
2. regenerated map/debug assets
3. regenerated scene/landmarks/goals
4. runtime tuning for chunk/render distances
5. gameplay pass for travel/resource pacing

Do not merge regenerated assets until the generated reports and debug preview are reviewed.
