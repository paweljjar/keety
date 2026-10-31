#include <catch2/catch_test_macros.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/ir/ast_lowerer.hpp>
#include <keety/lexer/lexer.hpp>
#include <keety/parser/parser.hpp>

using namespace keety;
using namespace keety::ir;
using namespace keety::parser;
using namespace keety::lexer;

static ModuleAST parse_source(std::string_view source, std::string module_name = "test_module") {
    Lexer lexer(1, source);
    Parser parser(lexer);
    return parser.parseModule(std::move(module_name));
}

TEST_CASE("AST lowerer converts arithmetic, logical and ternary expressions to KeetIR", "[ast_lowerer]") {
    SECTION("Arithmetic and unary expressions") {
        std::string src = R"(
            fn compute(x: int, y: int): int {
                let z: int = x + y * 2;
                return -z;
            }
        )";
        auto ast = parse_source(src);
        AstLowerer lowerer;
        auto mod = lowerer.lower_module(ast);

        REQUIRE(mod != nullptr);
        Function* fn = mod->find_function("compute");
        REQUIRE(fn != nullptr);

        std::string err;
        CHECK(fn->validate_cfg(err));
        CHECK(err.empty());

        std::string ir_text = fn->to_string();
        CHECK(ir_text.find("alloca mut z: int") != std::string::npos);
        CHECK(ir_text.find("mul y, 2") != std::string::npos);
        CHECK(ir_text.find("add x,") != std::string::npos);
        CHECK(ir_text.find("neg z") != std::string::npos);
        CHECK(ir_text.find("ret") != std::string::npos);
    }

    SECTION("Short-circuiting logical expressions") {
        std::string src = R"(
            fn test_and(a: bool, b: bool): bool {
                return a && b;
            }
        )";
        auto ast = parse_source(src);
        AstLowerer lowerer;
        auto mod = lowerer.lower_module(ast);

        REQUIRE(mod != nullptr);
        Function* fn = mod->find_function("test_and");
        REQUIRE(fn != nullptr);

        std::string err;
        CHECK(fn->validate_cfg(err));

        std::string ir_text = fn->to_string();
        CHECK(ir_text.find("and.rhs") != std::string::npos);
        CHECK(ir_text.find("and.end") != std::string::npos);
    }

    SECTION("Ternary conditional expressions") {
        std::string src = R"(
            fn choose(flag: bool): int {
                return flag ? 100 : 200;
            }
        )";
        auto ast = parse_source(src);
        AstLowerer lowerer;
        auto mod = lowerer.lower_module(ast);

        REQUIRE(mod != nullptr);
        Function* fn = mod->find_function("choose");
        REQUIRE(fn != nullptr);

        std::string err;
        CHECK(fn->validate_cfg(err));

        std::string ir_text = fn->to_string();
        CHECK(ir_text.find("ternary.then") != std::string::npos);
        CHECK(ir_text.find("ternary.else") != std::string::npos);
        CHECK(ir_text.find("ternary.end") != std::string::npos);
    }
}

TEST_CASE("AST lowerer translates conditional statements to branching CFGs", "[ast_lowerer]") {
    SECTION("If-else construct") {
        std::string src = R"(
            fn check_sign(val: int): int {
                if (val > 0) {
                    return 1;
                } else {
                    return -1;
                }
            }
        )";
        auto ast = parse_source(src);
        AstLowerer lowerer;
        auto mod = lowerer.lower_module(ast);

        REQUIRE(mod != nullptr);
        Function* fn = mod->find_function("check_sign");
        REQUIRE(fn != nullptr);

        std::string err;
        CHECK(fn->validate_cfg(err));

        std::string ir_text = fn->to_string();
        CHECK(ir_text.find("if.then") != std::string::npos);
        CHECK(ir_text.find("if.else") != std::string::npos);
        CHECK(ir_text.find("if.end") != std::string::npos);
        CHECK(ir_text.find("gt val, 0") != std::string::npos);
    }

    SECTION("If with init statement") {
        std::string src = R"(
            fn check_init(x: int): int {
                if (let d: int = x * 2; d > 10) {
                    return d;
                }
                return 0;
            }
        )";
        auto ast = parse_source(src);
        AstLowerer lowerer;
        auto mod = lowerer.lower_module(ast);

        REQUIRE(mod != nullptr);
        Function* fn = mod->find_function("check_init");
        REQUIRE(fn != nullptr);

        std::string err;
        CHECK(fn->validate_cfg(err));

        std::string ir_text = fn->to_string();
        CHECK(ir_text.find("alloca mut d: int") != std::string::npos);
        CHECK(ir_text.find("gt d, 10") != std::string::npos);
    }
}

