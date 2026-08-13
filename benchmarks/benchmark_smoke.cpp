#include <benchmark/benchmark.h>

static void BM_Smoke(benchmark::State& state)
{
    for (auto _ : state) {
        benchmark::DoNotOptimize(42);
    }
}

BENCHMARK(BM_Smoke);
