#!/usr/bin/env python3
"""Generate the mysterious island map and its scenery placement script."""

from __future__ import annotations

import math
import random
import json
import html
import base64
import struct
import zlib
from dataclasses import dataclass, field, replace
from functools import cache
from pathlib import Path
from typing import Sequence

from island_generator.cli import RuntimeOptions, parse_args
from island_generator.pipeline import run_generation_group
from island_generator.runtime import OutputLayout


DEFAULT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SEED = 1847
WORLD_SCALE = 3.0
HORIZONTAL_SCALE = WORLD_SCALE
VERTICAL_SCALE = 1.35
DEFAULT_GRID = 481
ROOT = DEFAULT_ROOT
MAP_PATH = ROOT / "assets" / "maps" / "demo_map.obj"
MAP_LOD_PATHS = (
    ROOT / "assets" / "maps" / "demo_map_lod1.obj",
    ROOT / "assets" / "maps" / "demo_map_lod2.obj",
)
SPLIT_MAP_PATHS = {
    "terrain": ROOT / "assets" / "maps" / "demo_map_terrain.obj",
    "structures": ROOT / "assets" / "maps" / "demo_map_structures.obj",
    "rocks": ROOT / "assets" / "maps" / "demo_map_rocks.obj",
}
TERRAIN_CHUNK_DIR = ROOT / "assets" / "maps" / "chunks"
INTERNAL_WATER_PATH = ROOT / "assets" / "maps" / "internal_water.obj"
LANDMARKS_PATH = ROOT / "assets" / "maps" / "landmarks.json"
WALKABILITY_PATH = ROOT / "assets" / "maps" / "walkability.pgm"
HEIGHTMAP_PATH = ROOT / "assets" / "maps" / "heightmap.pgm"
BIOME_MAP_PATH = ROOT / "assets" / "maps" / "biomes.ppm"
SLOPE_MAP_PATH = ROOT / "assets" / "maps" / "slope.pgm"
MOISTURE_MAP_PATH = ROOT / "assets" / "maps" / "moisture.pgm"
MATERIAL_MASK_PATHS = {
    "sand": ROOT / "assets" / "maps" / "material_sand.pgm",
    "grass": ROOT / "assets" / "maps" / "material_grass.pgm",
    "rock": ROOT / "assets" / "maps" / "material_rock.pgm",
    "volcanic": ROOT / "assets" / "maps" / "material_volcanic.pgm",
    "wetness": ROOT / "assets" / "maps" / "material_wetness.pgm",
}
TERRAIN_REPORT_PATH = ROOT / "assets" / "maps" / "terrain_report.json"
QUALITY_REPORT_PATH = ROOT / "assets" / "maps" / "generation_quality.json"
DEBUG_PREVIEW_PATH = ROOT / "assets" / "maps" / "debug_preview.html"
SCENE_PATH = ROOT / "assets" / "scripts" / "models.scene"
GAMEPLAY_GOALS_PATH = ROOT / "assets" / "scripts" / "gameplay_goals.json"
SEED = DEFAULT_SEED
GRID = DEFAULT_GRID
SPACING = 4.0
LOD_STEPS = (2, 4)
WALKABILITY_GRID = 161
DEBUG_MAP_GRID = 81


def configure_output_root(output_root: Path) -> None:
    global ROOT
    global MAP_PATH, MAP_LOD_PATHS, SPLIT_MAP_PATHS, TERRAIN_CHUNK_DIR, INTERNAL_WATER_PATH
    global LANDMARKS_PATH, WALKABILITY_PATH, HEIGHTMAP_PATH, BIOME_MAP_PATH
    global SLOPE_MAP_PATH, MOISTURE_MAP_PATH, MATERIAL_MASK_PATHS
    global TERRAIN_REPORT_PATH, QUALITY_REPORT_PATH, DEBUG_PREVIEW_PATH, SCENE_PATH, GAMEPLAY_GOALS_PATH

    layout = OutputLayout.from_root(output_root)
    ROOT = layout.root
    MAP_PATH = layout.map_path
    MAP_LOD_PATHS = layout.map_lod_paths
    SPLIT_MAP_PATHS = layout.split_map_paths
    TERRAIN_CHUNK_DIR = layout.terrain_chunk_dir
    INTERNAL_WATER_PATH = layout.internal_water_path
    LANDMARKS_PATH = layout.landmarks_path
    WALKABILITY_PATH = layout.walkability_path
    HEIGHTMAP_PATH = layout.heightmap_path
    BIOME_MAP_PATH = layout.biome_map_path
    SLOPE_MAP_PATH = layout.slope_map_path
    MOISTURE_MAP_PATH = layout.moisture_map_path
    MATERIAL_MASK_PATHS = layout.material_mask_paths
    TERRAIN_REPORT_PATH = layout.terrain_report_path
    QUALITY_REPORT_PATH = layout.quality_report_path
    DEBUG_PREVIEW_PATH = layout.debug_preview_path
    SCENE_PATH = layout.scene_path
    GAMEPLAY_GOALS_PATH = layout.gameplay_goals_path


def configure_runtime(options: RuntimeOptions) -> None:
    global SEED, GRID, RIVER_PATH, TRIBUTARY_PATHS

    SEED = options.seed
    GRID = options.grid
    clear_generation_caches()
    configure_output_root(options.output_dir)
    RIVER_PATH = trace_river()
    TRIBUTARY_PATHS = trace_tributaries()
    clear_generation_caches()


PLACEMENT_STATS: dict[str, dict[str, int]] = {}


def reset_placement_stats() -> None:
    PLACEMENT_STATS.clear()


def begin_placement_rule(name: str, requested: int) -> None:
    PLACEMENT_STATS[name] = {"requested": requested, "spawned": 0, "skipped": 0}


def record_placement(name: str, spawned: bool) -> None:
    stats = PLACEMENT_STATS.setdefault(name, {"requested": 0, "spawned": 0, "skipped": 0})
    stats["spawned" if spawned else "skipped"] += 1


def relative_output(path: Path) -> str:
    try:
        return path.relative_to(ROOT).as_posix()
    except ValueError:
        return path.as_posix()


@dataclass(frozen=True)
class TerrainConfig:
    island_radius_x: float = 188.0
    island_radius_z: float = 160.0
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
        TributaryConfig((126.0, 92.0), 0.78, 1.0, 2.1, 1.15),
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
class DecorationRuleConfig:
    name: str
    count: int
    allowed_biomes: tuple[str, ...]
    min_height: float
    max_height: float
    max_slope: float
    min_moisture: float = 0.0
    min_radius: float = 0.0
    max_radius: float = 1.0
    avoid_path_width: float = 0.0
    avoid_grotto_radius: float = 0.0


@dataclass(frozen=True)
class IslandModelConfig:
    terrain: TerrainConfig = field(default_factory=TerrainConfig)
    water: WaterConfig = field(default_factory=WaterConfig)
    volcano: VolcanoConfig = field(default_factory=VolcanoConfig)
    grottos: tuple[GrottoConfig, ...] = (
        GrottoConfig((-72.0, -54.0), (0.45, 0.89), 8.5, 7.5),
        GrottoConfig((65.0, 70.0), (-0.65, -0.76), 8.0, 7.0, 0.65),
        GrottoConfig((-91.0, 67.0), (0.95, -0.31), 7.5, 6.8, 0.75),
        GrottoConfig((138.0, -84.0), (-0.80, 0.60), 8.2, 7.2, 0.55),
        GrottoConfig((-130.0, 80.0), (0.78, -0.62), 8.8, 7.6, 0.85),
    )
    granite_house: tuple[float, float] = (-104.0, -10.0)
    shoreline_features: tuple[ShoreFeatureConfig, ...] = (
        ShoreFeatureConfig((111.0, 24.0), 38.0, 34.0, 0.25, "cove"),
        ShoreFeatureConfig((127.0, 24.0), 20.0, 13.0, 0.19, "cove"),
        ShoreFeatureConfig((96.0, -42.0), 28.0, 21.0, 0.11, "cove"),
        ShoreFeatureConfig((-126.0, -64.0), 30.0, 22.0, 0.10, "cove"),
        ShoreFeatureConfig((166.0, 78.0), 42.0, 26.0, 0.14, "cove"),
        ShoreFeatureConfig((-168.0, 88.0), 38.0, 28.0, 0.12, "cove"),
        ShoreFeatureConfig((154.0, -96.0), 36.0, 24.0, 0.10, "cove"),
        ShoreFeatureConfig((108.0, 49.0), 28.0, 18.0, 0.10, "headland"),
        ShoreFeatureConfig((108.0, -2.0), 28.0, 18.0, 0.10, "headland"),
        ShoreFeatureConfig((-25.0, 104.0), 42.0, 34.0, 0.12, "headland"),
        ShoreFeatureConfig((-114.0, 43.0), 24.0, 17.0, 0.08, "headland"),
        ShoreFeatureConfig((24.0, 148.0), 48.0, 30.0, 0.14, "headland"),
        ShoreFeatureConfig((-154.0, -18.0), 36.0, 26.0, 0.11, "headland"),
        ShoreFeatureConfig((172.0, -26.0), 42.0, 25.0, 0.12, "headland"),
    )
    reef_fields: tuple[tuple[float, float, float, float], ...] = (
        (92.0, 77.0, 19.0, 8.0),
        (122.0, -38.0, 17.0, 6.5),
        (-117.0, -82.0, 20.0, 7.5),
        (164.0, 96.0, 27.0, 9.0),
        (-174.0, 72.0, 24.0, 8.0),
        (178.0, -82.0, 22.0, 7.0),
    )
    islets: tuple[tuple[float, float, float, float], ...] = (
        (139.0, 61.0, 9.0, 5.0),
        (-136.0, -78.0, 10.0, 4.5),
        (192.0, 84.0, 15.0, 7.0),
        (-198.0, 54.0, 13.0, 6.5),
        (176.0, -128.0, 12.0, 5.5),
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
        RockSpireConfig((-158.0, 66.0), 5.8, 17.0),
        RockSpireConfig((164.0, -54.0), 5.1, 15.0),
        RockSpireConfig((72.0, 126.0), 4.5, 13.0),
        RockSpireConfig((-146.0, -102.0), 4.9, 14.5),
    )
    decoration_rules: tuple[DecorationRuleConfig, ...] = (
        DecorationRuleConfig("forest", 520, ("coastal_jungle", "temperate_forest", "wetland"), 0.1, 15.0, 0.82, 0.18, 0.0, 0.90, 7.0, 18.0),
        DecorationRuleConfig("understory", 620, ("wetland", "coastal_jungle", "temperate_forest", "highland"), -0.2, 12.0, 0.72, 0.25, 0.0, 0.92, 5.0, 14.0),
        DecorationRuleConfig("coastal_rocks", 260, ("beach", "highland", "volcanic"), -8.0, 3.2, 10.0, 0.0, 0.70, 1.05),
        DecorationRuleConfig("palms", 190, ("beach", "coastal_jungle", "wetland"), -0.45, 2.4, 0.45, 0.34, 0.0, 0.97),
        DecorationRuleConfig("mushrooms", 140, ("wetland",), -0.3, 4.2, 0.45, 0.68, 0.0, 0.92),
    )


def scale_xz(point: tuple[float, float]) -> tuple[float, float]:
    return point[0] * HORIZONTAL_SCALE, point[1] * HORIZONTAL_SCALE


