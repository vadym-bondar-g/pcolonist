#!/usr/bin/env python3
"""Generate the mysterious island map and its scenery placement script."""

from __future__ import annotations

import math
import random
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
MAP_PATH = ROOT / "assets" / "maps" / "demo_map.obj"
SCENE_PATH = ROOT / "assets" / "scripts" / "models.scene"
SEED = 1847
GRID = 181
SPACING = 8.0 / 3.0


@dataclass(frozen=True)
class TerrainConfig:
    island_radius_x: float = 132.0
    island_radius_z: float = 112.0
    coast_start: float = 0.76
    floor_height: float = -8.0
    terrace_step: float = 1.6
    terrace_strength: float = 0.62


TERRAIN = TerrainConfig()
GROTTOS = (
    (-72.0, -54.0),
    (65.0, 70.0),
    (-91.0, 67.0),
)


def smoothstep(edge0: float, edge1: float, value: float) -> float:
    value = max(0.0, min(1.0, (value - edge0) / (edge1 - edge0)))
    return value * value * (3.0 - 2.0 * value)


def noise(x: float, z: float) -> float:
    return (
        math.sin(x * 0.13 + z * 0.07) * 0.48
        + math.sin(x * 0.31 - z * 0.19) * 0.24
        + math.cos(x * 0.08 + z * 0.27) * 0.18
    )


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


def island_radius(x: float, z: float) -> float:
    angle = math.atan2(z / TERRAIN.island_radius_z, x / TERRAIN.island_radius_x)
    shoreline = 1.0 + math.sin(angle * 3.0 + 0.4) * 0.08 + math.sin(angle * 7.0 - 0.7) * 0.035
    eastern_bay = gaussian(x, z, 116.0, 18.0, 34.0, 30.0, 0.16)
    southern_peninsula = gaussian(x, z, -25.0, 104.0, 42.0, 34.0, 0.12)
    return math.hypot(x / TERRAIN.island_radius_x, z / TERRAIN.island_radius_z) / (
        shoreline + southern_peninsula - eastern_bay
    )


def flattening_profile(x: float, z: float) -> tuple[float, float]:
    clearing = math.exp(-(((x + 25.0) / 22.0) ** 2 + ((z - 75.0) / 22.0) ** 2) * 0.9)
    path = math.exp(-((x + 0.15 * z + 14.0) / 5.0) ** 2) * math.exp(-((z - 10.0) / 95.0) ** 2)
    temple_clearing = math.exp(-(((x / 20.0) ** 2) + (((z + 84.0) / 18.0) ** 2)) * 2.0)
    circle_clearing = math.exp(-((((x + 82.0) / 20.0) ** 2) + (((z - 38.0) / 20.0) ** 2)) * 2.0)
    tower_clearing = math.exp(-((((x - 91.0) / 17.0) ** 2) + (((z - 18.0) / 17.0) ** 2)) * 2.0)
    grotto_profiles = tuple(
        (math.exp(-((((x - gx) / 13.0) ** 2) + (((z - gz) / 17.0) ** 2)) * 1.7), raw_terrain_height(gx, gz))
        for gx, gz in GROTTOS
    )
    profiles = (
        (clearing, 0.7),
        (path * 0.72, 0.3),
        (temple_clearing, 0.35),
        (circle_clearing, 0.45),
        (tower_clearing, 0.35),
        *grotto_profiles,
    )
    return max(profiles, key=lambda profile: profile[0])


def raw_terrain_height(x: float, z: float) -> float:
    radius = island_radius(x, z)
    coast = max(0.0, radius - TERRAIN.coast_start)
    height = noise(x, z) * 0.9 - coast * coast * 26.0

    # A volcanic western massif, northern plateau and long central ridge divide the island.
    height += gaussian(x, z, -42.0, -27.0, 48.0, 42.0, 18.0)
    height += gaussian(x, z, -54.0, -34.0, 22.0, 20.0, 10.0)
    height += gaussian(x, z, -80.0, 48.0, 32.0, 27.0, 9.5)
    height += gaussian(x, z, 47.0, -66.0, 42.0, 27.0, 10.5)
    height += gaussian(x, z, 22.0, -18.0, 78.0, 15.0, 5.0)

    # The central volcano dominates the island; a deep summit crater holds magma.
    height += gaussian(x, z, 0.0, 0.0, 48.0, 48.0, 38.0)
    height -= gaussian(x, z, 0.0, 0.0, 15.0, 15.0, 25.0)

    # Low valleys form natural travel corridors and an eastern marsh/bay.
    height -= gaussian(x, z, 27.0, 35.0, 54.0, 42.0, 3.1)
    height -= gaussian(x, z, 2.0, 7.0, 24.0, 70.0, 1.8)
    height -= gaussian(x, z, 91.0, 18.0, 34.0, 28.0, 2.8)

    # An inland lake drains through a narrow river valley into the eastern bay.
    height -= gaussian(x, z, 31.0, -24.0, 23.0, 18.0, 5.2)
    for river_x, river_z in ((48.0, -20.0), (63.0, -12.0), (77.0, -3.0), (91.0, 8.0)):
        height -= gaussian(x, z, river_x, river_z, 21.0, 7.0, 2.4)
    return height


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


