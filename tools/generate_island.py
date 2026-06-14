#!/usr/bin/env python3
"""Generate the mysterious island map and its scenery placement script."""

from __future__ import annotations

import math
import random
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
MAP_PATH = ROOT / "assets" / "maps" / "demo_map.obj"
SCENE_PATH = ROOT / "assets" / "scripts" / "models.scene"
SEED = 1847
GRID = 121
SPACING = 8.0 / 3.0


def noise(x: float, z: float) -> float:
    return (
        math.sin(x * 0.13 + z * 0.07) * 0.48
        + math.sin(x * 0.31 - z * 0.19) * 0.24
        + math.cos(x * 0.08 + z * 0.27) * 0.18
    )


def gaussian(x: float, z: float, cx: float, cz: float, radius: float, height: float) -> float:
    distance = ((x - cx) ** 2 + (z - cz) ** 2) / (radius * radius)
    return math.exp(-distance * 2.5) * height


def terrain_height(x: float, z: float) -> float:
    radius = math.sqrt((x / 69.0) ** 2 + (z / 61.0) ** 2)
    coast = max(0.0, radius - 0.72)
    height = noise(x, z) * 0.42 - coast * coast * 15.0
    height += gaussian(x, z, -38.0, -24.0, 27.0, 8.0)
    height += gaussian(x, z, -48.0, 25.0, 18.0, 4.5)
    height += gaussian(x, z, 43.0, -34.0, 20.0, 5.5)
    height -= gaussian(x, z, 34.0, 25.0, 24.0, 1.5)

    # Keep the arrival clearing and paths physically compatible with the flat
    # ground collision used by the current engine.
    clearing = math.exp(-((x / 18.0) ** 2 + ((z - 8.0) / 16.0) ** 2) * 2.0)
    path = math.exp(-((x + 0.12 * z) / 5.0) ** 2) * math.exp(-((z + 8.0) / 60.0) ** 2)
    temple_clearing = math.exp(-(((x / 17.0) ** 2) + (((z + 42.0) / 15.0) ** 2)) * 2.0)
    circle_clearing = math.exp(-((((x + 43.0) / 17.0) ** 2) + (((z - 19.0) / 17.0) ** 2)) * 2.0)
    tower_clearing = math.exp(-((((x - 45.0) / 14.0) ** 2) + (((z - 10.0) / 14.0) ** 2)) * 2.0)
    height *= 1.0 - max(clearing, path, temple_clearing, circle_clearing, tower_clearing) * 0.96
    return max(-6.0, height)


def terrain_color(x: float, y: float, z: float) -> tuple[float, float, float]:
    radius = math.sqrt((x / 69.0) ** 2 + (z / 61.0) ** 2)
    if radius > 0.82 or y < -0.5:
        return 0.52, 0.43, 0.25
    if x > 18.0 and z > 7.0:
        return 0.18, 0.27, 0.2
    if y > 3.0:
        return 0.25, 0.29, 0.25
    variation = noise(x + 12.0, z - 4.0) * 0.035
    return 0.21 + variation, 0.34 + variation, 0.19 + variation * 0.5


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