def scale_distance(value: float) -> float:
    return value * HORIZONTAL_SCALE


def scale_height(value: float) -> float:
    return value * VERTICAL_SCALE


def scale_optional_height(value: float | None) -> float | None:
    return None if value is None else scale_height(value)


def scale_tributary(tributary: TributaryConfig) -> TributaryConfig:
    return replace(
        tributary,
        source=scale_xz(tributary.source),
        width_start=scale_distance(tributary.width_start),
        width_end=scale_distance(tributary.width_end),
        source_cut=scale_distance(tributary.source_cut),
    )


def scale_water(config: WaterConfig) -> WaterConfig:
    return replace(
        config,
        lake_center=scale_xz(config.lake_center),
        river_source=scale_xz(config.river_source),
        harbor_center=scale_xz(config.harbor_center),
        lake_level=scale_height(config.lake_level),
        lake_radius_x=scale_distance(config.lake_radius_x),
        lake_radius_z=scale_distance(config.lake_radius_z),
        lake_basin_radius_x=scale_distance(config.lake_basin_radius_x),
        lake_basin_radius_z=scale_distance(config.lake_basin_radius_z),
        lake_bed_center=scale_height(config.lake_bed_center),
        lake_bed_edge=scale_height(config.lake_bed_edge),
        river_width_start=scale_distance(config.river_width_start),
        river_width_end=scale_distance(config.river_width_end),
        river_bank_width=scale_distance(config.river_bank_width),
        river_source_cut=scale_distance(config.river_source_cut),
        river_outlet_level=scale_height(config.river_outlet_level),
        floodplain_width=scale_distance(config.floodplain_width),
        wet_bank_width=scale_distance(config.wet_bank_width),
        waterfall_drop=scale_height(config.waterfall_drop),
        tributaries=tuple(scale_tributary(tributary) for tributary in config.tributaries),
    )


def scale_volcano(config: VolcanoConfig) -> VolcanoConfig:
    return replace(
        config,
        center=scale_xz(config.center),
        cone_radius=scale_distance(config.cone_radius),
        cone_height=scale_height(config.cone_height),
        crater_radius=scale_distance(config.crater_radius),
        crater_depth=scale_height(config.crater_depth),
        rim_radius=scale_distance(config.rim_radius),
        rim_sharpness=scale_distance(config.rim_sharpness),
        rim_height=scale_height(config.rim_height),
        gully_depth=scale_height(config.gully_depth),
    )


def scale_grotto(config: GrottoConfig) -> GrottoConfig:
    return replace(
        config,
        position=scale_xz(config.position),
        chamber_radius=scale_distance(config.chamber_radius),
        mouth_width=scale_distance(config.mouth_width),
        floor_height=scale_optional_height(config.floor_height),
    )


def scale_shore_feature(config: ShoreFeatureConfig) -> ShoreFeatureConfig:
    return replace(
        config,
        center=scale_xz(config.center),
        radius_x=scale_distance(config.radius_x),
        radius_z=scale_distance(config.radius_z),
    )


def scale_rock_spire(config: RockSpireConfig) -> RockSpireConfig:
    return replace(
        config,
        position=scale_xz(config.position),
        radius=scale_distance(config.radius),
        height=scale_height(config.height),
    )


def scale_biome(config: BiomeConfig) -> BiomeConfig:
    return replace(
        config,
        min_height=scale_height(config.min_height),
        max_height=scale_height(config.max_height),
    )


def decoration_count_factor(name: str) -> float:
    return {
        "forest": 3.0,
        "understory": 3.5,
        "coastal_rocks": 3.0,
        "palms": 2.5,
        "mushrooms": 2.0,
    }.get(name, HORIZONTAL_SCALE)


def scale_decoration_rule(config: DecorationRuleConfig) -> DecorationRuleConfig:
    return replace(
        config,
        count=round(config.count * decoration_count_factor(config.name)),
        min_height=scale_height(config.min_height),
        max_height=scale_height(config.max_height),
        avoid_path_width=scale_distance(config.avoid_path_width),
        avoid_grotto_radius=scale_distance(config.avoid_grotto_radius),
    )


def scale_radius_tuple(value: tuple[float, float, float, float]) -> tuple[float, float, float, float]:
    return (
        value[0] * HORIZONTAL_SCALE,
        value[1] * HORIZONTAL_SCALE,
        value[2] * HORIZONTAL_SCALE,
        value[3] * HORIZONTAL_SCALE,
    )


def scale_config(config: IslandModelConfig) -> IslandModelConfig:
    return replace(
        config,
        terrain=replace(
            config.terrain,
            island_radius_x=scale_distance(config.terrain.island_radius_x),
            island_radius_z=scale_distance(config.terrain.island_radius_z),
            floor_height=scale_height(config.terrain.floor_height),
            terrace_step=scale_height(config.terrain.terrace_step),
        ),
        water=scale_water(config.water),
        volcano=scale_volcano(config.volcano),
        grottos=tuple(scale_grotto(grotto) for grotto in config.grottos),
        granite_house=scale_xz(config.granite_house),
        shoreline_features=tuple(scale_shore_feature(feature) for feature in config.shoreline_features),
        reef_fields=tuple(scale_radius_tuple(field) for field in config.reef_fields),
        islets=tuple(scale_radius_tuple(islet) for islet in config.islets),
        biomes=tuple(scale_biome(biome) for biome in config.biomes),
        rock_spires=tuple(scale_rock_spire(spire) for spire in config.rock_spires),
        decoration_rules=tuple(scale_decoration_rule(rule) for rule in config.decoration_rules),
    )


BASE_CONFIG = IslandModelConfig()
CONFIG = scale_config(BASE_CONFIG)
# Tune the island model here first. The generator below reads these grouped
# parameters instead of scattering landmark, water and volcano values inline.
TERRAIN = CONFIG.terrain
WATER = CONFIG.water
VOLCANO = CONFIG.volcano
GROTTOS = CONFIG.grottos
GROTTO_POSITIONS = tuple(grotto.position for grotto in CONFIG.grottos)
BIOMES = {biome.name: biome for biome in CONFIG.biomes}
DECORATION_RULES = {rule.name: rule for rule in CONFIG.decoration_rules}
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


@cache
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


@cache
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


@cache
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


@cache
def river_distance(x: float, z: float) -> float:
    return river_network_sample(x, z)[0]


def waterfall_offset(progress: float) -> float:
    return sum(
        math.exp(-((progress - step) / 0.028) ** 2) * WATER.waterfall_drop
        for step in WATER.waterfall_progresses
    )


@cache
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


@cache
def terrain_height(x: float, z: float) -> float:
    height = raw_terrain_height(x, z)
    height = terraced_height(height)
    flatten, target_height = flattening_profile(x, z)
    flatten *= 0.98
    height = height * (1.0 - flatten) + target_height * flatten
    return max(TERRAIN.floor_height, height)


@cache
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


@cache
def water_distance(x: float, z: float) -> float:
    return min(
        river_distance(x, z),
        math.hypot(x - LAKE_CENTER[0], z - LAKE_CENTER[1]) * 0.7,
        math.hypot(x - HARBOR_CENTER[0], z - HARBOR_CENTER[1]) * 0.8,
    )


@cache
def approximate_moisture(x: float, z: float) -> float:
    return max(0.0, min(1.0, 1.0 - water_distance(x, z) / 52.0 + fractal_noise(x + 71.0, z - 29.0, 3) * 0.22))


CACHED_FUNCTIONS = (
    island_radius,
    base_terrain_height,
    river_network_sample,
    river_distance,
    raw_terrain_height,
    terrain_height,
    biome_metrics,
    water_distance,
    approximate_moisture,
)


def clear_generation_caches() -> None:
    for function in CACHED_FUNCTIONS:
        function.cache_clear()


def generation_cache_stats() -> dict[str, dict[str, int]]:
    return {
        function.__name__: {
            "hits": function.cache_info().hits,
            "misses": function.cache_info().misses,
            "size": function.cache_info().currsize,
        }
        for function in CACHED_FUNCTIONS
    }


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
    if z > 18.0 and y < 13.0 and slope < 0.78 and moisture > BIOMES["coastal_jungle"].min_moisture * 0.65:
        return BIOMES["coastal_jungle"]
    if y > BIOMES["highland"].min_height or slope > 0.72:
        return BIOMES["highland"]
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


def walkability_score(x: float, z: float) -> float:
    height, slope, moisture = biome_metrics(x, z)
    return walkability_score_from_metrics(x, z, height, slope, moisture)


def walkability_score_from_metrics(x: float, z: float, height: float, slope: float, moisture: float) -> float:
    radius = island_radius(x, z)
    distance_to_water = water_distance(x, z)
    volcano_x, volcano_z = VOLCANO.center
    crater_distance = math.hypot(x - volcano_x, z - volcano_z)
    if radius > 1.0 or height < -0.25:
        return 0.0
    if crater_distance < VOLCANO.rim_radius + 1.0 and height > 8.0:
        return 0.08

    slope_score = 1.0 - smoothstep(0.28, 0.95, slope)
    beach_score = 0.25 * smoothstep(0.78, 0.93, radius) * (1.0 - smoothstep(0.45, 0.9, slope))
    river_bank_score = 0.18 * (1.0 - smoothstep(8.0, WATER.wet_bank_width, distance_to_water))
    plateau_score = 0.14 * smoothstep(1.5, 4.0, height) * (1.0 - smoothstep(0.16, 0.42, slope))
    path_score = 0.18 * (1.0 - smoothstep(3.5, 8.0, abs(x + 0.15 * z + 14.0)))
    wet_penalty = 0.35 * moisture * (1.0 - smoothstep(0.0, 7.0, distance_to_water))
    score = slope_score + beach_score + river_bank_score + plateau_score + path_score - wet_penalty
    return max(0.0, min(1.0, score))


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


def add_slab_on_ground(
    vertices: list[tuple[float, float, float, float, float, float]],
    faces: list[tuple[int, ...]],
    x: float,
    z: float,
    size: tuple[float, float, float],
    color: tuple[float, float, float],
    clearance: float = 0.08,
) -> None:
    """Place a walkable rectangular slab just above the sampled terrain."""
    add_box_on_ground(vertices, faces, x, z, size, color, clearance)


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
    half = (GRID - 1) * SPACING * 0.5
    span = half * 2.0
    lines = [
        "# Generated by tools/generate_island.py. Do not edit by hand.",
        f"o {name}",
        "",
    ]
    lines.extend("v {:.3f} {:.3f} {:.3f} {:.3f} {:.3f} {:.3f}".format(*vertex) for vertex in vertices)
    lines.append("")
    lines.extend(
        "vt {:.6f} {:.6f}".format(
            (vertex[0] + half) / span,
            1.0 - ((vertex[2] + half) / span),
        )
        for vertex in vertices
    )
    lines.append("")
    lines.extend("vn {:.5f} {:.5f} {:.5f}".format(*normal) for normal in normals)
    lines.append("")
    lines.extend("f " + " ".join(f"{index}/{index}/{index}" for index in face) for face in faces)
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


