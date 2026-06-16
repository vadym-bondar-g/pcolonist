#!/usr/bin/env python3
"""Generate the mysterious island map and its scenery placement script."""

from __future__ import annotations

import math
import random
from dataclasses import dataclass, field
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
MAP_PATH = ROOT / "assets" / "maps" / "demo_map.obj"
MAP_LOD_PATHS = (
    ROOT / "assets" / "maps" / "demo_map_lod1.obj",
    ROOT / "assets" / "maps" / "demo_map_lod2.obj",
)
INTERNAL_WATER_PATH = ROOT / "assets" / "maps" / "internal_water.obj"
SCENE_PATH = ROOT / "assets" / "scripts" / "models.scene"
SEED = 1847
GRID = 241
SPACING = 2.0
LOD_STEPS = (2, 4)


@dataclass(frozen=True)
class TerrainConfig:
    island_radius_x: float = 132.0
    island_radius_z: float = 112.0
    coast_start: float = 0.76
    floor_height: float = -8.0
    terrace_step: float = 1.6
    terrace_strength: float = 0.62


@dataclass(frozen=True)
class TributaryConfig:
    source: tuple[float, float]
    join_progress: float
    width_start: float = 1.1
    width_end: float = 2.2
    source_cut: float = 1.1


@dataclass(frozen=True)
class WaterConfig:
    lake_center: tuple[float, float] = (31.0, -24.0)
    river_source: tuple[float, float] = (39.0, -20.0)
    harbor_center: tuple[float, float] = (104.0, 25.0)
    lake_level: float = 4.35
    lake_radius_x: float = 17.0
    lake_radius_z: float = 12.0
    lake_basin_radius_x: float = 18.0
    lake_basin_radius_z: float = 13.0
    lake_bank_inner: float = 1.0
    lake_bank_outer: float = 1.28
    lake_bed_center: float = 2.65
    lake_bed_edge: float = 3.45
    river_width_start: float = 2.2
    river_width_end: float = 4.6
    river_bank_width: float = 7.5
    river_source_cut: float = 2.25
    river_outlet_level: float = -0.35
    floodplain_width: float = 14.0
    wet_bank_width: float = 22.0
    waterfall_drop: float = 0.9
    waterfall_progresses: tuple[float, ...] = (0.28, 0.56)
    tributaries: tuple[TributaryConfig, ...] = (
        TributaryConfig((-13.0, -48.0), 0.23, 1.1, 2.0, 1.25),
        TributaryConfig((63.0, -56.0), 0.48, 0.9, 1.8, 1.05),
        TributaryConfig((74.0, 45.0), 0.78, 1.0, 2.1, 1.15),
    )


@dataclass(frozen=True)
class VolcanoConfig:
    center: tuple[float, float] = (0.0, 0.0)
    cone_radius: float = 49.0
    cone_height: float = 38.0
    crater_radius: float = 15.0
    crater_depth: float = 26.0
    rim_radius: float = 18.0
    rim_sharpness: float = 4.2
    rim_height: float = 3.0
    gully_count: float = 9.0
    gully_depth: float = 2.2


@dataclass(frozen=True)
class RockSpireConfig:
    position: tuple[float, float]
    radius: float
    height: float


@dataclass(frozen=True)
class ShoreFeatureConfig:
    center: tuple[float, float]
    radius_x: float
    radius_z: float
    strength: float
    kind: str


@dataclass(frozen=True)
class GrottoConfig:
    position: tuple[float, float]
    entrance_direction: tuple[float, float]
    chamber_radius: float = 8.0
    mouth_width: float = 7.0
    floor_height: float | None = None


@dataclass(frozen=True)
class BiomeConfig:
    name: str
    color_low: tuple[float, float, float]
    color_high: tuple[float, float, float]
    min_height: float = -100.0
    max_height: float = 100.0
    min_moisture: float = 0.0
    max_slope: float = 10.0
    tree_density: float = 0.0
    bush_density: float = 0.0
    oak_preference: float = 0.5


@dataclass(frozen=True)
class IslandModelConfig:
    terrain: TerrainConfig = field(default_factory=TerrainConfig)
    water: WaterConfig = field(default_factory=WaterConfig)
    volcano: VolcanoConfig = field(default_factory=VolcanoConfig)
    grottos: tuple[GrottoConfig, ...] = (
        GrottoConfig((-72.0, -54.0), (0.45, 0.89), 8.5, 7.5),
        GrottoConfig((65.0, 70.0), (-0.65, -0.76), 8.0, 7.0),
        GrottoConfig((-91.0, 67.0), (0.95, -0.31), 7.5, 6.8),
    )
    granite_house: tuple[float, float] = (-104.0, -10.0)
    shoreline_features: tuple[ShoreFeatureConfig, ...] = (
        ShoreFeatureConfig((111.0, 24.0), 38.0, 34.0, 0.25, "cove"),
        ShoreFeatureConfig((127.0, 24.0), 20.0, 13.0, 0.19, "cove"),
        ShoreFeatureConfig((96.0, -42.0), 28.0, 21.0, 0.11, "cove"),
        ShoreFeatureConfig((-126.0, -64.0), 30.0, 22.0, 0.10, "cove"),
        ShoreFeatureConfig((108.0, 49.0), 28.0, 18.0, 0.10, "headland"),
        ShoreFeatureConfig((108.0, -2.0), 28.0, 18.0, 0.10, "headland"),
        ShoreFeatureConfig((-25.0, 104.0), 42.0, 34.0, 0.12, "headland"),
        ShoreFeatureConfig((-114.0, 43.0), 24.0, 17.0, 0.08, "headland"),
    )
    reef_fields: tuple[tuple[float, float, float, float], ...] = (
        (92.0, 77.0, 19.0, 8.0),
        (122.0, -38.0, 17.0, 6.5),
        (-117.0, -82.0, 20.0, 7.5),
    )
    islets: tuple[tuple[float, float, float, float], ...] = (
        (139.0, 61.0, 9.0, 5.0),
        (-136.0, -78.0, 10.0, 4.5),
    )
    biomes: tuple[BiomeConfig, ...] = (
        BiomeConfig("beach", (0.42, 0.34, 0.20), (0.68, 0.58, 0.36), -1.0, 1.2, 0.0, 0.55, 0.0, 0.05),
        BiomeConfig("wetland", (0.11, 0.23, 0.13), (0.24, 0.38, 0.20), -0.4, 4.5, 0.62, 0.48, 0.25, 0.85, 0.75),
        BiomeConfig("coastal_jungle", (0.12, 0.28, 0.07), (0.30, 0.48, 0.12), 0.0, 5.5, 0.45, 0.62, 0.78, 0.65, 0.55),
        BiomeConfig("temperate_forest", (0.13, 0.23, 0.08), (0.31, 0.37, 0.14), 1.0, 13.0, 0.22, 0.70, 0.68, 0.46, 0.72),
        BiomeConfig("highland", (0.20, 0.25, 0.18), (0.43, 0.42, 0.32), 5.0, 22.0, 0.0, 1.15, 0.16, 0.18, 0.35),
        BiomeConfig("volcanic", (0.14, 0.13, 0.12), (0.34, 0.32, 0.29), 8.0, 100.0, 0.0, 10.0, 0.0, 0.02, 0.0),
    )
    rock_spires: tuple[RockSpireConfig, ...] = (
        RockSpireConfig((-116.0, -31.0), 5.5, 18.0),
        RockSpireConfig((-118.0, 13.0), 4.2, 15.0),
        RockSpireConfig((-108.0, 34.0), 3.8, 13.0),
        RockSpireConfig((111.0, -8.0), 4.8, 11.0),
        RockSpireConfig((119.0, 50.0), 5.2, 14.0),
        RockSpireConfig((-72.0, -79.0), 4.0, 12.0),
    )


