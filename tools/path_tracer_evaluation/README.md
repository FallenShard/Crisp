# Vulkan path-tracer comparison

Renders the same scene through `VulkanRayTracingScene` and through Mitsuba, then compares the two images. Both
sides get the same OBJ meshes, materials, emitter, camera, pixel filter, and path depth.

```powershell
uv sync                                          # once
uv run python tools/evaluate_path_tracer.py      # every case, every metric
```

That renders any reference that is missing, captures each case from a release `CrispMain`, compares the linear
HDR pixels, and writes `Output/PathTracerEvaluation/report.html`.

| Flag | Effect |
| --- | --- |
| `--case NAME` | Run one case; repeatable. |
| `--spp N` | Override every case's sample count. |
| `--preset NAME` | Build preset supplying `CrispMain` (default `x64-release`). |
| `--output-dir DIR` | Somewhere other than `Output/PathTracerEvaluation`. |
| `--manifest FILE` | A case list other than `cases.json`. |
| `--skip-flip` | Drop HDR-FLIP, which dominates the runtime. |
| `--force-references` / `--force-captures` | Ignore what is already on disk. |

```text
references/  Mitsuba EXRs, each with a .json giving the spp, seed, variant and XML hash it was made with
candidates/  Crisp captures, and the engine config generated for each case
artifacts/   Previews, absolute-error heatmaps, FLIP heatmaps
report.json  Every metric, machine-readable
report.html  Self-contained, all images inlined
```

Both sides are cached, so re-running usually just redoes the comparison. A reference is re-rendered if its EXR
is gone, or if the .json beside it disagrees with the scene's spp or seed, the Mitsuba variant, the hash of
the XML, or hashes of assets listed in `assetFiles`. A capture is redone if the config generated for it changed.
That config is the cache key, so editing an input cannot leave an old capture sitting next to a fresh reference.

## Environment

`pyproject.toml` pins CPython 3.13. `flip-evaluator` has no wheel past it, and it has to import in the same
process as Mitsuba. `uv.lock` is committed, `.venv/` is not. The driver checks its own interpreter for both
packages up front, before it spends any time rendering.

## Cases

`cases.json` pairs a Mitsuba scene with the Crisp scene it mirrors:

```json
{
  "name": "cornell_box",
  "mitsubaScene": "mitsuba/cornell_box.xml",
  "crispScene": "tools/path_tracer_evaluation/crisp/cornell_box.json"
}
```

`mitsubaScene` is relative to this directory. `crispScene` is looked up under `Resources/` first, then under
the repository root. `samplesPerPixel` and `seed` come from the Crisp scene's `sampler` and drive both
renderers, so a reference scene should spell out `maxDepth`, `samplesPerPixel`, `seed`, `imageSize`, the whole
camera basis, and `reconstructionFilter`. `mitsubaVariant` and `samplesPerFrame` are set in the manifest.
`--spp` writes a temporary copy of the scene, so the renderer still takes every deterministic setting from a
scene file. The capture is named after the Mitsuba scene's stem, and that is how the two images get paired.

Adding a case means writing a Mitsuba XML that matches the Crisp scene's geometry, materials, emitter, camera
and path depth. Nothing checks that they match. When they do not, you get a metric regression with no bug
behind it.

The `homogeneous_medium` case isolates a bounded RGB medium between a point light and a diffuse plane. Mitsuba
uses `volpath` and a null cube for the boundary; its `sigma_t` is Crisp's absorption plus scattering, and its
albedo is scattering divided by `sigma_t` per channel. Both sides use the same Henyey-Greenstein anisotropy.
Run it with `uv run python tools/evaluate_path_tracer.py --case homogeneous_medium`.

The `grid_medium` case reads one scalar density file on both sides. Regenerate the checked-in
`Resources/Volumes/reference_density.vol` with
`uv run python tools/path_tracer_evaluation/generate_grid_volume.py`. Crisp samples it as a clamped, linearly
filtered 3D texture and takes its world bounds from the VOL header. Mitsuba uses `gridvolume` with the same bounds,
trilinear filter, constant albedo, and HG phase. The scene multiplies density by its extinction coefficients;
the reference applies the same scalar factor through the medium's `scale` parameter.

Two conventions the XML has to get right:

- Crisp point and directional lights carry total `power`. For a point light, Mitsuba wants `power / (4 pi)` as
  radiant intensity. For a directional light, `power` maps straight to irradiance.
- Rough dielectric turns off Mitsuba's visible-normal sampling, so both sides sample the full GGX or Beckmann
  distribution. That changes variance only, not the microfacet model.

A case can set `thresholds.maximum` and `thresholds.minimum` on any metric in `report.json`, and the driver
exits non-zero when a limit is breached. Negative and non-finite values always fail, whatever the thresholds
say.

## Metrics

Every metric runs on every case. The report shows mean and P95 FLIP, relative L2, HDR PSNR, MAE, RMSE,
absolute-error P95 and P99, and luminance bias. `report.json` also carries medians, maxima, MSE, raw L2 and the
PSNR peak. HDR-FLIP comes from NVIDIA's `flip-evaluator` and costs about 1.4 s for a 1920x1080 pair.

## Running one stage

`render_mitsuba.py` takes a folder of XMLs or a single file, `evaluate_renderings.py` takes a reference
directory and one or more candidate directories, and `generate_render_report.py` turns the report JSON into
HTML. They are separate processes because Mitsuba's variant is process-global: rendering wants `cuda_ad_rgb`,
reading EXRs wants `scalar_rgb`.
