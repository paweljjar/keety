#include <catch2/catch_test_macros.hpp>
#include <keety/ir/basic_block.hpp>
#include <keety/ir/function.hpp>
#include <keety/ir/instruction.hpp>
#include <keety/ir/module.hpp>
#include <keety/ir/operand.hpp>
#include <keety/typechecker/type.hpp>

#include <sstream>

using namespace keety;
using namespace keety::ir;
using namespace keety::typechecker;

TEST_CASE("KeetIR operands represent values and constants accurately", "[keet_ir]") {
    SECTION("Constants of different primitive kinds") {
        auto c_int = Operand::create_int(42);
        CHECK(c_int.is_constant());
        CHECK(c_int.to_string() == "42");

        auto c_float = Operand::create_float(3.14);
        CHECK(c_float.is_constant());
        CHECK(c_float.to_string() == "3.14");

        auto c_float_whole = Operand::create_float(5.0);
        CHECK(c_float_whole.is_constant());
        CHECK(c_float_whole.to_string() == "5.0");

        auto c_bool = Operand::create_bool(true);
        CHECK(c_bool.is_constant());
        CHECK(c_bool.to_string() == "true");

        auto c_char = Operand::create_char('Z');
        CHECK(c_char.is_constant());
        CHECK(c_char.to_string() == "'Z'");

        auto c_str = Operand::create_string("hello");
        CHECK(c_str.is_constant());
        CHECK(c_str.to_string() == "\"hello\"");

        auto c_null = Operand::create_null();
        CHECK(c_null.is_constant());
        CHECK(c_null.to_string() == "null");
    }

    SECTION("Variables and Virtual Registers") {
        auto int_type = std::make_shared<Type>(TypeKind::Int);
        auto var = Operand::create_var("totalCount", int_type);
        CHECK(var.is_variable());
        CHECK(var.get_name() == "totalCount");
        CHECK(var.to_string() == "totalCount");

        var.set_move(true);
        CHECK(var.is_move());
        CHECK(var.to_string() == "move totalCount");

        var.set_mut_borrow(true);
        CHECK(var.is_mut_borrow());
        CHECK(var.to_string() == "mut move totalCount");

        auto temp = Operand::create_temp(3, int_type);
        CHECK(temp.is_temp());
        CHECK(temp.get_temp_id() == 3);
        CHECK(temp.to_string() == "%3");

        auto glob = Operand::create_global("activeRequests", int_type);
        CHECK(glob.is_global());
        CHECK(glob.to_string() == "@activeRequests");
    }
}

