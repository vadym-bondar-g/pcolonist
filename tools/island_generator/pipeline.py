"""Generation group orchestration for island assets."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

Generator = Callable[[], list[Path]]


def run_generation_group(only: str, generators: dict[str, Generator]) -> list[Path]:
    if only != "all":
        return generators[only]()

    outputs: list[Path] = []
    for name in ("map", "debug", "scene"):
        outputs.extend(generators[name]())
    return outputs

