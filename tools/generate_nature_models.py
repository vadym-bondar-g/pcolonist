#!/usr/bin/env python3
"""Generate deterministic low-poly nature models used by the demo world."""

from __future__ import annotations

import math
import random
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
MODEL_DIR = ROOT / "assets" / "models"


class Obj:
    def __init__(self, name: str, material_library: str) -> None:
        self.lines = [f"mtllib {material_library}", f"o {name}"]
        self.vertex_count = 0

    def material(self, name: str) -> None:
        self.lines.append(f"usemtl {name}")

    def vertex(self, x: float, y: float, z: float) -> int:
        self.vertex_count += 1
        self.lines.append(f"v {x:.4f} {y:.4f} {z:.4f}")
        return self.vertex_count

    def face(self, *indices: int) -> None:
        self.lines.append("f " + " ".join(map(str, indices)))

    def tapered_cylinder(
        self,
        center: tuple[float, float, float],
        bottom_radius: float,
        top_radius: float,
        height: float,
        sides: int,
        phase: float = 0.0,
        segments: int = 4,
    ) -> None:
        cx, cy, cz = center
        rings: list[list[int]] = []
        for segment in range(segments + 1):
            progress = segment / segments
            radius = bottom_radius + (top_radius - bottom_radius) * progress
            ring: list[int] = []
            for index in range(sides):
                angle = phase + index / sides * math.tau
                variation = 1.0 + math.sin(index * 1.73 + segment * 0.62 + phase) * 0.025
                ring.append(self.vertex(
                    cx + math.cos(angle) * radius * variation,
                    cy + height * progress,
                    cz + math.sin(angle) * radius * variation,
                ))
            rings.append(ring)
        for lower, upper in zip(rings, rings[1:]):
            for index in range(sides):
                next_index = (index + 1) % sides
                self.face(lower[index], lower[next_index], upper[next_index], upper[index])
        self.face(*reversed(rings[0]))
        self.face(*rings[-1])

    def crown(
        self,
        center: tuple[float, float, float],
        radius: float,
        height: float,
        sides: int,
        phase: float,
        ring_count: int = 6,
    ) -> None:
        cx, cy, cz = center
        rings: list[list[int]] = []
        for ring_index in range(ring_count):
            progress = (ring_index + 1) / (ring_count + 1)
            vertical = -0.27 + progress * 1.27
            profile = math.sin(progress * math.pi) ** 0.62
            ring: list[int] = []
            for index in range(sides):
                angle = phase + index / sides * math.tau
                variation = 0.92 + 0.08 * math.sin(index * 2.31 + ring_index * 0.83 + phase)
                ring.append(self.vertex(
                    cx + math.cos(angle) * radius * profile * variation,
                    cy + height * vertical,
                    cz + math.sin(angle) * radius * profile * variation,
                ))
            rings.append(ring)
        top = self.vertex(cx, cy + height * 1.04, cz)
        bottom = self.vertex(cx, cy - height * 0.32, cz)
        for index in range(sides):
            next_index = (index + 1) % sides
            self.face(bottom, rings[0][next_index], rings[0][index])
        for lower, upper in zip(rings, rings[1:]):
            for index in range(sides):
                next_index = (index + 1) % sides
                self.face(lower[index], lower[next_index], upper[next_index], upper[index])
        for index in range(sides):
            next_index = (index + 1) % sides
            self.face(rings[-1][index], rings[-1][next_index], top)

    def write(self, path: Path) -> None:
        path.write_text("\n".join(self.lines) + "\n", encoding="utf-8")


