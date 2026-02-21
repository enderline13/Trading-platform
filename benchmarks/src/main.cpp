#include <benchmark/benchmark.h>

static void BM_TwoPlusTwo(benchmark::State& state) {
    for (auto _ : state) {
        int result = 2 + 2;

        benchmark::DoNotOptimize(result);
    }
}

BENCHMARK(BM_TwoPlusTwo);