CONFIG = IslandModelConfig()
# Tune the island model here first. The generator below reads these grouped
# parameters instead of scattering landmark, water and volcano values inline.
TERRAIN = CONFIG.terrain
WATER = CONFIG.water
VOLCANO = CONFIG.volcano
GROTTOS = CONFIG.grottos
GROTTO_POSITIONS = tuple(grotto.position for grotto in CONFIG.grottos)
BIOMES = {biome.name: biome for biome in CONFIG.biomes}
GRANITE_HOUSE = CONFIG.granite_house
HARBOR_CENTER = WATER.harbor_center
LAKE_CENTER = WATER.lake_center
RIVER_SOURCE = WATER.river_source
ROCK_SPIRES = CONFIG.rock_spires
RIVER_PATH: tuple[tuple[float, float], ...] = ()
TRIBUTARY_PATHS: tuple[tuple[tuple[float, float], ...], ...] = ()


def smoothstep(edge0: float, edge1: float, value: float) -> float:
    value = max(0.0, min(1.0, (value - edge0) / (edge1 - edge0)))
    return value * value * (3.0 - 2.0 * value)


def noise(x: float, z: float) -> float:
    return (
        math.sin(x * 0.13 + z * 0.07) * 0.48
        + math.sin(x * 0.31 - z * 0.19) * 0.24
        + math.cos(x * 0.08 + z * 0.27) * 0.18
    )


def hash_noise(x: int, z: int) -> float:
    value = (x * 0x1F123BB5) ^ (z * 0x5F356495) ^ SEED
    value = (value ^ (value >> 15)) * 0x2C1B3C6D
    value = (value ^ (value >> 12)) * 0x297A2D39
    return ((value ^ (value >> 15)) & 0xFFFFFFFF) / 0xFFFFFFFF * 2.0 - 1.0


def value_noise(x: float, z: float) -> float:
    cell_x = math.floor(x)
    cell_z = math.floor(z)
    local_x = smoothstep(0.0, 1.0, x - cell_x)
    local_z = smoothstep(0.0, 1.0, z - cell_z)
    bottom = hash_noise(cell_x, cell_z) * (1.0 - local_x) + hash_noise(cell_x + 1, cell_z) * local_x
    top = hash_noise(cell_x, cell_z + 1) * (1.0 - local_x) + hash_noise(cell_x + 1, cell_z + 1) * local_x
    return bottom * (1.0 - local_z) + top * local_z


def fractal_noise(x: float, z: float, octaves: int = 5) -> float:
    result = 0.0
    amplitude = 0.55
    frequency = 0.018
    for _ in range(octaves):
        result += value_noise(x * frequency, z * frequency) * amplitude
        frequency *= 2.07
        amplitude *= 0.5
    return result


def ridged_noise(x: float, z: float) -> float:
    broad = 1.0 - abs(value_noise(x * 0.024 + 17.0, z * 0.024 - 9.0))
    fine = 1.0 - abs(value_noise(x * 0.071 - 31.0, z * 0.071 + 14.0))
    return broad * broad * 0.72 + fine * fine * 0.28


def gaussian(
    x: float,
    z: float,
    cx: float,
    cz: float,
    radius_x: float,
    radius_z: float,
    height: float,
) -> float:
    distance = ((x - cx) / radius_x) ** 2 + ((z - cz) / radius_z) ** 2
    return math.exp(-distance * 2.5) * height


def normalized2(vector: tuple[float, float]) -> tuple[float, float]:
    length = math.hypot(vector[0], vector[1])
    if length <= 0.000001:
        return 1.0, 0.0
    return vector[0] / length, vector[1] / length


def segment_distance(x: float, z: float, start: tuple[float, float], end: tuple[float, float]) -> float:
    ax, az = start
    bx, bz = end
    length_squared = (bx - ax) ** 2 + (bz - az) ** 2
    progress = max(0.0, min(1.0, ((x - ax) * (bx - ax) + (z - az) * (bz - az)) / length_squared))
    return math.hypot(x - (ax + (bx - ax) * progress), z - (az + (bz - az) * progress))


def island_radius(x: float, z: float) -> float:
    angle = math.atan2(z / TERRAIN.island_radius_z, x / TERRAIN.island_radius_x)
    shoreline = (
        1.0
        + math.sin(angle * 3.0 + 0.4) * 0.08
        + math.sin(angle * 7.0 - 0.7) * 0.035
        + math.sin(angle * 13.0 + 1.8) * 0.018
        + value_noise(math.cos(angle) * 3.1 + 12.0, math.sin(angle) * 3.1 - 7.0) * 0.045
    )
    feature_offset = 0.0
    for feature in CONFIG.shoreline_features:
        cx, cz = feature.center
        amount = gaussian(x, z, cx, cz, feature.radius_x, feature.radius_z, feature.strength)
        feature_offset += amount if feature.kind == "headland" else -amount
    return math.hypot(x / TERRAIN.island_radius_x, z / TERRAIN.island_radius_z) / (shoreline + feature_offset)


def flattening_profile(x: float, z: float) -> tuple[float, float]:
    clearing = math.exp(-(((x + 25.0) / 22.0) ** 2 + ((z - 75.0) / 22.0) ** 2) * 0.9)
    path = math.exp(-((x + 0.15 * z + 14.0) / 5.0) ** 2) * math.exp(-((z - 10.0) / 95.0) ** 2)
    temple_clearing = math.exp(-(((x / 20.0) ** 2) + (((z + 84.0) / 18.0) ** 2)) * 2.0)
    circle_clearing = math.exp(-((((x + 82.0) / 20.0) ** 2) + (((z - 38.0) / 20.0) ** 2)) * 2.0)
    tower_clearing = math.exp(-((((x - 91.0) / 17.0) ** 2) + (((z - 18.0) / 17.0) ** 2)) * 2.0)
    granite_house_terrace = math.exp(-((((x + 102.0) / 17.0) ** 2) + (((z + 10.0) / 15.0) ** 2)) * 1.8)
    harbor_beach = math.exp(-((((x - 101.0) / 25.0) ** 2) + (((z - 25.0) / 21.0) ** 2)) * 1.5)
    grotto_profiles = tuple(
        (
            math.exp(-((((x - grotto.position[0]) / 13.0) ** 2) + (((z - grotto.position[1]) / 17.0) ** 2)) * 1.7),
            grotto.floor_height if grotto.floor_height is not None else base_terrain_height(*grotto.position) - 0.15,
        )
        for grotto in GROTTOS
    )
    profiles = (
        (clearing, 0.7),
        (path * 0.72, 0.3),
        (temple_clearing, 0.35),
        (circle_clearing, 0.45),
        (tower_clearing, 0.35),
        (granite_house_terrace * 0.92, 5.6),
        (harbor_beach * 0.7, -0.2),
        *grotto_profiles,
    )
    return max(profiles, key=lambda profile: profile[0])


