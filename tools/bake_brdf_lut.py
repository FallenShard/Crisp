#!/usr/bin/env python3
"""Bake Resources/Textures/BrdfLut.exr: the split-sum BRDF table the raster IBL path reads.

R = A and G = B, so a shader reconstructs the specular term as prefilter * (F0 * A + B). The first axis is
N.V and the second is perceptual roughness, both texel-centred: texel i covers parameter (i + 0.5) / N.

This is the only definition of the table; loadBrdfLut() in Crisp/Lights/EnvironmentLight.cpp just uploads what
it writes. Sampling is GGX with alpha = roughness^2, matching sampleGGXNormal in BSDFs/microfacet.part.glsl.

Masking is the exact Smith G1 that ggxSmithG1 evaluates, which is what the path tracer integrates -- not the
Schlick-GGX k = roughness^2 / 2 the retired brdf-lut.frag.glsl used. That approximation cost up to 0.23 of
A + B at grazing angles and mid roughness, which the raster paid as an energy deficit against the tracer.
Pass --legacy-schlick to reproduce it for comparison.

    python tools/bake_brdf_lut.py                  # bake, validate, write
    python tools/bake_brdf_lut.py --check          # validate the existing file, write nothing
    python tools/bake_brdf_lut.py --legacy-schlick # bake the shader's approximation instead
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUTPUT = REPO_ROOT / "Resources" / "Textures" / "BrdfLut.exr"

# Must match kBrdfLutExtent in Crisp/Crisp/Lights/EnvironmentLight.cpp.
LUT_SIZE = 512

SAMPLE_COUNT = 4096


def radical_inverse_vdc(bits: np.ndarray) -> np.ndarray:
    bits = bits.astype(np.uint32)
    bits = (bits << np.uint32(16)) | (bits >> np.uint32(16))
    bits = ((bits & np.uint32(0x55555555)) << np.uint32(1)) | ((bits & np.uint32(0xAAAAAAAA)) >> np.uint32(1))
    bits = ((bits & np.uint32(0x33333333)) << np.uint32(2)) | ((bits & np.uint32(0xCCCCCCCC)) >> np.uint32(2))
    bits = ((bits & np.uint32(0x0F0F0F0F)) << np.uint32(4)) | ((bits & np.uint32(0xF0F0F0F0)) >> np.uint32(4))
    bits = ((bits & np.uint32(0x00FF00FF)) << np.uint32(8)) | ((bits & np.uint32(0xFF00FF00)) >> np.uint32(8))
    return bits.astype(np.float64) * 2.3283064365386963e-10


def hammersley(count: int) -> np.ndarray:
    i = np.arange(count, dtype=np.uint32)
    return np.stack([i.astype(np.float64) / count, radical_inverse_vdc(i)], axis=1)


def importance_sample_ggx(unit_samples: np.ndarray, roughness: float) -> np.ndarray:
    """Mirrors ImportanceSampleGGX with N = +Z, including its tangent frame.

    For N = +Z the shader's |N.z| < 0.999 test fails, so it builds the frame from up = +X and ends up with
    tangent = -Y and bitangent = +X. That is a rotation of phi, which a rotationally symmetric integrand does
    not notice, but it is reproduced here so the sample sequence matches the GPU texel for texel.
    """
    alpha = roughness * roughness
    phi = 2.0 * np.pi * unit_samples[:, 0]
    cos_theta = np.sqrt((1.0 - unit_samples[:, 1]) / (1.0 + (alpha * alpha - 1.0) * unit_samples[:, 1]))
    sin_theta = np.sqrt(np.maximum(0.0, 1.0 - cos_theta * cos_theta))

    hx = np.cos(phi) * sin_theta
    hy = np.sin(phi) * sin_theta
    hz = cos_theta
    # tangent * H.x + bitangent * H.y + N * H.z, with tangent = (0,-1,0) and bitangent = (1,0,0).
    return np.stack([hy, -hx, hz], axis=1)


def geometry_schlick_ibl(n_dot_v: float, n_dot_l: np.ndarray, roughness: float) -> np.ndarray:
    """The shader's approximation: Schlick-GGX with the IBL k = roughness^2 / 2."""
    k = roughness * roughness / 2.0
    return (n_dot_v / (n_dot_v * (1.0 - k) + k)) * (n_dot_l / (n_dot_l * (1.0 - k) + k))


