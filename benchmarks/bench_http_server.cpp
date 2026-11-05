#include "bench_http_server.hpp"

#include <keet/http.hpp>
#include <keet/runtime.hpp>

#include <algorithm>
#include <chrono>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace keety::benchmarks {

namespace {

struct NativeRoute {
    std::string method;
    std::string path_prefix;
    std::function<keet::net::http::HttpResponseMessage(const keet::net::http::HttpRequestMessage&)> handler;
};

// Native C++ Reference Router
int64_t run_native_cpp(size_t n) {
    std::vector<NativeRoute> routes = {
        {"GET", "/", [](const keet::net::http::HttpRequestMessage&) {
            keet::net::http::HttpResponseMessage res;
            res.status = 200;
            res.body = "ROOT_OK";
            return res;
        }},
        {"GET", "/users/", [](const keet::net::http::HttpRequestMessage& req) {
            keet::net::http::HttpResponseMessage res;
            res.status = 200;
            auto it = req.params.find("id");
            res.body = "USER_" + (it != req.params.end() ? it->second : "");
            return res;
        }},
        {"POST", "/api/items", [](const keet::net::http::HttpRequestMessage&) {
            keet::net::http::HttpResponseMessage res;
            res.status = 200;
            res.body = "{\"status\":200}";
            return res;
        }}
    };

    auto& pool = keet::default_thread_pool();
    const size_t num_threads = 4;
    size_t chunk_size = (n + num_threads - 1) / num_threads;

    std::vector<std::future<int64_t>> futures;
    futures.reserve(num_threads);

    for (size_t t = 0; t < num_threads; ++t) {
        size_t start = t * chunk_size;
        size_t end = std::min(start + chunk_size, n);
        if (start >= end) break;

        futures.push_back(pool.enqueue([&routes, start, end]() -> int64_t {
            int64_t local_sum = 0;
            for (size_t i = start; i < end; ++i) {
                std::string method;
                std::string path;
                if (i % 3 == 0) {
                    method = "GET";
                    path = "/";
                } else if (i % 3 == 1) {
                    method = "GET";
                    path = "/users/" + std::to_string(i);
                } else {
                    method = "POST";
                    path = "/api/items";
                }

                keet::net::http::HttpRequestMessage req;
                req.line.method = method;
                req.line.path = path;

                for (const auto& r : routes) {
                    if (r.method == method) {
                        if (r.path_prefix == "/" && path == "/") {
                            auto res = r.handler(req);
                            local_sum += res.status + static_cast<int64_t>(res.body.length());
                            break;
                        } else if (r.path_prefix == "/users/" && path.rfind("/users/", 0) == 0) {
                            req.params["id"] = path.substr(7);
                            auto res = r.handler(req);
                            local_sum += res.status + static_cast<int64_t>(res.body.length());
                            break;
                        } else if (r.path_prefix == "/api/items" && path == "/api/items") {
                            auto res = r.handler(req);
                            local_sum += res.status + static_cast<int64_t>(res.body.length());
                            break;
                        }
                    }
                }
            }
            return local_sum;
        }));
    }

    int64_t total = 0;
    for (auto& fut : futures) {
        total += fut.get();
    }
    return total;
}

// KeetScript Radix Tree HTTP Router Engine
int64_t run_keet_http_router(size_t n) {
    keet::net::http::HttpRouter router;

    router.get("/", [](const keet::net::http::HttpRequestMessage&) -> keet::net::http::HttpResponseMessage {
        keet::net::http::HttpResponseMessage res;
        res.status = 200;
        res.body = "ROOT_OK";
        return res;
    });

    router.get("/users/:id", [](const keet::net::http::HttpRequestMessage& req) -> keet::net::http::HttpResponseMessage {
        keet::net::http::HttpResponseMessage res;
        res.status = 200;
        auto it = req.params.find("id");
        res.body = "USER_" + (it != req.params.end() ? it->second : "");
        return res;
    });

    router.post("/api/items", [](const keet::net::http::HttpRequestMessage&) -> keet::net::http::HttpResponseMessage {
        keet::net::http::HttpResponseMessage res;
        res.status = 200;
        res.body = "{\"status\":200}";
        return res;
    });

    auto& pool = keet::default_thread_pool();
    const size_t num_threads = 4;
    size_t chunk_size = (n + num_threads - 1) / num_threads;

    std::vector<std::future<int64_t>> futures;
    futures.reserve(num_threads);

    for (size_t t = 0; t < num_threads; ++t) {
        size_t start = t * chunk_size;
        size_t end = std::min(start + chunk_size, n);
        if (start >= end) break;

        futures.push_back(pool.enqueue([&router, start, end]() -> int64_t {
            int64_t local_sum = 0;
            for (size_t i = start; i < end; ++i) {
                std::string method;
                std::string path;
                if (i % 3 == 0) {
                    method = "GET";
                    path = "/";
                } else if (i % 3 == 1) {
                    method = "GET";
                    path = "/users/" + std::to_string(i);
                } else {
                    method = "POST";
                    path = "/api/items";
                }

                auto match = router.match(method, path);
                if (match.status == keet::net::http::RouteMatchStatus::Found && match.handler) {
                    keet::net::http::HttpRequestMessage req;
                    req.line.method = method;
                    req.line.path = path;
                    req.params = std::move(match.params);

                    auto res = match.handler(req);
                    local_sum += res.status + static_cast<int64_t>(res.body.length());
                }
            }
            return local_sum;
        }));
    }

    int64_t total = 0;
    for (auto& fut : futures) {
        total += fut.get();
    }
    return total;
}

} // anonymous namespace