def write_chunked_terrain_meshes(
    lod_index: int,
    vertices: list[tuple[float, float, float, float, float, float]],
    faces: list[tuple[int, ...]],
    chunk_size: float = 128.0,
) -> list[Path]:
    TERRAIN_CHUNK_DIR.mkdir(parents=True, exist_ok=True)
    if lod_index == 0:
        for stale in TERRAIN_CHUNK_DIR.glob("terrain_lod*.obj"):
            stale.unlink()

    chunk_faces: dict[tuple[int, int], list[tuple[int, ...]]] = {}
    for face in faces:
        center_x = sum(vertices[index - 1][0] for index in face) / len(face)
        center_z = sum(vertices[index - 1][2] for index in face) / len(face)
        key = math.floor(center_x / chunk_size), math.floor(center_z / chunk_size)
        chunk_faces.setdefault(key, []).append(face)

    outputs: list[Path] = []
    for (chunk_x, chunk_z), faces_for_chunk in sorted(chunk_faces.items()):
        remap: dict[int, int] = {}
        chunk_vertices: list[tuple[float, float, float, float, float, float]] = []
        chunk_indices: list[tuple[int, ...]] = []
        for face in faces_for_chunk:
            remapped_face: list[int] = []
            for source_index in face:
                if source_index not in remap:
                    remap[source_index] = len(chunk_vertices) + 1
                    chunk_vertices.append(vertices[source_index - 1])
                remapped_face.append(remap[source_index])
            chunk_indices.append(tuple(remapped_face))
        path = TERRAIN_CHUNK_DIR / f"terrain_lod{lod_index}_{chunk_x}_{chunk_z}.obj"
        write_obj(path, f"terrain_lod{lod_index}_{chunk_x}_{chunk_z}", chunk_vertices, chunk_indices)
        outputs.append(path)
    return outputs


def add_structure_geometry(
    vertices: list[tuple[float, float, float, float, float, float]],
    faces: list[tuple[int, ...]],
) -> None:
    stone = (0.27, 0.31, 0.3)
    dark_stone = (0.15, 0.2, 0.21)
    glow_stone = (0.18, 0.48, 0.5)

    # Sunken temple in the northern valley.
    add_slab_on_ground(vertices, faces, 0.0, -84.0, (18.0, 0.9, 14.0), dark_stone)
    for x in (-8.0, 8.0):
        for z in (-90.0, -78.0):
            add_box_on_ground(vertices, faces, x, z, (2.0, 6.0, 2.0), stone, 0.04)
    add_box_on_ground(vertices, faces, 0.0, -90.0, (18.0, 1.2, 2.0), stone, 3.1)
    add_box_on_ground(vertices, faces, 0.0, -84.0, (1.2, 5.2, 1.2), glow_stone, 0.04)

    # Broken watchtower overlooking the eastern bay.
    add_box_on_ground(vertices, faces, 91.0, 18.0, (8.0, 5.0, 8.0), dark_stone, 0.04)
    add_box_on_ground(vertices, faces, 91.0, 18.0, (10.0, 1.0, 10.0), stone, 5.05)
    add_box_on_ground(vertices, faces, 88.0, 15.0, (1.5, 5.0, 1.5), glow_stone, 5.65)

    # Western standing-stone circle.
    for index in range(9):
        angle = index / 9.0 * math.tau
        x = -82.0 + math.cos(angle) * 11.0
        z = 38.0 + math.sin(angle) * 11.0
        add_box_on_ground(vertices, faces, x, z, (1.3, 4.6 + (index % 3) * 0.7, 1.3), stone, 0.04)
    add_slab_on_ground(vertices, faces, -82.0, 38.0, (8.0, 0.7, 8.0), glow_stone)

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
    add_slab_on_ground(vertices, faces, 63.0, -12.0, (5.0, 0.8, 17.0), dark_stone, 0.18)
    add_slab_on_ground(vertices, faces, 91.0, 8.0, (5.0, 0.8, 16.0), dark_stone, 0.18)
    for bridge_x, bridge_z in ((63.0, -19.0), (63.0, -5.0), (91.0, 1.5), (91.0, 14.5)):
        add_box_on_ground(vertices, faces, bridge_x, bridge_z, (1.2, 2.2, 1.2), stone, 0.04)

    # Summit observatory and a southern landing pier create long-distance goals.
    summit_y = terrain_height(-54.0, -34.0)
    add_box(vertices, faces, (-54.0, summit_y + 0.4, -34.0), (10.0, 0.8, 10.0), dark_stone)
    add_box(vertices, faces, (-54.0, summit_y + 3.5, -34.0), (1.5, 6.2, 1.5), glow_stone)
    add_slab_on_ground(vertices, faces, -25.0, 110.0, (8.0, 0.8, 28.0), dark_stone, 0.12)
    for z in (99.0, 109.0, 119.0):
        for x in (-28.0, -22.0):
            add_box_on_ground(vertices, faces, x, z, (1.0, 2.6, 1.0), stone, -1.55)

    # New outer island landmarks make the expanded coastline readable from afar.
    add_slab_on_ground(vertices, faces, 148.0, -92.0, (13.0, 0.9, 10.0), dark_stone, 0.10)
    for offset_x, offset_z in ((-5.0, -4.0), (5.0, -4.0), (-5.0, 4.0), (5.0, 4.0)):
        add_box_on_ground(vertices, faces, 148.0 + offset_x, -92.0 + offset_z, (1.6, 5.8, 1.6), stone, 0.08)
    add_box_on_ground(vertices, faces, 148.0, -92.0, (2.0, 8.8, 2.0), glow_stone, 0.15)

    add_slab_on_ground(vertices, faces, -130.0, 88.0, (16.0, 0.8, 13.0), dark_stone, 0.12)
    for index in range(7):
        angle = index / 7.0 * math.tau
        add_box_on_ground(
            vertices,
            faces,
            -130.0 + math.cos(angle) * 9.0,
            88.0 + math.sin(angle) * 7.0,
            (1.1, 3.6 + (index % 2) * 1.4, 1.1),
            stone,
            0.06,
        )

    add_slab_on_ground(vertices, faces, 174.0, 75.0, (7.0, 0.8, 34.0), dark_stone, 0.10)
    for z in (62.0, 73.0, 84.0):
        for x in (171.0, 177.0):
            add_box_on_ground(vertices, faces, x, z, (0.9, 2.4, 0.9), stone, -1.50)


def add_rock_geometry(
    vertices: list[tuple[float, float, float, float, float, float]],
    faces: list[tuple[int, ...]],
) -> None:
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


def add_landmark_geometry(
    vertices: list[tuple[float, float, float, float, float, float]],
    faces: list[tuple[int, ...]],
) -> None:
    add_structure_geometry(vertices, faces)
    add_rock_geometry(vertices, faces)


def generate_map() -> None:
    vertices, faces = build_terrain_mesh(1)
    write_obj(SPLIT_MAP_PATHS["terrain"], "mysterious_island_terrain", vertices, faces)
    write_chunked_terrain_meshes(0, vertices, faces)
    add_landmark_geometry(vertices, faces)
    write_obj(MAP_PATH, "mysterious_island", vertices, faces)


def generate_split_maps() -> None:
    structure_vertices: list[tuple[float, float, float, float, float, float]] = []
    structure_faces: list[tuple[int, ...]] = []
    add_structure_geometry(structure_vertices, structure_faces)
    write_obj(SPLIT_MAP_PATHS["structures"], "mysterious_island_structures", structure_vertices, structure_faces)

    rock_vertices: list[tuple[float, float, float, float, float, float]] = []
    rock_faces: list[tuple[int, ...]] = []
    add_rock_geometry(rock_vertices, rock_faces)
    write_obj(SPLIT_MAP_PATHS["rocks"], "mysterious_island_rocks", rock_vertices, rock_faces)


def generate_lod_maps() -> None:
    for index, (path, step) in enumerate(zip(MAP_LOD_PATHS, LOD_STEPS), start=1):
        vertices, faces = build_terrain_mesh(step)
        write_chunked_terrain_meshes(index, vertices, faces)
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


def generate_walkability_map() -> None:
    half = (GRID - 1) * SPACING * 0.5
    step = (half * 2.0) / (WALKABILITY_GRID - 1)
    rows: list[list[int]] = []
    for row in range(WALKABILITY_GRID):
        z = half - row * step
        values: list[int] = []
        for column in range(WALKABILITY_GRID):
            x = column * step - half
            values.append(round(walkability_score(x, z) * 255.0))
        rows.append(values)

    lines = [
        "P2",
        "# Generated by tools/generate_island.py. 0=blocked, 255=easy traversal.",
        f"{WALKABILITY_GRID} {WALKABILITY_GRID}",
        "255",
    ]
    lines.extend(" ".join(str(value) for value in row) for row in rows)
    WALKABILITY_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


def sample_debug_grid() -> list[list[dict[str, object]]]:
    half = (GRID - 1) * SPACING * 0.5
    step = (half * 2.0) / (DEBUG_MAP_GRID - 1)
    rows: list[list[dict[str, object]]] = []
    for row in range(DEBUG_MAP_GRID):
        z = half - row * step
        values: list[dict[str, object]] = []
        for column in range(DEBUG_MAP_GRID):
            x = column * step - half
            height, slope, moisture = biome_metrics(x, z)
            biome = classify_biome(x, height, z, slope, moisture)
            values.append(
                {
                    "x": x,
                    "z": z,
                    "height": height,
                    "slope": slope,
                    "moisture": moisture,
                    "biome": biome.name,
                    "walkability": walkability_score_from_metrics(x, z, height, slope, moisture),
                    "land": island_radius(x, z) <= 1.0 and height >= -0.25,
                }
            )
        rows.append(values)
    return rows


def write_pgm(path: Path, comment: str, values: list[list[int]], maximum: int = 255) -> None:
    lines = [
        "P2",
        f"# Generated by tools/generate_island.py. {comment}",
        f"{len(values[0])} {len(values)}",
        str(maximum),
    ]
    lines.extend(" ".join(str(value) for value in row) for row in values)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def generate_heightmap(rows: list[list[dict[str, object]]]) -> None:
    heights = [float(sample["height"]) for row in rows for sample in row]
    low = min(heights)
    high = max(heights)
    scale = high - low if high > low else 1.0
    values = [
        [round((float(sample["height"]) - low) / scale * 255.0) for sample in row]
        for row in rows
    ]
    write_pgm(HEIGHTMAP_PATH, f"height remap: 0={low:.3f}, 255={high:.3f}.", values)


def generate_slope_map(rows: list[list[dict[str, object]]]) -> None:
    slopes = [float(sample["slope"]) for row in rows for sample in row]
    high = max(slopes)
    scale = high if high > 0.0 else 1.0
    values = [
        [round(min(1.0, float(sample["slope"]) / scale) * 255.0) for sample in row]
        for row in rows
    ]
    write_pgm(SLOPE_MAP_PATH, f"slope remap: 0=flat, 255={high:.3f}.", values)


def generate_moisture_map(rows: list[list[dict[str, object]]]) -> None:
    values = [
        [round(max(0.0, min(1.0, float(sample["moisture"]))) * 255.0) for sample in row]
        for row in rows
    ]
    write_pgm(MOISTURE_MAP_PATH, "moisture remap: 0=dry, 255=wet.", values)


