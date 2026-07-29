# Crisp developer workflow

This guide describes the supported Windows development workflow. Crisp uses
CMake presets as the source of truth and cmuck as a thin command-line interface
for activating MSVC, discovering targets through the CMake File API, and
invoking CMake and CTest.

## Prerequisites

- Visual Studio with the Desktop development with C++ workload
- CMake 3.28 or newer
- Ninja
- Git
- Python 3.11 or newer
- A Vulkan-capable device and current driver for renderer applications and
  device-backed tests

Install cmuck in editable mode from the repository root. Click and Rich are
installed from its declared Python dependencies.

```powershell
python -m pip install --editable .\tools\cmuck
```

cmuck searches parent directories for `CMakePresets.json`, so its commands work
from the repository root or any directory below it.

## Presets and modes

| Mode | Configure preset | Purpose |
| --- | --- | --- |
| `@mode/dev` | `x64-debug` | Development and debugging |
| `@mode/opt` | `x64-release` | Optimized applications and benchmarks |

The aliases are declared in `CMakePresets.json`. Every cmuck command also
accepts the corresponding `--preset` name directly.

## Configure

Configure once before asking cmuck to discover targets or tests:

```powershell
cmuck @mode/dev configure
cmuck @mode/opt configure
```

Arguments after `--` are passed to CMake. Quote a complete `-D` argument when
it contains a CMake type annotation such as `:PATH`; omitting a redundant type
is usually simpler in PowerShell.

```powershell
cmuck @mode/dev configure -- -DCRISP_BUILD_BENCHMARKS=OFF
```

## Discover and build targets

List all Crisp-owned targets or filter by category:

```powershell
cmuck @mode/dev targets
cmuck @mode/dev targets --type application
cmuck @mode/dev targets --type test
cmuck @mode/dev targets --type benchmark
```

Build one or more explicit targets. cmuck accepts an exact name or an
unambiguous partial name.

```powershell
cmuck @mode/dev build CrispMain
cmuck @mode/dev build CrispMain CrispJsonUtilsTest
```

Arguments after `--` are forwarded to `cmake --build`:

```powershell
cmuck @mode/dev build CrispMain -- --verbose
```

## Run applications

`cmuck run` builds the selected application before launching it from the
repository root. Arguments after `--` belong to the application.

```powershell
cmuck @mode/dev run CrispMain -- --config Args.json
```

## Tests

List all discovered CTest cases or filter their names:

```powershell
cmuck @mode/dev tests
cmuck @mode/dev tests Wavefront
```

Run every case owned by one test target, or select one complete CTest name:

```powershell
cmuck @mode/dev test CrispJsonUtilsTest
cmuck @mode/dev test CrispGltfLoaderTest.GltfLoaderTest.LoadsTrackedTriangle
```

With no selector, cmuck builds all Crisp test targets and runs the complete
suite. Many renderer and Vulkan tests require a working Vulkan device.

```powershell
cmuck @mode/dev test
```

CTest arguments follow `--`:

```powershell
cmuck @mode/dev test CrispJsonUtilsTest -- --repeat until-fail:10
```

cmuck intentionally leaves debugger integration to the editor. To debug one
case, configure `x64-debug`, select that preset in VS Code's CMake Tools
extension, and use the Test Explorer's **Debug Test** action.

## Benchmarks

Use the optimized mode for benchmarks. `cmuck bench` builds the benchmark target
before launching it, and forwards arguments after `--` to Google Benchmark.

```powershell
cmuck @mode/opt bench CrispWavefrontObjLoaderBenchmark
cmuck @mode/opt bench CrispWavefrontObjLoaderBenchmark -- --benchmark_filter=BM_LoadAjax
```

## Complete preset workflows

The CMake workflow presets configure, build, and test an entire configuration.
Direct CMake commands do not activate MSVC, so run these from a Visual Studio
Developer PowerShell or Developer Command Prompt.

```powershell
cmake --workflow --preset check-windows-debug
cmake --workflow --preset check-windows-release
```

## Optional external assets

Ordinary unit tests use small tracked fixtures. The large OBJ regressions,
Avocado glTF test, and OBJ benchmark are enabled by configuring the full local
`Resources` directory:

```powershell
cmuck @mode/dev configure -- -DCRISP_EXTERNAL_ASSET_DIR=D:\Projects\Crisp\Resources
```

The expected files include `Meshes\ajax.obj`, `Meshes\buddha.obj`,
`Meshes\shader_ball.obj`, `Meshes\camelhead.obj`, and
`glTFSamples\2.0\Avocado\glTF\Avocado.gltf`. The path is stored in the selected
build tree's CMake cache. Clear it with:

```powershell
cmuck @mode/dev configure -- -DCRISP_EXTERNAL_ASSET_DIR=
```

## Dependencies

The first configure populates pinned `FetchContent` dependencies. Ordinary
configures reuse those checkouts. There is no routine dependency-refresh
command: update the relevant `GIT_TAG` in `cmake\Dependencies.cmake` and run one
ordinary configure when deliberately changing a dependency version.

If dependency population becomes corrupted, follow the clean-recovery procedure
below rather than editing generated dependency sources.

## CMake File API recovery

If cmuck reports missing, stale, or inconsistent File API metadata:

1. Stop concurrent configure and build operations.
2. Remove only the reply directory for the affected preset.
3. Run one ordinary configure.

For Debug:

```powershell
Remove-Item -LiteralPath .\build\x64-debug\.cmake\api\v1\reply -Recurse -Force -ErrorAction SilentlyContinue
cmuck @mode/dev configure
```

The File API query is retained and cmuck also recreates it before configuring.

## Clean recovery

Deleting a complete preset build tree is the final fallback, not the first
response to stale metadata. Stop every configure, build, test, and running Crisp
process first, then remove only the affected preset directory and configure it
again.

```powershell
Remove-Item -LiteralPath .\build\x64-debug -Recurse -Force
cmuck @mode/dev configure
```

Use `build\x64-release` and `@mode/opt` for the optimized tree. Source files,
`CMakePresets.json`, and other tracked project configuration are not part of
this recovery operation.
