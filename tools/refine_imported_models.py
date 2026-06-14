#!/usr/bin/env python3
"""Add small geometry bevels to imported OBJ props while preserving UVs/materials.

Run with:
    blender --background --python tools/refine_imported_models.py
"""

from __future__ import annotations

from pathlib import Path

import bpy


ROOT = Path(__file__).resolve().parent.parent
MODEL_DIR = ROOT / "assets" / "models" / "kenney"
REFINED_MARKER = "# Refined by tools/refine_imported_models.py"


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for mesh in list(bpy.data.meshes):
        bpy.data.meshes.remove(mesh)
    for material in list(bpy.data.materials):
        bpy.data.materials.remove(material)


def refine(path: Path) -> tuple[int, int]:
    if path.read_text(encoding="utf-8", errors="ignore").startswith(REFINED_MARKER):
        return 0, 0
    clear_scene()
    bpy.ops.wm.obj_import(filepath=str(path), forward_axis="NEGATIVE_Z", up_axis="Y")
    meshes = [obj for obj in bpy.context.selected_objects if obj.type == "MESH"]
    original_polygons = sum(len(obj.data.polygons) for obj in meshes)
    for obj in meshes:
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)
        diagonal = max(obj.dimensions.length, 0.01)
        bevel = obj.modifiers.new(name="DetailBevel", type="BEVEL")
        bevel.width = diagonal * 0.008
        bevel.segments = 2
        bevel.limit_method = "ANGLE"
        bevel.angle_limit = 0.45
        bpy.ops.object.modifier_apply(modifier=bevel.name)
        for polygon in obj.data.polygons:
            polygon.use_smooth = True

    bpy.ops.wm.obj_export(
        filepath=str(path),
        export_selected_objects=True,
        export_uv=True,
        export_normals=True,
        export_materials=True,
        apply_modifiers=True,
        export_triangulated_mesh=True,
        forward_axis="NEGATIVE_Z",
        up_axis="Y",
        path_mode="RELATIVE",
    )
    path.write_text(REFINED_MARKER + "\n" + path.read_text(encoding="utf-8"), encoding="utf-8")
    refined_polygons = sum(len(obj.data.polygons) for obj in meshes)
    return original_polygons, refined_polygons


def main() -> None:
    for path in sorted(MODEL_DIR.glob("*.obj")):
        before, after = refine(path)
        if before == 0:
            print(f"{path.name}: already refined")
        else:
            print(f"{path.name}: {before} -> {after} polygons")


if __name__ == "__main__":
    main()