def smith_g1(cos_theta: np.ndarray, alpha: float) -> np.ndarray:
    """Exact Smith G1 for GGX, matching ggxSmithG1 in BSDFs/microfacet.part.glsl."""
    cos_theta = np.asarray(cos_theta, dtype=np.float64)
    sin_theta = np.sqrt(np.maximum(0.0, 1.0 - cos_theta * cos_theta))
    with np.errstate(divide="ignore", invalid="ignore"):
        tan_theta = np.where(cos_theta > 0.0, sin_theta / cos_theta, 0.0)
    a = alpha * np.abs(tan_theta)
    return np.where(np.abs(tan_theta) == 0.0, 1.0, 2.0 / (1.0 + np.sqrt(1.0 + a * a)))


def geometry_smith_exact(n_dot_v: float, n_dot_l: np.ndarray, roughness: float) -> np.ndarray:
    """Separable exact Smith, the product ggxGeometry forms from two ggxSmithG1 calls."""
    alpha = roughness * roughness
    return smith_g1(np.full_like(n_dot_l, n_dot_v), alpha) * smith_g1(n_dot_l, alpha)


def integrate_brdf(
    n_dot_v: float, roughness: float, unit_samples: np.ndarray, legacy: bool = False
) -> tuple[float, float]:
    v = np.array([np.sqrt(max(0.0, 1.0 - n_dot_v * n_dot_v)), 0.0, n_dot_v])

    h = importance_sample_ggx(unit_samples, roughness)
    v_dot_h = h @ v
    light = 2.0 * v_dot_h[:, None] * h - v
    # The shader normalizes L; H is already unit length, so this only removes round-off.
    light /= np.linalg.norm(light, axis=1, keepdims=True)

    n_dot_l = np.maximum(light[:, 2], 0.0)
    n_dot_h = np.maximum(h[:, 2], 0.0)
    v_dot_h = np.maximum(v_dot_h, 0.0)

    valid = (n_dot_l > 0.0) & (v_dot_h > 0.0)
    if not np.any(valid):
        return 0.0, 0.0

    g = geometry_schlick_ibl(n_dot_v, n_dot_l, roughness) if legacy else geometry_smith_exact(n_dot_v, n_dot_l, roughness)
    with np.errstate(divide="ignore", invalid="ignore"):
        g_vis = np.where(valid, (g * v_dot_h) / (n_dot_h * n_dot_v), 0.0)
    g_vis = np.nan_to_num(g_vis, nan=0.0, posinf=0.0, neginf=0.0)

    fc = np.power(1.0 - v_dot_h, 5.0)
    a = np.sum(np.where(valid, (1.0 - fc) * g_vis, 0.0)) / SAMPLE_COUNT
    b = np.sum(np.where(valid, fc * g_vis, 0.0)) / SAMPLE_COUNT
    return float(a), float(b)


def bake(legacy: bool = False) -> np.ndarray:
    """Returns the (LUT_SIZE, LUT_SIZE, 2) table, row = roughness, column = N.V."""
    table = np.zeros((LUT_SIZE, LUT_SIZE, 2), dtype=np.float64)
    # Texel-centred, because the full-screen quad the shader draws hands it (i + 0.5) / N.
    params = (np.arange(LUT_SIZE, dtype=np.float64) + 0.5) / LUT_SIZE
    unit_samples = hammersley(SAMPLE_COUNT)

    for row, roughness in enumerate(params):
        for column, n_dot_v in enumerate(params):
            table[row, column, 0], table[row, column, 1] = integrate_brdf(
                n_dot_v, roughness, unit_samples, legacy)
        if (row + 1) % 64 == 0:
            print(f"  row {row + 1}/{LUT_SIZE}", file=sys.stderr)
    return table


