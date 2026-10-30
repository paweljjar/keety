#include <catch2/catch_test_macros.hpp>
#include <keety/ir/ast_lowerer.hpp>
#include <keety/ir/pipeline_reserve.hpp>
#include <keety/lexer/lexer.hpp>
#include <keety/parser/parser.hpp>

using namespace keety;
using namespace keety::ir;
using namespace keety::parser;
using namespace keety::lexer;

static ModuleAST parse_source(std::string_view source, std::string module_name = "test_mod") {
    Lexer lexer(1, source);
    Parser parser(lexer);
    return parser.parseModule(std::move(module_name));
}

TEST_CASE("Pipeline reserve inserts reserve instruction following single map call", "[pipeline_reserve]") {
    std::string src = R"(
        fn transform(items: array<int>, f: (x: int) => int): array<int> {
            return items.map(f);
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("transform");
    REQUIRE(fn != nullptr);

    PipelineReservePass pass;
    size_t count = pass.run_on_function(*fn);
    CHECK(count == 1);

    // Verify ReserveInst is in basic block right after MethodCallInst
    bool found_map = false;
    bool found_reserve = false;

    for (const auto& bb : fn->get_blocks()) {
        const auto& insts = bb->get_instructions();
        for (size_t i = 0; i < insts.size(); ++i) {
            if (insts[i]->get_opcode() == Opcode::MethodCall) {
                const auto* mcall = static_cast<const MethodCallInst*>(insts[i].get());
                if (mcall->get_method() == "map") {
                    found_map = true;
                    REQUIRE(i + 1 < insts.size());
                    REQUIRE(insts[i + 1]->get_opcode() == Opcode::Reserve);
                    const auto* res = static_cast<const ReserveInst*>(insts[i + 1].get());
                    CHECK(res->get_collection() == *mcall->get_dest());
                    CHECK(res->get_capacity().get_name() == "items");
                    found_reserve = true;
                }
            }
        }
    }

    CHECK(found_map);
    CHECK(found_reserve);
}

TEST_CASE("Pipeline reserve optimizes chained map and filter stages to reference root collection", "[pipeline_reserve]") {
    std::string src = R"(
        fn pipeline(arr: array<int>, f: (x: int) => int, g: (x: int) => bool): array<int> {
            return arr.map(f).filter(g);
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("pipeline");
    REQUIRE(fn != nullptr);

    PipelineReservePass pass;
    size_t count = pass.run_on_function(*fn);
    CHECK(count == 2);

    std::vector<const ReserveInst*> reserves;
    for (const auto& bb : fn->get_blocks()) {
        for (const auto& inst : bb->get_instructions()) {
            if (inst->get_opcode() == Opcode::Reserve) {
                reserves.push_back(static_cast<const ReserveInst*>(inst.get()));
            }
        }
    }

    REQUIRE(reserves.size() == 2);
    // Both map and filter reserve directives must reference the root array "arr"
    CHECK(reserves[0]->get_capacity().get_name() == "arr");
    CHECK(reserves[1]->get_capacity().get_name() == "arr");

    // The two reserve directives target different stage destination registers
    CHECK(reserves[0]->get_collection() != reserves[1]->get_collection());
}

TEST_CASE("Pipeline reserve optimizes filter followed by map chain", "[pipeline_reserve]") {
    std::string src = R"(
        fn filter_map(values: array<int>, f: (x: int) => bool, g: (x: int) => int): array<int> {
            return values.filter(f).map(g);
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("filter_map");
    REQUIRE(fn != nullptr);

    PipelineReservePass pass;
    size_t count = pass.run_on_function(*fn);
    CHECK(count == 2);

    std::vector<const ReserveInst*> reserves;
    for (const auto& bb : fn->get_blocks()) {
        for (const auto& inst : bb->get_instructions()) {
            if (inst->get_opcode() == Opcode::Reserve) {
                reserves.push_back(static_cast<const ReserveInst*>(inst.get()));
            }
        }
    }

    REQUIRE(reserves.size() == 2);
    CHECK(reserves[0]->get_capacity().get_name() == "values");
    CHECK(reserves[1]->get_capacity().get_name() == "values");
}

TEST_CASE("Pipeline reserve handles literal array sources with constant integer capacity", "[pipeline_reserve]") {
    std::string src = R"(
        fn literal_pipeline(f: (x: int) => int): array<int> {
            return [10, 20, 30].map(f);
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("literal_pipeline");
    REQUIRE(fn != nullptr);

    PipelineReservePass pass;
    size_t count = pass.run_on_function(*fn);
    CHECK(count == 1);

    bool found_reserve = false;
    for (const auto& bb : fn->get_blocks()) {
        for (const auto& inst : bb->get_instructions()) {
            if (inst->get_opcode() == Opcode::Reserve) {
                const auto* res = static_cast<const ReserveInst*>(inst.get());
                CHECK(res->get_capacity().is_constant());
                CHECK(std::get<int64_t>(res->get_capacity().get_constant()) == 3);
                found_reserve = true;
            }
        }
    }
    CHECK(found_reserve);
}

TEST_CASE("Pipeline reserve ignores non-pipeline methods", "[pipeline_reserve]") {
    std::string src = R"(
        fn mutate_array(arr: array<int>): void {
            arr.push(42);
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("mutate_array");
    REQUIRE(fn != nullptr);

    PipelineReservePass pass;
    size_t count = pass.run_on_function(*fn);
    CHECK(count == 0);
}

TEST_CASE("Pipeline reserve pass is idempotent and prevents duplicate insertions", "[pipeline_reserve]") {
    std::string src = R"(
        fn idempotent_test(items: array<int>, f: (x: int) => int): array<int> {
            return items.map(f);
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("idempotent_test");
    REQUIRE(fn != nullptr);

    PipelineReservePass pass;
    size_t first_run = pass.run_on_function(*fn);
    CHECK(first_run == 1);

    // Second run should detect existing reserve and insert 0 additional directives
    size_t second_run = pass.run_on_function(*fn);
    CHECK(second_run == 0);
}

TEST_CASE("Pipeline reserve optimizes independent pipelines within the same function", "[pipeline_reserve]") {
    std::string src = R"(
        fn multi_pipelines(a: array<int>, b: array<int>, f: (x: int) => int, g: (x: int) => bool): void {
            let r1 = a.map(f);
            let r2 = b.filter(g);
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("multi_pipelines");
    REQUIRE(fn != nullptr);

    PipelineReservePass pass;
    size_t count = pass.run_on_function(*fn);
    CHECK(count == 2);

    std::vector<const ReserveInst*> reserves;
    for (const auto& bb : fn->get_blocks()) {
        for (const auto& inst : bb->get_instructions()) {
            if (inst->get_opcode() == Opcode::Reserve) {
                reserves.push_back(static_cast<const ReserveInst*>(inst.get()));
            }
        }
    }

    REQUIRE(reserves.size() == 2);
    CHECK(reserves[0]->get_capacity().get_name() == "a");
    CHECK(reserves[1]->get_capacity().get_name() == "b");
}