def generate_map() -> None:
    vertices: list[tuple[float, float, float, float, float, float]] = []
    faces: list[tuple[int, ...]] = []
    half = (GRID - 1) * SPACING * 0.5
    for row in range(GRID):
        z = row * SPACING - half
        for column in range(GRID):
            x = column * SPACING - half
            y = terrain_height(x, z)
            vertices.append((x, y, z, *terrain_color(x, y, z)))

    for row in range(GRID - 1):
        for column in range(GRID - 1):
            a = row * GRID + column + 1
            faces.append((a, a + GRID, a + GRID + 1, a + 1))

    stone = (0.27, 0.31, 0.3)
    dark_stone = (0.15, 0.2, 0.21)
    glow_stone = (0.18, 0.48, 0.5)

    # Sunken temple in the northern valley.
    add_box(vertices, faces, (0.0, 0.45, -42.0), (18.0, 0.9, 14.0), dark_stone)
    for x in (-8.0, 8.0):
        for z in (-48.0, -36.0):
            add_box(vertices, faces, (x, 3.0, z), (2.0, 6.0, 2.0), stone)
    add_box(vertices, faces, (0.0, 4.0, -48.0), (18.0, 1.2, 2.0), stone)
    add_box(vertices, faces, (0.0, 2.6, -42.0), (1.2, 5.2, 1.2), glow_stone)

    # Broken watchtower overlooking the eastern bay.
    add_box(vertices, faces, (45.0, 2.5, 10.0), (8.0, 5.0, 8.0), dark_stone)
    add_box(vertices, faces, (45.0, 6.2, 10.0), (10.0, 1.0, 10.0), stone)
    add_box(vertices, faces, (42.0, 9.0, 7.0), (1.5, 5.0, 1.5), glow_stone)

    # Western standing-stone circle.
    for index in range(9):
        angle = index / 9.0 * math.tau
        x = -43.0 + math.cos(angle) * 11.0
        z = 19.0 + math.sin(angle) * 11.0
        add_box(vertices, faces, (x, 2.3, z), (1.3, 4.6 + (index % 3) * 0.7, 1.3), stone)
    add_box(vertices, faces, (-43.0, 0.35, 19.0), (8.0, 0.7, 8.0), glow_stone)

    lines = [
        "# Generated by tools/generate_island.py. Do not edit by hand.",
        "o mysterious_island",
        "",
    ]
    lines.extend("v {:.3f} {:.3f} {:.3f} {:.3f} {:.3f} {:.3f}".format(*vertex) for vertex in vertices)
    lines.append("")
    lines.extend("f " + " ".join(str(index) for index in face) for face in faces)
    MAP_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


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
        "",
        "# Arrival camp.",
    ]
    spawn(lines, "tent", -7.0, 12.0, 0.9, (1.3, 1.2, 1.3))
    spawn(lines, "campfire", -2.0, 11.0, 1.5, (0.6, 0.3, 0.6))
    spawn(lines, "fallen_trunk", 6.0, 14.0, 1.2, (1.5, 0.6, 0.8))
    spawn(lines, "fence", -11.0, 15.0, 1.0, (0.3, 0.7, 1.6))
    spawn(lines, "fence", -8.0, 15.0, 1.0, (0.3, 0.7, 1.6))

    lines.extend(("", "# Dense forest, with clearings around landmarks and the main trail."))
    for _ in range(62):
        for _attempt in range(100):
            x = random.uniform(-58.0, 58.0)
            z = random.uniform(-52.0, 52.0)
            radius = math.sqrt((x / 65.0) ** 2 + (z / 58.0) ** 2)
            near_path = abs(x + 0.12 * z) < 7.0
            near_landmark = min(
                math.hypot(x, z + 42.0),
                math.hypot(x + 43.0, z - 19.0),
                math.hypot(x - 45.0, z - 10.0),
                math.hypot(x, z - 10.0),
            ) < 15.0
            if radius < 0.88 and not near_path and not near_landmark:
                break
        model = "oak" if random.random() < 0.42 else "tree"
        scale = random.uniform(0.75, 1.35)
        spawn(lines, model, x, z, scale, (0.78, 2.7, 0.78), random.uniform(0.0, math.tau))

    lines.extend(("", "# Low understory creates depth between the larger trees."))
    for _ in range(52):
        for _attempt in range(100):
            x = random.uniform(-58.0, 58.0)
            z = random.uniform(-52.0, 52.0)
            radius = math.sqrt((x / 65.0) ** 2 + (z / 58.0) ** 2)
            near_path = abs(x + 0.12 * z) < 5.0
            if radius < 0.9 and not near_path and terrain_height(x, z) > -0.8:
                break
        spawn_decor(lines, "bush", x, z, random.uniform(0.55, 1.25), random.uniform(0.0, math.tau))

    lines.extend(("", "# Rocks mark the coast and dangerous approaches."))
    for _ in range(34):
        angle = random.uniform(0.0, math.tau)
        radius = random.uniform(46.0, 63.0)
        x = math.cos(angle) * radius
        z = math.sin(angle) * radius * 0.84
        scale = random.uniform(0.7, 1.8)
        spawn(lines, "rock", x, z, scale, (1.3, 0.9, 1.1), random.uniform(0.0, math.tau))

    lines.extend(("", "# Mushrooms gather in the eastern marsh."))
    for _ in range(16):
        x = random.uniform(24.0, 49.0)
        z = random.uniform(16.0, 39.0)
        spawn(lines, "mushroom", x, z, random.uniform(0.8, 1.8), (0.35, 0.45, 0.35))

    SCENE_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    generate_map()
    generate_scene()
    print(f"Generated {MAP_PATH.relative_to(ROOT)} and {SCENE_PATH.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