TEST_CASE("AST lowerer translates while, for, and for-in loops to CFGs with jump targets", "[ast_lowerer]") {
    SECTION("While loop with break and continue") {
        std::string src = R"(
            fn loop_while(n: int): void {
                let i: int = 0;
                while (i < n) {
                    if (i == 5) {
                        break;
                    }
                    i = i + 1;
                }
            }
        )";
        auto ast = parse_source(src);
        AstLowerer lowerer;
        auto mod = lowerer.lower_module(ast);

        REQUIRE(mod != nullptr);
        Function* fn = mod->find_function("loop_while");
        REQUIRE(fn != nullptr);

        std::string err;
        CHECK(fn->validate_cfg(err));

        std::string ir_text = fn->to_string();
        CHECK(ir_text.find("while.cond") != std::string::npos);
        CHECK(ir_text.find("while.body") != std::string::npos);
        CHECK(ir_text.find("while.end") != std::string::npos);
    }

    SECTION("Classic for loop with labeled break") {
        std::string src = R"(
            fn loop_for(): void {
                for (let i: int = 0; i < 10; i = i + 1) as outer {
                    if (i == 7) {
                        break outer;
                    }
                }
            }
        )";
        auto ast = parse_source(src);
        AstLowerer lowerer;
        auto mod = lowerer.lower_module(ast);

        REQUIRE(mod != nullptr);
        Function* fn = mod->find_function("loop_for");
        REQUIRE(fn != nullptr);

        std::string err;
        CHECK(fn->validate_cfg(err));

        std::string ir_text = fn->to_string();
        CHECK(ir_text.find("for.cond") != std::string::npos);
        CHECK(ir_text.find("for.body") != std::string::npos);
        CHECK(ir_text.find("for.post") != std::string::npos);
        CHECK(ir_text.find("for.end") != std::string::npos);
    }

    SECTION("For-in collection loop") {
        std::string src = R"(
            fn iterate(items: array<int>): void {
                for (const x in items) {
                    let y: int = x;
                }
            }
        )";
        auto ast = parse_source(src);
        AstLowerer lowerer;
        auto mod = lowerer.lower_module(ast);

        REQUIRE(mod != nullptr);
        Function* fn = mod->find_function("iterate");
        REQUIRE(fn != nullptr);

        std::string err;
        CHECK(fn->validate_cfg(err));

        std::string ir_text = fn->to_string();
        CHECK(ir_text.find("forin.cond") != std::string::npos);
        CHECK(ir_text.find("forin.body") != std::string::npos);
        CHECK(ir_text.find("forin.post") != std::string::npos);
        CHECK(ir_text.find("forin.end") != std::string::npos);
        CHECK(ir_text.find("call_method items.length()") != std::string::npos);
    }
}

