"""Command-line options for the procedural island generator."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence


@dataclass(frozen=True)
class RuntimeOptions:
    seed: int
    grid: int
    output_dir: Path
    only: str


def grid_size(value: str) -> int:
    size = int(value)
    if size < 9:
        raise argparse.ArgumentTypeError("grid must be at least 9")
    return size


def parse_args(argv: Sequence[str] | None, default_root: Path, default_seed: int, default_grid: int) -> RuntimeOptions:
    parser = argparse.ArgumentParser(
        description="Generate the mysterious island map assets and scenery placement script.",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=default_seed,
        help=f"deterministic terrain/decor seed (default: {default_seed})",
    )
    parser.add_argument(
        "--grid",
        type=grid_size,
        default=default_grid,
        help=f"terrain vertices per side for the full-resolution mesh (default: {default_grid})",
    )
    parser.add_argument(
        "--only",
        choices=("all", "map", "scene", "debug"),
        default="all",
        help="limit generation to map meshes, scene placement, debug outputs, or everything",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=default_root,
        help="root directory that receives assets/maps and assets/scripts outputs",
    )
    args = parser.parse_args(argv)
    return RuntimeOptions(
        seed=args.seed,
        grid=args.grid,
        output_dir=args.output_dir.resolve(),
        only=args.only,
    )
