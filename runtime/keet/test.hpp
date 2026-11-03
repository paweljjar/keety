#pragma once

#include <keet/error.hpp>
#include <keet/io.hpp>
#include <keet/sys.hpp>
#include <keet/time.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace keet::test {

/// Error raised by failed assertions in KeetScript test cases.
struct AssertionError : public keet::error {
    std::string expected;
    std::string actual;
    std::string file;
    int line = 0;

    explicit AssertionError(std::string msg,
                            std::string exp = "",
                            std::string act = "",
                            std::string f = "",
                            int l = 0)
        : keet::error("AssertionError", std::move(msg)),
          expected(std::move(exp)),
          actual(std::move(act)),
          file(std::move(f)),
          line(l) {}
};

/// Details of a single test failure.
struct TestFailure {
    std::string message;
    std::string expected;
    std::string actual;
    std::string file;
    int line = 0;
    std::string errorType = "AssertionError";
};

/// Execution result for an individual test case.
struct TestCaseResult {
    std::string name;
    std::string suite;
    bool passed = true;
    int64_t durationNs = 0;
    std::optional<TestFailure> failure;
};

/// Aggregated summary of a test run.
struct TestSummary {
    int total = 0;
    int passed = 0;
    int failed = 0;
    int skipped = 0;
    int64_t totalDurationNs = 0;
    std::vector<TestCaseResult> results;
};

/// Helper to stringify any value for assertion error messages using keet::io formatting.
template <typename T>
[[nodiscard]] std::string stringifyValue(const T& val) {
    std::ostringstream oss;
    keet::io::detail::print_single_arg(oss, val);
    return oss.str();
}

// Specialization for std::string to wrap in quotes for clarity
template <>
[[nodiscard]] inline std::string stringifyValue<std::string>(const std::string& val) {
    return "\"" + val + "\"";
}

template <>
[[nodiscard]] inline std::string stringifyValue<const char*>(const char* const& val) {
    if (!val) return "null";
    return "\"" + std::string(val) + "\"";
}

/// Assert that two values are deeply equal.
template <typename T, typename U>
void assert_eq(const T& actual, const U& expected, const std::string& msg = "", const std::string& file = "", int line = 0) {
    if constexpr (std::is_same_v<T, U> || (std::is_convertible_v<T, U> && std::is_convertible_v<U, T>)) {
        if (actual == expected) {
            return;
        }
    } else {
        // Fallback for non-directly comparable types
        if (stringifyValue(actual) == stringifyValue(expected)) {
            return;
        }
    }

    std::string actStr = stringifyValue(actual);
    std::string expStr = stringifyValue(expected);
    std::string err = msg.empty() ? "assertEq failed" : msg;
    throw AssertionError(std::move(err), std::move(expStr), std::move(actStr), file, line);
}

/// Assert that two values are not equal.
template <typename T, typename U>
void assert_ne(const T& actual, const U& expected, const std::string& msg = "", const std::string& file = "", int line = 0) {
    bool equal = false;
    if constexpr (std::is_same_v<T, U> || (std::is_convertible_v<T, U> && std::is_convertible_v<U, T>)) {
        equal = (actual == expected);
    } else {
        equal = (stringifyValue(actual) == stringifyValue(expected));
    }

    if (equal) {
        std::string actStr = stringifyValue(actual);
        std::string err = msg.empty() ? "assertNe failed: values are unexpectedly equal" : msg;
        throw AssertionError(std::move(err), "!= " + actStr, actStr, file, line);
    }
}

/// Assert that a boolean condition is true.
inline void assert_true(bool condition, const std::string& msg = "", const std::string& file = "", int line = 0) {
    if (!condition) {
        std::string err = msg.empty() ? "assertTrue failed: expected condition to be true" : msg;
        throw AssertionError(std::move(err), "true", "false", file, line);
    }
}

/// Assert that a boolean condition is false.
inline void assert_false(bool condition, const std::string& msg = "", const std::string& file = "", int line = 0) {
    if (condition) {
        std::string err = msg.empty() ? "assertFalse failed: expected condition to be false" : msg;
        throw AssertionError(std::move(err), "false", "true", file, line);
    }
}

/// Unconditionally fail the current test.
inline void fail(const std::string& msg = "test failed explicitly", const std::string& file = "", int line = 0) {
    throw AssertionError(msg, "", "", file, line);
}