def base_terrain_height(x: float, z: float) -> float:
    radius = island_radius(x, z)
    coast = max(0.0, radius - TERRAIN.coast_start)
    inland = 1.0 - smoothstep(0.68, 1.04, radius)
    detail = fractal_noise(x, z)
    ridges = ridged_noise(x, z)
    height = noise(x, z) * 0.55 + detail * (0.7 + inland * 2.0) - coast * coast * 27.0
    height += ridges * inland * inland * 2.4

    # A broken rock shelf gives the coast readable cliffs, coves and beaches.
    cliff_band = smoothstep(0.73, 0.81, radius) * (1.0 - smoothstep(0.86, 0.95, radius))
    cliff_exposure = smoothstep(-0.1, 0.55, value_noise(x * 0.035 + 4.0, z * 0.035 - 11.0))
    beach_break = smoothstep(0.2, 0.75, value_noise(x * 0.022 - 20.0, z * 0.022 + 5.0))
    height += cliff_band * cliff_exposure * beach_break * (2.5 + ridges * 2.2)

    # A volcanic western massif, northern plateau and long central ridge divide the island.
    height += gaussian(x, z, -42.0, -27.0, 48.0, 42.0, 18.0)
    height += gaussian(x, z, -54.0, -34.0, 22.0, 20.0, 10.0)
    height += gaussian(x, z, -80.0, 48.0, 32.0, 27.0, 9.5)
    height += gaussian(x, z, 47.0, -66.0, 42.0, 27.0, 10.5)
    height += gaussian(x, z, 22.0, -18.0, 78.0, 15.0, 5.0)

    # Granite House stands in a high western cliff below a broad, habitable plateau.
    height += gaussian(x, z, -105.0, -11.0, 24.0, 35.0, 18.0)
    height += gaussian(x, z, -82.0, -8.0, 45.0, 42.0, 7.0)
    granite_face = smoothstep(0.0, 1.0, (x + 118.0) / 17.0) * (1.0 - smoothstep(0.0, 1.0, (x + 91.0) / 14.0))
    granite_span = 1.0 - smoothstep(22.0, 43.0, abs(z + 10.0))
    height += granite_face * granite_span * (5.5 + ridges * 2.0)

    # The central volcano dominates the island; a deep summit crater holds magma.
    volcano_x, volcano_z = VOLCANO.center
    height += gaussian(
        x,
        z,
        volcano_x,
        volcano_z,
        VOLCANO.cone_radius,
        VOLCANO.cone_radius,
        VOLCANO.cone_height,
    )
    height -= gaussian(
        x,
        z,
        volcano_x,
        volcano_z,
        VOLCANO.crater_radius,
        VOLCANO.crater_radius,
        VOLCANO.crater_depth,
    )
    crater_radius = math.hypot(x - volcano_x, z - volcano_z)
    crater_rim = math.exp(-((crater_radius - VOLCANO.rim_radius) / VOLCANO.rim_sharpness) ** 2)
    height += crater_rim * (VOLCANO.rim_height + ridges * 2.0)

    # Radial gullies make the volcano look weathered instead of perfectly smooth.
    angle = math.atan2(z - volcano_z, x - volcano_x)
    gully = max(0.0, math.sin(angle * VOLCANO.gully_count + detail * 2.0)) ** 7
    height -= (
        gully
        * smoothstep(VOLCANO.rim_radius + 1.0, VOLCANO.rim_radius + 13.0, crater_radius)
        * (1.0 - smoothstep(VOLCANO.rim_radius + 13.0, VOLCANO.cone_radius + 14.0, crater_radius))
        * VOLCANO.gully_depth
    )

    # Low valleys form natural travel corridors and an eastern marsh/bay.
    height -= gaussian(x, z, 27.0, 35.0, 54.0, 42.0, 3.1)
    height -= gaussian(x, z, 2.0, 7.0, 24.0, 70.0, 1.8)
    height -= gaussian(x, z, 91.0, 18.0, 34.0, 28.0, 2.8)

    # Grant Lake and the natural harbor anchor the generated drainage system.
    height -= gaussian(x, z, LAKE_CENTER[0], LAKE_CENTER[1], 23.0, 18.0, 5.2)
    lake_distance = math.hypot(
        (x - LAKE_CENTER[0]) / WATER.lake_basin_radius_x,
        (z - LAKE_CENTER[1]) / WATER.lake_basin_radius_z,
    )
    lake_bank_blend = 1.0 - smoothstep(WATER.lake_bank_inner, WATER.lake_bank_outer, lake_distance)
    lake_bed = WATER.lake_bed_center + smoothstep(0.0, 1.0, lake_distance) * (
        WATER.lake_bed_edge - WATER.lake_bed_center
    )
    lowered_lake_bed = min(height, lake_bed)
    height = height * (1.0 - lake_bank_blend) + lowered_lake_bed * lake_bank_blend
    height -= gaussian(x, z, HARBOR_CENTER[0], HARBOR_CENTER[1], 30.0, 23.0, 4.6)

    # Hidden grottos are carved into the terrain with flattened chambers and recessed mouths.
    for grotto in GROTTOS:
        gx, gz = grotto.position
        dx, dz = normalized2(grotto.entrance_direction)
        chamber = gaussian(x, z, gx, gz, grotto.chamber_radius, grotto.chamber_radius * 0.82, 1.8)
        mouth_start = (gx - dx * 2.5, gz - dz * 2.5)
        mouth_end = (gx + dx * 12.0, gz + dz * 12.0)
        mouth = math.exp(-((segment_distance(x, z, mouth_start, mouth_end) / grotto.mouth_width) ** 2) * 2.4)
        approach = gaussian(x, z, gx + dx * 9.0, gz + dz * 9.0, grotto.mouth_width * 1.8, 8.0, 0.9)
        height -= chamber + mouth * 1.15 + approach

    # Secondary erosion channels add readable drainage without blocking travel.
    channel_a = abs(math.sin(x * 0.035 + z * 0.018 + detail * 1.8))
    channel_b = abs(math.sin(x * 0.021 - z * 0.041 - detail * 1.4))
    channels = (1.0 - smoothstep(0.0, 0.11, min(channel_a, channel_b))) * inland
    height -= channels * smoothstep(2.0, 11.0, height) * 0.75
    return height


