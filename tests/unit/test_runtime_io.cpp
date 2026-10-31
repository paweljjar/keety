#include <catch2/catch_test_macros.hpp>
#include <keet/io.hpp>

#include <iostream>
#include <sstream>
#include <string>

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

    SECTION("readLine alias matches readln") {
        std::istringstream iss("AliasTest\n");
        REQUIRE(keet::io::readLine(iss) == "AliasTest");
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
