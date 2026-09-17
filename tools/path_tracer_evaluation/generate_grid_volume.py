#!/usr/bin/env python3
"""Generate the scalar Mitsuba VOL shared by the grid-medium comparison."""

from __future__ import annotations

import math
import struct
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
OUTPUT = REPO_ROOT / "Resources" / "Volumes" / "reference_density.vol"
RESOLUTION = 48
BOUNDS = (-1.0, -1.0, 0.2, 1.0, 1.0, 1.2)


def density(x: float, y: float, z: float) -> float:
    radial = math.exp(-((x - 0.5) ** 2 + (y - 0.5) ** 2) / (2.0 * 0.22**2))
    vertical = math.sin(math.pi * z) ** 2
    return radial * vertical


def main() -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT.open("wb") as file:
        file.write(struct.pack("<3sB5I6f", b"VOL", 3, 1, RESOLUTION, RESOLUTION, RESOLUTION, 1, *BOUNDS))
        for z in range(RESOLUTION):
            for y in range(RESOLUTION):
                for x in range(RESOLUTION):
                    file.write(struct.pack("<f", density(
                        (x + 0.5) / RESOLUTION,
                        (y + 0.5) / RESOLUTION,
                        (z + 0.5) / RESOLUTION,
                    )))
    print(OUTPUT)


if __name__ == "__main__":
    main()