/// Assert that a callback throws a KeetScript error (zonk), optionally matching the error name.
inline void assert_zonks(const std::function<void()>& fn,
                         const std::string& expectedErrorName = "",
                         const std::string& msg = "",
                         const std::string& file = "",
                         int line = 0) {
    try {
        fn();
    } catch (const keet::error& err) {
        if (!expectedErrorName.empty() && err.name != expectedErrorName) {
            std::string failMsg = msg.empty()
                ? "assertZonks: expected error '" + expectedErrorName + "' but caught '" + err.name + "'"
                : msg;
            throw AssertionError(std::move(failMsg), expectedErrorName, err.name, file, line);
        }
        return; // Success: threw expected error
    } catch (const std::exception& ex) {
        if (!expectedErrorName.empty()) {
            std::string failMsg = msg.empty()
                ? "assertZonks: expected error '" + expectedErrorName + "' but caught std::exception '" + ex.what() + "'"
                : msg;
            throw AssertionError(std::move(failMsg), expectedErrorName, ex.what(), file, line);
        }
        return; // Caught generic exception as requested
    }

    std::string failMsg = msg.empty()
        ? "assertZonks failed: expected expression to throw an error, but it succeeded"
        : msg;
    throw AssertionError(std::move(failMsg), expectedErrorName.empty() ? "error" : expectedErrorName, "no error", file, line);
}

/// Definition of a single test case item.
struct TestCase {
    std::string name;
    std::string suite;
    std::function<void()> fn;
    std::string file;
    int line = 0;
};

/// Global registry managing registered test cases and execution.
class Registry {
public:
    static Registry& instance() {
        static Registry reg;
        return reg;
    }

    void registerCase(std::string name, std::string suite, std::function<void()> fn, std::string file = "", int line = 0) {
        cases_.push_back(TestCase{
            std::move(name),
            std::move(suite),
            std::move(fn),
            std::move(file),
            line
        });
    }

    void setSuite(std::string suite) {
        currentSuite_ = std::move(suite);
    }

    [[nodiscard]] const std::string& currentSuite() const noexcept {
        return currentSuite_;
    }

    void clear() {
        cases_.clear();
        currentSuite_.clear();
    }

    [[nodiscard]] const std::vector<TestCase>& cases() const noexcept {
        return cases_;
    }

    /// Execute all registered test cases, optionally filtered by pattern (regex with substring fallback).
    TestSummary runAll(const std::string& filter = "", bool failFast = false) {
        TestSummary summary;
        auto startTotal = std::chrono::steady_clock::now();

        std::optional<std::regex> compiledRegex;
        if (!filter.empty()) {
            try {
                compiledRegex = std::regex(filter, std::regex::icase);
            } catch (const std::regex_error&) {
                compiledRegex = std::nullopt;
            }
        }

        for (const auto& tc : cases_) {
            // Apply filter if specified (regex or substring match across suite, name, or full identifier)
            if (!filter.empty()) {
                std::string full = tc.suite.empty() ? tc.name : tc.suite + " > " + tc.name;
                bool matched = false;
                if (compiledRegex.has_value()) {
                    matched = std::regex_search(full, *compiledRegex) ||
                              std::regex_search(tc.name, *compiledRegex) ||
                              (!tc.suite.empty() && std::regex_search(tc.suite, *compiledRegex));
                } else {
                    matched = (full.find(filter) != std::string::npos) ||
                              (tc.name.find(filter) != std::string::npos) ||
                              (!tc.suite.empty() && tc.suite.find(filter) != std::string::npos);
                }
                if (!matched) {
                    summary.skipped++;
                    continue;
                }
            }

            summary.total++;
            TestCaseResult res;
            res.name = tc.name;
            res.suite = tc.suite;

            auto t0 = std::chrono::steady_clock::now();
            try {
                tc.fn();
                res.passed = true;
                summary.passed++;
            } catch (const AssertionError& ae) {
                res.passed = false;
                summary.failed++;
                TestFailure failInfo;
                failInfo.message = ae.message;
                failInfo.expected = ae.expected;
                failInfo.actual = ae.actual;
                failInfo.file = ae.file.empty() ? tc.file : ae.file;
                failInfo.line = ae.line == 0 ? tc.line : ae.line;
                failInfo.errorType = ae.name;
                res.failure = std::move(failInfo);
            } catch (const keet::error& ke) {
                res.passed = false;
                summary.failed++;
                TestFailure failInfo;
                failInfo.message = ke.message;
                failInfo.file = tc.file;
                failInfo.line = tc.line;
                failInfo.errorType = ke.name;
                res.failure = std::move(failInfo);
            } catch (const std::exception& ex) {
                res.passed = false;
                summary.failed++;
                TestFailure failInfo;
                failInfo.message = ex.what();
                failInfo.file = tc.file;
                failInfo.line = tc.line;
                failInfo.errorType = "std::exception";
                res.failure = std::move(failInfo);
            } catch (...) {
                res.passed = false;
                summary.failed++;
                TestFailure failInfo;
                failInfo.message = "Unknown native exception thrown";
                failInfo.file = tc.file;
                failInfo.line = tc.line;
                failInfo.errorType = "UnknownException";
                res.failure = std::move(failInfo);
            }

            auto t1 = std::chrono::steady_clock::now();
            res.durationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
            summary.results.push_back(std::move(res));

            if (failFast && summary.failed > 0) {
                break;
            }
        }

        auto endTotal = std::chrono::steady_clock::now();
        summary.totalDurationNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endTotal - startTotal).count();
        return summary;
    }

