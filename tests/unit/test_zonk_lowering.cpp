#include <catch2/catch_test_macros.hpp>
#include <keety/ir/ast_lowerer.hpp>
#include <keety/ir/basic_block.hpp>
#include <keety/ir/function.hpp>
#include <keety/ir/instruction.hpp>
#include <keety/ir/module.hpp>
#include <keety/lexer/lexer.hpp>
#include <keety/parser/parser.hpp>

using namespace keety;
using namespace keety::ir;
using namespace keety::parser;
using namespace keety::lexer;

static ModuleAST parse_source(std::string_view source, std::string module_name = "zonk_test") {
    Lexer lexer(1, source);
    Parser parser(lexer);
    return parser.parseModule(std::move(module_name));
}

TEST_CASE("Zonk fallback value expressions lower to try-catch CFG with landing pads", "[zonk_lowering]") {
    std::string src = R"(
        fn loadPort(raw: string): int {
            let port: int = raw.toInt() zonk 8080;
            return port;
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("loadPort");
    REQUIRE(fn != nullptr);

    std::string err;
    CHECK(fn->validate_cfg(err));
    CHECK(err.empty());

    BasicBlock* try_bb = fn->find_basic_block("zonk.try.1");
    BasicBlock* catch_bb = fn->find_basic_block("zonk.catch.2");
    BasicBlock* end_bb = fn->find_basic_block("zonk.end.3");

    REQUIRE(try_bb != nullptr);
    REQUIRE(catch_bb != nullptr);
    REQUIRE(end_bb != nullptr);

    // Verify try block has landing pad set
    CHECK(try_bb->has_landing_pad());
    CHECK(try_bb->get_landing_pad() == catch_bb);

    // Verify catch block begins with landing_pad instruction
    CHECK(catch_bb->is_landing_pad());
    REQUIRE(!catch_bb->empty());
    CHECK(catch_bb->get_instructions().front()->get_opcode() == Opcode::LandingPad);

    // Verify CFG edges: try_bb has catch_bb and end_bb as successors
    const auto& try_succs = try_bb->get_successors();
    CHECK(std::find(try_succs.begin(), try_succs.end(), catch_bb) != try_succs.end());
    CHECK(std::find(try_succs.begin(), try_succs.end(), end_bb) != try_succs.end());

    // Verify catch_bb has try_bb as predecessor
    const auto& catch_preds = catch_bb->get_predecessors();
    CHECK(std::find(catch_preds.begin(), catch_preds.end(), try_bb) != catch_preds.end());

    std::string ir_text = fn->to_string();
    CHECK(ir_text.find("call_method raw.toInt()") != std::string::npos);
    CHECK(ir_text.find("landing_pad") != std::string::npos);
    CHECK(ir_text.find("store _zonk_res") != std::string::npos);
    CHECK(ir_text.find("8080") != std::string::npos);
}

TEST_CASE("Scoped zonk recovery block with yield lowers to landing pad and merge block", "[zonk_lowering]") {
    std::string src = R"(
        fn fetchConfig(path: string): string {
            let conf: string = read(path) zonk (err) {
                yield "{}";
            };
            return conf;
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("fetchConfig");
    REQUIRE(fn != nullptr);

    std::string err;
    CHECK(fn->validate_cfg(err));
    CHECK(err.empty());

    BasicBlock* try_bb = fn->find_basic_block("zonk.try.1");
    BasicBlock* catch_bb = fn->find_basic_block("zonk.catch.2");
    BasicBlock* end_bb = fn->find_basic_block("zonk.end.3");

    REQUIRE(try_bb != nullptr);
    REQUIRE(catch_bb != nullptr);
    REQUIRE(end_bb != nullptr);

    CHECK(try_bb->get_landing_pad() == catch_bb);
    CHECK(catch_bb->is_landing_pad());

    std::string ir_text = fn->to_string();
    CHECK(ir_text.find("alloca err: error") != std::string::npos);
    CHECK(ir_text.find("err = landing_pad") != std::string::npos);
    CHECK(ir_text.find("store _zonk_res") != std::string::npos);
    CHECK(ir_text.find("\"{}\"") != std::string::npos);
}

TEST_CASE("Scoped zonk recovery block with diverging return exits function cleanly", "[zonk_lowering]") {
    std::string src = R"(
        fn openConnection(host: string): int {
            let conn: int = connect(host) zonk (err) {
                return -1;
            };
            return conn;
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("openConnection");
    REQUIRE(fn != nullptr);

    std::string err;
    CHECK(fn->validate_cfg(err));
    CHECK(err.empty());

    BasicBlock* catch_bb = fn->find_basic_block("zonk.catch.2");
    REQUIRE(catch_bb != nullptr);
    CHECK(catch_bb->is_landing_pad());

    // The catch block terminates in return -1
    Instruction* term = catch_bb->get_terminator();
    REQUIRE(term != nullptr);
    CHECK(term->get_opcode() == Opcode::Return);

    std::string ir_text = fn->to_string();
    CHECK(ir_text.find("err = landing_pad") != std::string::npos);
    CHECK(ir_text.find("neg 1") != std::string::npos);
    CHECK(ir_text.find("ret %") != std::string::npos);
}

TEST_CASE("Zonk statement and error re-throwing lower to throw instructions", "[zonk_lowering]") {
    SECTION("Standalone zonk statement raises error") {
        std::string src = R"(
            fn failEarly(): void {
                zonk "Fatal crash";
            }
        )";
        auto ast = parse_source(src);
        AstLowerer lowerer;
        auto mod = lowerer.lower_module(ast);

        REQUIRE(mod != nullptr);
        Function* fn = mod->find_function("failEarly");
        REQUIRE(fn != nullptr);

        std::string err;
        CHECK(fn->validate_cfg(err));

        std::string ir_text = fn->to_string();
        CHECK(ir_text.find("throw \"Fatal crash\"") != std::string::npos);
    }

    SECTION("Re-raising error inside zonk handler") {
        std::string src = R"(
            fn propagate(url: string): string {
                let res: string = get(url) zonk (err) {
                    zonk err;
                };
                return res;
            }
        )";
        auto ast = parse_source(src);
        AstLowerer lowerer;
        auto mod = lowerer.lower_module(ast);

        REQUIRE(mod != nullptr);
        Function* fn = mod->find_function("propagate");
        REQUIRE(fn != nullptr);

        std::string err;
        CHECK(fn->validate_cfg(err));

        std::string ir_text = fn->to_string();
        CHECK(ir_text.find("err = landing_pad") != std::string::npos);
        CHECK(ir_text.find("throw err") != std::string::npos);
    }
}

TEST_CASE("Invoke instruction connects normal and exceptional landing pad edges in CFG", "[zonk_lowering]") {
    auto void_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Void);
    Function fn("invoke_demo", void_type);

    BasicBlock* entry = fn.create_basic_block("entry");
    BasicBlock* normal_bb = fn.create_basic_block("normal");
    BasicBlock* catch_bb = fn.create_basic_block("catch");

    fn.set_entry_block(entry);

    // Entry terminates with InvokeInst branching to normal_bb or catch_bb
    std::vector<Operand> args = {Operand::create_string("request")};
    entry->append_instruction(std::make_unique<InvokeInst>(
        std::nullopt, "dispatchRequest", args, normal_bb, catch_bb
    ));

    // Normal return
    normal_bb->append_instruction(std::make_unique<ReturnInst>());

    // Catch landing pad
    catch_bb->append_instruction(std::make_unique<LandingPadInst>(Operand::create_var("caught_err", nullptr)));
    catch_bb->append_instruction(std::make_unique<ReturnInst>());

    fn.recompute_cfg_edges();

    std::string err;
    CHECK(fn.validate_cfg(err));
    CHECK(err.empty());

    // Verify InvokeInst properties
    Instruction* term = entry->get_terminator();
    REQUIRE(term != nullptr);
    CHECK(term->get_opcode() == Opcode::Invoke);
    CHECK(term->is_terminator());

    auto succs = term->get_successors();
    REQUIRE(succs.size() == 2);
    CHECK(succs[0] == normal_bb);
    CHECK(succs[1] == catch_bb);

    std::string text = fn.to_string();
    CHECK(text.find("invoke @dispatchRequest(\"request\") to @normal.1 unwind @catch.2") != std::string::npos);
}

