#include <catch2/catch_test_macros.hpp>
#include <keety/ast/stmt.hpp>

using namespace keety::ast;
using namespace keety::diag;

TEST_CASE("IfStmt and BlockStmt node properties", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 20, 1, 21}};

    auto block = std::make_unique<BlockStmt>(span, std::vector<std::unique_ptr<Stmt>>{});
    REQUIRE(block->getStatements().empty());

    IfStmt ifNode(span, nullptr, nullptr, std::move(block), nullptr);
    REQUIRE_FALSE(ifNode.hasInit());
    REQUIRE_FALSE(ifNode.hasElseBranch());
    REQUIRE(ifNode.getThenBranch() != nullptr);
}

TEST_CASE("WhileStmt, ForStmt, and ForInStmt with labels", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 25, 1, 26}};

    auto body = std::make_unique<BlockStmt>(span, std::vector<std::unique_ptr<Stmt>>{});
    WhileStmt whileLoop(span, nullptr, nullptr, std::move(body), "outerWhile");
    REQUIRE(whileLoop.hasLabel());
    REQUIRE(whileLoop.getLabel() == "outerWhile");

    auto forBody = std::make_unique<BlockStmt>(span, std::vector<std::unique_ptr<Stmt>>{});
    ForStmt forLoop(span, nullptr, nullptr, nullptr, std::move(forBody), "loopHeader");
    REQUIRE(forLoop.hasLabel());
    REQUIRE(forLoop.getLabel() == "loopHeader");

    // Key-value for-in: for (const k, v in map)
    auto forInBody = std::make_unique<BlockStmt>(span, std::vector<std::unique_ptr<Stmt>>{});
    ForInStmt forIn(span, true, "key", "val", nullptr, std::move(forInBody), "");
    REQUIRE(forIn.isConst());
    REQUIRE(forIn.isKeyValue());
    REQUIRE(forIn.getKeyName() == "key");
    REQUIRE(forIn.getValueName() == "val");
    REQUIRE_FALSE(forIn.hasLabel());
}

TEST_CASE("SwitchStmt cases and labeled breaks", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 30, 1, 31}};

    std::vector<SwitchCase> cases;
    cases.push_back(SwitchCase{span, {}, std::make_unique<BlockStmt>(span, std::vector<std::unique_ptr<Stmt>>{})}); // default

    SwitchStmt sw(span, nullptr, nullptr, std::move(cases), "switchHeader");
    REQUIRE(sw.hasLabel());
    REQUIRE(sw.getLabel() == "switchHeader");
    REQUIRE(sw.getCases().size() == 1);
    REQUIRE(sw.getCases()[0].isDefault());

    BreakStmt brk(span, "switchHeader");
    REQUIRE(brk.hasTargetLabel());
    REQUIRE(brk.getTargetLabel() == "switchHeader");

    ContinueStmt cont(span, "loopHeader");
    REQUIRE(cont.hasTargetLabel());
    REQUIRE(cont.getTargetLabel() == "loopHeader");
}

TEST_CASE("ReturnStmt, YieldStmt, and ZonkStmt error raising", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 10, 1, 11}};

    ReturnStmt ret(span, nullptr);
    REQUIRE_FALSE(ret.hasValue());

    YieldStmt yld(span, nullptr);
    REQUIRE(yld.getValue() == nullptr);

    ZonkStmt znk(span, nullptr);
    REQUIRE(znk.getErrorExpr() == nullptr);
}