TEST_CASE("AST lowerer translates pattern matching switches to decision cascades", "[ast_lowerer]") {
    SECTION("Switch with value cases and default branch") {
        std::string src = R"(
            fn dispatch(status: int): string {
                switch (status) {
                    case (200) {
                        return "OK";
                    }
                    case (404) {
                        return "Not Found";
                    }
                    default {
                        return "Unknown";
                    }
                }
            }
        )";
        auto ast = parse_source(src);
        AstLowerer lowerer;
        auto mod = lowerer.lower_module(ast);

        REQUIRE(mod != nullptr);
        Function* fn = mod->find_function("dispatch");
        REQUIRE(fn != nullptr);

        std::string err;
        CHECK(fn->validate_cfg(err));

        std::string ir_text = fn->to_string();
        CHECK(ir_text.find("switch.case.0") != std::string::npos);
        CHECK(ir_text.find("switch.case.1") != std::string::npos);
        CHECK(ir_text.find("switch.default") != std::string::npos);
        CHECK(ir_text.find("switch.end") != std::string::npos);
        CHECK(ir_text.find("eq status, 200") != std::string::npos);
        CHECK(ir_text.find("eq status, 404") != std::string::npos);
    }

    SECTION("Switch with multiple values per case branch") {
        std::string src = R"(
            fn classify(code: int): int {
                switch (code) {
                    case (1, 2, 3) {
                        return 10;
                    }
                    default {
                        return 0;
                    }
                }
            }
        )";
        auto ast = parse_source(src);
        AstLowerer lowerer;
        auto mod = lowerer.lower_module(ast);

        REQUIRE(mod != nullptr);
        Function* fn = mod->find_function("classify");
        REQUIRE(fn != nullptr);

        std::string err;
        CHECK(fn->validate_cfg(err));

        std::string ir_text = fn->to_string();
        CHECK(ir_text.find("eq code, 1") != std::string::npos);
        CHECK(ir_text.find("eq code, 2") != std::string::npos);
        CHECK(ir_text.find("eq code, 3") != std::string::npos);
    }
}

TEST_CASE("AST lowerer translates module globals, calls and collections", "[ast_lowerer]") {
    std::string src = R"(
        export const MAX_SIZE: int = 1024;
        let activeCount: int = 0;

        fn process(): void {
            let arr: array<int> = [1, 2, 3];
            let m: map<string, int> = {"first": 1};
            let first_val: int = arr[0];
        }
    )";
    auto ast = parse_source(src, "my_module");
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    CHECK(mod->get_name() == "my_module");

    const auto* g_max = mod->find_global("MAX_SIZE");
    REQUIRE(g_max != nullptr);
    CHECK(g_max->is_const);
    CHECK(g_max->is_export);

    const auto* g_count = mod->find_global("activeCount");
    REQUIRE(g_count != nullptr);
    CHECK_FALSE(g_count->is_const);
    CHECK_FALSE(g_count->is_export);

    Function* fn = mod->find_function("process");
    REQUIRE(fn != nullptr);

    std::string err;
    CHECK(mod->validate(err));

    std::string text = mod->to_string();
    CHECK(text.find("array_construct [1, 2, 3]") != std::string::npos);
    CHECK(text.find("arr[0]") != std::string::npos);
}

TEST_CASE("AST lowerer translates standard io module calls and color expressions", "[ast_lowerer][io]") {
    SECTION("Translates io print, println, input, and Color member accesses") {
        std::string src = R"(
            import io;

            fn run_io(): void {
                io.print("Hello, ", 42);
                io.println("Done", io.Color.Green);
                let s: string = io.input("Enter name: ");
            }
        )";
        auto ast = parse_source(src, "io_test");
        AstLowerer lowerer;
        auto mod = lowerer.lower_module(ast);

        REQUIRE(mod != nullptr);
        Function* fn = mod->find_function("run_io");
        REQUIRE(fn != nullptr);

        std::string err;
        CHECK(fn->validate_cfg(err));

        std::string ir_text = fn->to_string();
        CHECK(ir_text.find("call @keet.io.print") != std::string::npos);
        CHECK(ir_text.find("call @keet.io.println") != std::string::npos);
        CHECK(ir_text.find("::keet::io::Color::Green") != std::string::npos);
        CHECK(ir_text.find("call @keet.io.input") != std::string::npos);
    }

    SECTION("Translates selective imports from io module") {
        std::string src = R"(
            from "io" import println, Color, red;

            fn run_selective(): void {
                println("Alert", Color.Red);
                let msg: string = red("Error");
            }
        )";
        auto ast = parse_source(src, "io_selective");
        AstLowerer lowerer;
        auto mod = lowerer.lower_module(ast);

        REQUIRE(mod != nullptr);
        Function* fn = mod->find_function("run_selective");
        REQUIRE(fn != nullptr);

        std::string err;
        CHECK(fn->validate_cfg(err));

        std::string ir_text = fn->to_string();
        CHECK(ir_text.find("call @keet.io.println") != std::string::npos);
        CHECK(ir_text.find("::keet::io::Color::Red") != std::string::npos);
        CHECK(ir_text.find("call @keet.io.red") != std::string::npos);
    }
}

