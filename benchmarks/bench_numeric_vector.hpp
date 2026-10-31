#pragma once

#include <cstddef>
#include <cstdint>

namespace keety::benchmarks {

/// Results of Benchmark 1: Heavy Numeric Vector Processing
struct NumericVectorBenchmarkResult {
    double native_cpp_median_ms{0.0};
    double keet_median_ms{0.0};
    double ratio{0.0};
    int64_t native_cpp_checksum{0};
    int64_t keet_checksum{0};
    bool passed{false};
};

/// Runs Benchmark 1: Heavy Numeric Vector Processing
/// Compares idiomatic native C++17 against KeetScript generated C++ execution
NumericVectorBenchmarkResult run_numeric_vector_benchmark(size_t vector_size = 10000000,
                                                         size_t warmup_iterations = 5,
                                                         size_t measurement_iterations = 10,
                                                         bool verbose = false);

} // namespace keety::benchmarks
