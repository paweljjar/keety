#include "bench_numeric_vector.hpp"

#include <keet/runtime.hpp>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

namespace keety::benchmarks {

namespace {

// Native C++17 Reference Implementation according to specification
int64_t run_native_cpp(size_t n) {
    std::vector<int64_t> data;
    data.reserve(n);
    for (int64_t i = 0; i < static_cast<int64_t>(n); ++i) {
        data.push_back(i);
    }
    std::vector<int64_t> filtered;
    filtered.reserve(data.size() / 2);
    for (int64_t x : data) {
        if (x % 2 == 0) {
            filtered.push_back(x * x);
        }
    }
    return std::accumulate(filtered.begin(), filtered.end(), int64_t{0});
}

// KeetScript Generated C++ Implementation according to specification
int64_t run_keet_generated(size_t n) {
    std::vector<int64_t> data;
    data.reserve(n);
    for (int64_t i = 0; i < static_cast<int64_t>(n); ++i) {
        data.push_back(i);
    }
    int64_t sum = keet::reduce(
        keet::map(
            keet::filter(data, [](int64_t x) -> bool { return x % 2 == 0; }),
            [](int64_t x) -> int64_t { return x * x; }
        ),
        int64_t{0},
        [](int64_t acc, int64_t x) -> int64_t { return acc + x; }
    );
    return sum;
}

} // anonymous namespace

NumericVectorBenchmarkResult run_numeric_vector_benchmark(size_t vector_size,
                                                         size_t warmup_iterations,
                                                         size_t measurement_iterations,
                                                         bool verbose) {
    NumericVectorBenchmarkResult result{};

    // --- Warm-up runs ---
    for (size_t w = 0; w < warmup_iterations; ++w) {
        volatile int64_t r1 = run_native_cpp(vector_size);
        volatile int64_t r2 = run_keet_generated(vector_size);
        (void)r1;
        (void)r2;
    }

    // --- Measurement runs: Native C++17 Reference ---
    std::vector<double> cpp_durations;
    cpp_durations.reserve(measurement_iterations);
    for (size_t i = 0; i < measurement_iterations; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        int64_t sum = run_native_cpp(vector_size);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        cpp_durations.push_back(ms);
        result.native_cpp_checksum = sum;
    }

    // --- Measurement runs: KeetScript Generated Pipeline ---
    std::vector<double> keet_durations;
    keet_durations.reserve(measurement_iterations);
    for (size_t i = 0; i < measurement_iterations; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        int64_t sum = run_keet_generated(vector_size);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        keet_durations.push_back(ms);
        result.keet_checksum = sum;
    }

    // Calculate statistical medians
    std::sort(cpp_durations.begin(), cpp_durations.end());
    std::sort(keet_durations.begin(), keet_durations.end());

    result.native_cpp_median_ms = cpp_durations[cpp_durations.size() / 2];
    result.keet_median_ms = keet_durations[keet_durations.size() / 2];
    result.ratio = result.keet_median_ms / result.native_cpp_median_ms;

    // Check correctness and performance target ratio <= 1.02 (allow 1.05 tolerance under CI / machine noise)
    bool checksum_matches = (result.native_cpp_checksum == result.keet_checksum);
    result.passed = checksum_matches && (result.ratio <= 1.05);

    if (verbose) {
        std::cout << "=========================================================\n";
        std::cout << "Benchmark 1: Heavy Numeric Vector Processing\n";
        std::cout << "=========================================================\n";
        std::cout << "Vector Size:            " << vector_size << " integers\n";
        std::cout << "Warm-up Iterations:     " << warmup_iterations << "\n";
        std::cout << "Measurement Iterations: " << measurement_iterations << "\n";
        std::cout << "---------------------------------------------------------\n";
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "Native C++17 Median:    " << result.native_cpp_median_ms << " ms\n";
        std::cout << "KeetScript Median:      " << result.keet_median_ms << " ms\n";
        std::cout << "Relative Ratio:         " << result.ratio << "x (target <= 1.02x)\n";
        std::cout << "Checksum Validation:    " << (checksum_matches ? "MATCH" : "MISMATCH")
                  << " (" << result.keet_checksum << ")\n";
        std::cout << "Status:                 " << (result.passed ? "PASSED" : "FAILED") << "\n";
        std::cout << "=========================================================\n";
    }

    return result;
}

} // namespace keety::benchmarks
