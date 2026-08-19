#!/usr/bin/env python3
"""Compare folders of linear RGB renderings against reference images."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any

import flip_evaluator as flip
import numpy as np


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference_dir", type=Path, help="Directory containing reference images")
    parser.add_argument(
        "candidate_dirs",
        nargs="+",
        type=Path,
        help="Candidate directories; multiple directories are treated as independent runs",
    )
    parser.add_argument("--output", type=Path, help="Write the JSON report to this file instead of stdout")
    parser.add_argument("--extension", default=".exr", help="Image extension to discover (default: .exr)")
    parser.add_argument(
        "--psnr-peak",
        type=float,
        help="Fixed PSNR peak value; by default each reference image's maximum absolute value is used",
    )
    parser.add_argument("--artifacts-dir", type=Path, help="Write image previews and error heatmaps to this directory")
    parser.add_argument(
        "--preview-exposure",
        type=float,
        default=0.0,
        help="Exposure adjustment in stops for reference and candidate previews (default: 0)",
    )
    parser.add_argument(
        "--heatmap-percentile",
        type=float,
        default=99.0,
        help="Positive absolute errors at this percentile map to the top of the heatmap (default: 99)",
    )
    parser.add_argument("--flip-ppd", type=float, default=67.0, help="FLIP pixels per degree (default: 67)")
    parser.add_argument(
        "--flip-tonemapper",
        choices=("ACES", "Hable", "Reinhard"),
        default="ACES",
        help="HDR-FLIP tone mapper (default: ACES)",
    )
    parser.add_argument("--skip-flip", action="store_true", help="Skip HDR-FLIP evaluation")
    return parser.parse_args()


def discover_images(root: Path, extension: str) -> dict[str, Path]:
    resolved_root = root.resolve()
    if not resolved_root.is_dir():
        raise ValueError(f"Image directory does not exist: {root}")
    normalized_extension = extension if extension.startswith(".") else f".{extension}"
    return {
        path.relative_to(resolved_root).as_posix(): path
        for path in sorted(resolved_root.rglob(f"*{normalized_extension}"))
    }


def load_linear_rgb(path: Path, mi: Any) -> np.ndarray:
    bitmap = mi.Bitmap(str(path)).convert(
        pixel_format=mi.Bitmap.PixelFormat.RGB,
        component_format=mi.Struct.Type.Float32,
        srgb_gamma=False,
    )
    image = np.array(bitmap, copy=True)
    if image.ndim != 3 or image.shape[2] != 3:
        raise ValueError(f"Expected an RGB image, got shape {image.shape}")
    return image.astype(np.float64, copy=False)


def linear_to_srgb(image: np.ndarray, exposure: float) -> np.ndarray:
    linear = np.clip(image * (2.0**exposure), 0.0, 1.0)
    return np.where(linear <= 0.0031308, linear * 12.92, 1.055 * np.power(linear, 1.0 / 2.4) - 0.055)


def save_rgb_image(path: Path, image: np.ndarray, exposure: float) -> None:
    from PIL import Image

    path.parent.mkdir(parents=True, exist_ok=True)
    encoded = np.uint8(np.clip(linear_to_srgb(image, exposure) * 255.0 + 0.5, 0.0, 255.0))
    Image.fromarray(encoded, mode="RGB").save(path)


def save_heatmap(path: Path, values: np.ndarray, scale: float, color_map: str) -> None:
    from matplotlib import colormaps
    from PIL import Image

    path.parent.mkdir(parents=True, exist_ok=True)
    normalized = np.clip(values / max(scale, np.finfo(np.float64).eps), 0.0, 1.0)
    encoded = np.uint8(colormaps[color_map](normalized, bytes=True)[..., :3])
    Image.fromarray(encoded, mode="RGB").save(path)


def safe_path_component(value: str) -> str:
    safe = "".join(character if character.isalnum() or character in "-_" else "-" for character in value)
    return safe.strip("-") or "run"


def json_compatible(value: Any) -> Any:
    if isinstance(value, dict):
        return {str(key): json_compatible(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [json_compatible(item) for item in value]
    if isinstance(value, np.generic):
        return value.item()
    return value


def evaluate_flip(
    reference: np.ndarray,
    candidate: np.ndarray,
    ppd: float,
    tonemapper: str,
) -> tuple[np.ndarray | None, dict[str, Any]]:
    if np.any(reference < 0.0) or np.any(candidate < 0.0):
        return None, {"error": "HDR-FLIP requires nonnegative input images"}

    try:
        error_map, _, used_parameters = flip.evaluate(
            np.ascontiguousarray(reference, dtype=np.float32),
            np.ascontiguousarray(candidate, dtype=np.float32),
            "HDR",
            applyMagma=False,
            parameters={"ppd": ppd, "tonemapper": tonemapper.lower()},
        )
        # flip.evaluate returns a single-channel map with a trailing axis.
        error_map = np.asarray(error_map, dtype=np.float64).squeeze(-1)
        if error_map.shape != reference.shape[:2]:
            raise ValueError(f"Unexpected FLIP error-map shape: {error_map.shape}")
        percentiles = np.percentile(error_map, [50.0, 95.0, 99.0])
        return error_map, {
            "mean": float(np.mean(error_map)),
            "median": float(percentiles[0]),
            "p95": float(percentiles[1]),
            "p99": float(percentiles[2]),
            "max": float(np.max(error_map)),
            "parameters": json_compatible(used_parameters),
        }
    except Exception as error:
        return None, {"error": str(error)}


def create_artifacts(
    root: Path,
    run_index: int,
    candidate_root: Path,
    relative_path: str,
    reference: np.ndarray,
    candidate: np.ndarray,
    flip_error_map: np.ndarray | None,
    preview_exposure: float,
    heatmap_percentile: float,
) -> tuple[dict[str, str], float]:
    run_name = f"run-{run_index:02d}-{safe_path_component(candidate_root.resolve().name)}"
    base_path = root.resolve() / run_name / Path(relative_path).with_suffix("")
    paths = {
        "referencePreview": base_path.with_name(base_path.name + ".reference.png"),
        "candidatePreview": base_path.with_name(base_path.name + ".candidate.png"),
        "absoluteDifferenceHeatmap": base_path.with_name(base_path.name + ".absolute-difference.png"),
    }

    save_rgb_image(paths["referencePreview"], reference, preview_exposure)
    save_rgb_image(paths["candidatePreview"], candidate, preview_exposure)

    per_pixel_error = np.max(np.abs(candidate - reference), axis=2)
    positive_errors = per_pixel_error[per_pixel_error > 0.0]
    heatmap_scale = float(np.percentile(positive_errors, heatmap_percentile)) if positive_errors.size else 1.0
    save_heatmap(paths["absoluteDifferenceHeatmap"], per_pixel_error, heatmap_scale, "inferno")

    if flip_error_map is not None:
        paths["flipHeatmap"] = base_path.with_name(base_path.name + ".flip.png")
        save_heatmap(paths["flipHeatmap"], flip_error_map, 1.0, "magma")

    return {name: str(path) for name, path in paths.items()}, heatmap_scale


def compare_images(reference: np.ndarray, candidate: np.ndarray, requested_peak: float | None) -> dict[str, Any]:
    if reference.shape != candidate.shape:
        return {"error": f"Shape mismatch: reference {reference.shape}, candidate {candidate.shape}"}

    reference_non_finite = int(reference.size - np.count_nonzero(np.isfinite(reference)))
    candidate_non_finite = int(candidate.size - np.count_nonzero(np.isfinite(candidate)))
    diagnostics = {
        "referenceNonFiniteValues": reference_non_finite,
        "candidateNonFiniteValues": candidate_non_finite,
        "candidateNegativeValues": int(np.count_nonzero(candidate < 0.0)),
    }
    if reference_non_finite or candidate_non_finite:
        return {**diagnostics, "error": "Cannot calculate metrics for images containing non-finite values"}

    difference = candidate - reference
    absolute_difference = np.abs(difference)
    squared_difference = difference * difference
    mse = float(np.mean(squared_difference))
    reference_norm = float(np.linalg.norm(reference.ravel()))
    l2 = float(np.linalg.norm(difference.ravel()))

    luminance_weights = np.array([0.2126, 0.7152, 0.0722])
    reference_mean_luminance = float(np.mean(reference @ luminance_weights))
    candidate_mean_luminance = float(np.mean(candidate @ luminance_weights))
    signed_mean_luminance_error = candidate_mean_luminance - reference_mean_luminance
    absolute_error_percentiles = np.percentile(absolute_difference, [50.0, 95.0, 99.0])
    epsilon = np.finfo(np.float64).eps

    peak = requested_peak if requested_peak is not None else float(np.max(np.abs(reference)))
    if peak <= 0.0:
        psnr_db = None
    elif mse == 0.0:
        psnr_db = None
    else:
        psnr_db = float(20.0 * math.log10(peak) - 10.0 * math.log10(mse))

    return {
        **diagnostics,
        "meanAbsoluteError": float(np.mean(absolute_difference)),
        "medianAbsoluteError": float(absolute_error_percentiles[0]),
        "absoluteErrorP95": float(absolute_error_percentiles[1]),
        "absoluteErrorP99": float(absolute_error_percentiles[2]),
        "maxAbsoluteError": float(np.max(absolute_difference)),
        "meanSquaredError": mse,
        "rootMeanSquaredError": math.sqrt(mse),
        "l2": l2,
        "relativeL2": l2 / max(reference_norm, epsilon),
        "psnrDb": psnr_db,
        "psnrPeak": peak,
        "exactMatch": mse == 0.0,
        "referenceMeanLuminance": reference_mean_luminance,
        "candidateMeanLuminance": candidate_mean_luminance,
        "signedMeanLuminanceError": signed_mean_luminance_error,
        "relativeMeanLuminanceError": abs(signed_mean_luminance_error)
        / max(abs(reference_mean_luminance), epsilon),
        "signedRelativeMeanLuminanceError": signed_mean_luminance_error
        / max(abs(reference_mean_luminance), epsilon),
    }


def mean_metrics(results: list[dict[str, Any]]) -> dict[str, float]:
    metric_names = (
        "meanAbsoluteError",
        "meanSquaredError",
        "rootMeanSquaredError",
        "relativeL2",
        "psnrDb",
        "relativeMeanLuminanceError",
    )
    metrics = {
        name: float(np.mean([result[name] for result in results if result.get(name) is not None]))
        for name in metric_names
        if any(result.get(name) is not None for result in results)
    }
    flip_means = [result["flip"]["mean"] for result in results if result.get("flip", {}).get("mean") is not None]
    if flip_means:
        metrics["meanFlipError"] = float(np.mean(flip_means))
    return metrics


def main() -> int:
    args = parse_args()
    if args.psnr_peak is not None and args.psnr_peak <= 0.0:
        raise SystemExit("--psnr-peak must be greater than zero")
    if not 0.0 < args.heatmap_percentile <= 100.0:
        raise SystemExit("--heatmap-percentile must be in the range (0, 100]")
    if args.flip_ppd <= 0.0:
        raise SystemExit("--flip-ppd must be greater than zero")

    try:
        import mitsuba as mi
    except ImportError as error:
        raise SystemExit("Mitsuba is required to read the images: pip install mitsuba") from error
    mi.set_variant("scalar_rgb")

    try:
        references = discover_images(args.reference_dir, args.extension)
        candidates = [discover_images(directory, args.extension) for directory in args.candidate_dirs]
    except ValueError as error:
        raise SystemExit(str(error)) from error
    if not references:
        raise SystemExit("No reference images found")

    reference_cache: dict[str, np.ndarray] = {}

    def get_reference(relative_path: str, reference_path: Path) -> np.ndarray:
        if relative_path not in reference_cache:
            reference_cache[relative_path] = load_linear_rgb(reference_path, mi)
        return reference_cache[relative_path]

    run_reports: list[dict[str, Any]] = []
    for run_index, (candidate_root, candidate_images) in enumerate(
        zip(args.candidate_dirs, candidates, strict=True)
    ):
        image_results: dict[str, Any] = {}
        for relative_path, reference_path in references.items():
            candidate_path = candidate_images.get(relative_path)
            if candidate_path is None:
                image_results[relative_path] = {"error": "Missing candidate image"}
                continue
            try:
                reference = get_reference(relative_path, reference_path)
                candidate = load_linear_rgb(candidate_path, mi)
                result = compare_images(reference, candidate, args.psnr_peak)
                if "error" not in result:
                    flip_error_map = None
                    if not args.skip_flip:
                        flip_error_map, result["flip"] = evaluate_flip(
                            reference,
                            candidate,
                            args.flip_ppd,
                            args.flip_tonemapper,
                        )
                    if args.artifacts_dir:
                        artifacts, heatmap_scale = create_artifacts(
                            args.artifacts_dir,
                            run_index,
                            candidate_root,
                            relative_path,
                            reference,
                            candidate,
                            flip_error_map,
                            args.preview_exposure,
                            args.heatmap_percentile,
                        )
                        result["artifacts"] = artifacts
                        result["absoluteDifferenceHeatmap"] = {
                            "channelReduction": "maximum absolute RGB channel error",
                            "colorMap": "inferno",
                            "scaleMax": heatmap_scale,
                            "scalePercentile": args.heatmap_percentile,
                        }
                image_results[relative_path] = result
            except Exception as error:
                image_results[relative_path] = {"error": str(error)}

        valid_results = [result for result in image_results.values() if "error" not in result]
        run_reports.append(
            {
                "directory": str(candidate_root.resolve()),
                "matchedImages": len(valid_results),
                "missingOrInvalidImages": len(references) - len(valid_results),
                "extraImages": sorted(set(candidate_images) - set(references)),
                "meanMetrics": mean_metrics(valid_results),
                "images": image_results,
            }
        )

    variance_results: dict[str, Any] = {}
    if len(candidates) > 1:
        for relative_path, reference_path in references.items():
            if not all(relative_path in run for run in candidates):
                continue
            try:
                reference = get_reference(relative_path, reference_path)
                run_images = [load_linear_rgb(run[relative_path], mi) for run in candidates]
                if any(image.shape != reference.shape for image in run_images):
                    continue
                stack = np.stack(run_images)
                if not np.all(np.isfinite(stack)):
                    continue
                variance = np.var(stack, axis=0, ddof=1)
                mean_image = np.mean(stack, axis=0)
                variance_results[relative_path] = {
                    "meanVariance": float(np.mean(variance)),
                    "maxVariance": float(np.max(variance)),
                    "meanImageMetrics": compare_images(reference, mean_image, args.psnr_peak),
                }
            except Exception as error:
                variance_results[relative_path] = {"error": str(error)}

    report = {
        "referenceDirectory": str(args.reference_dir.resolve()),
        "settings": {
            "artifactsDirectory": str(args.artifacts_dir.resolve()) if args.artifacts_dir else None,
            "previewExposure": args.preview_exposure,
            "heatmapPercentile": args.heatmap_percentile,
            "flipEnabled": not args.skip_flip,
            "flipPpd": args.flip_ppd if not args.skip_flip else None,
            "flipTonemapper": args.flip_tonemapper if not args.skip_flip else None,
        },
        "candidateRuns": run_reports,
        "crossRunVariance": variance_results,
    }
    report_text = json.dumps(report, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report_text, encoding="utf-8")
        print(f"Wrote metrics report to {args.output}")
    else:
        print(report_text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