def trace_downhill_path(
    source: tuple[float, float],
    target: tuple[float, float],
    max_steps: int,
    step_size: float = 4.0,
) -> tuple[tuple[float, float], ...]:
    path = [source]
    previous_direction = (1.0, 0.0)
    for _ in range(max_steps):
        x, z = path[-1]
        if len(path) > 4 and (base_terrain_height(x, z) < -0.55 or island_radius(x, z) > 0.98):
            break
        to_target = (target[0] - x, target[1] - z)
        distance = math.hypot(*to_target)
        if distance < 5.0:
            break
        direct_angle = math.atan2(to_target[1], to_target[0])
        candidates = []
        for offset in (-1.15, -0.82, -0.52, -0.28, 0.0, 0.28, 0.52, 0.82, 1.15):
            angle = direct_angle + offset
            candidate = (x + math.cos(angle) * step_size, z + math.sin(angle) * step_size)
            direction = ((candidate[0] - x) / step_size, (candidate[1] - z) / step_size)
            turn = 1.0 - (direction[0] * previous_direction[0] + direction[1] * previous_direction[1])
            score = (
                base_terrain_height(*candidate)
                + math.hypot(candidate[0] - target[0], candidate[1] - target[1]) * 0.11
                + turn * 0.55
                + abs(offset) * 0.08
            )
            candidates.append((score, candidate, direction))
        _, point, previous_direction = min(candidates, key=lambda candidate: candidate[0])
        path.append(point)
        if math.hypot(point[0] - target[0], point[1] - target[1]) < step_size * 1.2:
            path.append(target)
            break
    return tuple(path)


def trace_river() -> tuple[tuple[float, float], ...]:
    return trace_downhill_path(RIVER_SOURCE, HARBOR_CENTER, 48)


def path_point(path: tuple[tuple[float, float], ...], progress: float) -> tuple[float, float]:
    if len(path) == 1:
        return path[0]
    scaled = max(0.0, min(1.0, progress)) * (len(path) - 1)
    index = min(int(scaled), len(path) - 2)
    local = scaled - index
    start = path[index]
    end = path[index + 1]
    return start[0] * (1.0 - local) + end[0] * local, start[1] * (1.0 - local) + end[1] * local


def trace_tributaries() -> tuple[tuple[tuple[float, float], ...], ...]:
    return tuple(
        trace_downhill_path(tributary.source, path_point(RIVER_PATH, tributary.join_progress), 28, 3.4)
        for tributary in WATER.tributaries
    )


def path_sample(path: tuple[tuple[float, float], ...], x: float, z: float) -> tuple[float, float]:
    result = (float("inf"), 0.0)
    segment_count = max(1, len(path) - 1)
    for index, (start, end) in enumerate(zip(path, path[1:])):
        ax, az = start
        bx, bz = end
        length_squared = (bx - ax) ** 2 + (bz - az) ** 2
        if length_squared <= 0.000001:
            continue
        local = max(0.0, min(1.0, ((x - ax) * (bx - ax) + (z - az) * (bz - az)) / length_squared))
        distance = math.hypot(x - (ax + (bx - ax) * local), z - (az + (bz - az) * local))
        if distance < result[0]:
            result = distance, (index + local) / segment_count
    return result


def river_sample(x: float, z: float) -> tuple[float, float]:
    return path_sample(RIVER_PATH, x, z)


def river_network_sample(x: float, z: float) -> tuple[float, float, float, float, tuple[float, float]]:
    distance, progress = river_sample(x, z)
    width = WATER.river_width_start * (1.0 - progress) + WATER.river_width_end * progress
    source_cut = WATER.river_source_cut
    source = RIVER_PATH[0]
    for tributary, path in zip(WATER.tributaries, TRIBUTARY_PATHS):
        tributary_distance, tributary_progress = path_sample(path, x, z)
        tributary_width = tributary.width_start * (1.0 - tributary_progress) + tributary.width_end * tributary_progress
        if tributary_distance < distance:
            distance = tributary_distance
            progress = tributary_progress
            width = tributary_width
            source_cut = tributary.source_cut
            source = path[0]
    return distance, progress, width, source_cut, source


def river_distance(x: float, z: float) -> float:
    return river_network_sample(x, z)[0]


def waterfall_offset(progress: float) -> float:
    return sum(
        math.exp(-((progress - step) / 0.028) ** 2) * WATER.waterfall_drop
        for step in WATER.waterfall_progresses
    )


def raw_terrain_height(x: float, z: float) -> float:
    height = base_terrain_height(x, z)
    distance, progress, width, source_cut, source = river_network_sample(x, z)
    channel_bank = max(WATER.river_bank_width, width * 2.7)
    river_cut = 1.0 - smoothstep(width * 0.72, channel_bank, distance)
    floodplain = 1.0 - smoothstep(channel_bank, WATER.floodplain_width, distance)
    source_bed = base_terrain_height(*source) - source_cut
    outlet_bed = min(-1.1, WATER.river_outlet_level)
    river_bed = source_bed * (1.0 - progress) + outlet_bed * progress
    river_bed -= waterfall_offset(progress)
    channel_height = river_bed + smoothstep(0.0, channel_bank, distance) * 2.4
    height = height * (1.0 - river_cut) + min(height, channel_height) * river_cut
    height -= floodplain * smoothstep(0.2, 7.0, height) * 0.45
    return height


RIVER_PATH = trace_river()
TRIBUTARY_PATHS = trace_tributaries()


def terraced_height(height: float) -> float:
    if height <= 0.35:
        return height
    level = round(height / TERRAIN.terrace_step) * TERRAIN.terrace_step
    strength = TERRAIN.terrace_strength * smoothstep(0.35, 4.5, height)
    return height * (1.0 - strength) + level * strength


def terrain_height(x: float, z: float) -> float:
    height = raw_terrain_height(x, z)
    height = terraced_height(height)
    flatten, target_height = flattening_profile(x, z)
    flatten *= 0.98
    height = height * (1.0 - flatten) + target_height * flatten
    return max(TERRAIN.floor_height, height)


def biome_metrics(x: float, z: float) -> tuple[float, float, float]:
    height = terrain_height(x, z)
    sample = 2.0
    slope = math.hypot(
        terrain_height(x + sample, z) - terrain_height(x - sample, z),
        terrain_height(x, z + sample) - terrain_height(x, z - sample),
    ) / (sample * 2.0)
    water_distance = min(
        river_distance(x, z),
        math.hypot(x - LAKE_CENTER[0], z - LAKE_CENTER[1]) * 0.7,
        math.hypot(x - HARBOR_CENTER[0], z - HARBOR_CENTER[1]) * 0.8,
    )
    moisture = max(
        0.0,
        min(1.0, 1.0 - water_distance / 52.0 + fractal_noise(x + 71.0, z - 29.0, 3) * 0.22),
    )
    return height, slope, moisture