def material_weights(sample: dict[str, object]) -> dict[str, float]:
    height = float(sample["height"])
    slope = float(sample["slope"])
    moisture = float(sample["moisture"])
    biome_name = str(sample["biome"])
    land = bool(sample["land"])
    if not land:
        return {"sand": 0.0, "grass": 0.0, "rock": 0.0, "volcanic": 0.0, "wetness": 0.0}

    radius = island_radius(float(sample["x"]), float(sample["z"]))
    sand = max(
        smoothstep(1.7, 0.0, height),
        smoothstep(0.74, 0.96, radius) * (1.0 - smoothstep(0.34, 0.82, slope)),
    )
    rock = max(
        smoothstep(0.42, 1.08, slope),
        smoothstep(10.0, 23.0, height) * 0.72,
    )
    volcanic = 1.0 if biome_name == "volcanic" else 0.0
    volcanic = max(volcanic, smoothstep(14.0, 29.0, height) * smoothstep(0.52, 1.2, slope))
    wetness = max(
        moisture * smoothstep(13.0, 0.0, water_distance(float(sample["x"]), float(sample["z"]))),
        0.7 if biome_name == "wetland" else 0.0,
    )
    grass = 1.0 - max(sand * 0.86, rock * 0.82, volcanic * 0.92)
    if biome_name in {"temperate_forest", "coastal_jungle", "wetland"}:
        grass = max(grass, 0.62 + moisture * 0.24)
    return {
        "sand": max(0.0, min(1.0, sand)),
        "grass": max(0.0, min(1.0, grass)),
        "rock": max(0.0, min(1.0, rock)),
        "volcanic": max(0.0, min(1.0, volcanic)),
        "wetness": max(0.0, min(1.0, wetness)),
    }


def generate_material_masks(rows: list[list[dict[str, object]]]) -> None:
    masks = {name: [] for name in MATERIAL_MASK_PATHS}
    for row in rows:
        row_values = {name: [] for name in MATERIAL_MASK_PATHS}
        for sample in row:
            weights = material_weights(sample)
            for name, value in weights.items():
                row_values[name].append(round(value * 255.0))
        for name, values in row_values.items():
            masks[name].append(values)

    for name, values in masks.items():
        write_pgm(MATERIAL_MASK_PATHS[name], f"{name} material mask: 0=none, 255=full.", values)


def generate_biome_map(rows: list[list[dict[str, object]]]) -> None:
    lines = [
        "P3",
        "# Generated by tools/generate_island.py. Biome debug map using configured biome colors.",
        f"{len(rows[0])} {len(rows)}",
        "255",
    ]
    for row in rows:
        pixels: list[str] = []
        for sample in row:
            biome = BIOMES[str(sample["biome"])]
            color = blend_color(biome.color_low, biome.color_high, 0.62)
            if not bool(sample["land"]):
                color = (0.02, 0.10, 0.15)
            pixels.extend(str(max(0, min(255, round(component * 255.0)))) for component in color)
        lines.append(" ".join(pixels))
    BIOME_MAP_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


def generate_terrain_report(rows: list[list[dict[str, object]]]) -> None:
    samples = [sample for row in rows for sample in row]
    land_samples = [sample for sample in samples if bool(sample["land"])]
    heights = [float(sample["height"]) for sample in land_samples]
    slopes = [float(sample["slope"]) for sample in land_samples]
    walkability_values = [float(sample["walkability"]) for sample in land_samples]
    biome_counts = {name: 0 for name in BIOMES}
    for sample in land_samples:
        biome_counts[str(sample["biome"])] += 1

    def average(values: list[float]) -> float:
        return sum(values) / len(values) if values else 0.0

    def rounded_pair(position: tuple[float, float]) -> dict[str, float]:
        return {"x": round(position[0], 2), "z": round(position[1], 2)}

    payload = {
        "generated_by": "tools/generate_island.py",
        "seed": SEED,
        "grid": {
            "terrain_vertices_per_side": GRID,
            "debug_samples_per_side": DEBUG_MAP_GRID,
            "spacing": SPACING,
        },
        "height": {
            "min": round(min(heights), 3),
            "max": round(max(heights), 3),
            "average": round(average(heights), 3),
        },
        "slope": {
            "max": round(max(slopes), 3),
            "average": round(average(slopes), 3),
            "steep_sample_ratio": round(
                sum(1 for slope in slopes if slope > 0.72) / max(1, len(slopes)),
                4,
            ),
        },
        "walkability": {
            "average": round(average(walkability_values), 3),
            "good_sample_ratio": round(
                sum(1 for value in walkability_values if value >= 0.55) / max(1, len(walkability_values)),
                4,
            ),
        },
        "biomes": {
            name: {
                "samples": count,
                "ratio": round(count / max(1, len(land_samples)), 4),
            }
            for name, count in sorted(biome_counts.items())
        },
        "water": {
            "lake_center": rounded_pair(LAKE_CENTER),
            "river_source": rounded_pair(RIVER_SOURCE),
            "harbor_center": rounded_pair(HARBOR_CENTER),
            "river_points": len(RIVER_PATH),
            "tributary_points": [len(path) for path in TRIBUTARY_PATHS],
        },
        "decoration_rules": {
            name: {
                "count": rule.count,
                "allowed_biomes": rule.allowed_biomes,
                "height": [rule.min_height, rule.max_height],
                "max_slope": rule.max_slope,
                "min_moisture": rule.min_moisture,
                "radius": [rule.min_radius, rule.max_radius],
            }
            for name, rule in sorted(DECORATION_RULES.items())
        },
        "outputs": {
            "heightmap": HEIGHTMAP_PATH.relative_to(ROOT).as_posix(),
            "biome_map": BIOME_MAP_PATH.relative_to(ROOT).as_posix(),
            "slope_map": SLOPE_MAP_PATH.relative_to(ROOT).as_posix(),
            "moisture_map": MOISTURE_MAP_PATH.relative_to(ROOT).as_posix(),
            "material_masks": {
                name: path.relative_to(ROOT).as_posix()
                for name, path in sorted(MATERIAL_MASK_PATHS.items())
            },
            "walkability": WALKABILITY_PATH.relative_to(ROOT).as_posix(),
            "landmarks": LANDMARKS_PATH.relative_to(ROOT).as_posix(),
        },
    }
    TERRAIN_REPORT_PATH.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def generate_debug_maps_and_report() -> None:
    rows = sample_debug_grid()
    generate_heightmap(rows)
    generate_slope_map(rows)
    generate_moisture_map(rows)
    generate_material_masks(rows)
    generate_biome_map(rows)
    generate_terrain_report(rows)


def read_ascii_pnm(path: Path) -> tuple[str, int, int, int, list[int]]:
    tokens: list[str] = []
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            content = line.split("#", 1)[0].strip()
            if content:
                tokens.extend(content.split())
    if len(tokens) < 4:
        raise ValueError(f"{relative_output(path)} is not a valid ASCII PNM file")
    magic = tokens[0]
    width = int(tokens[1])
    height = int(tokens[2])
    maximum = int(tokens[3])
    values = [int(token) for token in tokens[4:]]
    expected = width * height * (3 if magic == "P3" else 1)
    if magic not in {"P2", "P3"} or len(values) != expected or maximum <= 0:
        raise ValueError(f"{relative_output(path)} has invalid PNM dimensions or pixel data")
    return magic, width, height, maximum, values


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    checksum = zlib.crc32(kind)
    checksum = zlib.crc32(payload, checksum)
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", checksum & 0xFFFFFFFF)


def png_data_uri_from_pnm(path: Path) -> str:
    magic, width, height, maximum, values = read_ascii_pnm(path)
    color_type = 2 if magic == "P3" else 0
    channels = 3 if magic == "P3" else 1
    rows: list[bytes] = []
    scale = 255.0 / maximum
    stride = width * channels
    for row in range(height):
        start = row * stride
        pixels = bytes(max(0, min(255, round(value * scale))) for value in values[start:start + stride])
        rows.append(b"\x00" + pixels)
    png = b"".join(
        (
            b"\x89PNG\r\n\x1a\n",
            png_chunk("IHDR".encode("ascii"), struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0)),
            png_chunk("IDAT".encode("ascii"), zlib.compress(b"".join(rows), 9)),
            png_chunk("IEND".encode("ascii"), b""),
        )
    )
    return "data:image/png;base64," + base64.b64encode(png).decode("ascii")


def load_json_if_available(path: Path) -> dict[str, object]:
    if not path.exists():
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


def render_preview_card(title: str, path: Path) -> str:
    if not path.exists():
        return (
            '<section class="card missing">'
            f"<h2>{html.escape(title)}</h2>"
            f"<p>{html.escape(relative_output(path))} not generated in this run.</p>"
            "</section>"
        )
    data_uri = png_data_uri_from_pnm(path)
    return (
        '<section class="card">'
        f"<h2>{html.escape(title)}</h2>"
        f'<img src="{data_uri}" alt="{html.escape(title)} preview">'
        f"<p>{html.escape(relative_output(path))}</p>"
        "</section>"
    )


def preview_image_uri(path: Path) -> str:
    if not path.exists():
        return ""
    return png_data_uri_from_pnm(path)


