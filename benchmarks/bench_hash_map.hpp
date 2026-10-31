#pragma once

#include <cstddef>
#include <cstdint>

namespace keety::benchmarks {

/// Results of Benchmark 2: Hash Map & Union Variant Dispatch
struct HashMapBenchmarkResult {
    double native_cpp_median_ms{0.0};
    double keet_median_ms{0.0};
    double ratio{0.0};
    int64_t native_cpp_checksum{0};
    int64_t keet_checksum{0};
    bool passed{false};
};

/// Runs Benchmark 2: Hash Map & Union Variant Dispatch
/// Compares idiomatic native C++17 against KeetScript generated C++ execution
HashMapBenchmarkResult run_hash_map_benchmark(size_t records = 1000000,
                                             size_t warmup_iterations = 5,
                                             size_t measurement_iterations = 10,
                                             bool verbose = false);

} // namespace keety::benchmarks
