#include "benchmark/benchmark_api.h"
#include <string>
#include <Executor.hpp>


// Define another benchmark
static void BM_StringCopy(benchmark::State& state) {
  auto task = [] {};
  concurrency::Executor<std::function<void()>, 1> executor;
  int i = 0;
  while (state.KeepRunning()) {
    executor.submit(task, ++i);
  }
}
BENCHMARK(BM_StringCopy);

BENCHMARK_MAIN();