def water_distance(x: float, z: float) -> float:
    return min(
        river_distance(x, z),
        math.hypot(x - LAKE_CENTER[0], z - LAKE_CENTER[1]) * 0.7,
        math.hypot(x - HARBOR_CENTER[0], z - HARBOR_CENTER[1]) * 0.8,
    )


def approximate_moisture(x: float, z: float) -> float:
    return max(0.0, min(1.0, 1.0 - water_distance(x, z) / 52.0 + fractal_noise(x + 71.0, z - 29.0, 3) * 0.22))


def classify_biome(
    x: float,
    y: float,
    z: float,
    slope: float = 0.0,
    moisture: float | None = None,
) -> BiomeConfig:
    radius = island_radius(x, z)
    moisture = approximate_moisture(x, z) if moisture is None else moisture
    distance_to_water = water_distance(x, z)
    volcano_x, volcano_z = VOLCANO.center
    if math.hypot(x - volcano_x, z - volcano_z) < VOLCANO.cone_radius and y > BIOMES["volcanic"].min_height:
        return BIOMES["volcanic"]
    if radius > 0.82 or y < BIOMES["beach"].max_height:
        return BIOMES["beach"]
    if (x > 45.0 and z > 5.0 and y < 4.8) or (moisture > 0.72 and distance_to_water < WATER.wet_bank_width):
        return BIOMES["wetland"]
    if y > BIOMES["highland"].min_height or slope > 0.72:
        return BIOMES["highland"]
    if z > 30.0 and moisture > BIOMES["coastal_jungle"].min_moisture:
        return BIOMES["coastal_jungle"]
    return BIOMES["temperate_forest"]


def blend_color(
    low: tuple[float, float, float],
    high: tuple[float, float, float],
    amount: float,
) -> tuple[float, float, float]:
    return tuple(low[index] * (1.0 - amount) + high[index] * amount for index in range(3))


def terrain_color(x: float, y: float, z: float) -> tuple[float, float, float]:
    biome = classify_biome(x, y, z)
    variation = max(0.0, min(1.0, 0.5 + fractal_noise(x + 12.0, z - 4.0, 4) * 0.85))
    color = blend_color(biome.color_low, biome.color_high, variation)
    distance_to_water = water_distance(x, z)
    wet_bank = 1.0 - smoothstep(WATER.river_bank_width, WATER.wet_bank_width, distance_to_water)
    if biome.name in {"wetland", "beach"} or wet_bank > 0.0:
        color = tuple(component * (1.0 - wet_bank * 0.28) for component in color)
        color = color[0] * 0.86, color[1] * 0.96, color[2] * 0.92
    return color


def add_box(
    vertices: list[tuple[float, float, float, float, float, float]],
    faces: list[tuple[int, ...]],
    center: tuple[float, float, float],
    size: tuple[float, float, float],
    color: tuple[float, float, float],
) -> None:
    cx, cy, cz = center
    sx, sy, sz = (value * 0.5 for value in size)
    start = len(vertices) + 1
    for dx, dy, dz in (
        (-sx, -sy, -sz), (sx, -sy, -sz), (sx, sy, -sz), (-sx, sy, -sz),
        (-sx, -sy, sz), (sx, -sy, sz), (sx, sy, sz), (-sx, sy, sz),
    ):
        vertices.append((cx + dx, cy + dy, cz + dz, *color))
    faces.extend(
        (
            (start, start + 3, start + 2, start + 1),
            (start + 4, start + 5, start + 6, start + 7),
            (start, start + 4, start + 7, start + 3),
            (start + 1, start + 2, start + 6, start + 5),
            (start + 3, start + 7, start + 6, start + 2),
        )
    )


def add_box_on_ground(
    vertices: list[tuple[float, float, float, float, float, float]],
    faces: list[tuple[int, ...]],
    x: float,
    z: float,
    size: tuple[float, float, float],
    color: tuple[float, float, float],
    vertical_offset: float = 0.0,
) -> None:
    add_box(
        vertices,
        faces,
        (x, terrain_height(x, z) + size[1] * 0.5 + vertical_offset, z),
        size,
        color,
    )


def add_rock_spire(
    vertices: list[tuple[float, float, float, float, float, float]],
    faces: list[tuple[int, ...]],
    x: float,
    z: float,
    radius: float,
    height: float,
    seed: float,
    ground_y: float | None = None,
) -> None:
    sides = 10
    rings: list[list[int]] = []
    ground = terrain_height(x, z) - 0.25 if ground_y is None else ground_y
    for level, (vertical, profile) in enumerate(((0.0, 1.0), (0.38, 0.82), (0.72, 0.55), (1.0, 0.16))):
        ring = []
        for index in range(sides):
            angle = index / sides * math.tau + level * 0.19
            variation = 0.76 + value_noise(index * 0.83 + seed, level * 1.17 - seed) * 0.18
            color = 0.19 + level * 0.025 + variation * 0.035
            ring.append(len(vertices) + 1)
            vertices.append((
                x + math.cos(angle) * radius * profile * variation,
                ground + height * vertical,
                z + math.sin(angle) * radius * profile * variation,
                color,
                color * 1.03,
                color,
            ))
        rings.append(ring)
    for lower, upper in zip(rings, rings[1:]):
        for index in range(sides):
            next_index = (index + 1) % sides
            faces.append((lower[index], lower[next_index], upper[next_index], upper[index]))
    faces.append(tuple(reversed(rings[0])))
    faces.append(tuple(rings[-1]))


def add_islet(
    vertices: list[tuple[float, float, float, float, float, float]],
    faces: list[tuple[int, ...]],
    x: float,
    z: float,
    radius_x: float,
    radius_z: float,
    height: float,
    seed: float,
) -> None:
    sides = 28
    sand = (0.46, 0.38, 0.22)
    grass = (0.18, 0.32, 0.10)
    rings: list[list[int]] = []
    for ring_index, (radius, y, color) in enumerate(((1.0, -0.55, sand), (0.62, height * 0.48, sand), (0.30, height, grass))):
        ring: list[int] = []
        for side in range(sides):
            angle = side / sides * math.tau
            variation = 0.88 + value_noise(side * 0.73 + seed, ring_index * 1.31 - seed) * 0.14
            ring.append(len(vertices) + 1)
            vertices.append((
                x + math.cos(angle) * radius_x * radius * variation,
                y,
                z + math.sin(angle) * radius_z * radius * variation,
                *color,
            ))
        rings.append(ring)
    center = len(vertices) + 1
    vertices.append((x, height + 0.22, z, *grass))
    for lower, upper in zip(rings, rings[1:]):
        for side in range(sides):
            next_side = (side + 1) % sides
            faces.append((lower[side], lower[next_side], upper[next_side], upper[side]))
    for side in range(sides):
        faces.append((rings[-1][side], rings[-1][(side + 1) % sides], center))


