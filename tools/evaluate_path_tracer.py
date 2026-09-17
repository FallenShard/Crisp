#!/usr/bin/env python3
"""Run the whole path-tracer evaluation: references, captures, metrics, HTML report.

With no arguments this renders every case in cases.json, compares each Crisp
capture against its Mitsuba reference, and writes the report under
Output/PathTracerEvaluation.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import subprocess
import sys
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parent
FIXTURE_DIR = TOOLS_DIR / "path_tracer_evaluation"
DEFAULT_MANIFEST = FIXTURE_DIR / "cases.json"
DEFAULT_OUTPUT_DIR = REPO_ROOT / "Output" / "PathTracerEvaluation"
DEFAULT_PRESET = "x64-release"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST, help="Case manifest (default: cases.json)")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR, help="Report and artifact root")
    parser.add_argument("--case", action="append", help="Run only this case; repeatable (default: every case)")
    parser.add_argument(
        "--preset",
        default=DEFAULT_PRESET,
        help=f"Build preset supplying CrispMain (default: {DEFAULT_PRESET})",
    )
    parser.add_argument("--spp", type=int, help="Override every case's sample count")
    parser.add_argument("--skip-flip", action="store_true", help="Skip HDR-FLIP; it dominates the runtime")
    parser.add_argument("--force-references", action="store_true", help="Re-render references even when current")
    parser.add_argument("--force-captures", action="store_true", help="Re-capture even when the candidate EXR exists")
    return parser.parse_args()


def check_dependencies() -> None:
    """Fail before the expensive stages rather than at the comparison that needs these."""
    missing = [name for name in ("mitsuba", "flip_evaluator") if importlib.util.find_spec(name) is None]
    if missing:
        raise SystemExit(
            f"{sys.executable}\ncannot import: {', '.join(missing)}\n"
            "Run this through the pinned environment instead:\n"
            "    uv sync\n"
            "    uv run python tools/evaluate_path_tracer.py"
        )


def load_cases(manifest_path: Path, selected: list[str] | None) -> tuple[dict, list[dict]]:
    if not manifest_path.is_file():
        raise SystemExit(f"Manifest not found: {manifest_path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    defaults = manifest.get("defaults", {})
    cases = manifest.get("cases", [])
    if not cases:
        raise SystemExit(f"Manifest defines no cases: {manifest_path}")
    if selected:
        by_name = {case["name"]: case for case in cases}
        unknown = sorted(set(selected) - set(by_name))
        if unknown:
            available = ", ".join(sorted(by_name))
            raise SystemExit(f"Unknown case(s): {', '.join(unknown)}. Available: {available}")
        cases = [by_name[name] for name in selected]
    return defaults, cases


def run(command: list[str], description: str) -> None:
    print(f"\n=== {description} ===", flush=True)
    completed = subprocess.run(command, cwd=REPO_ROOT)
    if completed.returncode != 0:
        raise SystemExit(f"{description} failed with exit code {completed.returncode}")


def file_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def reference_is_current(reference: Path, source: Path, spp: int, seed: int, variant: str) -> bool:
    """render_mitsuba.py leaves a sidecar recording the settings the EXR was made with."""
    metadata_path = reference.with_suffix(".json")
    if not reference.is_file() or not metadata_path.is_file():
        return False
    try:
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return False
    return (
        metadata.get("samplesPerPixel"),
        metadata.get("seed"),
        metadata.get("variant"),
        metadata.get("sourceSha256"),
    ) == (spp, seed, variant, file_sha256(source))


def render_reference(case: dict, settings: dict, references_dir: Path, force: bool) -> Path:
    scene_path = FIXTURE_DIR / case["mitsubaScene"]
    if not scene_path.is_file():
        raise SystemExit(f"[{case['name']}] Mitsuba scene not found: {scene_path}")
    reference = references_dir / f"{scene_path.stem}.exr"

    if not force and reference_is_current(
        reference, scene_path, settings["spp"], settings["seed"], settings["mitsubaVariant"]
    ):
        print(f"[{case['name']}] Reference is current: {reference}")
        return reference

    run(
        [
            sys.executable,
            str(TOOLS_DIR / "render_mitsuba.py"),
            str(scene_path),
            "--output-dir",
            str(references_dir),
            "--spp",
            str(settings["spp"]),
            "--seed",
            str(settings["seed"]),
            "--variant",
            settings["mitsubaVariant"],
            # evaluate_renderings.py renders its own preview from the same EXR.
            "--no-preview",
            "--force",
        ],
        f"[{case['name']}] Mitsuba reference at {settings['spp']} spp",
    )
    if not reference.is_file():
        raise SystemExit(f"[{case['name']}] Mitsuba produced no image at {reference}")
    return reference


def capture_candidate(
    case: dict,
    settings: dict,
    reference: Path,
    scene_path: Path,
    scene_fingerprint: str,
    candidates_dir: Path,
    preset: str,
    force: bool,
) -> None:
    candidate = candidates_dir / reference.name
    # saveExr does not create parent directories, so the capture target must exist up front.
    candidates_dir.mkdir(parents=True, exist_ok=True)
    config = {
        "resourcesPath": str(REPO_ROOT / "Resources"),
        "shaderSourcesPath": str(REPO_ROOT / "Crisp" / "Crisp" / "Shaders"),
        "outputDir": str(candidates_dir),
        "imguiFontPath": str(REPO_ROOT / "Resources" / "Fonts" / "Barlow-SemiBold.ttf"),
        "logLevel": "info",
        "vulkan": {
            "forceValidationLayers": False,
            "enableRayTracing": True,
        },
        "activeScene": "vulkan-ray-tracer",
        "scenes": {
            "vulkan-ray-tracer": {
                "scenePath": str(scene_path),
                "sceneFingerprint": scene_fingerprint,
                "samplesPerFrame": settings["samplesPerFrame"],
                "captureFilename": reference.name,
                "closeAfterCapture": True,
            },
        },
    }
    config_path = candidates_dir / f"{case['name']}.config.json"

    # The config records every setting the capture depends on, so it doubles as the
    # cache key: a stale candidate beside a re-rendered reference would compare two
    # different sample counts and read as a regression.
    if candidate.is_file() and not force and config_path.is_file():
        try:
            if json.loads(config_path.read_text(encoding="utf-8")) == config:
                print(f"[{case['name']}] Candidate already captured: {candidate}")
                return
        except json.JSONDecodeError:
            pass

    config_path.write_text(json.dumps(config, indent=2) + "\n", encoding="utf-8")

    mode = "@mode/opt" if preset == "x64-release" else "@mode/dev"

    cmuck_command = ["cmuck", mode, "run", "CrispMain", "--preset", preset]
    run(
        [*cmuck_command, "--", "--config_path", str(config_path)],
        f"[{case['name']}] Crisp capture at {settings['spp']} spp",
    )
    if not candidate.is_file():
        raise SystemExit(f"[{case['name']}] Crisp exited without writing {candidate}")


def resolve_crisp_scene(case: dict) -> Path:
    relative_scene_file = Path(case["crispScene"])
    resource_scene_file = REPO_ROOT / "Resources" / relative_scene_file
    repository_scene_file = REPO_ROOT / relative_scene_file
    if resource_scene_file.is_file():
        return resource_scene_file.resolve()
    if repository_scene_file.is_file():
        return repository_scene_file.resolve()
    raise SystemExit(
        f"[{case['name']}] Crisp scene not found under Resources or the repository root: {relative_scene_file}"
    )


def prepare_crisp_scene(case: dict, candidates_dir: Path, spp_override: int | None) -> tuple[Path, str, dict]:
    source_path = resolve_crisp_scene(case)
    try:
        scene = json.loads(source_path.read_text(encoding="utf-8"))
        if spp_override is not None:
            scene["sampler"]["samplesPerPixel"] = spp_override

        sampler = scene["sampler"]
        integrator = scene["integrator"]
        camera = scene["camera"]
        spp = sampler["samplesPerPixel"]
        seed = sampler.get("seed", 0)
        resolution = camera["imageSize"]
        filter_type = camera.get("reconstructionFilter", {"type": "box"})["type"]
        max_depth = integrator.get("maxDepth", 32)
    except (json.JSONDecodeError, KeyError, TypeError) as error:
        raise SystemExit(f"[{case['name']}] invalid Crisp reference settings in {source_path}: {error}") from error

    if not isinstance(spp, int) or isinstance(spp, bool) or spp <= 0:
        raise SystemExit(f"[{case['name']}] samplesPerPixel must be a positive integer")
    if not isinstance(seed, int) or isinstance(seed, bool) or not 0 <= seed <= 0xFFFFFFFF:
        raise SystemExit(f"[{case['name']}] seed must be a uint32 integer")
    if (
        not isinstance(resolution, list)
        or len(resolution) != 2
        or any(not isinstance(value, int) or isinstance(value, bool) or value <= 0 for value in resolution)
    ):
        raise SystemExit(f"[{case['name']}] imageSize must contain two positive integers")
    if not isinstance(max_depth, int) or isinstance(max_depth, bool) or max_depth <= 0:
        raise SystemExit(f"[{case['name']}] maxDepth must be a positive integer")
    if filter_type != "box":
        raise SystemExit(f"[{case['name']}] unsupported reference reconstruction filter: {filter_type}")

    serialized = json.dumps(scene, indent=2) + "\n"
    fingerprint = hashlib.sha256(serialized.encode("utf-8")).hexdigest()
    scene_dir = candidates_dir / "scenes"
    scene_dir.mkdir(parents=True, exist_ok=True)
    capture_scene_path = scene_dir / f"{case['name']}.json"
    capture_scene_path.write_text(serialized, encoding="utf-8")
    return capture_scene_path.resolve(), fingerprint, {"spp": spp, "seed": seed}


def validate_results(cases: list[dict], report_path: Path) -> None:
    report = json.loads(report_path.read_text(encoding="utf-8"))
    candidate_runs = report.get("candidateRuns", [])
    if len(candidate_runs) != 1:
        raise SystemExit(f"Expected one candidate run in {report_path}, found {len(candidate_runs)}")

    image_results = candidate_runs[0].get("images", {})
    failures: list[str] = []
    for case in cases:
        image_name = f"{Path(case['mitsubaScene']).stem}.exr"
        metrics = image_results.get(image_name)
        if metrics is None:
            failures.append(f"[{case['name']}] no metrics found for {image_name}")
            continue
        if "error" in metrics:
            failures.append(f"[{case['name']}] {metrics['error']}")
            continue

        thresholds = case.get("thresholds", {})
        for metric, limit in thresholds.get("maximum", {}).items():
            value = metrics.get(metric)
            if value is None or value > limit:
                failures.append(f"[{case['name']}] {metric}={value!r} exceeds maximum {limit}")
        for metric, limit in thresholds.get("minimum", {}).items():
            value = metrics.get(metric)
            if value is None or value < limit:
                failures.append(f"[{case['name']}] {metric}={value!r} is below minimum {limit}")

    if failures:
        raise SystemExit("Image validation failed:\n  " + "\n  ".join(failures))
    print("All images are valid and configured thresholds passed")


def main() -> int:
    args = parse_args()
    check_dependencies()
    defaults, cases = load_cases(args.manifest, args.case)

    output_dir = args.output_dir.resolve()
    references_dir = output_dir / "references"
    candidates_dir = output_dir / "candidates"
    artifacts_dir = output_dir / "artifacts"
    json_report = output_dir / "report.json"
    html_report = output_dir / "report.html"
    references_dir.mkdir(parents=True, exist_ok=True)

    print(f"Evaluating {len(cases)} case(s) into {output_dir}")
    for case in cases:
        scene_path, scene_fingerprint, scene_settings = prepare_crisp_scene(case, candidates_dir, args.spp)
        settings = {
            **scene_settings,
            "mitsubaVariant": case.get("mitsubaVariant", defaults.get("mitsubaVariant", "cuda_ad_rgb")),
            "samplesPerFrame": case.get("samplesPerFrame", defaults.get("samplesPerFrame", 16)),
        }
        reference = render_reference(case, settings, references_dir, args.force_references)
        capture_candidate(
            case,
            settings,
            reference,
            scene_path,
            scene_fingerprint,
            candidates_dir,
            args.preset,
            args.force_captures,
        )

    compare_command = [
        sys.executable,
        str(TOOLS_DIR / "evaluate_renderings.py"),
        str(references_dir),
        str(candidates_dir),
        "--artifacts-dir",
        str(artifacts_dir),
        "--output",
        str(json_report),
    ]
    if args.skip_flip:
        compare_command.append("--skip-flip")
    run(compare_command, "Comparing captures against references")
    validate_results(cases, json_report)

    run(
        [
            sys.executable,
            str(TOOLS_DIR / "generate_render_report.py"),
            str(json_report),
            "--output",
            str(html_report),
        ],
        "Generating HTML report",
    )

    print(f"\nReport: {html_report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
