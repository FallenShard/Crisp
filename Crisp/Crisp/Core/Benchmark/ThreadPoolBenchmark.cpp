#include <benchmark/benchmark.h>

#include <Crisp/Core/ThreadPool.hpp>

namespace crisp {

constexpr size_t N = 10'000'000;

void BM_SimpleThreadPool(benchmark::State& state) {
    // Perform setup here
    ThreadPool threadPool(state.range(0));
    std::vector<double> size(N, 0);
    for (auto _ : state) {
        threadPool.parallelFor(
            size.size(), [&size]([[maybe_unused]] const size_t globalIdx, [[maybe_unused]] const size_t i) {
                double s = 0;
                for (uint32_t k = 0; k < 5; ++k) {
                    s += std::exp(s);
                    s += std::cos(s);
                    s += std::acos(s);
                }
                size[globalIdx] += s;
            });
        benchmark::DoNotOptimize(size);
    }
}

BENCHMARK(BM_SimpleThreadPool)->Unit(benchmark::kMillisecond)->RangeMultiplier(2)->Range(1, 32)->UseRealTime(); // NOLINT

void BM_SimpleThreadPoolHwConc(benchmark::State& state) {
    // Perform setup here
    ThreadPool threadPool;
    fmt::print("Using {} workers.\n", threadPool.getThreadCount());
    std::vector<double> size(N, 0);
    for (auto _ : state) {
        threadPool.parallelFor(
            size.size(), [&size]([[maybe_unused]] const size_t globalIdx, [[maybe_unused]] const size_t i) {
                double s = 0;
                for (uint32_t k = 0; k < 5; ++k) {
                    s += std::exp(s);
                    s += std::cos(s);
                    s += std::acos(s);
                }
                size[globalIdx] += s;
            });
        benchmark::DoNotOptimize(size);
    }
}

BENCHMARK(BM_SimpleThreadPoolHwConc)->Unit(benchmark::kMillisecond)->UseRealTime(); // NOLINT

void BM_SimpleThreadPoolST(benchmark::State& state) {
    // Perform setup here
    ThreadPool threadPool;
    fmt::print("Using {} workers.\n", threadPool.getThreadCount());
    std::vector<double> size(N, 0);
    for (auto _ : state) {
        for (uint32_t globalIdx = 0; globalIdx < size.size(); ++globalIdx) {
            double s = 0;
            for (uint32_t k = 0; k < 5; ++k) {
                s += std::exp(s);
                s += std::cos(s);
                s += std::acos(s);
            }
            size[globalIdx] += s;
        }
        benchmark::DoNotOptimize(size);
    }
}

BENCHMARK(BM_SimpleThreadPoolST)->Unit(benchmark::kMillisecond)->UseRealTime(); // NOLINT

} // namespace crisp

// Run the benchmark.
BENCHMARK_MAIN(); // NOLINT