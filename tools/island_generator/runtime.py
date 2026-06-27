"""Runtime file layout for generated island assets."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class OutputLayout:
    root: Path
    map_path: Path
    map_lod_paths: tuple[Path, Path]
    terrain_chunk_dir: Path
    split_map_paths: dict[str, Path]
    internal_water_path: Path
    landmarks_path: Path
    walkability_path: Path
    heightmap_path: Path
    biome_map_path: Path
    slope_map_path: Path
    moisture_map_path: Path
    material_mask_paths: dict[str, Path]
    terrain_report_path: Path
    quality_report_path: Path
    debug_preview_path: Path
    scene_path: Path
    scene_chunk_dir: Path
    gameplay_goals_path: Path
    world_config_path: Path

    @classmethod
    def from_root(cls, root: Path) -> "OutputLayout":
        maps_dir = root / "assets" / "maps"
        scripts_dir = root / "assets" / "scripts"
        maps_dir.mkdir(parents=True, exist_ok=True)
        scripts_dir.mkdir(parents=True, exist_ok=True)

        return cls(
            root=root,
            map_path=maps_dir / "demo_map.obj",
            map_lod_paths=(
                maps_dir / "demo_map_lod1.obj",
                maps_dir / "demo_map_lod2.obj",
            ),
            terrain_chunk_dir=maps_dir / "chunks",
            split_map_paths={
                "terrain": maps_dir / "demo_map_terrain.obj",
                "structures": maps_dir / "demo_map_structures.obj",
                "rocks": maps_dir / "demo_map_rocks.obj",
            },
            internal_water_path=maps_dir / "internal_water.obj",
            landmarks_path=maps_dir / "landmarks.json",
            walkability_path=maps_dir / "walkability.pgm",
            heightmap_path=maps_dir / "heightmap.pgm",
            biome_map_path=maps_dir / "biomes.ppm",
            slope_map_path=maps_dir / "slope.pgm",
            moisture_map_path=maps_dir / "moisture.pgm",
            material_mask_paths={
                "sand": maps_dir / "material_sand.pgm",
                "grass": maps_dir / "material_grass.pgm",
                "rock": maps_dir / "material_rock.pgm",
                "volcanic": maps_dir / "material_volcanic.pgm",
                "wetness": maps_dir / "material_wetness.pgm",
            },
            terrain_report_path=maps_dir / "terrain_report.json",
            quality_report_path=maps_dir / "generation_quality.json",
            debug_preview_path=maps_dir / "debug_preview.html",
            scene_path=scripts_dir / "models.scene",
            scene_chunk_dir=scripts_dir / "chunks",
            gameplay_goals_path=scripts_dir / "gameplay_goals.json",
            world_config_path=maps_dir / "world_config.json",
        )

    def map_outputs(self) -> list[Path]:
        return [
            self.map_path,
            *self.split_map_paths.values(),
            *self.map_lod_paths,
            *sorted(self.terrain_chunk_dir.glob("*.obj")),
            self.internal_water_path,
            self.landmarks_path,
            self.world_config_path,
        ]

    def debug_outputs(self) -> list[Path]:
        return [
            self.walkability_path,
            self.heightmap_path,
            self.biome_map_path,
            self.slope_map_path,
            self.moisture_map_path,
            *self.material_mask_paths.values(),
            self.terrain_report_path,
            self.quality_report_path,
            self.debug_preview_path,
        ]

    def relative(self, path: Path) -> str:
        try:
            return path.relative_to(self.root).as_posix()
        except ValueError:
            return path.as_posix()
