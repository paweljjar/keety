#include <catch2/catch_test_macros.hpp>
#include <keety/diag/error_code.hpp>

using namespace keety::diag;

TEST_CASE("ErrorCode ranges and string representation", "[diag]") {
    // E0001 - E0999
    REQUIRE(getErrorCodeString(ErrorCode::UnterminatedStringLiteral) == "E0001");
    REQUIRE(getErrorCodeString(ErrorCode::ExpectedSemicolon) == "E0007");

    // E1000 - E1999
    REQUIRE(getErrorCodeString(ErrorCode::CircularModuleDependency) == "E1001");

    // E2000 - E2999
    REQUIRE(getErrorCodeString(ErrorCode::IdentifierShadowed) == "E2001");
    REQUIRE(getErrorCodeString(ErrorCode::ExportConstWrite) == "E2002");

    // E3000 - E3999
    REQUIRE(getErrorCodeString(ErrorCode::MissingTypeAnnotation) == "E3001");
    REQUIRE(getErrorCodeString(ErrorCode::ImplicitTypeWidening) == "E3002");

    // E4000 - E4999
    REQUIRE(getErrorCodeString(ErrorCode::DuplicateMutableBorrow) == "E4001");
    REQUIRE(getErrorCodeString(ErrorCode::ContainerAndElementBorrow) == "E4002");
    REQUIRE(getErrorCodeString(ErrorCode::MapElementMutableBorrow) == "E4003");
    REQUIRE(getErrorCodeString(ErrorCode::AsyncMutableParameter) == "E4004");

    // E5000 - E5999
    REQUIRE(getErrorCodeString(ErrorCode::MissingReturnStatement) == "E5001");
    REQUIRE(getErrorCodeString(ErrorCode::YieldOutsideZonkBlock) == "E5006");

    // E6000 - E6999
    REQUIRE(getErrorCodeString(ErrorCode::ExternSymbolNotFound) == "E6001");
}

TEST_CASE("ErrorCode metadata lookup", "[diag]") {
    auto info = getErrorCodeInfo(ErrorCode::DuplicateMutableBorrow);
    REQUIRE(info.code == "E4001");
    REQUIRE(info.title == "cannot pass multiple mutable references to the same storage path");
    REQUIRE_FALSE(info.description.empty());
}
