#include <catch2/catch_test_macros.hpp>
#include <keety/ir/ast_lowerer.hpp>
#include <keety/ir/recursive_boxing.hpp>
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

TEST_CASE("Recursive boxing detects self-referential class fields in lists and trees", "[recursive_boxing]") {
    std::string src = R"(
        class Node {
            let val: int;
            let next: Node | null;
        }

        class BinaryTree {
            let val: int;
            let left: BinaryTree | null;
            let right: BinaryTree | null;
        }

        class NonRecursive {
            let id: int;
            let tag: string;
        }
    )";
    auto ast = parse_source(src);

    RecursiveBoxingPass pass;
    pass.scan_classes(ast);

    CHECK(pass.is_recursive_class("Node"));
    CHECK(pass.is_recursive_field("Node", "next"));
    CHECK_FALSE(pass.is_recursive_field("Node", "val"));

    CHECK(pass.is_recursive_class("BinaryTree"));
    CHECK(pass.is_recursive_field("BinaryTree", "left"));
    CHECK(pass.is_recursive_field("BinaryTree", "right"));
    CHECK_FALSE(pass.is_recursive_field("BinaryTree", "val"));

    CHECK_FALSE(pass.is_recursive_class("NonRecursive"));
    CHECK_FALSE(pass.is_recursive_field("NonRecursive", "id"));
    CHECK_FALSE(pass.is_recursive_field("NonRecursive", "tag"));
}

TEST_CASE("Recursive boxing detects mutual cyclic class dependencies", "[recursive_boxing]") {
    std::string src = R"(
        class Alpha {
            let name: string;
            let beta: Beta;
        }

        class Beta {
            let count: int;
            let alpha: Alpha;
        }
    )";
    auto ast = parse_source(src);

    RecursiveBoxingPass pass;
    pass.scan_classes(ast);

    CHECK(pass.is_recursive_class("Alpha"));
    CHECK(pass.is_recursive_class("Beta"));

    CHECK(pass.is_recursive_field("Alpha", "beta"));
    CHECK_FALSE(pass.is_recursive_field("Alpha", "name"));

    CHECK(pass.is_recursive_field("Beta", "alpha"));
    CHECK_FALSE(pass.is_recursive_field("Beta", "count"));
}

TEST_CASE("Recursive boxing inserts BoxInst on member store to recursive field", "[recursive_boxing]") {
    std::string src = R"(
        class Node {
            let val: int;
            let next: Node | null;
        }

        fn update_next(node: Node, target: Node): void {
            node.next = target;
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("update_next");
    REQUIRE(fn != nullptr);

    RecursiveBoxingPass pass;
    pass.scan_classes(ast);
    size_t count = pass.run_on_function(*fn);
    CHECK(count == 1);

    // Verify BoxInst is placed immediately before MemberStoreInst
    bool found_box = false;
    for (const auto& bb : fn->get_blocks()) {
        const auto& insts = bb->get_instructions();
        for (size_t i = 0; i < insts.size(); ++i) {
            if (insts[i]->get_opcode() == Opcode::Box) {
                found_box = true;
                const auto* box = static_cast<const BoxInst*>(insts[i].get());
                CHECK(box->get_src().get_name() == "target");

                REQUIRE(i + 1 < insts.size());
                REQUIRE(insts[i + 1]->get_opcode() == Opcode::MemberStore);
                const auto* store = static_cast<const MemberStoreInst*>(insts[i + 1].get());
                CHECK(store->get_member() == "next");
                CHECK(store->get_val() == box->get_dest());
            }
        }
    }

    CHECK(found_box);
}

TEST_CASE("Recursive boxing inserts UnboxInst on member load from recursive field", "[recursive_boxing]") {
    std::string src = R"(
        class Node {
            let val: int;
            let next: Node | null;
        }

        fn get_next(node: Node): Node | null {
            let res = node.next;
            return res;
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("get_next");
    REQUIRE(fn != nullptr);

    RecursiveBoxingPass pass;
    pass.scan_classes(ast);
    size_t count = pass.run_on_function(*fn);
    CHECK(count == 1);

    // Verify UnboxInst is placed immediately after MemberLoadInst
    bool found_unbox = false;
    for (const auto& bb : fn->get_blocks()) {
        const auto& insts = bb->get_instructions();
        for (size_t i = 0; i < insts.size(); ++i) {
            if (insts[i]->get_opcode() == Opcode::MemberLoad) {
                const auto* load = static_cast<const MemberLoadInst*>(insts[i].get());
                if (load->get_member() == "next") {
                    REQUIRE(i + 1 < insts.size());
                    REQUIRE(insts[i + 1]->get_opcode() == Opcode::Unbox);
                    const auto* unbox = static_cast<const UnboxInst*>(insts[i + 1].get());
                    CHECK(unbox->get_src() == load->get_dest());
                    found_unbox = true;
                }
            }
        }
    }

    CHECK(found_unbox);
}

TEST_CASE("Recursive boxing inserts BoxInst for recursive fields in ClassConstructInst", "[recursive_boxing]") {
    Function fn("test_construct", std::make_shared<typechecker::Type>(typechecker::TypeKind::Void));
    auto node_type = std::make_shared<typechecker::CustomType>("Node");
    fn.add_parameter({"next_node", node_type, false, false, false});

    BasicBlock* entry = fn.create_basic_block("entry");
    fn.set_entry_block(entry);

    std::vector<std::pair<std::string, Operand>> fields = {
        {"val", Operand::create_int(42)},
        {"next", Operand::create_var("next_node", node_type)}
    };

    Operand dest = Operand::create_var("my_node", node_type);
    entry->append_instruction(std::make_unique<ClassConstructInst>(dest, "Node", std::move(fields)));
    entry->append_instruction(std::make_unique<ReturnInst>());
    fn.recompute_cfg_edges();

    RecursiveBoxingPass pass;
    pass.register_class("Node", {
        {"val", std::make_shared<typechecker::Type>(typechecker::TypeKind::Int)},
        {"next", std::make_shared<typechecker::UnionType>(std::vector<typechecker::TypePtr>{
            node_type, std::make_shared<typechecker::Type>(typechecker::TypeKind::Null)
        })}
    });

    size_t count = pass.run_on_function(fn);
    CHECK(count == 1);

    const auto& insts = entry->get_instructions();
    REQUIRE(insts.size() == 3);
    REQUIRE(insts[0]->get_opcode() == Opcode::Box);
    const auto* box = static_cast<const BoxInst*>(insts[0].get());
    CHECK(box->get_src().get_name() == "next_node");

    REQUIRE(insts[1]->get_opcode() == Opcode::ClassConstruct);
    const auto* cc = static_cast<const ClassConstructInst*>(insts[1].get());
    CHECK(cc->get_fields()[1].first == "next");
    CHECK(cc->get_fields()[1].second == box->get_dest());
}

TEST_CASE("Recursive boxing pass is idempotent across multiple passes", "[recursive_boxing]") {
    std::string src = R"(
        class Node {
            let val: int;
            let next: Node | null;
        }

        fn update_and_read(node: Node, target: Node): Node | null {
            node.next = target;
            return node.next;
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("update_and_read");
    REQUIRE(fn != nullptr);

    RecursiveBoxingPass pass;
    pass.scan_classes(ast);

    size_t first_run = pass.run_on_function(*fn);
    CHECK(first_run == 2); // 1 box for store, 1 unbox for load

    // Second run should detect existing box and unbox instructions and make 0 additions
    size_t second_run = pass.run_on_function(*fn);
    CHECK(second_run == 0);
}