def validate(table: np.ndarray) -> bool:
    """Checks the properties the split-sum reconstruction depends on."""
    ok = True
    params = (np.arange(LUT_SIZE, dtype=np.float64) + 0.5) / LUT_SIZE
    a, b = table[:, :, 0], table[:, :, 1]

    if a.min() < 0.0 or b.min() < 0.0:
        print(f"  FAIL: negative entries: A min {a.min():.5f}, B min {b.min():.5f}")
        ok = False
    if not np.all(np.isfinite(table)):
        print("  FAIL: non-finite entries")
        ok = False

    # A white metal (F0 = 1) reflects everything a mirror would, so A + B must be 1 at roughness 0. This is the
    # number that decides whether the raster loses energy against a reference tracer.
    total = a + b
    print(f"A + B at roughness {params[0]:.5f}: min {total[0].min():.5f}, max {total[0].max():.5f}")
    if abs(total[0].max() - 1.0) > 0.02:
        print("  FAIL: a mirror-smooth lobe should reconstruct to 1.")
        ok = False

    print(f"A + B overall: min {total.min():.5f} (at roughness {params[np.argmin(total) // LUT_SIZE]:.4f})")
    if total.max() > 1.0 + 1e-3:
        print(f"  FAIL: A + B exceeds 1: {total.max():.5f}")
        ok = False

    # Grazing angles keep the least energy, so the deficit has to grow as N.V falls.
    for roughness in (0.25, 0.5, 0.75, 1.0):
        row = min(int(roughness * LUT_SIZE), LUT_SIZE - 1)
        print(
            f"  roughness {params[row]:.3f}: A+B at N.V=0.05 {total[row, int(0.05 * LUT_SIZE)]:.4f}, "
            f"at N.V=0.95 {total[row, int(0.95 * LUT_SIZE)]:.4f}"
        )
    return ok


def write_exr(table: np.ndarray, output_path: Path) -> None:
    """Row 0 is roughness 0 and column 0 is grazing, matching the texture the shader samples."""
    import OpenEXR

    header = {"compression": OpenEXR.ZIP_COMPRESSION, "type": OpenEXR.scanlineimage}
    channels = {"R": table[:, :, 0].astype(np.float32), "G": table[:, :, 1].astype(np.float32)}
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with OpenEXR.File(header, channels) as exr:
        exr.write(str(output_path))


def read_exr(path: Path) -> np.ndarray:
    import OpenEXR

    with OpenEXR.File(str(path)) as exr:
        channels = exr.channels()
        table = np.zeros((LUT_SIZE, LUT_SIZE, 2), dtype=np.float64)
        table[:, :, 0] = channels["R"].pixels
        table[:, :, 1] = channels["G"].pixels
    return table


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="EXR to write")
    parser.add_argument("--check", action="store_true", help="Validate the existing file instead of baking")
    parser.add_argument("--legacy-schlick", action="store_true", help="Bake the shader's k = r^2/2 approximation")
    args = parser.parse_args()

    if args.check:
        if not args.output.exists():
            print(f"{args.output} does not exist; run without --check to bake it.")
            return 1
        print(f"Validating {args.output}")
        return 0 if validate(read_exr(args.output)) else 1

    print(f"Baking {LUT_SIZE}x{LUT_SIZE} with {SAMPLE_COUNT} samples per texel", file=sys.stderr)
    table = bake(args.legacy_schlick)

    print()
    if not validate(table):
        print("\nValidation failed; not writing.")
        return 1

    write_exr(table, args.output)
    print(f"\nWrote {args.output} ({args.output.stat().st_size} bytes)")

    print("Re-reading to confirm the round trip:")
    return 0 if validate(read_exr(args.output)) else 1


if __name__ == "__main__":
    sys.exit(main())
