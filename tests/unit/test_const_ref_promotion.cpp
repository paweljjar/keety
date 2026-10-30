#include <catch2/catch_test_macros.hpp>
#include <keety/ir/ast_lowerer.hpp>
#include <keety/ir/const_ref_promotion.hpp>
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

TEST_CASE("Const-reference promotion promotes unmutated composite parameters", "[const_ref_promotion]") {
    std::string src = R"(
        class UserInfo {
            let name: string;
            let age: int;
        }

        fn inspect_data(text: string, list: array<int>, mapping: map<string, int>, user: UserInfo): int {
            return list.length();
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("inspect_data");
    REQUIRE(fn != nullptr);

    ConstRefPromotionPass pass;
    size_t count = pass.run_on_function(*fn);
    CHECK(count == 4);

    const auto& params = fn->get_parameters();
    REQUIRE(params.size() == 4);

    CHECK(params[0].name == "text");
    CHECK(params[0].is_const_ref);

    CHECK(params[1].name == "list");
    CHECK(params[1].is_const_ref);

    CHECK(params[2].name == "mapping");
    CHECK(params[2].is_const_ref);

    CHECK(params[3].name == "user");
    CHECK(params[3].is_const_ref);

    std::string fn_str = fn->to_string();
    CHECK(fn_str.find("const& text: string") != std::string::npos);
    CHECK(fn_str.find("const& list: array<int>") != std::string::npos);
    CHECK(fn_str.find("const& mapping: map<string, int>") != std::string::npos);
    CHECK(fn_str.find("const& user: UserInfo") != std::string::npos);
}

TEST_CASE("Const-reference promotion ignores scalar primitive parameters", "[const_ref_promotion]") {
    std::string src = R"(
        fn compute_scalars(count: int, ratio: float, active: bool, tag: char): int {
            return count;
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("compute_scalars");
    REQUIRE(fn != nullptr);

    ConstRefPromotionPass pass;
    size_t count = pass.run_on_function(*fn);
    CHECK(count == 0);

    const auto& params = fn->get_parameters();
    REQUIRE(params.size() == 4);
    for (const auto& p : params) {
        CHECK_FALSE(p.is_const_ref);
    }
}

TEST_CASE("Const-reference promotion rejects parameters reassigned via store", "[const_ref_promotion]") {
    std::string src = R"(
        fn reassign_param(text: string): void {
            text = "overwritten";
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("reassign_param");
    REQUIRE(fn != nullptr);

    ConstRefPromotionPass pass;
    size_t count = pass.run_on_function(*fn);
    CHECK(count == 0);

    const auto& params = fn->get_parameters();
    REQUIRE(params.size() == 1);
    CHECK_FALSE(params[0].is_const_ref);
}

TEST_CASE("Const-reference promotion rejects collection parameters mutated via mutating methods", "[const_ref_promotion]") {
    std::string src = R"(
        fn append_data(list: array<string>, item: string): void {
            list.push(item);
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("append_data");
    REQUIRE(fn != nullptr);

    ConstRefPromotionPass pass;
    size_t count = pass.run_on_function(*fn);
    // 'item' is not mutated so it is promoted; 'list' is mutated via push so it is NOT promoted
    CHECK(count == 1);

    const auto& params = fn->get_parameters();
    REQUIRE(params.size() == 2);
    CHECK(params[0].name == "list");
    CHECK_FALSE(params[0].is_const_ref);

    CHECK(params[1].name == "item");
    CHECK(params[1].is_const_ref);
}

TEST_CASE("Const-reference promotion rejects collections mutated via index store", "[const_ref_promotion]") {
    std::string src = R"(
        fn set_element(arr: array<string>): void {
            arr[0] = "first";
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("set_element");
    REQUIRE(fn != nullptr);

    ConstRefPromotionPass pass;
    size_t count = pass.run_on_function(*fn);
    CHECK(count == 0);

    const auto& params = fn->get_parameters();
    REQUIRE(params.size() == 1);
    CHECK_FALSE(params[0].is_const_ref);
}

TEST_CASE("Const-reference promotion rejects class instances mutated via member store", "[const_ref_promotion]") {
    std::string src = R"(
        class Entity {
            let name: string;
        }

        fn update_entity(e: Entity): void {
            e.name = "new_name";
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("update_entity");
    REQUIRE(fn != nullptr);

    ConstRefPromotionPass pass;
    size_t count = pass.run_on_function(*fn);
    CHECK(count == 0);

    const auto& params = fn->get_parameters();
    REQUIRE(params.size() == 1);
    CHECK_FALSE(params[0].is_const_ref);
}

TEST_CASE("Const-reference promotion rejects parameters passed as mutable reference borrows", "[const_ref_promotion]") {
    std::string src = R"(
        fn pass_mut(item: string): void {
            modify_str(mut item);
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("pass_mut");
    REQUIRE(fn != nullptr);

    ConstRefPromotionPass pass;
    size_t count = pass.run_on_function(*fn);
    CHECK(count == 0);

    const auto& params = fn->get_parameters();
    REQUIRE(params.size() == 1);
    CHECK_FALSE(params[0].is_const_ref);
}

TEST_CASE("Const-reference promotion skips parameters explicitly declared with mut", "[const_ref_promotion]") {
    std::string src = R"(
        fn mut_param_func(mut data: string): void {
            print(data);
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("mut_param_func");
    REQUIRE(fn != nullptr);

    ConstRefPromotionPass pass;
    size_t count = pass.run_on_function(*fn);
    CHECK(count == 0);

    const auto& params = fn->get_parameters();
    REQUIRE(params.size() == 1);
    CHECK(params[0].is_mut);
    CHECK_FALSE(params[0].is_const_ref);
}

TEST_CASE("Const-reference promotion respects moved status from prior optimization passes", "[const_ref_promotion]") {
    std::string src = R"(
        fn return_string(s: string): string {
            return s;
        }
    )";
    auto ast = parse_source(src);
    AstLowerer lowerer;
    auto mod = lowerer.lower_module(ast);

    REQUIRE(mod != nullptr);
    Function* fn = mod->find_function("return_string");
    REQUIRE(fn != nullptr);

    // Run MoveInsertionPass first (Pass 2)
    MoveInsertionPass move_pass;
    size_t moves = move_pass.run_on_function(*fn);
    CHECK(moves == 1);

    // Parameter was moved to be returned; Pass 3 must NOT promote it to const&
    ConstRefPromotionPass const_ref_pass;
    size_t count = const_ref_pass.run_on_function(*fn);
    CHECK(count == 0);

    const auto& params = fn->get_parameters();
    REQUIRE(params.size() == 1);
    CHECK_FALSE(params[0].is_const_ref);
}

TEST_CASE("Const-reference promotion supports union types configuration", "[const_ref_promotion]") {
    Function fn("test_union", std::make_shared<typechecker::Type>(typechecker::TypeKind::Void));

    auto int_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::Int);
    auto str_type = std::make_shared<typechecker::Type>(typechecker::TypeKind::String);
    auto union_type = std::make_shared<typechecker::UnionType>(std::vector<typechecker::TypePtr>{int_type, str_type});

    fn.add_parameter({"val", union_type, false, false, false});
    BasicBlock* entry = fn.create_basic_block("entry");
    fn.set_entry_block(entry);
    entry->append_instruction(std::make_unique<ReturnInst>());
    fn.recompute_cfg_edges();

    // With include_unions = false
    ConstRefPromotionOptions opts_no_union;
    opts_no_union.include_unions = false;
    ConstRefPromotionPass pass_no_union(opts_no_union);
    CHECK_FALSE(pass_no_union.is_promotable(fn.get_parameters()[0], fn));

    // With include_unions = true (default)
    ConstRefPromotionPass pass_default;
    CHECK(pass_default.is_promotable(fn.get_parameters()[0], fn));

    size_t promoted = pass_default.run_on_function(fn);
    CHECK(promoted == 1);
    CHECK(fn.get_parameters()[0].is_const_ref);
}
