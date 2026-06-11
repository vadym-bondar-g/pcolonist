#!/usr/bin/env python3
"""Validate and import an OBJ model into pcolonist assets."""

from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path


MODEL_ID_PATTERN = re.compile(r"^[a-z][a-z0-9_]*$")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Validate an OBJ model, copy it into assets/models/custom, and register it."
    )
    parser.add_argument("obj", type=Path, help="Path to the source .obj file")
    parser.add_argument("--id", required=True, help="Model id: lowercase letters, digits, underscores")
    parser.add_argument(
        "--spawn",
        nargs=9,
        type=float,
        metavar=("PX", "PY", "PZ", "SX", "SY", "SZ", "HX", "HY", "HZ"),
        help="Also add one spawn_model entry: position, scale, collider half extents",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Replace an existing imported model with the same id",
    )
    return parser.parse_args()


def project_root() -> Path:
    return Path(__file__).resolve().parent.parent


def material_textures(path: Path) -> list[str]:
    textures: list[str] = []
    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        parts = raw_line.split()
        if parts and parts[0] == "map_Kd":
            if len(parts) != 2:
                raise ValueError(f"map_Kd path must not contain whitespace or options at {path}:{line_number}")
            if Path(parts[1]).suffix.lower() != ".png":
                raise ValueError(f"Only PNG diffuse textures are supported: {parts[1]}")
            textures.append(parts[1])
    return textures


def validate_obj(path: Path) -> tuple[list[str], list[str]]:
    if not path.is_file():
        raise ValueError(f"OBJ file does not exist: {path}")
    if path.suffix.lower() != ".obj":
        raise ValueError("Only .obj models are supported")
    if any(character.isspace() for character in path.name):
        raise ValueError("OBJ filename must not contain whitespace")

    vertex_count = 0
    face_count = 0
    material_libraries: list[str] = []
    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        parts = raw_line.split()
        if not parts or parts[0].startswith("#"):
            continue
        if parts[0] == "v":
            if len(parts) < 4:
                raise ValueError(f"Invalid vertex at line {line_number}")
            vertex_count += 1
        elif parts[0] == "f":
            if len(parts) < 4:
                raise ValueError(f"Face has fewer than three vertices at line {line_number}")
            face_count += 1
        elif parts[0] == "mtllib":
            if len(parts) != 2:
                raise ValueError(f"mtllib path must not contain whitespace at line {line_number}")
            material_libraries.append(parts[1])

    if vertex_count == 0 or face_count == 0:
        raise ValueError("OBJ must contain vertices and faces")

    textures: list[str] = []
    for library in material_libraries:
        library_path = path.parent / library
        if not library_path.is_file():
            raise ValueError(f"Referenced material library does not exist: {library_path}")
        if library_path.parent.resolve() != path.parent.resolve():
            raise ValueError("Material libraries must be next to the OBJ file")
        textures.extend(material_textures(library_path))

    for texture in textures:
        texture_path = path.parent / texture
        if not texture_path.is_file():
            raise ValueError(f"Referenced diffuse texture does not exist: {texture_path}")
        if texture_path.parent.resolve() != path.parent.resolve():
            raise ValueError("Diffuse textures must be next to the OBJ file")

    return material_libraries, textures


def register_model(scene_path: Path, model_id: str, asset_path: str, spawn: list[float] | None) -> None:
    lines = scene_path.read_text(encoding="utf-8").splitlines()
    declaration = f"model {model_id} {asset_path}"
    existing_declaration = next(
        (index for index, line in enumerate(lines) if line.split()[:2] == ["model", model_id]),
        None,
    )
    if existing_declaration is None:
        declarations = [index for index, line in enumerate(lines) if line.startswith("model ")]
        insertion = declarations[-1] + 1 if declarations else len(lines)
        lines.insert(insertion, declaration)
    else:
        lines[existing_declaration] = declaration

    if spawn is not None:
        values = " ".join(f"{value:g}" for value in spawn)
        lines.append(f"spawn_model {model_id} {values}")

    scene_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_arguments()
    if not MODEL_ID_PATTERN.fullmatch(args.id):
        raise ValueError("Model id must match: [a-z][a-z0-9_]*")

    source = args.obj.expanduser().resolve()
    material_libraries, textures = validate_obj(source)
    root = project_root()
    destination = root / "assets" / "models" / "custom" / args.id
    if destination.exists() and not args.force:
        raise ValueError(f"Model '{args.id}' already exists; use --force to replace it")
    if destination.exists():
        shutil.rmtree(destination)

    destination.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination / source.name)
    for library in material_libraries:
        shutil.copy2(source.parent / library, destination / library)
    for texture in textures:
        shutil.copy2(source.parent / texture, destination / texture)

    asset_path = f"models/custom/{args.id}/{source.name}"
    register_model(root / "assets" / "scripts" / "models.scene", args.id, asset_path, args.spawn)
    print(f"Imported '{args.id}' as {asset_path}")
    if args.spawn is None:
        print("Add a spawn_model entry to assets/scripts/models.scene to place it in the world.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, UnicodeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
