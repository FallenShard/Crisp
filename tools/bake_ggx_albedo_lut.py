#!/usr/bin/env python3
"""Bake Resources/Textures/GgxAlbedoLut.exr: R = E(mu, alpha), G = E_avg(alpha), second axis sqrt(alpha).

The lobe MUST stay a transcription of Crisp/Crisp/Shaders/BSDFs/microfacet.part.glsl -- sampleGGXNormal and the
separable ggxSmithG1 product. See docs/openpbr-path-tracer.md.

    python tools/bake_ggx_albedo_lut.py            # bake, validate, write
    python tools/bake_ggx_albedo_lut.py --check    # validate the existing file, write nothing
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUTPUT = REPO_ROOT / "Resources" / "Textures" / "GgxAlbedoLut.exr"

# Must match kGgxAlbedoLutExtent in Crisp/Renderer/GgxAlbedoLut.hpp and kGgxAlbedoLutSize in the GLSL.
LUT_SIZE = 64

DIRECTIONAL_SAMPLES = 1 << 16
AVERAGE_MU_STEPS = 256

MIN_MU = 1e-3
MIN_ALPHA = 1e-6


def radical_inverse_vdc(indices: np.ndarray) -> np.ndarray:
    """Van der Corput radical inverse, base 2 -- the bit-reversal the bake shader spells out."""
    bits = indices.astype(np.uint32)
    bits = (bits << np.uint32(16)) | (bits >> np.uint32(16))
    bits = ((bits & np.uint32(0x55555555)) << np.uint32(1)) | ((bits & np.uint32(0xAAAAAAAA)) >> np.uint32(1))
    bits = ((bits & np.uint32(0x33333333)) << np.uint32(2)) | ((bits & np.uint32(0xCCCCCCCC)) >> np.uint32(2))
    bits = ((bits & np.uint32(0x0F0F0F0F)) << np.uint32(4)) | ((bits & np.uint32(0xF0F0F0F0)) >> np.uint32(4))
    bits = ((bits & np.uint32(0x00FF00FF)) << np.uint32(8)) | ((bits & np.uint32(0xFF00FF00)) >> np.uint32(8))
    return bits.astype(np.float64) * 2.3283064365386963e-10


def hammersley(sample_count: int) -> np.ndarray:
    indices = np.arange(sample_count, dtype=np.uint32)
    return np.stack([indices.astype(np.float64) / sample_count, radical_inverse_vdc(indices)], axis=1)


def sample_ggx_normal(unit_samples: np.ndarray, alpha: float) -> np.ndarray:
    """Mirrors sampleGGXNormal: samples the full NDF, so the estimator below cancels D against the pdf."""
    tan_theta_squared = alpha * alpha * unit_samples[:, 1] / (1.0 - unit_samples[:, 1])
    cos_theta = 1.0 / np.sqrt(1.0 + tan_theta_squared)
    phi = 2.0 * np.pi * unit_samples[:, 0]
    sin_theta = np.sqrt(np.maximum(0.0, 1.0 - cos_theta * cos_theta))
    return np.stack([sin_theta * np.cos(phi), sin_theta * np.sin(phi), cos_theta], axis=1)


def ggx_smith_g1(v: np.ndarray, microfacet_normals: np.ndarray, alpha: float) -> np.ndarray:
    """Mirrors ggxSmithG1, including its two early-outs."""
    v_dot_h = np.einsum("ij,ij->i", v, microfacet_normals) if v.ndim == 2 else microfacet_normals @ v
    v_z = v[:, 2] if v.ndim == 2 else np.full(len(microfacet_normals), v[2])

    sin_theta_squared = 1.0 - v_z * v_z
    tan_theta = np.where(sin_theta_squared > 0.0, np.sqrt(np.maximum(sin_theta_squared, 0.0)) / v_z, 0.0)
    a = alpha * np.abs(tan_theta)
    g1 = 2.0 / (1.0 + np.sqrt(1.0 + a * a))

    g1 = np.where(np.abs(tan_theta) == 0.0, 1.0, g1)
    return np.where(v_dot_h * v_z <= 0.0, 0.0, g1)


def directional_albedo(mu: float, alpha: float, microfacet_normals: np.ndarray) -> float:
    """E(mu, alpha) with F = 1. Sampling the NDF cancels D against the pdf, which is why this converges at
    every alpha where a uniform hemisphere quadrature cannot resolve the lobe below alpha ~0.1."""
    wi = np.array([np.sqrt(max(0.0, 1.0 - mu * mu)), 0.0, mu])

    h_dot_i = microfacet_normals @ wi
    wo = 2.0 * h_dot_i[:, None] * microfacet_normals - wi

    valid = (h_dot_i > 0.0) & (wo[:, 2] > 0.0)

    integrand = (
        ggx_smith_g1(wi, microfacet_normals, alpha)
        * ggx_smith_g1(wo, microfacet_normals, alpha)
        * np.einsum("ij,ij->i", wo, microfacet_normals)
        / (mu * microfacet_normals[:, 2])
    )
    return float(np.sum(np.where(valid, integrand, 0.0)) / len(microfacet_normals))


def bake() -> np.ndarray:
    """Returns the (LUT_SIZE, LUT_SIZE, 2) table, row = roughness, column = mu."""
    table = np.zeros((LUT_SIZE, LUT_SIZE, 2), dtype=np.float64)

    # Endpoint-mapped: texel i holds parameter i / (N - 1). ggxAlbedoLutUv is the matching forward remap.
    params = np.arange(LUT_SIZE, dtype=np.float64) / (LUT_SIZE - 1)
    mus = np.maximum(params, MIN_MU)

    # E_avg = 2 * integral E(mu) mu dmu, over the same E beside it: Kulla-Conty conserves energy only when the
    # two are genuinely the same function.
    average_mus = (np.arange(AVERAGE_MU_STEPS, dtype=np.float64) + 0.5) / AVERAGE_MU_STEPS

    unit_samples = hammersley(DIRECTIONAL_SAMPLES)

    for row, roughness in enumerate(params):
        alpha = max(roughness * roughness, MIN_ALPHA)

        # The sampled normals depend only on (unit sample, alpha), so one set serves every mu in this row.
        microfacet_normals = sample_ggx_normal(unit_samples, alpha)

        for column, mu in enumerate(mus):
            table[row, column, 0] = directional_albedo(mu, alpha, microfacet_normals)

        average = 2.0 * float(
            np.mean([directional_albedo(mu, alpha, microfacet_normals) * mu for mu in average_mus])
        )
        table[row, :, 1] = average

        print(f"  roughness {roughness:.4f} (alpha {alpha:.6f}): E_avg = {average:.6f}", file=sys.stderr)

    return table


def validate(table: np.ndarray) -> bool:
    """Checks the two properties the compensation formulas actually depend on."""
    ok = True
    params = np.arange(LUT_SIZE, dtype=np.float64) / (LUT_SIZE - 1)
    mus = np.maximum(params, MIN_MU)

    # 1. E_avg is genuinely the average of the E beside it -- the whole invariant, since both formulas close the
    #    F = 1 furnace analytically only when it holds.
    worst_average = 0.0
    for row in range(LUT_SIZE):
        reintegrated = 2.0 * np.trapezoid(table[row, :, 0] * mus, mus)
        worst_average = max(worst_average, abs(reintegrated - table[row, 0, 1]))
    print(f"E_avg vs. re-integrated E: worst absolute error {worst_average:.5f}")
    if worst_average > 0.01:
        print("  FAIL: E_avg is not the average of the E beside it; compensation will not conserve energy.")
        ok = False

    # 2. E bounded as single scattering must be: never above 1, never negative.
    if table[:, :, 0].min() < 0.0 or table[:, :, 0].max() > 1.0 + 1e-4:
        print(f"  FAIL: E out of [0, 1]: min {table[:, :, 0].min():.5f}, max {table[:, :, 0].max():.5f}")
        ok = False
    else:
        print(f"E range: [{table[:, :, 0].min():.5f}, {table[:, :, 0].max():.5f}]")

    # 3. The endpoints the material explorer's furnace comparison is judged by eye against.
    print(f"E(mu=1, roughness=0)   = {table[0, -1, 0]:.5f}  (expect ~1.0, smooth loses no energy)")
    print(f"E(mu=1, roughness=1)   = {table[-1, -1, 0]:.5f}")
    print(f"E_avg(roughness=1)     = {table[-1, 0, 1]:.5f}")
    if table[0, -1, 0] < 0.99:
        print("  FAIL: a mirror-smooth lobe should reflect essentially all of it.")
        ok = False

    # 4. Check 1 restated as the number a furnace render puts on screen: with F = 1 the Kulla-Conty tint
    #    collapses to 1 and the compensated albedo below closes to 1 exactly when the stored average is the one
    #    the E row integrates to.
    for row in (LUT_SIZE // 4, LUT_SIZE // 2, LUT_SIZE - 1):
        e = table[row, :, 0]
        e_avg_stored = table[row, 0, 1]
        e_avg_reintegrated = 2.0 * np.trapezoid(e * mus, mus)

        compensated = e + (1.0 - e) * (1.0 - e_avg_reintegrated) / (1.0 - e_avg_stored)
        worst = float(np.max(np.abs(compensated - 1.0)))
        print(
            f"Kulla-Conty furnace at roughness {params[row]:.3f}: "
            f"uncompensated E_avg {e_avg_stored:.4f}, worst deviation from 1.0 = {worst:.6f}"
        )
        if worst > 0.01:
            print("  FAIL: compensated albedo does not close to 1.")
            ok = False

    return ok


def write_exr(table: np.ndarray, output_path: Path) -> None:
    """Row 0 is roughness 0 and column 0 is grazing, the order ggxAlbedoLutUv reads back.

    Full float rather than half because Turquin divides by E, where half's ~1e-3 relative error would land in
    the same order as the table's own interpolation error.
    """
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
    args = parser.parse_args()

    if args.check:
        if not args.output.exists():
            print(f"{args.output} does not exist; run without --check to bake it.")
            return 1
        print(f"Validating {args.output}")
        return 0 if validate(read_exr(args.output)) else 1

    print(f"Baking {LUT_SIZE}x{LUT_SIZE} with {DIRECTIONAL_SAMPLES} samples per texel", file=sys.stderr)
    table = bake()

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
