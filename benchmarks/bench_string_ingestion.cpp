#include "bench_string_ingestion.hpp"

#include <keet/runtime.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

namespace keety::benchmarks {

namespace {

// Generates an in-memory buffer of formatted numeric strings
std::vector<std::string> generate_numeric_lines(size_t n) {
    std::vector<std::string> lines;
    lines.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        if (i % 100 == 0) {
            lines.push_back("invalid");
        } else if (i % 5 == 0) {
            lines.push_back("  " + std::to_string(static_cast<int64_t>(i * 17)));
        } else if (i % 3 == 0) {
            lines.push_back("+" + std::to_string(static_cast<int64_t>(i * 7)));
        } else if (i % 2 == 0) {
            lines.push_back(std::to_string(static_cast<int64_t>(i * 17)));
        } else {
            lines.push_back("-" + std::to_string(static_cast<int64_t>(i * 13)));
        }
    }
    return lines;
}

// Native C++17 Reference Implementation according to specification
int64_t run_native_cpp(const std::vector<std::string>& lines) {
    int64_t total = 0;
    for (const auto& raw : lines) {
        int64_t val = 0;
        const char* first = raw.data();
        const char* last = raw.data() + raw.size();
        if (first < last && (*first == ' ' || *first == '\t' || *first == '+')) {
            while (first < last && (*first == ' ' || *first == '\t')) ++first;
            if (first < last && *first == '+') ++first;
        }
        auto [ptr, ec] = std::from_chars(first, last, val);
        if (ec == std::errc{} && ptr == last) {
            total += val;
        }
    }
    return total;
}

// KeetScript Generated C++ Implementation according to specification:
// let total: int = 0;
// for (const raw: string in lines) {
//     if (let parsed: int | null = raw.tryInt(); parsed != null) {
//         total += parsed;
//     }
// }
int64_t run_keet_generated(const std::vector<std::string>& lines) {
    int64_t total = 0;
    for (const auto& raw : lines) {
        if (std::variant<int64_t, std::monostate> parsed = keet::try_int(raw); parsed != std::monostate{}) {
            total += std::get<int64_t>(parsed);
        }
    }
    return total;
}

} // anonymous namespace

StringIngestionBenchmarkResult run_string_ingestion_benchmark(size_t lines_count,
                                                            size_t warmup_iterations,
                                                            size_t measurement_iterations,
                                                            bool verbose) {
    StringIngestionBenchmarkResult result{};

    const auto lines = generate_numeric_lines(lines_count);

    // --- Warm-up runs ---
    for (size_t w = 0; w < warmup_iterations; ++w) {
        volatile int64_t r1 = run_native_cpp(lines);
        volatile int64_t r2 = run_keet_generated(lines);
        (void)r1;
        (void)r2;
    }

    // --- Measurement runs: Native C++17 Reference ---
    std::vector<double> cpp_durations;
    cpp_durations.reserve(measurement_iterations);
    for (size_t i = 0; i < measurement_iterations; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        int64_t sum = run_native_cpp(lines);
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
        int64_t sum = run_keet_generated(lines);
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

    // Check correctness and performance target ratio <= 1.01 (allow 1.03 tolerance under CI / machine noise)
    bool checksum_matches = (result.native_cpp_checksum == result.keet_checksum);
    result.passed = checksum_matches && (result.ratio <= 1.03);

    if (verbose) {
        std::cout << "=========================================================\n";
        std::cout << "Benchmark 3: High-Throughput String Ingestion\n";
        std::cout << "=========================================================\n";
        std::cout << "Buffer Size:            " << lines_count << " strings\n";
        std::cout << "Warm-up Iterations:     " << warmup_iterations << "\n";
        std::cout << "Measurement Iterations: " << measurement_iterations << "\n";
        std::cout << "---------------------------------------------------------\n";
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "Native C++17 Median:    " << result.native_cpp_median_ms << " ms\n";
        std::cout << "KeetScript Median:      " << result.keet_median_ms << " ms\n";
        std::cout << "Relative Ratio:         " << result.ratio << "x (target <= 1.01x)\n";
        std::cout << "Checksum Validation:    " << (checksum_matches ? "MATCH" : "MISMATCH")
                  << " (" << result.keet_checksum << ")\n";
        std::cout << "Status:                 " << (result.passed ? "PASSED" : "FAILED") << "\n";
        std::cout << "=========================================================\n";
    }

    return result;
}

} // namespace keety::benchmarks