TEST_CASE("KeetIR instructions model operations and terminators correctly", "[keet_ir]") {
    auto int_type = std::make_shared<Type>(TypeKind::Int);
    auto string_type = std::make_shared<Type>(TypeKind::String);
    auto bool_type = std::make_shared<Type>(TypeKind::Bool);

    SECTION("Binary and Unary instructions") {
        auto t0 = Operand::create_temp(0, int_type);
        auto t1 = Operand::create_temp(1, int_type);
        auto t2 = Operand::create_temp(2, int_type);

        BinaryInst add_inst(BinaryOpKind::Add, t2, t0, t1);
        CHECK(add_inst.get_opcode() == Opcode::Binary);
        CHECK_FALSE(add_inst.is_terminator());
        CHECK(add_inst.to_string() == "%2 = add %0, %1");

        UnaryInst neg_inst(UnaryOpKind::Neg, t2, t0);
        CHECK(neg_inst.get_opcode() == Opcode::Unary);
        CHECK(neg_inst.to_string() == "%2 = neg %0");
    }

    SECTION("Memory and assignment instructions") {
        AllocaInst alloca_inst("count", int_type, true);
        CHECK(alloca_inst.get_opcode() == Opcode::Alloca);
        CHECK(alloca_inst.to_string() == "alloca mut count: int");

        auto v_count = Operand::create_var("count", int_type);
        auto t0 = Operand::create_temp(0, int_type);
        LoadInst load_inst(t0, v_count);
        CHECK(load_inst.to_string() == "%0 = load count");

        StoreInst store_inst(v_count, Operand::create_int(10));
        CHECK(store_inst.to_string() == "store count, 10");

        AssignInst assign_inst(v_count, t0);
        CHECK(assign_inst.to_string() == "count = %0");
    }

    SECTION("Branch and control flow terminators") {
        BasicBlock target1("bb1");
        BasicBlock target2("bb2");

        BranchInst br_inst(&target1);
        CHECK(br_inst.is_terminator());
        CHECK(br_inst.get_successors().size() == 1);
        CHECK(br_inst.get_successors()[0] == &target1);
        CHECK(br_inst.to_string() == "br @bb1");

        CondBranchInst cond_br(Operand::create_bool(true), &target1, &target2);
        CHECK(cond_br.is_terminator());
        CHECK(cond_br.get_successors().size() == 2);
        CHECK(cond_br.to_string() == "br true, @bb1, @bb2");

        SwitchInst switch_inst(Operand::create_temp(0, int_type), &target2);
        switch_inst.add_case(Operand::create_int(1), &target1);
        CHECK(switch_inst.is_terminator());
        CHECK(switch_inst.get_successors().size() == 2);

        ReturnInst ret_void;
        CHECK(ret_void.is_terminator());
        CHECK(ret_void.to_string() == "ret");
        CHECK(ret_void.get_successors().empty());

        ReturnInst ret_val(Operand::create_int(100));
        CHECK(ret_val.is_terminator());
        CHECK(ret_val.to_string() == "ret 100");

        ThrowInst throw_inst(Operand::create_string("out of memory"));
        CHECK(throw_inst.is_terminator());
        CHECK(throw_inst.to_string() == "throw \"out of memory\"");

        UnreachableInst unreachable_inst;
        CHECK(unreachable_inst.is_terminator());
        CHECK(unreachable_inst.to_string() == "unreachable");
    }

    SECTION("Calls and invocations") {
        auto t0 = Operand::create_temp(0, int_type);
        std::vector<Operand> args = {Operand::create_int(1), Operand::create_int(2)};
        CallInst call_inst(t0, "sum", args);
        CHECK(call_inst.to_string() == "%0 = call @sum(1, 2)");

        auto receiver = Operand::create_var("list", nullptr);
        MethodCallInst method_inst(std::nullopt, receiver, "push", {Operand::create_int(42)});
        CHECK(method_inst.to_string() == "call_method list.push(42)");
    }

    SECTION("Aggregates, indexing and reserve optimization") {
        auto t0 = Operand::create_temp(0, nullptr);
        ArrayConstructInst arr_inst(t0, {Operand::create_int(1), Operand::create_int(2)}, 16);
        CHECK(arr_inst.to_string() == "%0 = array_construct [1, 2] [reserve: 16]");

        MapConstructInst map_inst(t0, {{Operand::create_string("key"), Operand::create_int(99)}});
        CHECK(map_inst.to_string() == "%0 = map_construct {\"key\": 99}");

        ClassConstructInst cls_inst(t0, "Point", {{"x", Operand::create_int(10)}, {"y", Operand::create_int(20)}});
        CHECK(cls_inst.to_string() == "%0 = class_construct Point {x: 10, y: 20}");

        ReserveInst reserve_inst(Operand::create_var("vec", nullptr), Operand::create_int(100));
        CHECK(reserve_inst.to_string() == "reserve vec, 100");
    }

    SECTION("Type operations and boxing") {
        auto t0 = Operand::create_temp(0, string_type);
        TypeOfInst typeof_inst(t0, Operand::create_var("val", nullptr));
        CHECK(typeof_inst.to_string() == "%0 = typeof val");

        TypeCastInst cast_inst(t0, Operand::create_var("val", nullptr), string_type);
        CHECK(cast_inst.to_string() == "%0 = type_cast<string> val");

        BoxInst box_inst(t0, Operand::create_var("node", nullptr));
        CHECK(box_inst.to_string() == "%0 = box node");

        UnboxInst unbox_inst(t0, Operand::create_var("boxedNode", nullptr));
        CHECK(unbox_inst.to_string() == "%0 = unbox boxedNode");

        LandingPadInst landing_pad(Operand::create_var("err", nullptr));
        CHECK(landing_pad.to_string() == "err = landing_pad");
    }
}

TEST_CASE("BasicBlock manages instruction stream and terminators", "[keet_ir]") {
    BasicBlock bb("entry");
    CHECK(bb.empty());
    CHECK(bb.size() == 0);
    CHECK_FALSE(bb.has_terminator());

    auto int_type = std::make_shared<Type>(TypeKind::Int);
    bb.append_instruction(std::make_unique<AllocaInst>("x", int_type, false));
    CHECK(bb.size() == 1);
    CHECK_FALSE(bb.has_terminator());

    bb.append_instruction(std::make_unique<ReturnInst>(Operand::create_int(0)));
    CHECK(bb.size() == 2);
    CHECK(bb.has_terminator());
    CHECK(bb.get_terminator()->get_opcode() == Opcode::Return);

    // Overwriting terminator
    BasicBlock exit_bb("exit");
    bb.set_terminator(std::make_unique<BranchInst>(&exit_bb));
    CHECK(bb.size() == 2);
    CHECK(bb.get_terminator()->get_opcode() == Opcode::Branch);
}

