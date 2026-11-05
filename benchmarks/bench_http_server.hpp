#pragma once

#include <cstddef>
#include <cstdint>

namespace keety::benchmarks {

/// Results of Benchmark 5: HTTP Server Throughput & Concurrent Latency
struct HttpServerBenchmarkResult {
    double native_cpp_median_ms{0.0};
    double keet_median_ms{0.0};
    double ratio{0.0};
    double requests_per_sec{0.0};
    int64_t native_cpp_checksum{0};
    int64_t keet_checksum{0};
    bool passed{false};
};

/// Runs Benchmark 5: HTTP Server Throughput & Concurrent Latency
/// Compares raw request routing and handler dispatch between native C++ and Keet HTTP runtime
HttpServerBenchmarkResult run_http_server_benchmark(size_t request_count = 10000,
                                                   size_t warmup_iterations = 2,
                                                   size_t measurement_iterations = 3,
                                                   bool verbose = false);

} // namespace keety::benchmarks
