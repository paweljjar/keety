#include <catch2/catch_test_macros.hpp>
#include <keet/io.hpp>

#include <iostream>
#include <sstream>
#include <string>

#if !defined(_WIN32)
#include <unistd.h>
#else
#include <io.h>
#endif

namespace {

struct ColorOverrideGuard {
    ColorOverrideGuard(bool enable) {
        keet::io::set_color_enabled(enable);
    }
    ~ColorOverrideGuard() {
        keet::io::reset_color_enabled();
    }
};

} // namespace

TEST_CASE("keet::io ANSI escape styling and color helpers", "[runtime][io]") {
    SECTION("Inline color helpers with colors enabled") {
        ColorOverrideGuard guard(true);
        REQUIRE(keet::io::is_color_enabled());

        REQUIRE(keet::io::red("error") == "\033[31merror\033[0m");
        REQUIRE(keet::io::green("ok") == "\033[32mok\033[0m");
        REQUIRE(keet::io::yellow("warning") == "\033[33mwarning\033[0m");
        REQUIRE(keet::io::blue("info") == "\033[34minfo\033[0m");
        REQUIRE(keet::io::magenta("debug") == "\033[35mdebug\033[0m");
        REQUIRE(keet::io::cyan("trace") == "\033[36mtrace\033[0m");
        REQUIRE(keet::io::white("bright") == "\033[37mbright\033[0m");
        REQUIRE(keet::io::gray("dim") == "\033[90mdim\033[0m");
        REQUIRE(keet::io::bold("highlight") == "\033[1mhighlight\033[0m");

        REQUIRE(keet::io::colorize("custom", keet::io::Color::Red) == "\033[31mcustom\033[0m");
        REQUIRE(keet::io::colorize("plain", keet::io::Color::Default) == "plain");
    }

    SECTION("Inline color helpers with colors disabled") {
        ColorOverrideGuard guard(false);
        REQUIRE_FALSE(keet::io::is_color_enabled());

        REQUIRE(keet::io::red("error") == "error");
        REQUIRE(keet::io::green("ok") == "ok");
        REQUIRE(keet::io::yellow("warning") == "warning");
        REQUIRE(keet::io::blue("info") == "info");
        REQUIRE(keet::io::magenta("debug") == "debug");
        REQUIRE(keet::io::cyan("trace") == "trace");
        REQUIRE(keet::io::white("bright") == "bright");
        REQUIRE(keet::io::gray("dim") == "dim");
        REQUIRE(keet::io::bold("highlight") == "highlight");

        REQUIRE(keet::io::colorize("custom", keet::io::Color::Red) == "custom");
        REQUIRE(keet::io::colorize("plain", keet::io::Color::Default) == "plain");
    }
}

TEST_CASE("keet::io::print and println stream output formatting", "[runtime][io]") {
    ColorOverrideGuard guard(false);

    SECTION("Outputs single primitive and composite types") {
        std::ostringstream oss;

        keet::io::print_to(oss, static_cast<int64_t>(42));
        REQUIRE(oss.str() == "42");
        oss.str("");

        keet::io::print_to(oss, 3.14);
        REQUIRE(oss.str() == "3.14");
        oss.str("");

        keet::io::print_to(oss, true);
        REQUIRE(oss.str() == "true");
        oss.str("");

        keet::io::print_to(oss, false);
        REQUIRE(oss.str() == "false");
        oss.str("");

        keet::io::print_to(oss, keet::BoolByte(true));
        REQUIRE(oss.str() == "true");
        oss.str("");

        keet::io::print_to(oss, keet::BoolByte(false));
        REQUIRE(oss.str() == "false");
        oss.str("");

        keet::io::print_to(oss, std::string("keet"));
        REQUIRE(oss.str() == "keet");
    }

    SECTION("Space separation across multiple arguments") {
        std::ostringstream oss;
        keet::io::println_to(oss, "Status:", 200, true, "OK");
        REQUIRE(oss.str() == "Status: 200 true OK\n");
    }

    SECTION("Zero argument print and println behavior") {
        std::ostringstream oss;
        keet::io::print_to(oss);
        REQUIRE(oss.str().empty());

        keet::io::println_to(oss);
        REQUIRE(oss.str() == "\n");
    }

    SECTION("Direct stream argument forwarding in print and println") {
        std::ostringstream oss;
        keet::io::print(oss, "Alpha", 100);
        REQUIRE(oss.str() == "Alpha 100");
        oss.str("");

        keet::io::println(oss, "Beta", 200);
        REQUIRE(oss.str() == "Beta 200\n");
    }

    SECTION("Global std::cout redirection") {
        std::ostringstream oss;
        auto* oldBuf = std::cout.rdbuf(oss.rdbuf());

        keet::io::println("Redirected", 777);

        std::cout.rdbuf(oldBuf);
        REQUIRE(oss.str() == "Redirected 777\n");
    }
}

