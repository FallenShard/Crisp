#!/usr/bin/env python3
"""Render all Mitsuba XML scenes below one or more directories."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from time import perf_counter


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folders", nargs="+", type=Path, help="Folders containing Mitsuba XML scenes, or XML files")
    parser.add_argument("--output-dir", required=True, type=Path, help="Directory for rendered EXR images")
    parser.add_argument("--spp", type=int, help="Override the scene's sample count")
    parser.add_argument("--seed", type=int, default=0, help="Mitsuba render seed (default: 0)")
    parser.add_argument("--variant", default="scalar_rgb", help="Mitsuba variant (default: scalar_rgb)")
    parser.add_argument(
        "--preview-exposure",
        type=float,
        default=2.0,
        help="Exposure adjustment in stops for the sRGB PNG preview (default: 2)",
    )
    parser.add_argument("--no-preview", action="store_true", help="Do not write an sRGB PNG preview")
    parser.add_argument("--force", action="store_true", help="Overwrite existing images")
    return parser.parse_args()


def discover_scenes(folders: list[Path]) -> list[tuple[Path, Path]]:
    scenes: list[tuple[Path, Path]] = []
    multiple_roots = len(folders) > 1
    for folder in folders:
        root = folder.resolve()
        if root.is_file():
            scenes.append((root, Path(root.name)))
            continue
        if not root.is_dir():
            raise ValueError(f"Scene folder does not exist: {folder}")
        prefix = Path(root.name) if multiple_roots else Path()
        scenes.extend((scene, prefix / scene.relative_to(root)) for scene in sorted(root.rglob("*.xml")))
    return scenes


def main() -> int:
    args = parse_args()
    if args.spp is not None and args.spp <= 0:
        raise SystemExit("--spp must be greater than zero")

    try:
        import mitsuba as mi
    except ImportError as error:
        raise SystemExit("Mitsuba is required: pip install mitsuba") from error

    mi.set_variant(args.variant)

    try:
        scenes = discover_scenes(args.folders)
    except ValueError as error:
        raise SystemExit(str(error)) from error
    if not scenes:
        raise SystemExit("No Mitsuba XML scenes found")

    output_dir = args.output_dir.resolve()
    failures = 0
    for scene_path, relative_path in scenes:
        output_path = (output_dir / relative_path).with_suffix(".exr")
        preview_path = output_path.with_suffix(".png")
        metadata_path = output_path.with_suffix(".json")
        if output_path.exists() and not args.force:
            print(f"Skipping existing image: {output_path}")
            continue

        output_path.parent.mkdir(parents=True, exist_ok=True)
        print(f"Rendering {scene_path} -> {output_path}")
        start_time = perf_counter()
        try:
            scene = mi.load_file(str(scene_path)) # type: ignore
            render_args = {"seed": args.seed}
            if args.spp is not None:
                render_args["spp"] = args.spp
            image = mi.render(scene, **render_args)
            mi.Bitmap(image).write(str(output_path))
            if not args.no_preview:
                mi.util.write_bitmap(str(preview_path), image * (2.0**args.preview_exposure)) # type: ignore
            elapsed_seconds = perf_counter() - start_time
            metadata = { # type: ignore
                "mitsubaVersion": getattr(mi, "__version__", "unknown"),
                "variant": args.variant,
                "sourceScene": str(scene_path),
                "sourceSha256": hashlib.sha256(scene_path.read_bytes()).hexdigest(),
                "samplesPerPixel": args.spp,
                "seed": args.seed,
                "previewExposure": None if args.no_preview else args.preview_exposure,
                "renderSeconds": elapsed_seconds,
            }
            metadata_path.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
            print(f"Finished in {elapsed_seconds:.2f} s")
        except Exception as error:  # Keep rendering independent scenes after one failure.
            failures += 1
            print(f"Failed to render {scene_path}: {error}", file=sys.stderr)

    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