private:
    Registry() = default;
    std::vector<TestCase> cases_;
    std::string currentSuite_;
};

/// Escapes a string for JSON output.
inline std::string jsonEscape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

/// Format test summary into human-readable terminal output.
inline void printSummary(const TestSummary& summary, std::ostream& out, bool color = true) {
    const char* cPass = color ? "\033[32m" : "";
    const char* cFail = color ? "\033[31m" : "";
    const char* cDim  = color ? "\033[90m" : "";
    const char* cBold = color ? "\033[1m"  : "";
    const char* cReset = color ? "\033[0m" : "";

    std::string currentSuite;
    for (const auto& res : summary.results) {
        if (res.suite != currentSuite) {
            currentSuite = res.suite;
            if (!currentSuite.empty()) {
                out << "\n" << cBold << currentSuite << cReset << "\n";
            }
        }

        double durationMs = static_cast<double>(res.durationNs) / 1'000'000.0;
        if (res.passed) {
            out << "  " << cPass << "✓" << cReset << " " << res.name
                << " " << cDim << "(" << durationMs << "ms)" << cReset << "\n";
        } else {
            out << "  " << cFail << "✕" << cReset << " " << res.name
                << " " << cDim << "(" << durationMs << "ms)" << cReset << "\n";
            if (res.failure) {
                const auto& f = *res.failure;
                out << "    " << cFail << f.errorType << ": " << f.message << cReset << "\n";
                if (!f.expected.empty() || !f.actual.empty()) {
                    out << "      " << cDim << "Expected: " << cReset << cPass << f.expected << cReset << "\n";
                    out << "      " << cDim << "Actual:   " << cReset << cFail << f.actual << cReset << "\n";
                }
                if (!f.file.empty()) {
                    out << "      " << cDim << "--> " << f.file;
                    if (f.line > 0) out << ":" << f.line;
                    out << cReset << "\n";
                }
            }
        }
    }

    double totalMs = static_cast<double>(summary.totalDurationNs) / 1'000'000.0;
    out << "\n" << cBold << "Test Results:" << cReset << " ";
    if (summary.failed == 0) {
        out << cPass << summary.passed << " passed" << cReset;
    } else {
        out << cFail << summary.failed << " failed" << cReset << ", "
            << cPass << summary.passed << " passed" << cReset;
    }
    out << " (" << summary.total << " total";
    if (summary.skipped > 0) {
        out << ", " << summary.skipped << " skipped";
    }
    out << ") " << cDim << "[" << totalMs << "ms]" << cReset << "\n";
}

/// Format test summary as JSON.
inline std::string summaryToJson(const TestSummary& summary) {
    std::ostringstream json;
    json << "{\n";
    json << "  \"total\": " << summary.total << ",\n";
    json << "  \"passed\": " << summary.passed << ",\n";
    json << "  \"failed\": " << summary.failed << ",\n";
    json << "  \"skipped\": " << summary.skipped << ",\n";
    json << "  \"totalDurationNs\": " << summary.totalDurationNs << ",\n";
    json << "  \"results\": [\n";

    for (size_t i = 0; i < summary.results.size(); ++i) {
        const auto& r = summary.results[i];
        json << "    {\n";
        json << "      \"name\": \"" << jsonEscape(r.name) << "\",\n";
        json << "      \"suite\": \"" << jsonEscape(r.suite) << "\",\n";
        json << "      \"passed\": " << (r.passed ? "true" : "false") << ",\n";
        json << "      \"durationNs\": " << r.durationNs;
        if (r.failure) {
            json << ",\n      \"failure\": {\n";
            json << "        \"message\": \"" << jsonEscape(r.failure->message) << "\",\n";
            json << "        \"expected\": \"" << jsonEscape(r.failure->expected) << "\",\n";
            json << "        \"actual\": \"" << jsonEscape(r.failure->actual) << "\",\n";
            json << "        \"file\": \"" << jsonEscape(r.failure->file) << "\",\n";
            json << "        \"line\": " << r.failure->line << ",\n";
            json << "        \"errorType\": \"" << jsonEscape(r.failure->errorType) << "\"\n";
            json << "      }\n";
        } else {
            json << "\n";
        }
        json << "    }" << (i + 1 < summary.results.size() ? "," : "") << "\n";
    }

    json << "  ]\n";
    json << "}\n";
    return json.str();
}