TEST_CASE("keet::io whole-line color formatting", "[runtime][io]") {
    SECTION("Trailing Color formatting with colors enabled") {
        ColorOverrideGuard guard(true);
        std::ostringstream oss;

        keet::io::println_to(oss, "Fatal failure", keet::io::Color::Red);
        REQUIRE(oss.str() == "\033[31mFatal failure\033[0m\n");
        oss.str("");

        keet::io::print_to(oss, "Notice:", 123, keet::io::Color::Cyan);
        REQUIRE(oss.str() == "\033[36mNotice: 123\033[0m");
        oss.str("");

        keet::io::println_to(oss, "Standard line", keet::io::Color::Default);
        REQUIRE(oss.str() == "Standard line\n");
    }

    SECTION("Trailing Color formatting with colors disabled") {
        ColorOverrideGuard guard(false);
        std::ostringstream oss;

        keet::io::println_to(oss, "Fatal failure", keet::io::Color::Red);
        REQUIRE(oss.str() == "Fatal failure\n");
        oss.str("");

        keet::io::print_to(oss, "Notice:", 123, keet::io::Color::Cyan);
        REQUIRE(oss.str() == "Notice: 123");
    }

    SECTION("Explicit print_colored and println_colored functions") {
        ColorOverrideGuard guard(true);
        std::ostringstream oss;

        keet::io::println_colored_to(oss, keet::io::Color::Green, "System ready");
        REQUIRE(oss.str() == "\033[32mSystem ready\033[0m\n");
        oss.str("");

        keet::io::print_colored_to(oss, keet::io::Color::Yellow, "Warning:", 404);
        REQUIRE(oss.str() == "\033[33mWarning: 404\033[0m");
    }
}

TEST_CASE("keet::io stream input reading", "[runtime][io]") {
    SECTION("readln with UNIX LF line endings") {
        std::istringstream iss("Alpha\nBeta\nGamma");
        REQUIRE(keet::io::readln(iss) == "Alpha");
        REQUIRE(keet::io::readln(iss) == "Beta");
        REQUIRE(keet::io::readln(iss) == "Gamma");
        REQUIRE(keet::io::readln(iss).empty());
    }

    SECTION("readln with Windows CRLF line endings") {
        std::istringstream iss("First\r\nSecond\r\nThird\r\n");
        REQUIRE(keet::io::readln(iss) == "First");
        REQUIRE(keet::io::readln(iss) == "Second");
        REQUIRE(keet::io::readln(iss) == "Third");
        REQUIRE(keet::io::readln(iss).empty());
    }

    SECTION("readln without terminating newline") {
        std::istringstream iss("SingleLineContent");
        REQUIRE(keet::io::readln(iss) == "SingleLineContent");
        REQUIRE(keet::io::readln(iss).empty());
    }

    SECTION("readLine reads line as variant matching string | null") {
        std::istringstream iss("ReadLineTest\n");
        auto lineRes = keet::io::readLine(iss);
        REQUIRE(std::holds_alternative<std::string>(lineRes));
        REQUIRE(std::get<std::string>(lineRes) == "ReadLineTest");
    }

    SECTION("input prompts and flushes before reading line") {
        std::istringstream inStream("Piotr\r\n");
        std::ostringstream outStream;

        std::string response = keet::io::input("Enter username: ", inStream, outStream);

        REQUIRE(outStream.str() == "Enter username: ");
        REQUIRE(response == "Piotr");
    }

    SECTION("input with empty prompt") {
        std::istringstream inStream("NoPromptResponse\n");
        std::ostringstream outStream;

        std::string response = keet::io::input("", inStream, outStream);

        REQUIRE(outStream.str().empty());
        REQUIRE(response == "NoPromptResponse");
    }
}

