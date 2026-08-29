# Vulkan path-tracer comparison

This fixture compares `VulkanRayTracingScene` with a Mitsuba reference using
the same OBJ meshes, material parameters, emitter, camera, pixel filter, and
maximum path depth.

Set up the pinned environment once:

```powershell
uv sync
```

Then run everything — every case, every metric — with no arguments:

```powershell
uv run python tools/evaluate_path_tracer.py
```

That renders any missing Mitsuba reference, captures each case from a release
`CrispMain`, compares the linear HDR pixels, and writes
`Output/PathTracerEvaluation/report.html` for a human to open.

| Flag | Effect |
| --- | --- |
| `--case NAME` | Run one case; repeatable. |
| `--spp N` | Override every case's sample count. |
| `--preset NAME` | Build preset supplying `CrispMain` (default `x64-release`). |
| `--output-dir DIR` | Somewhere other than `Output/PathTracerEvaluation`. |
| `--manifest FILE` | A case list other than `cases.json`. |
| `--skip-flip` | Drop HDR-FLIP. Rarely worth it; see below. |
| `--force-references` / `--force-captures` | Ignore what is already on disk. |

Output layout under the report directory:

```text
references/  Mitsuba EXRs plus a sidecar recording spp, seed, and variant
candidates/  Crisp captures, the generated engine config per case, and
             whatever else the engine drops in its output dir (pipeline.cache,
             profiler.json)
artifacts/   Previews, absolute-error heatmaps, FLIP heatmaps
report.json  Every metric, machine-readable
report.html  Self-contained, all images inlined
```

Both sides are cached, so a re-run costs only the comparison. A reference
re-renders when its EXR is missing or its sidecar disagrees with the manifest's
spp, seed, or variant. A capture re-runs when its EXR is missing or the stored
config differs from the one the current settings produce — that config is the
cache key precisely because a stale capture sitting beside a freshly rendered
reference would compare two different sample counts and read as a regression.

## Environment

The root `pyproject.toml` pins the toolchain to CPython 3.13, because
`flip-evaluator` publishes no wheel past 3.13 and it has to import alongside
Mitsuba in one process. `uv.lock` is committed; `.venv/` is not.

The driver checks its own interpreter for `mitsuba` and `flip_evaluator` before
doing anything expensive. Running it with the wrong Python used to render a
reference and capture a frame before failing at the comparison.

## Cases

`cases.json` lists what gets evaluated. Each case pairs a Mitsuba scene with the
Crisp scene it mirrors:

```json
{
  "name": "cornell_box",
  "mitsubaScene": "mitsuba/cornell_box.xml",
  "crispScene": "VesperScenes/Nori-PA-4/cbox-mats.json"
}
```

`mitsubaScene` is relative to this directory. `crispScene` first resolves relative
to `Resources/`, preserving existing asset paths, and then relative to the repository
root so tracked evaluation fixtures can live beside their Mitsuba counterparts. Any
of `spp`, `seed`, `mitsubaVariant`, or `samplesPerFrame` can be set per case to override
the manifest `defaults`. The candidate EXR is named after the Mitsuba scene's stem,
which is how the comparison pairs the two images.

Adding a case means authoring a Mitsuba XML that matches the Crisp scene's
geometry, materials, emitter, camera, and path depth. Nothing checks that
correspondence — a mismatch shows up as a metric regression that is not one.

## Metrics

Every metric is computed on every case; there is nothing to select. The report
tabulates mean and P95 FLIP, relative L2, HDR PSNR, MAE, RMSE, absolute-error
P95 and P99, luminance bias, and counts of non-finite and negative pixels.
Images containing non-finite values are rejected rather than scored.
`report.json` additionally carries per-case medians, maxima, MSE, raw L2, and
the PSNR peak, plus cross-run variance when several candidate directories are
compared by hand with `evaluate_renderings.py`.

HDR-FLIP comes from NVIDIA's `flip-evaluator` package. It costs about 1.4 s for
a 1920x1080 pair, so `--skip-flip` exists but rarely earns its keep.

This used to be a hand-written port in `tools/flip_metric.py`, pinned against
the reference implementation by a committed `.npz` fixture, because
`flip-evaluator` and Mitsuba could not share an interpreter. On 3.13 they can.
The port agreed with the package to 3.8e-5 and ran 33x slower, so it was
deleted along with its fixture, its test, and its regeneration script.

## Running a stage by hand

The driver shells out to three scripts that remain usable on their own:
`render_mitsuba.py` (a folder of XMLs, or one XML file), `evaluate_renderings.py`
(a reference directory against one or more candidate directories), and
`generate_render_report.py` (report JSON to HTML). They run as separate
processes because Mitsuba's variant is process-global and rendering wants
`cuda_ad_rgb` while reading EXRs wants `scalar_rgb`.