TEST_CASE("Control Flow Graph correctly links basic blocks and validates structure", "[keet_ir]") {
    auto int_type = std::make_shared<Type>(TypeKind::Int);
    auto bool_type = std::make_shared<Type>(TypeKind::Bool);

    Function fn("max", int_type);
    fn.add_parameter({"a", int_type, false, false, false});
    fn.add_parameter({"b", int_type, false, false, false});

    BasicBlock* entry = fn.create_basic_block("entry");
    BasicBlock* then_bb = fn.create_basic_block("then");
    BasicBlock* else_bb = fn.create_basic_block("else");
    BasicBlock* merge_bb = fn.create_basic_block("merge");

    fn.set_entry_block(entry);

    // Entry block: condition check and branch
    auto cond = fn.create_temp(bool_type);
    entry->append_instruction(std::make_unique<BinaryInst>(
        BinaryOpKind::Gt, cond, Operand::create_var("a", int_type), Operand::create_var("b", int_type)
    ));
    entry->append_instruction(std::make_unique<CondBranchInst>(cond, then_bb, else_bb));

    // Then block
    then_bb->append_instruction(std::make_unique<BranchInst>(merge_bb));

    // Else block
    else_bb->append_instruction(std::make_unique<BranchInst>(merge_bb));

    // Merge block
    merge_bb->append_instruction(std::make_unique<ReturnInst>(Operand::create_var("a", int_type)));

    // Recompute CFG edges
    fn.recompute_cfg_edges();

    // Verify entry block edges
    CHECK(entry->get_successors().size() == 2);
    CHECK(entry->get_predecessors().empty());
    CHECK(then_bb->get_predecessors().size() == 1);
    CHECK(then_bb->get_predecessors()[0] == entry);
    CHECK(else_bb->get_predecessors().size() == 1);
    CHECK(else_bb->get_predecessors()[0] == entry);

    // Verify merge block edges
    CHECK(merge_bb->get_predecessors().size() == 2);
    CHECK(merge_bb->get_successors().empty());

    // Validate CFG
    std::string err;
    bool is_valid = fn.validate_cfg(err);
    CHECK(is_valid);
    CHECK(err.empty());
}

TEST_CASE("CFG validation catches malformed control flow graphs", "[keet_ir]") {
    auto int_type = std::make_shared<Type>(TypeKind::Int);

    SECTION("Missing terminator fails validation") {
        Function fn("broken", int_type);
        BasicBlock* entry = fn.create_basic_block("entry");
        fn.set_entry_block(entry);
        entry->append_instruction(std::make_unique<AllocaInst>("x", int_type, false));

        std::string err;
        CHECK_FALSE(fn.validate_cfg(err));
        CHECK(err.find("missing a terminator") != std::string::npos);
    }

    SECTION("Instructions placed after terminator fail validation") {
        Function fn("broken_after_term", int_type);
        BasicBlock* entry = fn.create_basic_block("entry");
        fn.set_entry_block(entry);
        entry->append_instruction(std::make_unique<ReturnInst>(Operand::create_int(1)));
        entry->append_instruction(std::make_unique<AllocaInst>("x", int_type, false));

        std::string err;
        CHECK_FALSE(fn.validate_cfg(err));
        CHECK(err.find("instructions after terminator") != std::string::npos);
    }

    SECTION("Edge asymmetry fails validation") {
        Function fn("broken_edges", int_type);
        BasicBlock* b1 = fn.create_basic_block("b1");
        BasicBlock* b2 = fn.create_basic_block("b2");
        fn.set_entry_block(b1);

        b1->append_instruction(std::make_unique<BranchInst>(b2));
        b2->append_instruction(std::make_unique<ReturnInst>());

        // Manually introduce asymmetry without recomputing edges
        b1->add_successor(b2);
        // deliberately omit b2->add_predecessor(b1)

        std::string err;
        CHECK_FALSE(fn.validate_cfg(err));
        CHECK(err.find("CFG edge asymmetry") != std::string::npos);
    }
}

TEST_CASE("KeetIR Module correctly houses globals, functions and serializes to text", "[keet_ir]") {
    auto int_type = std::make_shared<Type>(TypeKind::Int);

    Module mod("math_utils");
    mod.add_global({"max_limit", int_type, true, true, Operand::create_int(1000)});

    auto fn = std::make_unique<Function>("square", int_type);
    fn->add_parameter({"x", int_type, false, false, false});
    BasicBlock* entry = fn->create_basic_block("entry");
    fn->set_entry_block(entry);

    auto t0 = fn->create_temp(int_type);
    entry->append_instruction(std::make_unique<BinaryInst>(
        BinaryOpKind::Mul, t0, Operand::create_var("x", int_type), Operand::create_var("x", int_type)
    ));
    entry->append_instruction(std::make_unique<ReturnInst>(t0));

    Function* fn_ptr = mod.add_function(std::move(fn));
    REQUIRE(fn_ptr != nullptr);

    mod.recompute_cfg();

    std::string err;
    CHECK(mod.validate(err));

    std::string text = mod.to_string();
    CHECK(text.find("module \"math_utils\"") != std::string::npos);
    CHECK(text.find("export const @max_limit: int = 1000;") != std::string::npos);
    CHECK(text.find("fn @square(x: int): int") != std::string::npos);
    CHECK(text.find("%0 = mul x, x") != std::string::npos);
    CHECK(text.find("ret %0") != std::string::npos);
}
