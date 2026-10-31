#pragma once

#include <cstddef>
#include <cstdint>

namespace keety::benchmarks {

/// Results of Benchmark 4: Asynchronous Task Concurrency
struct TaskConcurrencyBenchmarkResult {
    double native_cpp_median_ms{0.0};
    double keet_median_ms{0.0};
    double ratio{0.0};
    int64_t native_cpp_checksum{0};
    int64_t keet_checksum{0};
    bool passed{false};
};

/// Runs Benchmark 4: Asynchronous Task Concurrency
/// Compares idiomatic native C++17 against KeetScript generated C++ execution
TaskConcurrencyBenchmarkResult run_task_concurrency_benchmark(size_t task_count = 50000,
                                                            size_t warmup_iterations = 3,
                                                            size_t measurement_iterations = 5,
                                                            bool verbose = false);

} // namespace keety::benchmarks