def generate_tree() -> None:
    obj = Obj("layered_tree", "tree.mtl")
    obj.material("Bark")
    obj.tapered_cylinder((0.0, 0.0, 0.0), 0.46, 0.25, 3.55, 28, 0.18, 7)
    for angle, y, length in ((0.4, 2.0, 1.3), (2.5, 2.4, 1.15), (4.6, 2.75, 1.0)):
        x = math.cos(angle) * length * 0.34
        z = math.sin(angle) * length * 0.34
        obj.tapered_cylinder((x, y, z), 0.18, 0.07, length, 16, angle, 4)
    obj.material("LeafDark")
    obj.crown((0.0, 2.55, 0.0), 1.72, 2.0, 28, 0.12, 8)
    obj.crown((-0.75, 2.4, 0.48), 1.05, 1.45, 22, 0.46, 7)
    obj.crown((0.35, 2.35, 0.82), 0.88, 1.25, 20, 0.22, 7)
    obj.material("LeafLight")
    obj.crown((0.68, 3.0, -0.36), 1.18, 1.62, 24, 0.78, 7)
    obj.crown((-0.18, 3.65, 0.12), 1.02, 1.5, 24, 0.25, 7)
    obj.crown((-0.62, 3.35, -0.48), 0.82, 1.15, 20, 0.55, 6)
    obj.write(MODEL_DIR / "tree.obj")
    (MODEL_DIR / "tree.mtl").write_text(
        "newmtl Bark\nKd 0.24 0.12 0.055\nKs 0.05 0.05 0.05\nNs 8\n"
        "newmtl LeafDark\nKd 0.075 0.25 0.07\nKs 0.03 0.03 0.03\nNs 5\n"
        "newmtl LeafLight\nKd 0.16 0.42 0.10\nKs 0.04 0.04 0.04\nNs 7\n",
        encoding="utf-8",
    )


def generate_bush() -> None:
    obj = Obj("bush", "bush.mtl")
    obj.material("BushDark")
    obj.crown((-0.35, 0.35, 0.1), 0.82, 0.95, 22, 0.3, 7)
    obj.crown((0.38, 0.28, -0.2), 0.7, 0.84, 22, 0.7, 7)
    obj.material("BushLight")
    obj.crown((0.02, 0.55, 0.28), 0.72, 0.92, 24, 0.1, 7)
    obj.crown((0.18, 0.42, -0.52), 0.55, 0.70, 20, 0.48, 6)
    obj.write(MODEL_DIR / "bush.obj")
    (MODEL_DIR / "bush.mtl").write_text(
        "newmtl BushDark\nKd 0.07 0.22 0.06\nKs 0.02 0.02 0.02\nNs 4\n"
        "newmtl BushLight\nKd 0.18 0.38 0.09\nKs 0.03 0.03 0.03\nNs 5\n",
        encoding="utf-8",
    )


def generate_rock() -> None:
    random.seed(912)
    obj = Obj("faceted_rock", "rock.mtl")
    obj.material("Rock")
    rings: list[list[int]] = []
    ring_count = 32
    for level, (y, radius) in enumerate((
        (0.0, 1.25),
        (0.24, 1.28),
        (0.48, 1.20),
        (0.75, 1.10),
        (1.02, 0.91),
        (1.28, 0.68),
        (1.50, 0.38),
    )):
        ring: list[int] = []
        for index in range(ring_count):
            angle = index / ring_count * math.tau + level * 0.13
            variation = random.uniform(0.82, 1.16)
            ring.append(obj.vertex(math.cos(angle) * radius * variation, y, math.sin(angle) * radius * variation))
        rings.append(ring)
    top = obj.vertex(0.08, 1.72, -0.05)
    for lower, upper in zip(rings, rings[1:]):
        for index in range(ring_count):
            next_index = (index + 1) % ring_count
            obj.face(lower[index], lower[next_index], upper[next_index], upper[index])
    for index in range(ring_count):
        obj.face(rings[-1][index], rings[-1][(index + 1) % ring_count], top)
    obj.face(*reversed(rings[0]))
    obj.write(MODEL_DIR / "rock.obj")
    (MODEL_DIR / "rock.mtl").write_text(
        "newmtl Rock\nKd 0.29 0.31 0.28\nKs 0.08 0.08 0.08\nNs 18\n",
        encoding="utf-8",
    )


def main() -> None:
    generate_tree()
    generate_bush()
    generate_rock()
    print("Generated tree.obj, bush.obj and rock.obj")


if __name__ == "__main__":
    main()