def subtract(a: tuple[float, float, float], b: tuple[float, float, float]) -> tuple[float, float, float]:
    return a[0] - b[0], a[1] - b[1], a[2] - b[2]


def cross(a: tuple[float, float, float], b: tuple[float, float, float]) -> tuple[float, float, float]:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def normalize(vector: tuple[float, float, float]) -> tuple[float, float, float]:
    length = math.sqrt(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2])
    if length <= 0.000001:
        return 0.0, 1.0, 0.0
    return vector[0] / length, vector[1] / length, vector[2] / length


def add_vector(a: tuple[float, float, float], b: tuple[float, float, float]) -> tuple[float, float, float]:
    return a[0] + b[0], a[1] + b[1], a[2] + b[2]


def vertex_normals(
    vertices: list[tuple[float, float, float, float, float, float]],
    faces: list[tuple[int, ...]],
) -> list[tuple[float, float, float]]:
    normals = [(0.0, 0.0, 0.0) for _ in vertices]
    for face in faces:
        if len(face) < 3:
            continue
        origin = vertices[face[0] - 1][:3]
        for index in range(1, len(face) - 1):
            a = vertices[face[index] - 1][:3]
            b = vertices[face[index + 1] - 1][:3]
            normal = cross(subtract(a, origin), subtract(b, origin))
            normals[face[0] - 1] = add_vector(normals[face[0] - 1], normal)
            normals[face[index] - 1] = add_vector(normals[face[index] - 1], normal)
            normals[face[index + 1] - 1] = add_vector(normals[face[index + 1] - 1], normal)
    return [normalize(normal) for normal in normals]


def write_obj(
    path: Path,
    name: str,
    vertices: list[tuple[float, float, float, float, float, float]],
    faces: list[tuple[int, ...]],
) -> None:
    normals = vertex_normals(vertices, faces)
    lines = [
        "# Generated by tools/generate_island.py. Do not edit by hand.",
        f"o {name}",
        "",
    ]
    lines.extend("v {:.3f} {:.3f} {:.3f} {:.3f} {:.3f} {:.3f}".format(*vertex) for vertex in vertices)
    lines.append("")
    lines.extend("vn {:.5f} {:.5f} {:.5f}".format(*normal) for normal in normals)
    lines.append("")
    lines.extend("f " + " ".join(f"{index}//{index}" for index in face) for face in faces)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def build_terrain_mesh(
    grid_step: int,
) -> tuple[list[tuple[float, float, float, float, float, float]], list[tuple[int, ...]]]:
    vertices: list[tuple[float, float, float, float, float, float]] = []
    faces: list[tuple[int, ...]] = []
    half = (GRID - 1) * SPACING * 0.5
    rows = list(range(0, GRID, grid_step))
    columns = list(range(0, GRID, grid_step))
    if rows[-1] != GRID - 1:
        rows.append(GRID - 1)
    if columns[-1] != GRID - 1:
        columns.append(GRID - 1)

    for row in rows:
        z = row * SPACING - half
        for column in columns:
            x = column * SPACING - half
            y = terrain_height(x, z)
            vertices.append((x, y, z, *terrain_color(x, y, z)))

    stride = len(columns)
    for row_index in range(len(rows) - 1):
        for column_index in range(len(columns) - 1):
            a = row_index * stride + column_index + 1
            b = a + 1
            c = a + stride
            d = c + 1
            if (row_index + column_index) % 2 == 0:
                faces.extend(((a, c, d), (a, d, b)))
            else:
                faces.extend(((a, c, b), (b, c, d)))
    return vertices, faces


def add_landmark_geometry(
    vertices: list[tuple[float, float, float, float, float, float]],
    faces: list[tuple[int, ...]],
) -> None:
    stone = (0.27, 0.31, 0.3)
    dark_stone = (0.15, 0.2, 0.21)
    glow_stone = (0.18, 0.48, 0.5)

    # Sunken temple in the northern valley.
    add_box(vertices, faces, (0.0, 0.45, -84.0), (18.0, 0.9, 14.0), dark_stone)
    for x in (-8.0, 8.0):
        for z in (-90.0, -78.0):
            add_box(vertices, faces, (x, 3.0, z), (2.0, 6.0, 2.0), stone)
    add_box(vertices, faces, (0.0, 4.0, -90.0), (18.0, 1.2, 2.0), stone)
    add_box(vertices, faces, (0.0, 2.6, -84.0), (1.2, 5.2, 1.2), glow_stone)

    # Broken watchtower overlooking the eastern bay.
    add_box(vertices, faces, (91.0, 2.5, 18.0), (8.0, 5.0, 8.0), dark_stone)
    add_box(vertices, faces, (91.0, 6.2, 18.0), (10.0, 1.0, 10.0), stone)
    add_box(vertices, faces, (88.0, 9.0, 15.0), (1.5, 5.0, 1.5), glow_stone)

    # Western standing-stone circle.
    for index in range(9):
        angle = index / 9.0 * math.tau
        x = -82.0 + math.cos(angle) * 11.0
        z = 38.0 + math.sin(angle) * 11.0
        add_box(vertices, faces, (x, 2.3, z), (1.3, 4.6 + (index % 3) * 0.7, 1.3), stone)
    add_box(vertices, faces, (-82.0, 0.35, 38.0), (8.0, 0.7, 8.0), glow_stone)

    # Granite House facade, recessed entrance and elevated lookout terrace.
    granite_x, granite_z = GRANITE_HOUSE
    granite_y = terrain_height(granite_x, granite_z)
    add_box(vertices, faces, (granite_x, granite_y + 0.35, granite_z), (20.0, 0.7, 14.0), dark_stone)
    add_box(vertices, faces, (granite_x - 8.0, granite_y + 4.0, granite_z - 6.0), (3.0, 8.0, 2.5), stone)
    add_box(vertices, faces, (granite_x + 8.0, granite_y + 4.0, granite_z - 6.0), (3.0, 8.0, 2.5), stone)
    add_box(vertices, faces, (granite_x, granite_y + 8.0, granite_z - 6.0), (19.0, 2.0, 2.5), stone)
    add_box(vertices, faces, (granite_x, granite_y + 2.0, granite_z - 6.4), (7.0, 4.0, 0.8), dark_stone)
    add_box(vertices, faces, (granite_x, granite_y + 9.4, granite_z + 1.0), (18.0, 0.8, 12.0), stone)

    # Walkable bridges cross the lake's outlet and lower river.
    add_box(vertices, faces, (63.0, 0.0, -12.0), (5.0, 0.8, 17.0), dark_stone)
    add_box(vertices, faces, (91.0, 0.0, 8.0), (5.0, 0.8, 16.0), dark_stone)
    for bridge_x, bridge_z in ((63.0, -19.0), (63.0, -5.0), (91.0, 1.5), (91.0, 14.5)):
        add_box(vertices, faces, (bridge_x, 0.9, bridge_z), (1.2, 2.2, 1.2), stone)

    # Summit observatory and a southern landing pier create long-distance goals.
    summit_y = terrain_height(-54.0, -34.0)
    add_box(vertices, faces, (-54.0, summit_y + 0.4, -34.0), (10.0, 0.8, 10.0), dark_stone)
    add_box(vertices, faces, (-54.0, summit_y + 3.5, -34.0), (1.5, 6.2, 1.5), glow_stone)
    add_box(vertices, faces, (-25.0, -0.05, 110.0), (8.0, 0.8, 28.0), dark_stone)
    for z in (99.0, 109.0, 119.0):
        for x in (-28.0, -22.0):
            add_box(vertices, faces, (x, -1.1, z), (1.0, 2.6, 1.0), stone)

    # Irregular granite needles and coastal stacks break up the height-field silhouette.
    for index, spire in enumerate(ROCK_SPIRES):
        x, z = spire.position
        add_rock_spire(vertices, faces, x, z, spire.radius, spire.height, index * 3.7 + 1.0)

    # Shallow reef fields and small offshore islets make the coastline less elliptical.
    for field_index, (center_x, center_z, radius_x, radius_z) in enumerate(CONFIG.reef_fields):
        for rock_index in range(12):
            angle = rock_index / 12.0 * math.tau + field_index * 0.37
            radial = 0.38 + (rock_index % 5) * 0.13
            x = center_x + math.cos(angle) * radius_x * radial
            z = center_z + math.sin(angle) * radius_z * (0.55 + radial * 0.45)
            seed = field_index * 19.0 + rock_index * 2.7
            add_rock_spire(
                vertices,
                faces,
                x,
                z,
                1.2 + value_noise(seed, seed * 0.7) * 0.55,
                1.0 + value_noise(seed + 4.0, seed - 2.0) * 1.3,
                seed,
                -0.7 + value_noise(seed - 1.0, seed + 3.0) * 0.25,
            )
    for index, (x, z, radius_x, radius_z) in enumerate(CONFIG.islets):
        add_islet(vertices, faces, x, z, radius_x, radius_z, 1.6 + index * 0.35, index * 7.1 + 2.0)