namespace {

struct EnvVarGuard {
    std::string var_name;
    std::optional<std::string> old_val;

    EnvVarGuard(std::string name, std::optional<std::string> new_val)
        : var_name(std::move(name)) {
        const char* existing = std::getenv(var_name.c_str());
        if (existing) {
            old_val = std::string(existing);
        }
        set_val(new_val);
    }

    ~EnvVarGuard() {
        set_val(old_val);
    }

    void set_val(const std::optional<std::string>& val) {
#if defined(_WIN32)
        if (val.has_value()) {
            _putenv_s(var_name.c_str(), val->c_str());
        } else {
            _putenv_s(var_name.c_str(), "");
        }
#else
        if (val.has_value()) {
            setenv(var_name.c_str(), val->c_str(), 1);
        } else {
            unsetenv(var_name.c_str());
        }
#endif
    }
};

class SyncTrackingBuffer : public std::stringbuf {
public:
    int sync_calls = 0;

    SyncTrackingBuffer() = default;
    explicit SyncTrackingBuffer(const std::string& s) : std::stringbuf(s) {}

protected:
    int sync() override {
        ++sync_calls;
        return std::stringbuf::sync();
    }
};

class InputOrderCheckBuffer : public std::streambuf {
    std::string data;
    const SyncTrackingBuffer& out_buf;
    bool checked = false;
public:
    bool prompt_flushed_before_read = false;

    InputOrderCheckBuffer(std::string s, const SyncTrackingBuffer& obuf)
        : data(std::move(s)), out_buf(obuf) {
        if (!data.empty()) {
            setg(&data[0], &data[0], &data[0]);
        }
    }

protected:
    int_type underflow() override {
        if (!checked) {
            checked = true;
            if (out_buf.sync_calls > 0) {
                prompt_flushed_before_read = true;
            }
            if (!data.empty()) {
                setg(&data[0], &data[0], &data[0] + data.size());
                return traits_type::to_int_type(data[0]);
            }
        }
        return traits_type::eof();
    }
};

} // namespace

TEST_CASE("keet::io stream buffering and explicit flush semantics", "[runtime][io][flush]") {
    SECTION("print and println do not automatically invoke sync or flush") {
        SyncTrackingBuffer buf;
        std::ostream stream(&buf);

        keet::io::print_to(stream, "buffer chunk 1");
        CHECK(buf.sync_calls == 0);

        keet::io::println_to(stream, "buffer chunk 2");
        CHECK(buf.sync_calls == 0);

        keet::io::flush(stream);
        CHECK(buf.sync_calls == 1);
    }

    SECTION("input flushes prompt to output stream before reading from input") {
        SyncTrackingBuffer out_buf;
        std::ostream out_stream(&out_buf);

        InputOrderCheckBuffer in_buf("response_value\n", out_buf);
        std::istream in_stream(&in_buf);

        std::string res = keet::io::input("Enter name: ", in_stream, out_stream);
        CHECK(res == "response_value");
        CHECK(out_buf.str() == "Enter name: ");
        CHECK(out_buf.sync_calls >= 1);
        CHECK(in_buf.prompt_flushed_before_read);
    }
}

TEST_CASE("keet::io NO_COLOR specification compliance and precedence", "[runtime][io][no_color]") {
    SECTION("NO_COLOR unset allows normal terminal color evaluation") {
        EnvVarGuard envGuard("NO_COLOR", std::nullopt);
        keet::io::reset_color_enabled();

        std::ostringstream nonTtyStream;
        REQUIRE_FALSE(keet::io::is_color_enabled(nonTtyStream));
    }

    SECTION("NO_COLOR set to empty string disables automatic colors") {
        EnvVarGuard envGuard("NO_COLOR", "");
        keet::io::reset_color_enabled();

        std::ostringstream stream;
        REQUIRE_FALSE(keet::io::is_color_enabled(stream));
    }

    SECTION("NO_COLOR set to non-empty string disables automatic colors") {
        EnvVarGuard envGuard("NO_COLOR", "1");
        keet::io::reset_color_enabled();

        std::ostringstream stream;
        REQUIRE_FALSE(keet::io::is_color_enabled(stream));
    }

    SECTION("Explicit override takes precedence over NO_COLOR variable") {
        EnvVarGuard envGuard("NO_COLOR", "1");

        keet::io::set_color_enabled(true);
        REQUIRE(keet::io::is_color_enabled());

        keet::io::set_color_enabled(false);
        REQUIRE_FALSE(keet::io::is_color_enabled());

        keet::io::reset_color_enabled();
    }
}

