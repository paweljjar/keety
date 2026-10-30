#include <catch2/catch_test_macros.hpp>
#include <keety/ir/ast_lowerer.hpp>
#include <keety/ir/basic_block.hpp>
#include <keety/ir/function.hpp>
#include <keety/ir/instruction.hpp>
#include <keety/ir/liveness_analysis.hpp>
#include <keety/lexer/lexer.hpp>
#include <keety/parser/parser.hpp>

using namespace keety;
using namespace keety::ir;
using namespace keety::parser;
using namespace keety::lexer;

static ModuleAST parse_source(std::string_view source, std::string module_name = "liveness_test") {
    Lexer lexer(1, source);
    Parser parser(lexer);
    return parser.parseModule(std::move(module_name));
}

TEST_CASE("Static liveness analysis tracks straight-line variable live ranges and last uses", "[liveness_analysis]") {
    std::string src = R"(
        fn linear_flow(x: int): int {
            let a: int = x + 1;
            let b: int = a + 2;
            let c: int = b * 3;
            return c;
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("linear_flow");
    REQUIRE(fn != nullptr);

    LivenessAnalysis analyzer;
    LivenessResult result = analyzer.analyze(*fn);

    const LiveRange* range_a = result.get_live_range("a");
    REQUIRE(range_a != nullptr);
    CHECK(range_a->definitions.size() == 2); // alloca and store
    CHECK(range_a->uses.size() == 1);
    CHECK(range_a->last_uses.size() == 1);
    CHECK(range_a->uses[0] == range_a->last_uses[0]);

    const LiveRange* range_b = result.get_live_range("b");
    REQUIRE(range_b != nullptr);
    CHECK(range_b->uses.size() == 1);
    CHECK(range_b->last_uses.size() == 1);
    CHECK(range_b->uses[0] == range_b->last_uses[0]);

    const LiveRange* range_c = result.get_live_range("c");
    REQUIRE(range_c != nullptr);
    CHECK(range_c->uses.size() == 1);
    CHECK(range_c->last_uses.size() == 1);

    // Function parameter x
    const LiveRange* range_x = result.get_live_range("x");
    REQUIRE(range_x != nullptr);
    CHECK(range_x->uses.size() == 1);
    CHECK(range_x->last_uses.size() == 1);
}

TEST_CASE("Static liveness analysis identifies multiple sequential reads and detects only final read as last use", "[liveness_analysis]") {
    std::string src = R"(
        fn multiple_reads(x: int): int {
            let total: int = 0;
            total = total + x;
            total = total + x;
            total = total + x;
            return total;
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("multiple_reads");
    REQUIRE(fn != nullptr);

    LivenessAnalysis analyzer;
    LivenessResult result = analyzer.analyze(*fn);

    const LiveRange* range_x = result.get_live_range("x");
    REQUIRE(range_x != nullptr);
    CHECK(range_x->uses.size() == 3);
    CHECK(range_x->last_uses.size() == 1);

    // The first two uses are NOT last uses
    CHECK_FALSE(result.is_last_use(range_x->uses[0], "x"));
    CHECK_FALSE(result.is_last_use(range_x->uses[1], "x"));

    // The third use IS the last use
    CHECK(result.is_last_use(range_x->uses[2], "x"));
}

TEST_CASE("Static liveness analysis tracks live-in and live-out sets across conditional branches", "[liveness_analysis]") {
    std::string src = R"(
        fn branch_liveness(flag: bool, val: int): int {
            let res: int = 0;
            if (flag) {
                res = val + 10;
            } else {
                res = val + 20;
            }
            return res;
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("branch_liveness");
    REQUIRE(fn != nullptr);

    LivenessAnalysis analyzer;
    LivenessResult result = analyzer.analyze(*fn);

    // val is read in both then and else branch, but never read after if-else
    const LiveRange* range_val = result.get_live_range("val");
    REQUIRE(range_val != nullptr);
    CHECK(range_val->uses.size() == 2);
    CHECK(range_val->last_uses.size() == 2);

    // Both uses are last uses along their respective control paths
    CHECK(result.is_last_use(range_val->uses[0], "val"));
    CHECK(result.is_last_use(range_val->uses[1], "val"));

    // In end block, val is dead
    BasicBlock* end_bb = fn->find_basic_block("if.end.3");
    REQUIRE(end_bb != nullptr);
    CHECK_FALSE(result.is_live_in(end_bb, "val"));
    CHECK(result.is_live_in(end_bb, "res"));
}

TEST_CASE("Static liveness analysis tracks loop variables and back-edge liveness", "[liveness_analysis]") {
    std::string src = R"(
        fn loop_liveness(n: int): int {
            let acc: int = 0;
            let i: int = 0;
            while (i < n) {
                acc = acc + i;
                i = i + 1;
            }
            return acc;
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("loop_liveness");
    REQUIRE(fn != nullptr);

    LivenessAnalysis analyzer;
    LivenessResult result = analyzer.analyze(*fn);

    BasicBlock* cond_bb = fn->find_basic_block("while.cond.1");
    BasicBlock* body_bb = fn->find_basic_block("while.body.2");
    BasicBlock* end_bb = fn->find_basic_block("while.end.3");

    REQUIRE(cond_bb != nullptr);
    REQUIRE(body_bb != nullptr);
    REQUIRE(end_bb != nullptr);

    // Both i and acc are live along the loop back-edge
    CHECK(result.is_live_in(cond_bb, "i"));
    CHECK(result.is_live_in(cond_bb, "acc"));
    CHECK(result.is_live_in(cond_bb, "n"));

    CHECK(result.is_live_out(body_bb, "i"));
    CHECK(result.is_live_out(body_bb, "acc"));

    // At loop exit, i and n are dead, but acc is live to return
    CHECK(result.is_live_in(end_bb, "acc"));
    CHECK_FALSE(result.is_live_in(end_bb, "i"));
    CHECK_FALSE(result.is_live_in(end_bb, "n"));
}

TEST_CASE("Static liveness analysis supports module-level multi-function inspection", "[liveness_analysis]") {
    std::string src = R"(
        fn add(a: int, b: int): int {
            return a + b;
        }

        fn sub(a: int, b: int): int {
            return a - b;
        }
    )";
    auto ast = parse_source(src, "math_mod");
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);

    LivenessAnalysis analyzer;
    auto module_results = analyzer.analyze_module(*mod);

    CHECK(module_results.size() == 2);

    Function* fn_add = mod->find_function("add");
    Function* fn_sub = mod->find_function("sub");

    REQUIRE(fn_add != nullptr);
    REQUIRE(fn_sub != nullptr);

    REQUIRE(module_results.find(fn_add) != module_results.end());
    REQUIRE(module_results.find(fn_sub) != module_results.end());

    const auto& res_add = module_results[fn_add];
    const LiveRange* range_a = res_add.get_live_range("a");
    REQUIRE(range_a != nullptr);
    CHECK(range_a->last_uses.size() == 1);

    std::string report = res_add.to_string();
    CHECK(report.find("; Liveness Analysis Result for function: @add") != std::string::npos);
}