def generate_map() -> None:
    vertices, faces = build_terrain_mesh(1)
    add_landmark_geometry(vertices, faces)
    write_obj(MAP_PATH, "mysterious_island", vertices, faces)


def generate_lod_maps() -> None:
    for index, (path, step) in enumerate(zip(MAP_LOD_PATHS, LOD_STEPS), start=1):
        vertices, faces = build_terrain_mesh(step)
        add_landmark_geometry(vertices, faces)
        write_obj(path, f"mysterious_island_lod{index}", vertices, faces)


def generate_internal_water() -> None:
    color = (0.025, 0.22, 0.31)
    vertices: list[tuple[float, float, float, float, float, float]] = []
    faces: list[tuple[int, ...]] = []

    def add_water_ribbon(
        path: tuple[tuple[float, float], ...],
        start_level: float,
        end_level: float,
        start_width: float,
        end_width: float,
    ) -> None:
        left: list[int] = []
        right: list[int] = []
        for index, (x, z) in enumerate(path):
            previous = path[max(index - 1, 0)]
            following = path[min(index + 1, len(path) - 1)]
            dx = following[0] - previous[0]
            dz = following[1] - previous[1]
            length = max(math.hypot(dx, dz), 0.001)
            normal = (-dz / length, dx / length)
            progress = index / (len(path) - 1)
            width = start_width * (1.0 - progress) + end_width * progress
            level = start_level * (1.0 - progress) + end_level * progress - waterfall_offset(progress) * 0.55
            left.append(len(vertices) + 1)
            vertices.append((x + normal[0] * width, level, z + normal[1] * width, *color))
            right.append(len(vertices) + 1)
            vertices.append((x - normal[0] * width, level, z - normal[1] * width, *color))
        for index in range(len(path) - 1):
            faces.append((left[index], left[index + 1], right[index + 1], right[index]))

    # Grant Lake: a low-poly ellipse with a slightly irregular shoreline.
    lake_level = WATER.lake_level
    center = len(vertices) + 1
    vertices.append((LAKE_CENTER[0], lake_level, LAKE_CENTER[1], *color))
    lake_ring = []
    for index in range(64):
        angle = index / 64.0 * math.tau
        variation = 0.9 + value_noise(math.cos(angle) * 2.1 + 8.0, math.sin(angle) * 2.1 - 5.0) * 0.12
        lake_ring.append(len(vertices) + 1)
        vertices.append((
            LAKE_CENTER[0] + math.cos(angle) * WATER.lake_radius_x * variation,
            lake_level,
            LAKE_CENTER[1] + math.sin(angle) * WATER.lake_radius_z * variation,
            *color,
        ))
    for index in range(len(lake_ring)):
        faces.append((center, lake_ring[(index + 1) % len(lake_ring)], lake_ring[index]))

    # Mercy River and tributaries: descending ribbons following traced drainage paths.
    add_water_ribbon(
        RIVER_PATH,
        lake_level,
        WATER.river_outlet_level,
        WATER.river_width_start,
        WATER.river_width_end,
    )
    for tributary, path in zip(WATER.tributaries, TRIBUTARY_PATHS):
        join_level = lake_level * (1.0 - tributary.join_progress) + WATER.river_outlet_level * tributary.join_progress
        source_level = terrain_height(*path[0]) + 0.12
        add_water_ribbon(path, source_level, join_level, tributary.width_start, tributary.width_end)

    write_obj(INTERNAL_WATER_PATH, "grant_lake_and_mercy_river", vertices, faces)


def spawn(
    lines: list[str],
    model: str,
    x: float,
    z: float,
    scale: float,
    half: tuple[float, float, float],
    yaw: float = 0.0,
) -> None:
    lines.append(
        f"spawn_model {model} {x:.1f} {terrain_height(x, z):.2f} {z:.1f} "
        f"{scale:.2f} {scale:.2f} {scale:.2f} "
        f"{half[0]:.2f} {half[1]:.2f} {half[2]:.2f} {yaw:.3f}"
    )


def spawn_decor(lines: list[str], model: str, x: float, z: float, scale: float, yaw: float = 0.0) -> None:
    lines.append(
        f"spawn_decor {model} {x:.1f} {terrain_height(x, z):.2f} {z:.1f} "
        f"{scale:.2f} {scale:.2f} {scale:.2f} {yaw:.3f}"
    )


def spawn_collider(
    lines: list[str],
    position: tuple[float, float, float],
    half: tuple[float, float, float],
) -> None:
    lines.append(
        f"spawn_collider {position[0]:.2f} {position[1]:.2f} {position[2]:.2f} "
        f"{half[0]:.2f} {half[1]:.2f} {half[2]:.2f}"
    )