def generate_debug_preview() -> Path:
    terrain_report = load_json_if_available(TERRAIN_REPORT_PATH)
    quality_report = load_json_if_available(QUALITY_REPORT_PATH)
    gameplay_goals = load_json_if_available(GAMEPLAY_GOALS_PATH)
    issues = quality_report.get("issues", []) if isinstance(quality_report.get("issues", []), list) else []
    goals = gameplay_goals.get("goals", []) if isinstance(gameplay_goals.get("goals", []), list) else []
    height = terrain_report.get("height", {}) if isinstance(terrain_report.get("height", {}), dict) else {}
    slope = terrain_report.get("slope", {}) if isinstance(terrain_report.get("slope", {}), dict) else {}
    walkability = terrain_report.get("walkability", {}) if isinstance(terrain_report.get("walkability", {}), dict) else {}
    placement = (
        quality_report.get("scene", {}).get("placement", {})
        if isinstance(quality_report.get("scene", {}), dict)
        else {}
    )
    cards = [
        ("Biome Map", BIOME_MAP_PATH),
        ("Heightmap", HEIGHTMAP_PATH),
        ("Walkability", WALKABILITY_PATH),
        ("Slope", SLOPE_MAP_PATH),
        ("Moisture", MOISTURE_MAP_PATH),
        *[(f"{name.title()} Mask", path) for name, path in sorted(MATERIAL_MASK_PATHS.items())],
    ]
    issue_items = "\n".join(
        f"<li><strong>{html.escape(str(issue.get('severity', 'info')))}</strong> "
        f"{html.escape(str(issue.get('code', 'issue')))}: {html.escape(str(issue.get('message', '')))}</li>"
        for issue in issues
    ) or "<li>No quality issues reported.</li>"
    placement_rows = "\n".join(
        "<tr>"
        f"<td>{html.escape(name)}</td>"
        f"<td>{stats.get('requested', 0)}</td>"
        f"<td>{stats.get('spawned', 0)}</td>"
        f"<td>{stats.get('skipped', 0)}</td>"
        "</tr>"
        for name, stats in sorted(placement.items())
        if isinstance(stats, dict)
    ) or '<tr><td colspan="4">Scene placement was not generated in this run.</td></tr>'
    goal_items = "\n".join(
        '<li class="goal-item">'
        f"<strong>{html.escape(str(goal.get('title', 'Без названия')))}</strong>"
        f"<span>{html.escape(str(goal.get('description', '')))}</span>"
        "</li>"
        for goal in goals
        if isinstance(goal, dict)
    ) or '<li class="goal-item"><strong>Цели не сгенерированы</strong><span>Запустите генератор с --only scene или --only all.</span></li>'
    card_markup = "\n".join(render_preview_card(title, path) for title, path in cards)
    minimap_uri = preview_image_uri(BIOME_MAP_PATH)
    minimap_markup = (
        f'<img src="{minimap_uri}" alt="Island minimap">'
        if minimap_uri
        else "<div class=\"minimap-empty\">Нет карты</div>"
    )
    locations_path = ROOT / "assets" / "scripts" / "island_locations_100.json"
    location_payload = load_json_if_available(locations_path)
    locations = location_payload if isinstance(location_payload, list) else []
    location_styles = ""
    location_markup = ""
    location_script = ""
    if locations:
        location_data = json.dumps(locations, ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")
        location_styles = """
    .location-panel { margin-top: 14px; }
    .location-controls { display: grid; gap: 10px; margin: 10px 0 12px; }
    .location-search { width: 100%; box-sizing: border-box; border: 1px solid #46524c; background: #101412; color: #f0f3ef; border-radius: 6px; padding: 8px 10px; }
    .location-toolbar { display: flex; flex-wrap: wrap; gap: 8px; }
    .location-toolbar button { border: 1px solid #46524c; background: #151917; color: #d7dddf; border-radius: 6px; padding: 6px 9px; cursor: pointer; }
    .location-toolbar button.active { background: #2b6f68; border-color: #47a79c; color: #f4fffb; }
    .location-map { position: relative; aspect-ratio: 7 / 6; overflow: hidden; border: 1px solid #303633; background: #0f1211; border-radius: 6px; }
    .location-map img { position: absolute; inset: 0; width: 100%; height: 100%; object-fit: cover; border: 0; opacity: .78; }
    .location-marker { position: absolute; width: 10px; height: 10px; border-radius: 50%; border: 2px solid #101412; box-shadow: 0 0 0 1px rgba(255,255,255,.45), 0 4px 10px rgba(0,0,0,.45); transform: translate(-50%, -50%); cursor: pointer; padding: 0; }
    .location-marker.hidden { display: none; }
    .location-marker:hover, .location-marker.selected { width: 14px; height: 14px; z-index: 3; }
    .location-details { margin-top: 12px; color: #c9d1cd; min-height: 118px; display: grid; gap: 8px; }
    .location-details h3 { margin: 0; color: #f0f3ef; font-size: 18px; }
    .location-details p { color: #c9d1cd; }
    .location-meta { color: #8fb9b0; font-size: 12px; }
    .location-facts { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 8px; }
    .location-fact { border: 1px solid #303633; border-radius: 6px; padding: 8px; background: #151917; }
    .location-fact span { display: block; color: #8fb9b0; font-size: 12px; margin-bottom: 3px; }
    .location-list { max-height: 280px; overflow: auto; margin-top: 12px; border-top: 1px solid #303633; }
    .location-list button { width: 100%; border: 0; border-bottom: 1px solid #303633; background: transparent; color: #d7dddf; text-align: left; padding: 8px 4px; cursor: pointer; }
    .location-list button:hover, .location-list button.selected { background: #242b28; }
"""
        location_markup = f"""
        <section class="panel location-panel">
          <h2>{len(locations)} объектов острова</h2>
          <p>Интерактивный слой поверх карты биомов. Координаты: x/z из world space, y из дизайн-данных.</p>
          <div class="location-controls">
            <input class="location-search" id="location-search" type="search" placeholder="Поиск по названию, ресурсу, луту или инструменту">
            <div class="location-toolbar" id="location-filters"></div>
          </div>
          <div class="location-map" id="location-map">
            <img src="{minimap_uri}" alt="Location overlay map">
          </div>
          <div class="location-details" id="location-details">Выберите маркер, чтобы увидеть ресурсы, угрозы, лут и сюжетные следы.</div>
          <div class="location-list" id="location-list"></div>
        </section>"""
        location_script = """
  <script id="location-data" type="application/json">{data}</script>
  <script>
    (() => {{
      const locations = JSON.parse(document.getElementById('location-data').textContent);
      const map = document.getElementById('location-map');
      const filters = document.getElementById('location-filters');
      const search = document.getElementById('location-search');
      const details = document.getElementById('location-details');
      const list = document.getElementById('location-list');
      const colors = {{
        'Побережье': '#e7c46a',
        'Тропический лес': '#45b26b',
        'Болота': '#6aa68a',
        'Горы': '#b5b8b1',
        'Пещеры': '#9b7bd8',
        'Вулкан': '#df604c',
        'Тайные зоны': '#55c7d8'
      }};
      const bounds = {{ minX: -145, maxX: 145, minZ: -125, maxZ: 125 }};
      let activeBiome = 'Все';
      let selected = null;
      let selectedRow = null;
      let searchText = '';
      const biomes = ['Все', ...Array.from(new Set(locations.map(location => location.biome)))];
      const biomeCounts = locations.reduce((counts, location) => {{
        counts[location.biome] = (counts[location.biome] || 0) + 1;
        return counts;
      }}, {{ Все: locations.length }});
      const markers = [];
      const appendText = (parent, tag, text, className = '') => {{
        const node = document.createElement(tag);
        node.textContent = text;
        if (className) {{
          node.className = className;
        }}
        parent.appendChild(node);
        return node;
      }};
      const listValue = values => values?.length ? values.join(', ') : 'нет';
      const matchesSearch = location => {{
        if (!searchText) {{
          return true;
        }}
        const haystack = [
          location.name,
          location.biome,
          location.description,
          ...(location.resources || []),
          ...(location.enemies || []),
          ...(location.loot || []),
          ...(location.story_clues || []),
          ...(location.required_tools || [])
        ].join(' ').toLocaleLowerCase('ru-RU');
        return haystack.includes(searchText);
      }};
      const show = (location, marker) => {{
        selected?.classList.remove('selected');
        selectedRow?.classList.remove('selected');
        selected = marker || markers.find(entry => entry.location === location)?.node || null;
        selectedRow = markers.find(entry => entry.location === location)?.row || null;
        selected?.classList.add('selected');
        selectedRow?.classList.add('selected');
        selectedRow?.scrollIntoView({{ block: 'nearest' }});
        details.replaceChildren();
        appendText(details, 'h3', location.name);
        appendText(details, 'div', `${{location.biome}} [${{location.position.join(', ')}}]`, 'location-meta');
        appendText(details, 'p', location.description);
        const facts = document.createElement('div');
        facts.className = 'location-facts';
        [
          ['Ресурсы', listValue(location.resources)],
          ['Враги', listValue(location.enemies)],
          ['Лут', listValue(location.loot)],
          ['Следы', listValue(location.story_clues)],
          ['Инструменты', listValue(location.required_tools)]
        ].forEach(([label, value]) => {{
          const fact = document.createElement('div');
          fact.className = 'location-fact';
          appendText(fact, 'span', label);
          appendText(fact, 'div', value);
          facts.appendChild(fact);
        }});
        details.appendChild(facts);
      }};
      const applyFilter = () => {{
        markers.forEach(({{ location, node, row }}) => {{
          const visible = (activeBiome === 'Все' || location.biome === activeBiome) && matchesSearch(location);
          node.classList.toggle('hidden', !visible);
          row.style.display = visible ? '' : 'none';
        }});
      }};
      biomes.forEach(biome => {{
        const button = document.createElement('button');
        button.textContent = `${{biome}} (${{biomeCounts[biome] || 0}})`;
        button.className = biome === activeBiome ? 'active' : '';
        button.addEventListener('click', () => {{
          activeBiome = biome;
          filters.querySelectorAll('button').forEach(filter => filter.classList.toggle('active', filter === button));
          applyFilter();
        }});
        filters.appendChild(button);
      }});
      search.addEventListener('input', () => {{
        searchText = search.value.trim().toLocaleLowerCase('ru-RU');
        applyFilter();
      }});
      locations.forEach((location, index) => {{
        const [x, , z] = location.position;
        const left = ((x - bounds.minX) / (bounds.maxX - bounds.minX)) * 100;
        const top = ((bounds.maxZ - z) / (bounds.maxZ - bounds.minZ)) * 100;
        const marker = document.createElement('button');
        marker.className = 'location-marker';
        marker.style.left = `${{left}}%`;
        marker.style.top = `${{top}}%`;
        marker.style.background = colors[location.biome] || '#fff';
        marker.title = `${{index + 1}}. ${{location.name}} - ${{location.biome}}`;
        marker.setAttribute('aria-label', marker.title);
        marker.addEventListener('click', () => show(location, marker));
        map.appendChild(marker);
        const row = document.createElement('button');
        row.textContent = `${{String(index + 1).padStart(2, '0')}}. ${{location.name}} - ${{location.biome}}`;
        row.addEventListener('click', () => show(location, marker));
        list.appendChild(row);
        markers.push({{ location, node: marker, row }});
      }});
      show(locations[0], markers[0].node);
    }})();
  </script>
""".format(data=location_data)
    document = f"""<!doctype html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <title>Island Debug Preview</title>
  <style>
    body {{ margin: 0; font: 14px/1.45 system-ui, sans-serif; color: #d7dddf; background: #151817; }}
    .layout {{ display: grid; grid-template-columns: minmax(260px, 320px) minmax(0, 1fr); min-height: 100vh; }}
    .goals {{ position: sticky; top: 0; height: 100vh; overflow: auto; border-right: 1px solid #343b38; background: #111412; padding: 18px; box-sizing: border-box; }}
    .content {{ min-width: 0; padding-bottom: 180px; }}
    header, main {{ max-width: 1180px; margin: 0 auto; padding: 24px; }}
    header {{ border-bottom: 1px solid #343b38; }}
    h1 {{ margin: 0 0 8px; font-size: 28px; }}
    h2 {{ margin: 0 0 10px; font-size: 16px; }}
    p {{ margin: 0; color: #9ba6a2; }}
    .goal-list {{ list-style: none; margin: 0; padding: 0; display: grid; gap: 10px; }}
    .goal-item {{ border: 1px solid #333b37; background: #1d2220; border-radius: 6px; padding: 12px; }}
    .goal-item strong {{ display: block; color: #f0f3ef; font-size: 14px; }}
    .goal-item span {{ display: block; color: #9ba6a2; font-size: 12px; margin-top: 5px; }}
    .metrics, .grid {{ display: grid; gap: 14px; }}
    .metrics {{ grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); margin: 18px 0; }}
    .metric, .card, .panel {{ border: 1px solid #333b37; background: #1d2220; border-radius: 6px; padding: 14px; }}
    .metric strong {{ display: block; font-size: 22px; color: #f0f3ef; }}
    .grid {{ grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); }}
    img {{ width: 100%; image-rendering: pixelated; border: 1px solid #303633; background: #0f1211; }}
    .missing {{ opacity: 0.62; }}
    table {{ width: 100%; border-collapse: collapse; }}
    th, td {{ padding: 7px 8px; border-bottom: 1px solid #303633; text-align: left; }}
    ul {{ margin: 0; padding-left: 18px; }}
    .minimap {{ position: fixed; right: 18px; bottom: 18px; width: 220px; border: 1px solid #47514b; background: #111412; border-radius: 6px; padding: 10px; box-shadow: 0 14px 38px rgba(0,0,0,.42); z-index: 5; }}
    .minimap h2 {{ font-size: 13px; margin-bottom: 8px; }}
    .minimap img {{ display: block; aspect-ratio: 1 / 1; object-fit: cover; }}
    .minimap-empty {{ display: grid; place-items: center; aspect-ratio: 1 / 1; border: 1px solid #303633; color: #9ba6a2; }}
    {location_styles}
    @media (max-width: 860px) {{
      .layout {{ display: block; }}
      .goals {{ position: static; height: auto; border-right: 0; border-bottom: 1px solid #343b38; }}
      .minimap {{ width: 150px; }}
    }}
  </style>
</head>
<body>
  <div class="layout">
    <aside class="goals">
      <h2>Игровые цели</h2>
      <ol class="goal-list">{goal_items}</ol>
    </aside>
    <div class="content">
      <header>
        <h1>Island Debug Preview</h1>
        <p>Seed {SEED}, grid {GRID}, status {html.escape(str(quality_report.get('status', 'unknown')))}</p>
        <div class="metrics">
          <div class="metric"><span>Height</span><strong>{height.get("min", "-")} to {height.get("max", "-")}</strong></div>
          <div class="metric"><span>Avg slope</span><strong>{slope.get("average", "-")}</strong></div>
          <div class="metric"><span>Good walkability</span><strong>{walkability.get("good_sample_ratio", "-")}</strong></div>
          <div class="metric"><span>Issues</span><strong>{len(issues)}</strong></div>
        </div>
      </header>
      <main>
        <div class="grid">
          {card_markup}
        </div>
        {location_markup}
        <section class="panel">
          <h2>Placement</h2>
          <table><thead><tr><th>Rule</th><th>Requested</th><th>Spawned</th><th>Skipped</th></tr></thead><tbody>
          {placement_rows}
          </tbody></table>
        </section>
        <section class="panel">
          <h2>Quality Issues</h2>
          <ul>{issue_items}</ul>
        </section>
      </main>
    </div>
  </div>
  <aside class="minimap">
    <h2>Миникарта</h2>
    {minimap_markup}
  </aside>
  {location_script}
</body>
</html>
"""
    DEBUG_PREVIEW_PATH.write_text(document, encoding="utf-8")
    return DEBUG_PREVIEW_PATH


def landmark_entry(
    name: str,
    category: str,
    x: float,
    z: float,
    y_offset: float = 1.6,
    radius: float = 6.0,
) -> dict[str, object]:
    return {
        "name": name,
        "category": category,
        "position": {
            "x": round(x, 2),
            "y": round(terrain_height(x, z) + y_offset, 2),
            "z": round(z, 2),
        },
        "radius": radius,
        "walkability": round(walkability_score(x, z), 3),
    }


def generate_landmarks() -> None:
    landmarks: list[dict[str, object]] = [
        landmark_entry("arrival_camp", "camp", -32.0, 77.0, 1.2, 10.0),
        landmark_entry("granite_house", "shelter", GRANITE_HOUSE[0], GRANITE_HOUSE[1], 2.0, 12.0),
        landmark_entry("natural_harbor", "harbor", 91.0, 14.5, 1.2, 18.0),
        landmark_entry("grant_lake", "water", LAKE_CENTER[0], LAKE_CENTER[1], 1.2, 18.0),
        landmark_entry("mercy_river_source", "water", RIVER_PATH[0][0], RIVER_PATH[0][1], 1.0, 7.0),
        landmark_entry("mercy_river_mouth", "water", RIVER_PATH[-1][0], RIVER_PATH[-1][1], 1.0, 9.0),
        landmark_entry("volcano_crater", "volcano", VOLCANO.center[0], VOLCANO.center[1], 8.0, 20.0),
        landmark_entry("sunken_temple", "ruin", 0.0, -84.0, 1.3, 12.0),
        landmark_entry("watchtower", "ruin", 91.0, 18.0, 2.0, 10.0),
        landmark_entry("standing_stones", "ruin", -82.0, 38.0, 1.5, 12.0),
    ]
    for index, grotto in enumerate(GROTTOS, start=1):
        x, z = grotto.position
        landmarks.append(landmark_entry(f"hidden_grotto_{index}", "grotto", x, z, 1.4, grotto.chamber_radius))
    for index, progress in enumerate(WATER.waterfall_progresses, start=1):
        x, z = path_point(RIVER_PATH, progress)
        landmarks.append(landmark_entry(f"mercy_falls_{index}", "waterfall", x, z, 1.0, 7.0))
    for index, path in enumerate(TRIBUTARY_PATHS, start=1):
        x, z = path[0]
        landmarks.append(landmark_entry(f"tributary_source_{index}", "water", x, z, 1.0, 6.0))
    for index, (x, z, radius_x, radius_z) in enumerate(CONFIG.islets, start=1):
        landmarks.append(landmark_entry(f"offshore_islet_{index}", "islet", x, z, 1.2, max(radius_x, radius_z)))

    payload = {
        "generated_by": "tools/generate_island.py",
        "seed": SEED,
        "coordinate_system": "OBJ world coordinates, y sampled from generated terrain",
        "landmarks": landmarks,
    }
    LANDMARKS_PATH.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


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
    reset_placement_stats()
    lines = [
        "# Generated by tools/generate_island.py. Do not edit by hand.",
        "# model <id> <asset-path>",
        "# frame_counter on|off",
        "# spawn_model <id> px py pz sx sy sz colliderHalfX colliderHalfY colliderHalfZ [yaw]",
        "# spawn_decor <id> px py pz sx sy sz [yaw]",
        "",
        "frame_counter on",
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
        "model basalt_scree_tiny models/basalt_scree_tiny.obj",
        "model basalt_scree_small models/basalt_scree_small.obj",
        "model basalt_scree_medium models/basalt_scree_medium.obj",
        "model basalt_scree_large models/basalt_scree_large.obj",
        "model basalt_scree_shard models/basalt_scree_shard.obj",
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

    lines.extend(("", "# Basalt scree on volcanic slopes. Small decor-only stones keep traversal clear."))
    basalt_scree = [
        ("basalt_scree_large", -38.0, -4.0, 1.18, 0.35),
        ("basalt_scree_medium", -34.0, -7.0, 1.05, 1.24),
        ("basalt_scree_small", -42.0, -1.0, 0.88, 2.60),
        ("basalt_scree_tiny", -31.0, -2.0, 0.76, 4.30),
        ("basalt_scree_shard", -26.0, 14.0, 1.12, 0.80),
        ("basalt_scree_medium", -22.0, 11.0, 1.02, 2.10),
        ("basalt_scree_tiny", -29.0, 18.0, 0.72, 3.35),
        ("basalt_scree_large", 12.0, -28.0, 1.16, 1.75),
        ("basalt_scree_small", 16.0, -31.0, 0.88, 4.90),
        ("basalt_scree_shard", 9.0, -24.0, 1.08, 2.95),
        ("basalt_scree_medium", 6.0, 18.0, 1.06, 0.45),
        ("basalt_scree_small", 10.0, 20.0, 0.86, 2.80),
        ("basalt_scree_tiny", 3.0, 15.0, 0.72, 5.10),
        ("basalt_scree_large", 0.0, 0.0, 1.22, 1.30),
        ("basalt_scree_medium", -5.0, 4.0, 1.02, 3.20),
        ("basalt_scree_small", 4.0, -6.0, 0.84, 4.70),
        ("basalt_scree_shard", -18.0, -8.0, 1.08, 0.20),
        ("basalt_scree_tiny", -15.0, -12.0, 0.72, 2.40),
        ("basalt_scree_large", 0.0, 18.0, 1.25, 0.15),
        ("basalt_scree_medium", -3.5, 19.5, 1.05, 1.65),
        ("basalt_scree_shard", 3.8, 16.5, 1.10, 3.10),
        ("basalt_scree_small", 1.6, 21.8, 0.92, 4.40),
        ("basalt_scree_tiny", -1.8, 15.2, 0.78, 2.55),
    ]
    for model, x, z, scale, yaw in basalt_scree:
        spawn_decor(lines, model, x, z, scale, yaw)
    scree_models = (
        "basalt_scree_tiny",
        "basalt_scree_small",
        "basalt_scree_medium",
        "basalt_scree_shard",
    )
    for index in range(72):
        angle = random.uniform(0.0, math.tau)
        radius = random.uniform(VOLCANO.crater_radius + 8.0, VOLCANO.cone_radius + 26.0)
        x = VOLCANO.center[0] + math.cos(angle) * radius * random.uniform(0.72, 1.12)
        z = VOLCANO.center[1] + math.sin(angle) * radius * random.uniform(0.72, 1.12)
        if island_radius(x, z) > 0.86:
            continue
        height, slope, moisture = biome_metrics(x, z)
        if height < 7.5 or slope > 1.35:
            continue
        spawn_decor(
            lines,
            scree_models[index % len(scree_models)],
            x,
            z,
            random.uniform(0.62, 1.18),
            random.uniform(0.0, math.tau),
        )

    lines.extend(("", "# Dense forest, with clearings around landmarks and the main trail."))
    forest_rule = DECORATION_RULES["forest"]
    begin_placement_rule("forest", forest_rule.count)
    for _ in range(forest_rule.count):
        candidate: tuple[float, float, BiomeConfig] | None = None
        for _attempt in range(100):
            x = random.uniform(-TERRAIN.island_radius_x * 0.92, TERRAIN.island_radius_x * 0.92)
            z = random.uniform(-TERRAIN.island_radius_z * 0.92, TERRAIN.island_radius_z * 0.92)
            radius = island_radius(x, z)
            near_path = abs(x + 0.15 * z + 14.0) < forest_rule.avoid_path_width
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
                forest_rule.min_radius < radius < forest_rule.max_radius
                and not near_path
                and not near_landmark
                and forest_rule.min_height < height < forest_rule.max_height
                and slope < forest_rule.max_slope
                and moisture >= forest_rule.min_moisture
                and biome.name in forest_rule.allowed_biomes
                and biome.tree_density > 0.0
                and random.random() < forest_chance
            ):
                candidate = (x, z, biome)
                break
        if candidate is None:
            record_placement("forest", False)
            continue
        x, z, biome = candidate
        model = "oak" if random.random() < biome.oak_preference else "tree"
        scale = random.uniform(0.75, 1.35)
        spawn(lines, model, x, z, scale, (0.46, 2.35, 0.46), random.uniform(0.0, math.tau))
        record_placement("forest", True)

    lines.extend(("", "# Low understory creates depth between the larger trees."))
    understory_rule = DECORATION_RULES["understory"]
    begin_placement_rule("understory", understory_rule.count)
    for _ in range(understory_rule.count):
        candidate: tuple[float, float] | None = None
        for _attempt in range(100):
            x = random.uniform(-TERRAIN.island_radius_x * 0.94, TERRAIN.island_radius_x * 0.94)
            z = random.uniform(-TERRAIN.island_radius_z * 0.94, TERRAIN.island_radius_z * 0.94)
            radius = island_radius(x, z)
            near_path = abs(x + 0.15 * z + 14.0) < understory_rule.avoid_path_width
            near_grotto = min(math.hypot(x - gx, z - gz) for gx, gz in GROTTO_POSITIONS) < understory_rule.avoid_grotto_radius
            height, slope, moisture = biome_metrics(x, z)
            biome = classify_biome(x, height, z, slope, moisture)
            if (
                understory_rule.min_radius < radius < understory_rule.max_radius
                and not near_path
                and not near_grotto
                and understory_rule.min_height < height < understory_rule.max_height
                and slope < understory_rule.max_slope
                and moisture >= understory_rule.min_moisture
                and biome.name in understory_rule.allowed_biomes
                and biome.bush_density > 0.0
                and random.random() < biome.bush_density * max(moisture, 0.35)
            ):
                candidate = (x, z)
                break
        if candidate is None:
            record_placement("understory", False)
            continue
        x, z = candidate
        spawn_decor(lines, "bush", x, z, random.uniform(0.55, 1.25), random.uniform(0.0, math.tau))
        record_placement("understory", True)

    lines.extend(("", "# Rocks mark the coast and dangerous approaches."))
    rock_rule = DECORATION_RULES["coastal_rocks"]
    begin_placement_rule("coastal_rocks", rock_rule.count)
    for _ in range(rock_rule.count):
        candidate: tuple[float, float] | None = None
        for _attempt in range(60):
            angle = random.uniform(0.0, math.tau)
            radius = random.uniform(TERRAIN.island_radius_x * 0.70, TERRAIN.island_radius_x * 1.03)
            x = math.cos(angle) * radius
            z = math.sin(angle) * radius * (TERRAIN.island_radius_z / TERRAIN.island_radius_x)
            height, slope, moisture = biome_metrics(x, z)
            biome = classify_biome(x, height, z, slope, moisture)
            if (
                rock_rule.min_radius < island_radius(x, z) < rock_rule.max_radius
                and rock_rule.min_height < height < rock_rule.max_height
                and biome.name in rock_rule.allowed_biomes
                and not (height > 3.0 and slope < 0.35)
            ):
                candidate = (x, z)
                break
        if candidate is None:
            record_placement("coastal_rocks", False)
            continue
        x, z = candidate
        scale = random.uniform(0.7, 1.8)
        spawn(lines, "rock", x, z, scale, (0.82, 0.7, 0.74), random.uniform(0.0, math.tau))
        record_placement("coastal_rocks", True)

    lines.extend(("", "# Palms frame beaches and the sheltered natural harbor."))
    palm_rule = DECORATION_RULES["palms"]
    begin_placement_rule("palms", palm_rule.count)
    for _ in range(palm_rule.count):
        candidate: tuple[float, float] | None = None
        for _attempt in range(100):
            angle = random.uniform(0.0, math.tau)
            radius = random.uniform(TERRAIN.island_radius_x * 0.66, TERRAIN.island_radius_x * 0.99)
            x = math.cos(angle) * radius
            z = math.sin(angle) * radius * (TERRAIN.island_radius_z / TERRAIN.island_radius_x)
            height, slope, moisture = biome_metrics(x, z)
            biome = classify_biome(x, height, z, slope, moisture)
            near_harbor = math.hypot(x - HARBOR_CENTER[0], z - HARBOR_CENTER[1]) < 38.0
            if (
                palm_rule.min_radius < island_radius(x, z) < palm_rule.max_radius
                and palm_rule.min_height < height < palm_rule.max_height
                and slope < palm_rule.max_slope
                and moisture > palm_rule.min_moisture
                and biome.name in palm_rule.allowed_biomes
                and (near_harbor or z > 35.0)
            ):
                candidate = (x, z)
                break
        if candidate is None:
            record_placement("palms", False)
            continue
        x, z = candidate
        scale = random.uniform(0.75, 1.35)
        spawn_decor(lines, "palm", x, z, scale, random.uniform(0.0, math.tau))
        record_placement("palms", True)

    lines.extend(("", "# Mushrooms gather in the eastern marsh."))
    mushroom_rule = DECORATION_RULES["mushrooms"]
    begin_placement_rule("mushrooms", mushroom_rule.count)
    for _ in range(mushroom_rule.count):
        candidate: tuple[float, float] | None = None
        for _attempt in range(100):
            x = random.uniform(42.0, TERRAIN.island_radius_x * 0.82)
            z = random.uniform(-8.0, TERRAIN.island_radius_z * 0.76)
            height, slope, moisture = biome_metrics(x, z)
            biome = classify_biome(x, height, z, slope, moisture)
            if (
                biome.name in mushroom_rule.allowed_biomes
                and mushroom_rule.min_height < height < mushroom_rule.max_height
                and slope < mushroom_rule.max_slope
                and moisture > mushroom_rule.min_moisture
            ):
                candidate = (x, z)
                break
        if candidate is None:
            record_placement("mushrooms", False)
            continue
        x, z = candidate
        spawn_decor(lines, "mushroom", x, z, random.uniform(0.8, 1.8), random.uniform(0.0, math.tau))
        record_placement("mushrooms", True)

    SCENE_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


def generate_gameplay_goals() -> None:
    payload = {
        "generated_by": "tools/generate_island.py",
        "language": "ru",
        "seed": SEED,
        "actions": [
            {
                "id": "light_fire",
                "title": "Разжечь костер",
                "category": "выживание",
                "description": "Использовать сухую древесину и кремень, чтобы создать безопасный источник тепла и света.",
                "requires": ["дерево", "кремень"],
                "produces": ["огонь", "безопасная зона лагеря"],
                "landmark_hint": "arrival_camp",
            },
            {
                "id": "keep_fire_alive",
                "title": "Поддерживать огонь",
                "category": "выживание",
                "description": "Добавлять дрова в костер во время ночи, дождя или шторма.",
                "requires": ["огонь", "дерево"],
                "produces": ["тепло", "защита от темноты"],
                "landmark_hint": "arrival_camp",
            },
            {
                "id": "chop_tree",
                "title": "Рубить деревья",
                "category": "добыча",
                "description": "Использовать топор на деревьях, чтобы получить древесину для костра, инструментов и построек.",
                "requires": ["топор"],
                "produces": ["дерево"],
                "landmark_hint": "temperate_forest",
            },
            {
                "id": "mine_stone",
                "title": "Добывать камень",
                "category": "добыча",
                "description": "Разбивать прибрежные и горные камни, чтобы получить материал для инструментов и печи.",
                "requires": ["кирка или тяжелый камень"],
                "produces": ["камень", "кремень"],
                "landmark_hint": "natural_harbor",
            },
            {
                "id": "gather_mushrooms",
                "title": "Собирать грибы",
                "category": "собирательство",
                "description": "Искать грибы во влажных низинах и болотах, затем использовать их для еды или лечения.",
                "requires": [],
                "produces": ["грибы"],
                "landmark_hint": "eastern_wetland",
            },
            {
                "id": "collect_water",
                "title": "Набрать воду",
                "category": "выживание",
                "description": "Набрать воду у озера или реки и подготовить ее к употреблению.",
                "requires": ["емкость"],
                "produces": ["сырая вода"],
                "landmark_hint": "grant_lake",
            },
            {
                "id": "boil_water",
                "title": "Кипятить воду",
                "category": "выживание",
                "description": "Очистить сырую воду на костре, чтобы снизить риск болезни.",
                "requires": ["сырая вода", "огонь"],
                "produces": ["чистая вода"],
                "landmark_hint": "arrival_camp",
            },
            {
                "id": "craft_stone_axe",
                "title": "Сделать каменный топор",
                "category": "крафт",
                "description": "Собрать дерево, камень и волокна, чтобы сделать первый надежный инструмент.",
                "requires": ["дерево", "камень", "волокна"],
                "produces": ["топор"],
                "landmark_hint": "arrival_camp",
            },
            {
                "id": "build_shelter",
                "title": "Построить укрытие",
                "category": "строительство",
                "description": "Поставить простой навес или укрепить найденное убежище перед непогодой.",
                "requires": ["дерево", "волокна"],
                "produces": ["укрытие"],
                "landmark_hint": "granite_house",
            },
            {
                "id": "explore_grotto",
                "title": "Исследовать грот",
                "category": "исследование",
                "description": "Войти в скрытый грот с факелом и отметить найденные проходы.",
                "requires": ["факел"],
                "produces": ["открытая область", "редкий ресурс"],
                "landmark_hint": "hidden_grotto_1",
            },
            {
                "id": "activate_ruin_marker",
                "title": "Активировать знак руин",
                "category": "исследование",
                "description": "Осмотреть древний объект и добавить его в журнал острова.",
                "requires": [],
                "produces": ["запись в журнале"],
                "landmark_hint": "standing_stones",
            },
            {
                "id": "build_boat",
                "title": "Построить лодку",
                "category": "строительство",
                "description": "Собрать материалы у гавани и построить лодку для малых островков или побега.",
                "requires": ["дерево", "веревка", "смола"],
                "produces": ["лодка"],
                "landmark_hint": "natural_harbor",
            },
        ],
        "goals": [
            {
                "id": "survive_first_day",
                "title": "День 1: выжить после высадки",
                "description": "Найти место для лагеря, собрать древесину и разжечь первый костер.",
                "priority": "main",
                "steps": [
                    {"action": "chop_tree", "text": "Собрать первую древесину."},
                    {"action": "light_fire", "text": "Разжечь костер у лагеря."},
                    {"action": "keep_fire_alive", "text": "Поддерживать огонь до утра."},
                ],
                "landmarks": ["arrival_camp"],
            },
            {
                "id": "find_clean_water",
                "title": "Найти воду",
                "description": "Добраться до озера Гранта или реки Милосердия и подготовить воду к употреблению.",
                "priority": "main",
                "steps": [
                    {"action": "collect_water", "text": "Набрать сырую воду."},
                    {"action": "boil_water", "text": "Прокипятить воду на костре."},
                ],
                "landmarks": ["grant_lake", "mercy_river_source", "mercy_river_mouth"],
            },
            {
                "id": "craft_first_tool",
                "title": "Сделать первый инструмент",
                "description": "Добыть камень и собрать древесину, чтобы скрафтить каменный топор.",
                "priority": "main",
                "steps": [
                    {"action": "mine_stone", "text": "Добыть камень или кремень."},
                    {"action": "craft_stone_axe", "text": "Сделать каменный топор."},
                ],
                "landmarks": ["natural_harbor", "arrival_camp"],
            },
            {
                "id": "secure_shelter",
                "title": "Найти безопасное убежище",
                "description": "Обнаружить Granite House и подготовить его как долговременную базу.",
                "priority": "main",
                "steps": [
                    {"action": "build_shelter", "text": "Укрепить вход и место для сна."},
                    {"action": "light_fire", "text": "Поставить постоянный источник света."},
                ],
                "landmarks": ["granite_house"],
            },
            {
                "id": "map_island",
                "title": "Составить карту острова",
                "description": "Посетить ключевые точки острова и открыть ориентиры в журнале.",
                "priority": "exploration",
                "steps": [
                    {"action": "activate_ruin_marker", "text": "Отметить круг стоячих камней."},
                    {"action": "explore_grotto", "text": "Исследовать хотя бы один скрытый грот."},
                    {"action": "activate_ruin_marker", "text": "Осмотреть сторожевую башню и затонувший храм."},
                ],
                "landmarks": ["standing_stones", "watchtower", "sunken_temple", "hidden_grotto_1"],
            },
            {
                "id": "prepare_for_storm",
                "title": "Подготовиться к шторму",
                "description": "Запасти воду, еду и дрова, затем переждать непогоду в укрытии.",
                "priority": "survival",
                "steps": [
                    {"action": "gather_mushrooms", "text": "Собрать запас еды."},
                    {"action": "boil_water", "text": "Подготовить чистую воду."},
                    {"action": "keep_fire_alive", "text": "Поддерживать костер во время шторма."},
                ],
                "landmarks": ["granite_house", "hidden_grotto_1", "arrival_camp"],
            },
            {
                "id": "escape_or_claim_island",
                "title": "Покинуть остров или сделать его домом",
                "description": "Построить лодку для побега или превратить Granite House в полноценную базу.",
                "priority": "final",
                "steps": [
                    {"action": "build_boat", "text": "Построить лодку у природной гавани."},
                    {"action": "build_shelter", "text": "Укрепить Granite House как запасной финал."},
                ],
                "landmarks": ["natural_harbor", "granite_house", "offshore_islet_1"],
            },
        ],
    }
    GAMEPLAY_GOALS_PATH.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def add_quality_issue(
    issues: list[dict[str, object]],
    severity: str,
    code: str,
    message: str,
    details: dict[str, object] | None = None,
) -> None:
    issue: dict[str, object] = {
        "severity": severity,
        "code": code,
        "message": message,
    }
    if details:
        issue["details"] = details
    issues.append(issue)


def path_length(path: tuple[tuple[float, float], ...]) -> float:
    return sum(math.hypot(end[0] - start[0], end[1] - start[1]) for start, end in zip(path, path[1:]))


def obj_file_stats(path: Path) -> dict[str, int]:
    stats = {"vertices": 0, "faces": 0}
    if not path.exists():
        return stats
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            if line.startswith("v "):
                stats["vertices"] += 1
            elif line.startswith("f "):
                stats["faces"] += 1
    return stats


def scene_spawn_stats() -> dict[str, int]:
    stats = {"spawn_model": 0, "spawn_decor": 0, "spawn_collider": 0}
    if not SCENE_PATH.exists():
        return stats
    with SCENE_PATH.open("r", encoding="utf-8") as handle:
        for line in handle:
            command = line.split(maxsplit=1)[0] if line.strip() else ""
            if command in stats:
                stats[command] += 1
    return stats


def check_generated_files(outputs: list[Path], issues: list[dict[str, object]]) -> dict[str, object]:
    file_stats: dict[str, object] = {}
    for path in outputs:
        if path == QUALITY_REPORT_PATH:
            continue
        relative = relative_output(path)
        if not path.exists():
            add_quality_issue(issues, "error", "missing_output", f"Expected output is missing: {relative}")
            file_stats[relative] = {"exists": False}
            continue
        size = path.stat().st_size
        stats: dict[str, object] = {"exists": True, "bytes": size}
        if size == 0:
            add_quality_issue(issues, "error", "empty_output", f"Generated output is empty: {relative}")
        if path.suffix == ".obj":
            obj_stats = obj_file_stats(path)
            stats.update(obj_stats)
            if obj_stats["vertices"] == 0 or obj_stats["faces"] == 0:
                add_quality_issue(
                    issues,
                    "error",
                    "empty_obj_geometry",
                    f"OBJ output has no usable geometry: {relative}",
                    obj_stats,
                )
        if path.suffix == ".json":
            try:
                json.loads(path.read_text(encoding="utf-8"))
            except json.JSONDecodeError as error:
                add_quality_issue(
                    issues,
                    "error",
                    "invalid_json_output",
                    f"Generated JSON output is invalid: {relative}",
                    {"line": error.lineno, "column": error.colno},
                )
        file_stats[relative] = stats
    return file_stats


def check_river_network(issues: list[dict[str, object]]) -> dict[str, object]:
    river_length = path_length(RIVER_PATH)
    mouth_distance = math.hypot(RIVER_PATH[-1][0] - HARBOR_CENTER[0], RIVER_PATH[-1][1] - HARBOR_CENTER[1])
    stats = {
        "river_points": len(RIVER_PATH),
        "river_length": round(river_length, 2),
        "mouth_distance_to_harbor": round(mouth_distance, 2),
        "tributary_points": [len(path) for path in TRIBUTARY_PATHS],
        "tributary_lengths": [round(path_length(path), 2) for path in TRIBUTARY_PATHS],
    }
    if len(RIVER_PATH) < 8:
        add_quality_issue(issues, "error", "short_river_path", "Mercy River has too few sampled points.", stats)
    if river_length < 80.0:
        add_quality_issue(issues, "error", "short_river_length", "Mercy River is too short to read as a drainage feature.", stats)
    if mouth_distance > 20.0:
        add_quality_issue(issues, "warning", "river_misses_harbor", "Mercy River mouth is far from the harbor target.", stats)
    for index, path in enumerate(TRIBUTARY_PATHS, start=1):
        if len(path) < 4 or path_length(path) < 18.0:
            add_quality_issue(
                issues,
                "warning",
                "short_tributary",
                f"Tributary {index} is very short.",
                {"points": len(path), "length": round(path_length(path), 2)},
            )
    return stats


def check_landmarks(issues: list[dict[str, object]]) -> dict[str, object]:
    if not LANDMARKS_PATH.exists():
        return {"checked": 0, "available": False}
    payload = json.loads(LANDMARKS_PATH.read_text(encoding="utf-8"))
    landmarks = payload.get("landmarks", [])
    low_walkability_categories = {"water", "waterfall", "volcano", "islet"}
    checked = 0
    for landmark in landmarks:
        position = landmark.get("position", {})
        name = str(landmark.get("name", "unknown"))
        category = str(landmark.get("category", "unknown"))
        x = float(position.get("x", 0.0))
        y = float(position.get("y", 0.0))
        z = float(position.get("z", 0.0))
        terrain_y = terrain_height(x, z)
        y_offset = y - terrain_y
        radius = island_radius(x, z)
        walkability = float(landmark.get("walkability", walkability_score(x, z)))
        checked += 1
        if category != "islet" and radius > 1.03:
            add_quality_issue(
                issues,
                "error",
                "landmark_off_island",
                f"Landmark {name} is outside the generated island.",
                {"x": x, "z": z, "island_radius": round(radius, 3)},
            )
        if y_offset < 0.2 or y_offset > 12.0:
            add_quality_issue(
                issues,
                "warning",
                "landmark_height_offset",
                f"Landmark {name} has an unusual height offset from terrain.",
                {"y_offset": round(y_offset, 3), "terrain_y": round(terrain_y, 3), "landmark_y": y},
            )
        if category not in low_walkability_categories and walkability < 0.08:
            add_quality_issue(
                issues,
                "warning",
                "low_landmark_walkability",
                f"Landmark {name} may be hard to reach.",
                {"walkability": round(walkability, 3), "category": category},
            )
    return {"checked": checked, "available": True}


def check_debug_report(issues: list[dict[str, object]]) -> dict[str, object]:
    if not TERRAIN_REPORT_PATH.exists():
        return {"available": False}
    report = json.loads(TERRAIN_REPORT_PATH.read_text(encoding="utf-8"))
    walkability = report.get("walkability", {})
    slope = report.get("slope", {})
    good_ratio = float(walkability.get("good_sample_ratio", 0.0))
    steep_ratio = float(slope.get("steep_sample_ratio", 0.0))
    if good_ratio < 0.18:
        add_quality_issue(
            issues,
            "warning",
            "low_walkability_ratio",
            "Generated terrain has a low ratio of easy traversal samples.",
            {"good_sample_ratio": good_ratio},
        )
    if steep_ratio > 0.65:
        add_quality_issue(
            issues,
            "warning",
            "high_steep_ratio",
            "Generated terrain has a high ratio of steep samples.",
            {"steep_sample_ratio": steep_ratio},
        )
    return {"available": True, "good_sample_ratio": good_ratio, "steep_sample_ratio": steep_ratio}


def check_placement(issues: list[dict[str, object]]) -> dict[str, dict[str, int]]:
    for name, stats in sorted(PLACEMENT_STATS.items()):
        requested = max(1, stats["requested"])
        skipped_ratio = stats["skipped"] / requested
        if skipped_ratio > 0.15:
            add_quality_issue(
                issues,
                "warning",
                "high_placement_skip_ratio",
                f"Decoration rule {name} skipped many requested placements.",
                {"rule": name, **stats, "skipped_ratio": round(skipped_ratio, 3)},
            )
    return {name: dict(stats) for name, stats in sorted(PLACEMENT_STATS.items())}


def generate_quality_report(only: str, outputs: list[Path]) -> Path:
    issues: list[dict[str, object]] = []
    payload = {
        "generated_by": "tools/generate_island.py",
        "seed": SEED,
        "grid": GRID,
        "group": only,
        "files": check_generated_files(outputs, issues),
        "river": check_river_network(issues),
        "landmarks": check_landmarks(issues),
        "debug": check_debug_report(issues),
        "scene": {
            "spawns": scene_spawn_stats(),
            "placement": check_placement(issues),
        },
        "cache": generation_cache_stats(),
        "issues": issues,
        "status": "fail" if any(issue["severity"] == "error" for issue in issues) else "pass",
    }
    QUALITY_REPORT_PATH.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return QUALITY_REPORT_PATH


def map_output_paths() -> list[Path]:
    return [
        MAP_PATH,
        *SPLIT_MAP_PATHS.values(),
        *MAP_LOD_PATHS,
        *sorted(TERRAIN_CHUNK_DIR.glob("*.obj")),
        INTERNAL_WATER_PATH,
        LANDMARKS_PATH,
    ]


def debug_output_paths() -> list[Path]:
    return [
        WALKABILITY_PATH,
        HEIGHTMAP_PATH,
        BIOME_MAP_PATH,
        SLOPE_MAP_PATH,
        MOISTURE_MAP_PATH,
        *MATERIAL_MASK_PATHS.values(),
        TERRAIN_REPORT_PATH,
    ]


def generate_map_outputs() -> list[Path]:
    generate_map()
    generate_split_maps()
    generate_lod_maps()
    generate_internal_water()
    generate_landmarks()
    return map_output_paths()


def generate_debug_outputs() -> list[Path]:
    generate_walkability_map()
    generate_debug_maps_and_report()
    return debug_output_paths()


def generate_scene_outputs() -> list[Path]:
    generate_scene()
    generate_gameplay_goals()
    return [SCENE_PATH, GAMEPLAY_GOALS_PATH]


def run_generation(only: str) -> list[Path]:
    return run_generation_group(
        only,
        {
            "map": generate_map_outputs,
            "debug": generate_debug_outputs,
            "scene": generate_scene_outputs,
        },
    )


def main(argv: Sequence[str] | None = None) -> None:
    options = parse_args(argv, DEFAULT_ROOT, DEFAULT_SEED, DEFAULT_GRID)
    configure_runtime(options)
    outputs = run_generation(options.only)
    outputs.append(generate_quality_report(options.only, outputs))
    outputs.append(generate_debug_preview())
    print("Generated " + ", ".join(relative_output(path) for path in outputs))


if __name__ == "__main__":
    main()