TEST_CASE("Fallible fs.readText in zonk expression lowers to CFG with landing pads", "[zonk_lowering][fs]") {
    std::string src = R"(
        import fs;

        fn loadConfigFile(path: string): string {
            let config: string = fs.readText(path) zonk "{}";
            return config;
        }
    )";
    auto ast = parse_source(src, "fs_zonk_test");
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("loadConfigFile");
    REQUIRE(fn != nullptr);

    std::string err;
    CHECK(fn->validate_cfg(err));
    CHECK(err.empty());

    BasicBlock* try_bb = fn->find_basic_block("zonk.try.1");
    BasicBlock* catch_bb = fn->find_basic_block("zonk.catch.2");
    BasicBlock* end_bb = fn->find_basic_block("zonk.end.3");

    REQUIRE(try_bb != nullptr);
    REQUIRE(catch_bb != nullptr);
    REQUIRE(end_bb != nullptr);

    CHECK(try_bb->has_landing_pad());
    CHECK(try_bb->get_landing_pad() == catch_bb);
    CHECK(catch_bb->is_landing_pad());
}

TEST_CASE("Fallible sys.execOutput in zonk expression lowers to CFG with landing pads", "[zonk_lowering][sys]") {
    std::string src = R"(
        import sys;

        fn runSafeCommand(cmd: string): string {
            let out: string = sys.execOutput(cmd) zonk "";
            return out;
        }
    )";
    auto ast = parse_source(src, "sys_zonk_test");
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("runSafeCommand");
    REQUIRE(fn != nullptr);

    std::string err;
    CHECK(fn->validate_cfg(err));
    CHECK(err.empty());

    BasicBlock* try_bb = fn->find_basic_block("zonk.try.1");
    BasicBlock* catch_bb = fn->find_basic_block("zonk.catch.2");
    BasicBlock* end_bb = fn->find_basic_block("zonk.end.3");

    REQUIRE(try_bb != nullptr);
    REQUIRE(catch_bb != nullptr);
    REQUIRE(end_bb != nullptr);

    CHECK(try_bb->has_landing_pad());
    CHECK(try_bb->get_landing_pad() == catch_bb);
    CHECK(catch_bb->is_landing_pad());
}