TEST_CASE("keet::io end-of-input, empty line, and error handling", "[runtime][io][eof]") {
    SECTION("Distinguish valid empty line from end of file using readLine") {
        std::istringstream iss("\nSecond\n");

        auto first = keet::io::readLine(iss);
        REQUIRE(std::holds_alternative<std::string>(first));
        REQUIRE(std::get<std::string>(first).empty());
        REQUIRE_FALSE(keet::io::eof(iss));

        auto second = keet::io::readLine(iss);
        REQUIRE(std::holds_alternative<std::string>(second));
        REQUIRE(std::get<std::string>(second) == "Second");
        REQUIRE_FALSE(keet::io::eof(iss));

        auto eofResult = keet::io::readLine(iss);
        REQUIRE(std::holds_alternative<std::monostate>(eofResult));
        REQUIRE(keet::io::eof(iss));
    }

    SECTION("readLineOpt returns empty string on empty line and nullopt on EOF") {
        std::istringstream iss("\nData\n");

        auto opt1 = keet::io::readLineOpt(iss);
        REQUIRE(opt1.has_value());
        REQUIRE(opt1.value().empty());

        auto opt2 = keet::io::readLineOpt(iss);
        REQUIRE(opt2.has_value());
        REQUIRE(opt2.value() == "Data");

        auto opt3 = keet::io::readLineOpt(iss);
        REQUIRE_FALSE(opt3.has_value());
    }

    SECTION("Final line without terminating newline is read before EOF") {
        std::istringstream iss("NoTrailingNewline");

        auto lineRes = keet::io::readLine(iss);
        REQUIRE(std::holds_alternative<std::string>(lineRes));
        REQUIRE(std::get<std::string>(lineRes) == "NoTrailingNewline");
        REQUIRE(keet::io::eof(iss));

        auto nextRes = keet::io::readLine(iss);
        REQUIRE(std::holds_alternative<std::monostate>(nextRes));
        REQUIRE(keet::io::eof(iss));
    }

    SECTION("readln backward compatibility across line and EOF transitions") {
        std::istringstream iss("First\n\nFinal");

        REQUIRE(keet::io::readln(iss) == "First");
        REQUIRE_FALSE(keet::io::eof(iss));

        REQUIRE(keet::io::readln(iss).empty());
        REQUIRE_FALSE(keet::io::eof(iss));

        REQUIRE(keet::io::readln(iss) == "Final");
        REQUIRE(keet::io::eof(iss));

        REQUIRE(keet::io::readln(iss).empty());
        REQUIRE(keet::io::eof(iss));
    }

    SECTION("readln on empty stream returns empty string and marks EOF") {
        std::istringstream emptyStream("");
        REQUIRE(keet::io::readln(emptyStream).empty());
        REQUIRE(keet::io::eof(emptyStream));

        auto lineEmpty = keet::io::readLine(emptyStream);
        REQUIRE(std::holds_alternative<std::monostate>(lineEmpty));
    }

    SECTION("readln and readLine throw IOError on stream badbit failure") {
        std::istringstream badStream("broken");
        badStream.setstate(std::ios::badbit);

        REQUIRE_THROWS_AS(keet::io::readln(badStream), keet::error);
        REQUIRE_THROWS_AS(keet::io::readLine(badStream), keet::error);
        REQUIRE_THROWS_AS(keet::io::readLineOpt(badStream), keet::error);
    }
}

