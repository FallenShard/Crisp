#!/usr/bin/env python3
"""Bake Crisp environment maps with Filament's cmgen.

The output is directly consumable by loadImageBasedLightingData(): an
equirectangular HDR image, a diffuse-irradiance horizontal cross, and nine
GGX-prefiltered horizontal crosses with 512-to-2 pixel cube faces.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import uuid
from pathlib import Path


CUBE_SIZE = 512
IRRADIANCE_SIZE = 128
MIP_COUNT = 9
FACE_NAMES = ("px", "nx", "py", "ny", "pz", "nz")
SOURCE_EXTENSIONS = (".hdr", ".exr", ".png", ".psd")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "inputs",
        nargs="+",
        type=Path,
        help="Environment image, environment directory, or root containing environment directories",
    )
    parser.add_argument(
        "--cmgen",
        type=Path,
        help="Path to cmgen (otherwise FILAMENT_CMGEN or PATH is used)",
    )
    parser.add_argument(
        "--magick",
        type=Path,
        help="Path to ImageMagick's magick executable (otherwise PATH is used)",
    )
    parser.add_argument(
        "--output-root",
        type=Path,
        help="Parent directory for baked maps (default: next to each source environment directory)",
    )
    parser.add_argument(
        "--suffix",
        default="Cmgen",
        help="Suffix for generated environment names (default: Cmgen; use an empty string for original names)",
    )
    parser.add_argument(
        "--samples",
        type=int,
        default=4096,
        help="GGX integration sample count (default: 4096)",
    )
    parser.add_argument("--force", action="store_true", help="Replace existing output directories")
    parser.add_argument("--keep-faces", action="store_true", help="Keep cmgen's individual face images")
    parser.add_argument("--dry-run", action="store_true", help="List inputs and outputs without running tools")
    return parser.parse_args()


def find_tool(explicit: Path | None, environment_variable: str, executable: str) -> Path:
    candidate = explicit or (Path(value) if (value := os.environ.get(environment_variable)) else None)
    if candidate is None:
        found = shutil.which(executable)
        candidate = Path(found) if found else None
    if candidate is None or not candidate.is_file():
        raise ValueError(
            f"Could not find {executable}. Pass --{executable}, set {environment_variable}, or add it to PATH."
        )
    return candidate.resolve()


def is_source_environment(path: Path) -> bool:
    return (
        path.is_file()
        and path.suffix.lower() in SOURCE_EXTENSIONS
        and path.stem == path.parent.name
    )


def discover_sources(inputs: list[Path], generated_suffix: str) -> list[Path]:
    sources: set[Path] = set()
    for input_path in inputs:
        path = input_path.resolve()
        if path.is_file():
            if path.suffix.lower() not in SOURCE_EXTENSIONS:
                raise ValueError(f"Unsupported environment-map format: {path}")
            sources.add(path)
            continue
        if not path.is_dir():
            raise ValueError(f"Input does not exist: {path}")

        direct_sources = [path / f"{path.name}{extension}" for extension in SOURCE_EXTENSIONS]
        source = next((candidate for candidate in direct_sources if candidate.is_file()), None)
        if source is not None:
            sources.add(source)
            continue

        for candidate in path.rglob("*"):
            if is_source_environment(candidate):
                if generated_suffix and candidate.stem.endswith(generated_suffix):
                    continue
                sources.add(candidate)

    if not sources:
        raise ValueError("No source environment maps found")
    return sorted(sources)


def run(command: list[str]) -> None:
    subprocess.run(command, check=True)


def resize_face(magick: Path, source: Path, destination: Path, size: int) -> None:
    run([str(magick), str(source), "-filter", "Lanczos", "-resize", f"{size}x{size}!", str(destination)])


def create_horizontal_cross(magick: Path, faces: dict[str, Path], size: int, destination: Path) -> None:
    # Layout expected by loadCubeMapFacesFromHCrossImage():
    #           +Y
    #       -X  +Z  +X  -Z
    #           -Y
    placements = (
        ("py", size, 0),
        ("nx", 0, size),
        ("pz", size, size),
        ("px", 2 * size, size),
        ("nz", 3 * size, size),
        ("ny", size, 2 * size),
    )
    # HDR faces are linear RGB. ImageMagick's default canvas is tagged sRGB,
    # which would otherwise apply an unwanted transfer function on composite.
    command = [str(magick), "-size", f"{4 * size}x{3 * size}", "xc:black", "-colorspace", "RGB"]
    for face, x, y in placements:
        command.extend([str(faces[face]), "-geometry", f"+{x}+{y}", "-composite"])
    command.append(str(destination))
    run(command)


def convert_source_to_hdr(magick: Path, source: Path, destination: Path) -> None:
    if source.suffix.lower() == ".hdr":
        shutil.copy2(source, destination)
    else:
        run([str(magick), str(source), str(destination)])


def bake_environment(
    source: Path,
    output_root: Path | None,
    suffix: str,
    samples: int,
    cmgen: Path,
    magick: Path,
    force: bool,
    keep_faces: bool,
    dry_run: bool,
) -> bool:
    output_name = f"{source.stem}{suffix}"
    if output_root:
        parent = output_root.resolve()
    else:
        parent = source.parent.parent if source.parent.name == source.stem else source.parent
    output_dir = parent / output_name
    if output_dir.exists() and not force:
        print(f"Skipping existing output (pass --force to replace): {output_dir}")
        return False

    print(f"Baking {source} -> {output_dir}")
    if dry_run:
        return True

    parent.mkdir(parents=True, exist_ok=True)
    staging_dir = parent / f"{output_name}.staging-{uuid.uuid4().hex}"
    staging_dir.mkdir()
    try:
        # Keep intermediate images under the workspace-backed staging directory.
        # Some Windows sandbox configurations deny cleanup after an external tool
        # writes into the user's system temporary directory.
        cmgen_root = staging_dir / "cmgen-output"
        cmgen_root.mkdir()
        command = [
            str(cmgen),
            "--quiet",
            f"--size={CUBE_SIZE}",
            f"--ibl-samples={samples}",
            "--ibl-min-lod-size=2",
            "--no-mirror",
            "--format=hdr",
            f"--ibl-ld={cmgen_root}",
            f"--ibl-irradiance={cmgen_root}",
            str(source),
        ]
        run(command)

        face_dir = cmgen_root / source.stem
        for mip in range(MIP_COUNT):
            face_size = CUBE_SIZE >> mip
            faces = {face: face_dir / f"m{mip}_{face}.hdr" for face in FACE_NAMES}
            missing = [path for path in faces.values() if not path.is_file()]
            if missing:
                raise RuntimeError(f"cmgen did not produce expected mip {mip} faces: {missing}")
            destination = staging_dir / f"{output_name}_rad_{mip}_{4 * face_size}x{3 * face_size}.hdr"
            create_horizontal_cross(magick, faces, face_size, destination)

        resized_irradiance_dir = cmgen_root / "irradiance-128"
        resized_irradiance_dir.mkdir()
        irradiance_faces: dict[str, Path] = {}
        for face in FACE_NAMES:
            source_face = face_dir / f"i_{face}.hdr"
            if not source_face.is_file():
                raise RuntimeError(f"cmgen did not produce expected irradiance face: {source_face}")
            destination_face = resized_irradiance_dir / f"i_{face}.hdr"
            resize_face(magick, source_face, destination_face, IRRADIANCE_SIZE)
            irradiance_faces[face] = destination_face
        create_horizontal_cross(
            magick,
            irradiance_faces,
            IRRADIANCE_SIZE,
            staging_dir / f"{output_name}_irr.hdr",
        )

        convert_source_to_hdr(magick, source, staging_dir / f"{output_name}.hdr")

        if keep_faces:
            face_dir.replace(staging_dir / "cmgen-faces")
        shutil.rmtree(cmgen_root)

        if output_dir.exists():
            shutil.rmtree(output_dir)
        staging_dir.replace(output_dir)
    except Exception:
        shutil.rmtree(staging_dir, ignore_errors=True)
        raise

    print(f"Finished {output_dir}")
    return True


def main() -> int:
    args = parse_args()
    if args.samples <= 0:
        raise SystemExit("--samples must be greater than zero")

    try:
        sources = discover_sources(args.inputs, args.suffix)
        if args.dry_run:
            cmgen = Path(args.cmgen) if args.cmgen else Path("cmgen")
            magick = Path(args.magick) if args.magick else Path("magick")
        else:
            cmgen = find_tool(args.cmgen, "FILAMENT_CMGEN", "cmgen")
            magick = find_tool(args.magick, "CRISP_IMAGEMAGICK", "magick")

        completed = 0
        for source in sources:
            completed += bake_environment(
                source=source,
                output_root=args.output_root,
                suffix=args.suffix,
                samples=args.samples,
                cmgen=cmgen,
                magick=magick,
                force=args.force,
                keep_faces=args.keep_faces,
                dry_run=args.dry_run,
            )
    except (OSError, subprocess.CalledProcessError, RuntimeError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    print(f"Baked {completed} of {len(sources)} environment map(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