def generate_scene() -> None:
    random.seed(SEED)
    lines = [
        "# Generated by tools/generate_island.py. Do not edit by hand.",
        "# model <id> <asset-path>",
        "# spawn_model <id> px py pz sx sy sz colliderHalfX colliderHalfY colliderHalfZ [yaw]",
        "# spawn_decor <id> px py pz sx sy sz [yaw]",
        "",
        "model tree models/tree.obj",
        "model rock models/rock.obj",
        "model oak models/kenney/Large_Oak_Green_01.obj",
        "model tent models/kenney/Tent_01.obj",
        "model campfire models/kenney/Campfire_01.obj",
        "model fallen_trunk models/kenney/Fallen_Trunk_01.obj",
        "model fence models/kenney/Wood_Fence_01.obj",
        "model mushroom models/kenney/Mushroom_Red_01.obj",
        "model bush models/bush.obj",
        "model grotto models/grotto.obj",
        "model palm models/palm.obj",
        "",
        "# Arrival camp.",
    ]
    spawn(lines, "tent", -32.0, 77.0, 0.9, (1.3, 1.2, 1.3))
    spawn(lines, "campfire", -27.0, 76.0, 1.5, (0.6, 0.3, 0.6))
    spawn(lines, "fallen_trunk", -19.0, 79.0, 1.2, (1.5, 0.6, 0.8))
    spawn(lines, "fence", -36.0, 80.0, 1.0, (0.3, 0.7, 1.6))
    spawn(lines, "fence", -33.0, 80.0, 1.0, (0.3, 0.7, 1.6))

    lines.extend(("", "# Walkable stone grottos with open entrances and physical walls."))
    for grotto in GROTTOS:
        grotto_x, grotto_z = grotto.position
        ground = terrain_height(grotto_x, grotto_z)
        spawn_decor(lines, "grotto", grotto_x, grotto_z, 1.0)
        spawn_collider(lines, (grotto_x - 3.75, ground + 2.0, grotto_z), (0.65, 2.0, 5.0))
        spawn_collider(lines, (grotto_x + 3.75, ground + 2.0, grotto_z), (0.65, 2.0, 5.0))
        spawn_collider(lines, (grotto_x, ground + 4.0, grotto_z), (3.1, 0.45, 5.0))

    lines.extend(("", "# Dense forest, with clearings around landmarks and the main trail."))
    for _ in range(280):
        for _attempt in range(100):
            x = random.uniform(-116.0, 116.0)
            z = random.uniform(-98.0, 104.0)
            radius = island_radius(x, z)
            near_path = abs(x + 0.15 * z + 14.0) < 7.0
            near_landmark = min(
                math.hypot(x, z),
                math.hypot(x, z + 84.0),
                math.hypot(x + 82.0, z - 38.0),
                math.hypot(x - 91.0, z - 18.0),
                math.hypot(x, z - 10.0),
                *(math.hypot(x - gx, z - gz) for gx, gz in GROTTO_POSITIONS),
            ) < 18.0
            height, slope, moisture = biome_metrics(x, z)
            biome = classify_biome(x, height, z, slope, moisture)
            forest_chance = biome.tree_density * moisture * (1.0 - smoothstep(0.32, 0.82, slope))
            if (
                radius < 0.88
                and not near_path
                and not near_landmark
                and 0.1 < height < 15.0
                and biome.tree_density > 0.0
                and random.random() < forest_chance
            ):
                break
        model = "oak" if random.random() < biome.oak_preference else "tree"
        scale = random.uniform(0.75, 1.35)
        spawn(lines, model, x, z, scale, (0.78, 2.7, 0.78), random.uniform(0.0, math.tau))

    lines.extend(("", "# Low understory creates depth between the larger trees."))
    for _ in range(260):
        for _attempt in range(100):
            x = random.uniform(-116.0, 116.0)
            z = random.uniform(-98.0, 104.0)
            radius = island_radius(x, z)
            near_path = abs(x + 0.15 * z + 14.0) < 5.0
            near_grotto = min(math.hypot(x - gx, z - gz) for gx, gz in GROTTO_POSITIONS) < 14.0
            height, slope, moisture = biome_metrics(x, z)
            biome = classify_biome(x, height, z, slope, moisture)
            if (
                radius < 0.9
                and not near_path
                and not near_grotto
                and -0.2 < height < 12.0
                and slope < 0.72
                and biome.bush_density > 0.0
                and random.random() < biome.bush_density * max(moisture, 0.35)
            ):
                break
        spawn_decor(lines, "bush", x, z, random.uniform(0.55, 1.25), random.uniform(0.0, math.tau))

    lines.extend(("", "# Rocks mark the coast and dangerous approaches."))
    for _ in range(120):
        angle = random.uniform(0.0, math.tau)
        radius = random.uniform(98.0, 126.0)
        x = math.cos(angle) * radius
        z = math.sin(angle) * radius * 0.86
        height, slope, _ = biome_metrics(x, z)
        if height > 3.0 and slope < 0.35:
            continue
        scale = random.uniform(0.7, 1.8)
        spawn(lines, "rock", x, z, scale, (1.3, 0.9, 1.1), random.uniform(0.0, math.tau))

    lines.extend(("", "# Palms frame beaches and the sheltered natural harbor."))
    for _ in range(95):
        for _attempt in range(100):
            angle = random.uniform(0.0, math.tau)
            radius = random.uniform(88.0, 119.0)
            x = math.cos(angle) * radius
            z = math.sin(angle) * radius * 0.86
            height, slope, moisture = biome_metrics(x, z)
            biome = classify_biome(x, height, z, slope, moisture)
            near_harbor = math.hypot(x - HARBOR_CENTER[0], z - HARBOR_CENTER[1]) < 38.0
            if (
                island_radius(x, z) < 0.95
                and -0.45 < height < 2.2
                and slope < 0.45
                and moisture > 0.36
                and biome.name in {"beach", "coastal_jungle", "wetland"}
                and (near_harbor or z > 35.0)
            ):
                break
        scale = random.uniform(0.75, 1.35)
        spawn_decor(lines, "palm", x, z, scale, random.uniform(0.0, math.tau))

    lines.extend(("", "# Mushrooms gather in the eastern marsh."))
    for _ in range(64):
        for _attempt in range(100):
            x = random.uniform(45.0, 108.0)
            z = random.uniform(-5.0, 70.0)
            height, slope, moisture = biome_metrics(x, z)
            biome = classify_biome(x, height, z, slope, moisture)
            if biome.name == "wetland" and -0.3 < height < 4.0 and slope < 0.45 and moisture > 0.7:
                break
        spawn(lines, "mushroom", x, z, random.uniform(0.8, 1.8), (0.35, 0.45, 0.35))

    SCENE_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    generate_map()
    generate_lod_maps()
    generate_internal_water()
    generate_scene()
    lod_outputs = ", ".join(path.relative_to(ROOT).as_posix() for path in MAP_LOD_PATHS)
    print(
        f"Generated {MAP_PATH.relative_to(ROOT)}, "
        f"{lod_outputs}, "
        f"{INTERNAL_WATER_PATH.relative_to(ROOT)} and {SCENE_PATH.relative_to(ROOT)}"
    )


if __name__ == "__main__":
    main()