TEST_CASE("keet::io standard error functions and stream independence", "[runtime][io][stderr]") {
    SECTION("eprint and eprintln redirect cleanly via cerr buffer") {
        std::ostringstream errStream;
        auto* oldErr = std::cerr.rdbuf(errStream.rdbuf());

        keet::io::eprint("Warning:", 503);
        keet::io::eprintln(" Service unavailable");

        std::cerr.rdbuf(oldErr);
        REQUIRE(errStream.str() == "Warning: 503 Service unavailable\n");
    }

    SECTION("Stream-specific terminal detection evaluates stdout, stderr, and memory streams independently") {
        EnvVarGuard envGuard("NO_COLOR", std::nullopt);
        keet::io::reset_color_enabled();

#if !defined(_WIN32)
        // Verify query against real stream descriptors
        CHECK(keet::io::detect_terminal(std::cout) == (isatty(fileno(stdout)) != 0));
        CHECK(keet::io::detect_terminal(std::cerr) == (isatty(fileno(stderr)) != 0));

        // When stdout is redirected to a non-terminal pipe, detect_terminal(std::cout) evaluates false
        int pipeFds[2];
        if (pipe(pipeFds) == 0) {
            int savedStdout = dup(fileno(stdout));
            if (savedStdout >= 0) {
                fflush(stdout);
                dup2(pipeFds[1], fileno(stdout));
                CHECK_FALSE(keet::io::detect_terminal(std::cout));
                fflush(stdout);
                dup2(savedStdout, fileno(stdout));
                close(savedStdout);
            }
            close(pipeFds[0]);
            close(pipeFds[1]);
        }
#else
        CHECK(keet::io::detect_terminal(std::cout) == (_isatty(_fileno(stdout)) != 0));
        CHECK(keet::io::detect_terminal(std::cerr) == (_isatty(_fileno(stderr)) != 0));
#endif

        // Non-standard memory streams are consistently detected as non-terminals
        std::ostringstream outOss;
        std::ostringstream errOss;

        CHECK_FALSE(keet::io::detect_terminal(outOss));
        CHECK_FALSE(keet::io::detect_terminal(errOss));
        CHECK_FALSE(keet::io::is_color_enabled(outOss));
        CHECK_FALSE(keet::io::is_color_enabled(errOss));
    }
}

TEST_CASE("keet::io formatting of KeetScript values and collections", "[runtime][io][formatting]") {
    ColorOverrideGuard guard(false);

    SECTION("Null, optional, and variant values") {
        std::ostringstream oss;
        keet::io::print_to(oss, std::monostate{});
        REQUIRE(oss.str() == "null");
        oss.str("");

        keet::io::print_to(oss, nullptr);
        REQUIRE(oss.str() == "null");
        oss.str("");

        std::optional<int64_t> optSome = 42;
        std::optional<int64_t> optNone = std::nullopt;
        keet::io::print_to(oss, optSome, optNone);
        REQUIRE(oss.str() == "42 null");
        oss.str("");

        std::variant<int64_t, std::string> var1 = int64_t(100);
        std::variant<int64_t, std::string> var2 = std::string("text");
        keet::io::print_to(oss, var1, var2);
        REQUIRE(oss.str() == "100 text");
        oss.str("");

        std::variant<std::string, std::monostate> varNull = std::monostate{};
        keet::io::print_to(oss, varNull);
        REQUIRE(oss.str() == "null");
    }

    SECTION("Vector collections and nested vectors") {
        std::ostringstream oss;
        std::vector<int64_t> vec = {1, 2, 3};
        keet::io::print_to(oss, vec);
        REQUIRE(oss.str() == "[1, 2, 3]");
        oss.str("");

        std::vector<std::vector<int64_t>> nested = {{10, 20}, {30, 40}};
        keet::io::print_to(oss, nested);
        REQUIRE(oss.str() == "[[10, 20], [30, 40]]");
        oss.str("");

        std::vector<int64_t> emptyVec;
        keet::io::print_to(oss, emptyVec);
        REQUIRE(oss.str() == "[]");
    }

    SECTION("Map collections and entries") {
        std::ostringstream oss;
        std::map<std::string, int64_t> m = {{"a", 1}, {"b", 2}};
        keet::io::print_to(oss, m);
        REQUIRE(oss.str() == "{a: 1, b: 2}");
        oss.str("");

        std::unordered_map<std::string, int64_t> um = {{"key", 99}};
        keet::io::print_to(oss, um);
        REQUIRE(oss.str() == "{key: 99}");
    }

    SECTION("Types with toString and to_string member functions") {
        struct CustomItem {
            std::string toString() const { return "Custom(42)"; }
        };
        struct CustomLowerItem {
            std::string to_string() const { return "LowerCustom(84)"; }
        };

        std::ostringstream oss;
        keet::io::print_to(oss, CustomItem{});
        REQUIRE(oss.str() == "Custom(42)");
        oss.str("");

        keet::io::print_to(oss, CustomLowerItem{});
        REQUIRE(oss.str() == "LowerCustom(84)");
    }
}