def terrain_color(x: float, y: float, z: float) -> tuple[float, float, float]:
    radius = island_radius(x, z)
    if radius > 0.82 or y < -0.5:
        return 0.52, 0.43, 0.25
    if math.hypot(x, z) < 48.0 and y > 8.0:
        ash = max(0.0, min(1.0, (y - 8.0) / 18.0))
        return 0.22 + ash * 0.08, 0.20 + ash * 0.06, 0.18 + ash * 0.04
    if x > 45.0 and z > 5.0:
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
            b = a + 1
            c = a + GRID
            d = c + 1
            if (row + column) % 2 == 0:
                faces.extend(((a, c, d), (a, d, b)))
            else:
                faces.extend(((a, c, b), (b, c, d)))

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

    # Granite refuge built into the western cliffs.
    add_box_on_ground(vertices, faces, -102.0, -10.0, (18.0, 0.8, 13.0), dark_stone)
    for x in (-108.0, -96.0):
        add_box_on_ground(vertices, faces, x, -14.0, (2.0, 5.0, 2.0), stone)
    add_box_on_ground(vertices, faces, -102.0, -16.0, (16.0, 1.0, 2.0), stone, 4.4)

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
        "",
        "# Arrival camp.",
    ]
    spawn(lines, "tent", -32.0, 77.0, 0.9, (1.3, 1.2, 1.3))
    spawn(lines, "campfire", -27.0, 76.0, 1.5, (0.6, 0.3, 0.6))
    spawn(lines, "fallen_trunk", -19.0, 79.0, 1.2, (1.5, 0.6, 0.8))
    spawn(lines, "fence", -36.0, 80.0, 1.0, (0.3, 0.7, 1.6))
    spawn(lines, "fence", -33.0, 80.0, 1.0, (0.3, 0.7, 1.6))

    lines.extend(("", "# Walkable stone grottos with open entrances and physical walls."))
    for grotto_x, grotto_z in GROTTOS:
        ground = terrain_height(grotto_x, grotto_z)
        spawn_decor(lines, "grotto", grotto_x, grotto_z, 1.0)
        spawn_collider(lines, (grotto_x - 3.75, ground + 2.0, grotto_z), (0.65, 2.0, 5.0))
        spawn_collider(lines, (grotto_x + 3.75, ground + 2.0, grotto_z), (0.65, 2.0, 5.0))
        spawn_collider(lines, (grotto_x, ground + 4.0, grotto_z), (3.1, 0.45, 5.0))

    lines.extend(("", "# Dense forest, with clearings around landmarks and the main trail."))
    for _ in range(150):
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
                *(math.hypot(x - gx, z - gz) for gx, gz in GROTTOS),
            ) < 18.0
            if radius < 0.88 and not near_path and not near_landmark and terrain_height(x, z) > -0.1:
                break
        model = "oak" if random.random() < 0.42 else "tree"
        scale = random.uniform(0.75, 1.35)
        spawn(lines, model, x, z, scale, (0.78, 2.7, 0.78), random.uniform(0.0, math.tau))

    lines.extend(("", "# Low understory creates depth between the larger trees."))
    for _ in range(125):
        for _attempt in range(100):
            x = random.uniform(-116.0, 116.0)
            z = random.uniform(-98.0, 104.0)
            radius = island_radius(x, z)
            near_path = abs(x + 0.15 * z + 14.0) < 5.0
            near_grotto = min(math.hypot(x - gx, z - gz) for gx, gz in GROTTOS) < 14.0
            if radius < 0.9 and not near_path and not near_grotto and terrain_height(x, z) > -0.8:
                break
        spawn_decor(lines, "bush", x, z, random.uniform(0.55, 1.25), random.uniform(0.0, math.tau))

    lines.extend(("", "# Rocks mark the coast and dangerous approaches."))
    for _ in range(68):
        angle = random.uniform(0.0, math.tau)
        radius = random.uniform(98.0, 126.0)
        x = math.cos(angle) * radius
        z = math.sin(angle) * radius * 0.86
        scale = random.uniform(0.7, 1.8)
        spawn(lines, "rock", x, z, scale, (1.3, 0.9, 1.1), random.uniform(0.0, math.tau))

    lines.extend(("", "# Mushrooms gather in the eastern marsh."))
    for _ in range(32):
        x = random.uniform(58.0, 103.0)
        z = random.uniform(22.0, 65.0)
        spawn(lines, "mushroom", x, z, random.uniform(0.8, 1.8), (0.35, 0.45, 0.35))

    SCENE_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    generate_map()
    generate_scene()
    print(f"Generated {MAP_PATH.relative_to(ROOT)} and {SCENE_PATH.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