/// Format test summary as TAP version 13.
inline std::string summaryToTap(const TestSummary& summary, const std::string& filePrefix = "") {
    std::ostringstream tap;
    tap << "TAP version 13\n";
    tap << "1.." << summary.results.size() << "\n";
    for (size_t i = 0; i < summary.results.size(); ++i) {
        const auto& r = summary.results[i];
        std::string fullName;
        if (!filePrefix.empty()) {
            fullName = filePrefix + " > ";
        }
        if (!r.suite.empty()) {
            fullName += r.suite + " > ";
        }
        fullName += r.name;

        int64_t caseMs = r.durationNs / 1'000'000;
        if (r.passed) {
            tap << "ok " << (i + 1) << " - " << fullName << " # time=" << caseMs << "ms\n";
        } else {
            tap << "not ok " << (i + 1) << " - " << fullName << " # time=" << caseMs << "ms\n";
            if (r.failure) {
                tap << "  ---\n";
                tap << "  message: \"" << jsonEscape(r.failure->message) << "\"\n";
                if (!r.failure->expected.empty()) {
                    tap << "  expected: \"" << jsonEscape(r.failure->expected) << "\"\n";
                }
                if (!r.failure->actual.empty()) {
                    tap << "  actual: \"" << jsonEscape(r.failure->actual) << "\"\n";
                }
                if (!r.failure->file.empty()) {
                    tap << "  file: \"" << jsonEscape(r.failure->file) << "\"\n";
                }
                if (r.failure->line > 0) {
                    tap << "  line: " << r.failure->line << "\n";
                }
                tap << "  ...\n";
            }
        }
    }
    return tap.str();
}

} // namespace keet::test

namespace keet_module_test {

/// User-facing KeetScript registration: test.case(name, fn)
inline void _keet_case(const std::string& name, const std::function<void()>& fn) {
    auto& reg = keet::test::Registry::instance();
    reg.registerCase(name, reg.currentSuite(), fn);
}

/// User-facing KeetScript registration: test.suite(name, fn)
inline void _keet_suite(const std::string& name, const std::function<void()>& fn) {
    auto& reg = keet::test::Registry::instance();
    std::string prev = reg.currentSuite();
    reg.setSuite(prev.empty() ? name : prev + " > " + name);
    try {
        fn();
    } catch (...) {
        reg.setSuite(prev);
        throw;
    }
    reg.setSuite(prev);
}

/// Assert equality
template <typename T, typename U>
inline void assertEq(const T& actual, const U& expected, const std::string& msg = "") {
    keet::test::assert_eq(actual, expected, msg);
}

/// Assert inequality
template <typename T, typename U>
inline void assertNe(const T& actual, const U& expected, const std::string& msg = "") {
    keet::test::assert_ne(actual, expected, msg);
}

/// Assert true
inline void assertTrue(bool cond, const std::string& msg = "") {
    keet::test::assert_true(cond, msg);
}

/// Assert false
inline void assertFalse(bool cond, const std::string& msg = "") {
    keet::test::assert_false(cond, msg);
}

/// Assert zonks (throws KeetScript error)
inline void assertZonks(const std::function<void()>& fn,
                        const std::string& expectedError = "",
                        const std::string& msg = "") {
    keet::test::assert_zonks(fn, expectedError, msg);
}

/// Unconditionally fail
inline void fail(const std::string& msg = "test failed explicitly") {
    keet::test::fail(msg);
}

/// Run all registered tests and return exit code (0 if all passed, 1 if any failed)
inline int64_t run(const std::string& filter = "") {
    std::string activeFilter = filter;
    bool jsonMode = false;
    bool tapMode = false;
    bool failFast = false;
    const auto& args = keet::sys::args();
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--json" || args[i] == "--keet-test-json") jsonMode = true;
        else if (args[i] == "--tap") tapMode = true;
        else if (args[i] == "--fail-fast") failFast = true;
        else if (activeFilter.empty() && (args[i] == "--filter" || args[i] == "-f") && i + 1 < args.size()) activeFilter = args[++i];
        else if (activeFilter.empty() && args[i].rfind("--filter=", 0) == 0) activeFilter = args[i].substr(9);
        else if (activeFilter.empty() && args[i].rfind("-f=", 0) == 0) activeFilter = args[i].substr(3);
    }
    auto summary = keet::test::Registry::instance().runAll(activeFilter, failFast);
    if (jsonMode) {
        std::cout << keet::test::summaryToJson(summary) << "\n";
    } else if (tapMode) {
        std::cout << keet::test::summaryToTap(summary) << "\n";
    } else {
        keet::test::printSummary(summary, std::cout, true);
    }
    return summary.failed > 0 ? 1 : 0;
}

} // namespace keet_module_test
