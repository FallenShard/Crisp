#include <filesystem>

#include <benchmark/benchmark.h>

#include <Crisp/Mesh/Io/WavefrontObjLoader.hpp>

namespace crisp {
const std::filesystem::path ResourceDir("D:/Projects/Crisp/Resources/Meshes");

void BM_SomeFunction(benchmark::State& state) {
    // Perform setup here
    for (auto _ : state) {
        const auto mesh = loadWavefrontObj(ResourceDir / "ajax.obj");
        (void)mesh;
    }
}

BENCHMARK(BM_SomeFunction)->Unit(benchmark::kMillisecond); // NOLINT

} // namespace crisp

// Run the benchmark.
BENCHMARK_MAIN(); // NOLINT