TEST_CASE("keet::io ANSI styling, nested styling and Unicode safety", "[runtime][io][ansi]") {
    ColorOverrideGuard guard(true);

    SECTION("Empty strings return empty without inserting escape codes") {
        REQUIRE(keet::io::colorize("", keet::io::Color::Red).empty());
        REQUIRE(keet::io::bold("").empty());
        REQUIRE(keet::io::red("").empty());
    }

    SECTION("Nested styling preserves enclosing color across reset codes") {
        std::string inner = keet::io::bold("BoldWord");
        REQUIRE(inner == "\033[1mBoldWord\033[0m");

        std::string outer = keet::io::red("Start " + inner + " End");
        REQUIRE(outer == "\033[31mStart \033[1mBoldWord\033[0m\033[31m End\033[0m");
    }

    SECTION("Nested bold with inner color preserves outer bold formatting") {
        std::string inner = keet::io::red("Alert");
        REQUIRE(inner == "\033[31mAlert\033[0m");

        std::string outer = keet::io::bold("Start " + inner + " End");
        REQUIRE(outer == "\033[1mStart \033[31mAlert\033[0m\033[1m End\033[0m");
    }

    SECTION("Nested color with inner color preserves outer color formatting") {
        std::string inner = keet::io::green("Middle");
        std::string outer = keet::io::red("Alpha " + inner + " Omega");
        REQUIRE(outer == "\033[31mAlpha \033[32mMiddle\033[0m\033[31m Omega\033[0m");
    }

    SECTION("Single wrapped styled string does not duplicate trailing reset code") {
        std::string wrapped = keet::io::bold(keet::io::red("Alert"));
        REQUIRE(wrapped == "\033[1m\033[31mAlert\033[0m");

        std::string doubleWrapped = keet::io::green(keet::io::bold(keet::io::red("Core")));
        REQUIRE(doubleWrapped == "\033[32m\033[1m\033[31mCore\033[0m");
    }

    SECTION("Nested bold and color combinations with surrounding text") {
        // Text both before and after inner styled segment
        std::string beforeAndAfter = keet::io::bold("Pre " + keet::io::yellow("Warn") + " Post");
        REQUIRE(beforeAndAfter == "\033[1mPre \033[33mWarn\033[0m\033[1m Post\033[0m");

        // Multiple inner styled segments with text before, between, and after
        std::string multiInner = keet::io::bold("Lead " + keet::io::red("Err") + " Mid " + keet::io::green("Ok") + " Tail");
        REQUIRE(multiInner == "\033[1mLead \033[31mErr\033[0m\033[1m Mid \033[32mOk\033[0m\033[1m Tail\033[0m");

        // Text before only (nested styled segment at the end)
        std::string beforeOnly = keet::io::bold("Prefix " + keet::io::blue("Info"));
        REQUIRE(beforeOnly == "\033[1mPrefix \033[34mInfo\033[0m");

        // Text after only (nested styled segment at the beginning)
        std::string afterOnly = keet::io::bold(keet::io::magenta("Tag") + " Suffix");
        REQUIRE(afterOnly == "\033[1m\033[35mTag\033[0m\033[1m Suffix\033[0m");
    }

    SECTION("UTF-8 Unicode string characters are preserved with styling") {
        std::string unicodeSample = "Zażółć gęślą jaźń";
        std::string colored = keet::io::green(unicodeSample);
        REQUIRE(colored == "\033[32mZażółć gęślą jaźń\033[0m");
    }
}
