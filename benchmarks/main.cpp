#include "bench_numeric_vector.hpp"
#include "bench_hash_map.hpp"
#include "bench_string_ingestion.hpp"
#include "bench_task_concurrency.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

int main(int argc, char* argv[]) {
    bool verbose = true;
    int target_benchmark = 0; // 0 = all benchmarks
    size_t custom_size = 0;
    size_t warmup = 5;
    size_t measure = 10;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-q" || arg == "--quiet") {
            verbose = false;
        } else if (arg == "--benchmark" && i + 1 < argc) {
            target_benchmark = std::atoi(argv[++i]);
        } else if (arg == "--size" && i + 1 < argc) {
            custom_size = static_cast<size_t>(std::atoll(argv[++i]));
        } else if (arg == "--warmup" && i + 1 < argc) {
            warmup = static_cast<size_t>(std::atoi(argv[++i]));
        } else if (arg == "--measure" && i + 1 < argc) {
            measure = static_cast<size_t>(std::atoi(argv[++i]));
        }
    }

    bool all_passed = true;

    // Benchmark 1: Heavy Numeric Vector Processing
    if (target_benchmark == 1 || target_benchmark == 0) {
        size_t b1_size = custom_size > 0 ? custom_size : 10000000;
        auto res = keety::benchmarks::run_numeric_vector_benchmark(b1_size, warmup, measure, verbose);
        if (!res.passed) {
            all_passed = false;
        }
    }

    // Benchmark 2: Hash Map & Union Variant Dispatch
    if (target_benchmark == 2 || target_benchmark == 0) {
        size_t b2_size = custom_size > 0 ? custom_size : 1000000;
        auto res = keety::benchmarks::run_hash_map_benchmark(b2_size, warmup, measure, verbose);
        if (!res.passed) {
            all_passed = false;
        }
    }

    // Benchmark 3: High-Throughput String Ingestion
    if (target_benchmark == 3 || target_benchmark == 0) {
        size_t b3_size = custom_size > 0 ? custom_size : 5000000;
        auto res = keety::benchmarks::run_string_ingestion_benchmark(b3_size, warmup, measure, verbose);
        if (!res.passed) {
            all_passed = false;
        }
    }

    // Benchmark 4: Asynchronous Task Concurrency
    if (target_benchmark == 4 || target_benchmark == 0) {
        size_t b4_size = custom_size > 0 ? custom_size : 50000;
        size_t b4_warmup = (warmup == 5) ? 3 : warmup;
        size_t b4_measure = (measure == 10) ? 5 : measure;
        auto res = keety::benchmarks::run_task_concurrency_benchmark(b4_size, b4_warmup, b4_measure, verbose);
        if (!res.passed) {
            all_passed = false;
        }
    }

    return all_passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
