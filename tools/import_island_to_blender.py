#!/usr/bin/env python3
"""Import the generated pcolonist island into a Blender scene.

Run with:
    blender --background --python tools/import_island_to_blender.py -- --output /tmp/pcolonist_island.blend

Or open Blender normally and run the script from the Text editor.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import sys
from dataclasses import dataclass
from pathlib import Path


try:
    SCRIPT_PATH = Path(__file__).resolve()
except NameError:
    SCRIPT_PATH = Path.cwd() / "tools" / "import_island_to_blender.py"
ROOT = SCRIPT_PATH.parent.parent
ASSET_SENTINEL = Path("assets") / "maps" / "demo_map_terrain.obj"

try:
    import bpy
except ModuleNotFoundError:
    if "--help" in sys.argv or "-h" in sys.argv:
        bpy = None
    else:
        raise SystemExit("This script must be run by Blender: blender --python tools/import_island_to_blender.py -- [options]")


@dataclass(frozen=True)
class Spawn:
    model_id: str
    position: tuple[float, float, float]
    scale: tuple[float, float, float]
    yaw: float


@dataclass
class ImportStats:
    map_objects: int = 0
    prop_templates: int = 0
    prop_instances: int = 0


def parse_args() -> argparse.Namespace:
    if "--" in sys.argv:
        argv = sys.argv[sys.argv.index("--") + 1 :]
    elif bpy is None:
        argv = sys.argv[1:]
    else:
        argv = []
    parser = argparse.ArgumentParser(description="Build a Blender review scene from generated island assets.")
    parser.add_argument("--root", type=Path, default=ROOT, help="project or generated output root")
    parser.add_argument("--output", type=Path, default=None, help="optional .blend file to save")
    parser.add_argument("--prop-limit", type=int, default=0, help="maximum prop spawns to import; 0 imports all")
    parser.add_argument("--skip-props", action="store_true", help="import terrain, structures and water only")
    parser.add_argument("--full-map", action="store_true", help="import demo_map.obj instead of split terrain/structure/rock meshes")
    return parser.parse_args(argv)


def has_generated_assets(path: Path) -> bool:
    return (path / ASSET_SENTINEL).exists()


def candidate_roots(path: Path) -> list[Path]:
    candidates: list[Path] = []

    def append(candidate: Path | None) -> None:
        if candidate is None:
            return
        try:
            resolved = candidate.expanduser().resolve()
        except OSError:
            return
        if resolved not in candidates:
            candidates.append(resolved)

    def append_with_parents(candidate: Path | None) -> None:
        if candidate is None:
            return
        append(candidate)
        append(candidate.parent)
        append(candidate.parent.parent)
        for parent in candidate.parents:
            append(parent)

    append_with_parents(path)
    append_with_parents(ROOT)
    append_with_parents(Path.cwd())

    for variable in ("PCOLONIST_ROOT", "PCOLONIST_PROJECT_ROOT"):
        value = os.environ.get(variable)
        if value:
            append_with_parents(Path(value))

    if bpy is not None:
        for text in bpy.data.texts:
            if text.filepath:
                append_with_parents(Path(text.filepath))

    append_with_parents(Path.home() / "CLionProjects" / "pcolonist")
    append_with_parents(Path("/home/usr/CLionProjects/pcolonist"))
    return candidates


def project_root(path: Path) -> Path:
    for candidate in candidate_roots(path):
        if has_generated_assets(candidate):
            return candidate
    raise FileNotFoundError(
        "Cannot find generated island assets. Pass the project root explicitly, for example: "
        "--root /home/usr/CLionProjects/pcolonist"
    )


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()
    for mesh in list(bpy.data.meshes):
        bpy.data.meshes.remove(mesh)
    for material in list(bpy.data.materials):
        bpy.data.materials.remove(material)
    for collection in list(bpy.data.collections):
        bpy.data.collections.remove(collection)


def collection(name: str) -> bpy.types.Collection:
    existing = bpy.data.collections.get(name)
    if existing is not None:
        return existing
    created = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(created)
    return created


def import_obj(path: Path, target: bpy.types.Collection, name: str) -> list[bpy.types.Object]:
    before = set(bpy.context.scene.objects)
    if hasattr(bpy.ops.wm, "obj_import"):
        bpy.ops.wm.obj_import(filepath=str(path), forward_axis="NEGATIVE_Z", up_axis="Y")
    else:
        bpy.ops.import_scene.obj(filepath=str(path), axis_forward="-Z", axis_up="Y")
    imported = [obj for obj in bpy.context.scene.objects if obj not in before]
    for index, obj in enumerate(imported):
        obj.name = name if len(imported) == 1 else f"{name}_{index + 1}"
        for source in list(obj.users_collection):
            source.objects.unlink(obj)
        target.objects.link(obj)
        if obj.type == "MESH":
            obj.data.name = obj.name
            for polygon in obj.data.polygons:
                polygon.use_smooth = True
    return imported


def engine_location(position: tuple[float, float, float]) -> tuple[float, float, float]:
    x, y, z = position
    return x, -z, y


def parse_scene(path: Path) -> tuple[dict[str, Path], list[Spawn]]:
    models: dict[str, Path] = {}
    spawns: list[Spawn] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        tokens = line.split()
        if tokens[0] == "model" and len(tokens) >= 3:
            models[tokens[1]] = Path(tokens[2])
        elif tokens[0] in {"spawn_model", "spawn_decor"} and len(tokens) >= 8:
            yaw = float(tokens[11] if tokens[0] == "spawn_model" and len(tokens) >= 12 else tokens[8] if len(tokens) >= 9 else 0.0)
            spawns.append(
                Spawn(
                    model_id=tokens[1],
                    position=(float(tokens[2]), float(tokens[3]), float(tokens[4])),
                    scale=(float(tokens[5]), float(tokens[6]), float(tokens[7])),
                    yaw=yaw,
                )
            )
    return models, spawns


def add_ocean(root: Path) -> int:
    config_path = root / "assets" / "maps" / "world_config.json"
    if not config_path.exists():
        return 0
    world_config = json.loads(config_path.read_text(encoding="utf-8"))
    ocean = world_config.get("water", {}).get("ocean", {})
    half = ocean.get("half_extents", {})
    position = ocean.get("position", {})
    size = max(float(half.get("x", 960.0)), float(half.get("z", 960.0))) * 2.0
    bpy.ops.mesh.primitive_plane_add(size=size, location=engine_location((0.0, float(position.get("y", 0.0)), 0.0)))
    plane = bpy.context.object
    plane.name = "ocean_review_plane"
    material = bpy.data.materials.new("ocean_review")
    material.diffuse_color = (0.02, 0.22, 0.36, 0.34)
    plane.data.materials.append(material)
    water = collection("Water")
    for source in list(plane.users_collection):
        source.objects.unlink(plane)
    water.objects.link(plane)
    return 1


def import_static_map(root: Path, use_full_map: bool) -> int:
    maps = root / "assets" / "maps"
    static = collection("Island")
    water = collection("Water")
    imported_count = 0
    if use_full_map:
        imported_count += len(import_obj(maps / "demo_map.obj", static, "full_generated_island"))
    else:
        for filename, name in (
            ("demo_map_terrain.obj", "terrain"),
            ("demo_map_structures.obj", "structures"),
            ("demo_map_rocks.obj", "rocks"),
        ):
            path = maps / filename
            if path.exists():
                imported_count += len(import_obj(path, static, name))
    internal_water = maps / "internal_water.obj"
    if internal_water.exists():
        imported_count += len(import_obj(internal_water, water, "lake_and_river"))
    imported_count += add_ocean(root)
    return imported_count


def build_prop_scene(root: Path, prop_limit: int) -> tuple[int, int]:
    scene_path = root / "assets" / "scripts" / "models.scene"
    if not scene_path.exists():
        return 0, 0
    model_paths, spawns = parse_scene(scene_path)
    if prop_limit > 0:
        spawns = spawns[:prop_limit]

    templates = collection("PropTemplates")
    templates.hide_viewport = True
    props = collection("Props")
    loaded: dict[str, bpy.types.Collection] = {}
    for model_id, relative in sorted(model_paths.items()):
        path = root / "assets" / relative
        if not path.exists():
            continue
        model_collection = bpy.data.collections.new(f"template_{model_id}")
        templates.children.link(model_collection)
        import_obj(path, model_collection, model_id)
        loaded[model_id] = model_collection

    instance_count = 0
    for index, spawn in enumerate(spawns, start=1):
        source = loaded.get(spawn.model_id)
        if source is None:
            continue
        instance = bpy.data.objects.new(f"{spawn.model_id}_{index:04d}", None)
        instance.instance_type = "COLLECTION"
        instance.instance_collection = source
        instance.location = engine_location(spawn.position)
        instance.rotation_euler = (0.0, 0.0, -spawn.yaw)
        instance.scale = spawn.scale
        props.objects.link(instance)
        instance_count += 1
    return len(loaded), instance_count


def add_review_camera() -> None:
    bpy.ops.object.light_add(type="SUN", location=(0.0, 0.0, 180.0))
    bpy.context.object.name = "review_sun"
    bpy.context.object.data.energy = 3.0
    bpy.ops.object.camera_add(location=(0.0, -1050.0, 520.0), rotation=(math.radians(62.0), 0.0, 0.0))
    bpy.context.scene.camera = bpy.context.object
    bpy.context.scene.render.resolution_x = 1920
    bpy.context.scene.render.resolution_y = 1080
    bpy.context.scene.view_settings.view_transform = "Filmic"
    bpy.context.scene.view_settings.look = "Medium High Contrast"


def main() -> None:
    args = parse_args()
    root = project_root(args.root)
    stats = ImportStats()
    clear_scene()
    stats.map_objects = import_static_map(root, args.full_map)
    if not args.skip_props:
        stats.prop_templates, stats.prop_instances = build_prop_scene(root, args.prop_limit)
    add_review_camera()
    if args.output is not None:
        bpy.ops.wm.save_as_mainfile(filepath=str(args.output.resolve()))
    print(
        "Imported pcolonist island review scene "
        f"from {root}: map_objects={stats.map_objects}, "
        f"prop_templates={stats.prop_templates}, prop_instances={stats.prop_instances}"
    )


if __name__ == "__main__":
    main()
