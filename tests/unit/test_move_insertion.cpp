#include <catch2/catch_test_macros.hpp>
#include <keety/ir/ast_lowerer.hpp>
#include <keety/ir/liveness_analysis.hpp>
#include <keety/ir/move_insertion.hpp>
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

TEST_CASE("Move insertion transforms return statements with last-use local variables into moves", "[move_insertion]") {
    std::string src = R"(
        fn test_return(item: string): string {
            return item;
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("test_return");
    REQUIRE(fn != nullptr);

    MoveInsertionPass pass;
    size_t moves = pass.run_on_function(*fn);
    CHECK(moves == 1);

    // Verify ReturnInst operand is marked as move
    REQUIRE(!fn->get_blocks().empty());
    const auto& insts = fn->get_blocks().front()->get_instructions();
    REQUIRE(!insts.empty());

    const Instruction* last_inst = insts.back().get();
    REQUIRE(last_inst->get_opcode() == Opcode::Return);
    const auto* ret = static_cast<const ReturnInst*>(last_inst);
    REQUIRE(ret->has_value());
    CHECK(ret->get_value()->is_move());
    CHECK(ret->get_value()->get_name() == "item");
    CHECK(ret->to_string() == "ret move item");
}

TEST_CASE("Move insertion transforms last-use assignments and stores into moves", "[move_insertion]") {
    std::string src = R"(
        fn test_store(source: string): string {
            let target: string = source;
            return target;
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("test_store");
    REQUIRE(fn != nullptr);

    MoveInsertionPass pass;
    size_t moves = pass.run_on_function(*fn);
    CHECK(moves == 2);

    bool found_store_move = false;
    bool found_return_move = false;

    for (const auto& bb : fn->get_blocks()) {
        for (const auto& inst : bb->get_instructions()) {
            if (inst->get_opcode() == Opcode::Store) {
                const auto* store = static_cast<const StoreInst*>(inst.get());
                if (store->get_dest().get_name() == "target" && store->get_val().is_move()) {
                    CHECK(store->get_val().get_name() == "source");
                    found_store_move = true;
                }
            } else if (inst->get_opcode() == Opcode::Return) {
                const auto* ret = static_cast<const ReturnInst*>(inst.get());
                if (ret->has_value() && ret->get_value()->is_move()) {
                    CHECK(ret->get_value()->get_name() == "target");
                    found_return_move = true;
                }
            }
        }
    }

    CHECK(found_store_move);
    CHECK(found_return_move);
}

TEST_CASE("Move insertion transforms method call arguments at last-use into moves while preserving receiver", "[move_insertion]") {
    std::string src = R"(
        fn test_method(elem: string): void {
            let list: array<string> = [];
            list.push(elem);
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("test_method");
    REQUIRE(fn != nullptr);

    MoveInsertionPass pass;
    size_t moves = pass.run_on_function(*fn);
    CHECK(moves >= 1);

    bool found_method_move = false;
    for (const auto& bb : fn->get_blocks()) {
        for (const auto& inst : bb->get_instructions()) {
            if (inst->get_opcode() == Opcode::MethodCall) {
                const auto* mcall = static_cast<const MethodCallInst*>(inst.get());
                if (mcall->get_method() == "push") {
                    CHECK_FALSE(mcall->get_receiver().is_move());
                    REQUIRE(mcall->get_args().size() == 1);
                    CHECK(mcall->get_args()[0].is_move());
                    CHECK(mcall->get_args()[0].get_name() == "elem");
                    found_method_move = true;
                }
            }
        }
    }

    CHECK(found_method_move);
}

TEST_CASE("Move insertion selectively transforms only the final sequential read", "[move_insertion]") {
    std::string src = R"(
        fn multi_read(val: string): void {
            sink(val);
            sink(val);
            sink(val);
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("multi_read");
    REQUIRE(fn != nullptr);

    MoveInsertionPass pass;
    size_t moves = pass.run_on_function(*fn);
    CHECK(moves == 1);

    std::vector<bool> arg_moves;
    for (const auto& bb : fn->get_blocks()) {
        for (const auto& inst : bb->get_instructions()) {
            if (inst->get_opcode() == Opcode::Call) {
                const auto* call = static_cast<const CallInst*>(inst.get());
                if (call->get_callee() == "sink") {
                    REQUIRE(call->get_args().size() == 1);
                    arg_moves.push_back(call->get_args()[0].is_move());
                }
            }
        }
    }

    REQUIRE(arg_moves.size() == 3);
    CHECK_FALSE(arg_moves[0]);
    CHECK_FALSE(arg_moves[1]);
    CHECK(arg_moves[2]);
}

TEST_CASE("Move insertion optimizes last reads across divergent conditional branches", "[move_insertion]") {
    std::string src = R"(
        fn branch_move(flag: bool, val: string): string {
            if (flag) {
                return val;
            } else {
                consume(val);
                return "fallback";
            }
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("branch_move");
    REQUIRE(fn != nullptr);

    MoveInsertionPass pass;
    size_t moves = pass.run_on_function(*fn);
    CHECK(moves == 2);

    bool then_val_moved = false;
    bool else_val_moved = false;

    for (const auto& bb : fn->get_blocks()) {
        for (const auto& inst : bb->get_instructions()) {
            if (inst->get_opcode() == Opcode::Return) {
                const auto* ret = static_cast<const ReturnInst*>(inst.get());
                if (ret->has_value() && ret->get_value()->get_name() == "val" && ret->get_value()->is_move()) {
                    then_val_moved = true;
                }
            } else if (inst->get_opcode() == Opcode::Call) {
                const auto* call = static_cast<const CallInst*>(inst.get());
                if (call->get_callee() == "consume" && !call->get_args().empty() &&
                    call->get_args()[0].get_name() == "val" && call->get_args()[0].is_move()) {
                    else_val_moved = true;
                }
            }
        }
    }

    CHECK(then_val_moved);
    CHECK(else_val_moved);
}

TEST_CASE("Move insertion preserves loop live ranges and does not move loop variables inside body", "[move_insertion]") {
    std::string src = R"(
        fn loop_move(n: int, s: string): void {
            let i: int = 0;
            while (i < n) {
                keep(s);
                i = i + 1;
            }
            sink(s);
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("loop_move");
    REQUIRE(fn != nullptr);

    MoveInsertionPass pass;
    size_t moves = pass.run_on_function(*fn);
    CHECK(moves >= 1);

    bool keep_moved = false;
    bool sink_moved = false;

    for (const auto& bb : fn->get_blocks()) {
        for (const auto& inst : bb->get_instructions()) {
            if (inst->get_opcode() == Opcode::Call) {
                const auto* call = static_cast<const CallInst*>(inst.get());
                if (call->get_callee() == "keep" && !call->get_args().empty() && call->get_args()[0].get_name() == "s") {
                    keep_moved = call->get_args()[0].is_move();
                } else if (call->get_callee() == "sink" && !call->get_args().empty() && call->get_args()[0].get_name() == "s") {
                    sink_moved = call->get_args()[0].is_move();
                }
            }
        }
    }

    CHECK_FALSE(keep_moved);
    CHECK(sink_moved);
}

TEST_CASE("Move insertion rejects mutable reference parameters from move transformation", "[move_insertion]") {
    std::string src = R"(
        fn test_mut(mut val: string): void {
            sink(val);
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("test_mut");
    REQUIRE(fn != nullptr);

    MoveInsertionPass pass;
    size_t moves = pass.run_on_function(*fn);
    CHECK(moves == 0);

    for (const auto& bb : fn->get_blocks()) {
        for (const auto& inst : bb->get_instructions()) {
            if (inst->get_opcode() == Opcode::Call) {
                const auto* call = static_cast<const CallInst*>(inst.get());
                if (call->get_callee() == "sink") {
                    REQUIRE(call->get_args().size() == 1);
                    CHECK_FALSE(call->get_args()[0].is_move());
                }
            }
        }
    }
}

TEST_CASE("Move insertion does not move duplicate occurrences of the same variable in a single call", "[move_insertion]") {
    Function fn("test_duplicate", std::make_shared<typechecker::Type>(typechecker::TypeKind::Void));
    auto str_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::String);
    fn.add_parameter({"data", str_type, false, false, false});

    BasicBlock* entry = fn.create_basic_block("entry");
    fn.set_entry_block(entry);

    std::vector<Operand> args = {
        Operand::create_var("data", str_type),
        Operand::create_var("data", str_type)
    };
    entry->append_instruction(std::make_unique<CallInst>(std::nullopt, "pair_sink", std::move(args)));
    entry->append_instruction(std::make_unique<ReturnInst>());
    fn.recompute_cfg_edges();

    MoveInsertionPass pass;
    size_t moves = pass.run_on_function(fn);
    CHECK(moves == 0);

    const auto& call = static_cast<const CallInst&>(*entry->get_instructions()[0]);
    CHECK_FALSE(call.get_args()[0].is_move());
    CHECK_FALSE(call.get_args()[1].is_move());
}

TEST_CASE("Move insertion respects move_scalars configuration option", "[move_insertion]") {
    std::string src = R"(
        fn test_scalar(x: int): int {
            return x;
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;

    // Run with move_scalars = false
    auto mod_no_scalar = lowerer.lower_module(ast);
    Function* fn_no_scalar = mod_no_scalar->find_function("test_scalar");
    REQUIRE(fn_no_scalar != nullptr);

    MoveInsertionOptions opts;
    opts.move_scalars = false;
    MoveInsertionPass pass_no_scalar(opts);
    size_t moves_no_scalar = pass_no_scalar.run_on_function(*fn_no_scalar);
    CHECK(moves_no_scalar == 0);

    // Run with move_scalars = true (default)
    auto mod_with_scalar = lowerer.lower_module(ast);
    Function* fn_with_scalar = mod_with_scalar->find_function("test_scalar");
    REQUIRE(fn_with_scalar != nullptr);

    MoveInsertionPass pass_default;
    size_t moves_default = pass_default.run_on_function(*fn_with_scalar);
    CHECK(moves_default == 1);
}
