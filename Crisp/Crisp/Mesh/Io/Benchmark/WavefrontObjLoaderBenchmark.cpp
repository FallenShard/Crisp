#include <benchmark/benchmark.h>

#include <Crisp/Mesh/Io/ExternalAssetConfig.hpp>
#include <Crisp/Mesh/Io/WavefrontObjLoader.hpp>

namespace crisp {

void BM_LoadAjax(benchmark::State& state) {
    if (test::kExternalAssetDir.empty()) {
        state.SkipWithError("Set CRISP_EXTERNAL_ASSET_DIR to the full Crisp Resources directory");
        return;
    }

    for (auto _ : state) {
        auto mesh = loadWavefrontObj(test::kExternalAssetDir / "Meshes" / "ajax.obj");
        benchmark::DoNotOptimize(mesh.positions.data());
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_LoadAjax)->Unit(benchmark::kMillisecond); // NOLINT

} // namespace crisp

BENCHMARK_MAIN(); // NOLINT