HttpServerBenchmarkResult run_http_server_benchmark(size_t request_count,
                                                   size_t warmup_iterations,
                                                   size_t measurement_iterations,
                                                   bool verbose) {
    HttpServerBenchmarkResult result{};

    // --- Warm-up runs ---
    for (size_t w = 0; w < warmup_iterations; ++w) {
        volatile int64_t r1 = run_native_cpp(request_count);
        volatile int64_t r2 = run_keet_http_router(request_count);
        (void)r1;
        (void)r2;
    }

    // --- Measurement runs: Native C++ Reference ---
    std::vector<double> cpp_durations;
    cpp_durations.reserve(measurement_iterations);
    for (size_t i = 0; i < measurement_iterations; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        int64_t sum = run_native_cpp(request_count);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        cpp_durations.push_back(ms);
        result.native_cpp_checksum = sum;
    }

    // --- Measurement runs: KeetScript Radix Tree HTTP Engine ---
    std::vector<double> keet_durations;
    keet_durations.reserve(measurement_iterations);
    for (size_t i = 0; i < measurement_iterations; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        int64_t sum = run_keet_http_router(request_count);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        keet_durations.push_back(ms);
        result.keet_checksum = sum;
    }

    // Statistical medians
    std::sort(cpp_durations.begin(), cpp_durations.end());
    std::sort(keet_durations.begin(), keet_durations.end());

    result.native_cpp_median_ms = cpp_durations[cpp_durations.size() / 2];
    result.keet_median_ms = keet_durations[keet_durations.size() / 2];
    result.ratio = result.keet_median_ms / result.native_cpp_median_ms;
    result.requests_per_sec = (static_cast<double>(request_count) / result.keet_median_ms) * 1000.0;

    bool checksum_matches = (result.native_cpp_checksum == result.keet_checksum);
    // Dynamic Radix tree routing with segment splitting has ~1.5-2.5x overhead over hardcoded static prefix matching
    result.passed = checksum_matches && (result.ratio <= 3.0);

    if (verbose) {
        std::cout << "=========================================================\n";
        std::cout << "Benchmark 5: HTTP Server Throughput & Concurrent Latency\n";
        std::cout << "=========================================================\n";
        std::cout << "Request Count:          " << request_count << " requests\n";
        std::cout << "Warm-up Iterations:     " << warmup_iterations << "\n";
        std::cout << "Measurement Iterations: " << measurement_iterations << "\n";
        std::cout << "---------------------------------------------------------\n";
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "Native C++ Median:      " << result.native_cpp_median_ms << " ms\n";
        std::cout << "Keet HTTP Router Median:" << result.keet_median_ms << " ms\n";
        std::cout << "Relative Ratio:         " << result.ratio << "x (target <= 3.00x)\n";
        std::cout << "Throughput:             " << std::setprecision(1) << result.requests_per_sec << " req/sec\n";
        std::cout << "Checksum Validation:    " << (checksum_matches ? "MATCH" : "MISMATCH")
                  << " (" << result.keet_checksum << ")\n";
        std::cout << "Status:                 " << (result.passed ? "PASSED" : "FAILED") << "\n";
        std::cout << "=========================================================\n";
    }

    return result;
}

} // namespace keety::benchmarks
