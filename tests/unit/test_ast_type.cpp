#include <catch2/catch_test_macros.hpp>
#include <keety/ast/type.hpp>

using namespace keety::ast;
using namespace keety::diag;

TEST_CASE("PrimitiveType and names", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 3, 1, 4}};

    PrimitiveType intType(span, PrimitiveTypeKind::Int);
    REQUIRE(intType.getKind() == PrimitiveTypeKind::Int);
    REQUIRE(intType.getTypeName() == "int");

    PrimitiveType strType(span, PrimitiveTypeKind::String);
    REQUIRE(strType.getTypeName() == "string");

    PrimitiveType voidType(span, PrimitiveTypeKind::Void);
    REQUIRE(voidType.getTypeName() == "void");
}

TEST_CASE("ArrayType and MapType compositions", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 10, 1, 11}};

    // array<int>
    auto elem = std::make_unique<PrimitiveType>(span, PrimitiveTypeKind::Int);
    ArrayType arrType(span, std::move(elem));
    REQUIRE(arrType.getElementType() != nullptr);
    REQUIRE(static_cast<const PrimitiveType*>(arrType.getElementType())->getKind() == PrimitiveTypeKind::Int);

    // map<string, int>
    auto keyT = std::make_unique<PrimitiveType>(span, PrimitiveTypeKind::String);
    auto valT = std::make_unique<PrimitiveType>(span, PrimitiveTypeKind::Int);
    MapType mapType(span, std::move(keyT), std::move(valT));
    REQUIRE(mapType.getKeyType() != nullptr);
    REQUIRE(mapType.getValueType() != nullptr);
}

TEST_CASE("UnionType with multiple variants", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 15, 1, 16}};

    // int | string | null
    std::vector<std::unique_ptr<TypeAnnotation>> variants;
    variants.push_back(std::make_unique<PrimitiveType>(span, PrimitiveTypeKind::Int));
    variants.push_back(std::make_unique<PrimitiveType>(span, PrimitiveTypeKind::String));
    variants.push_back(std::make_unique<PrimitiveType>(span, PrimitiveTypeKind::Null));

    UnionType unionT(span, std::move(variants));
    REQUIRE(unionT.getVariants().size() == 3);
}

TEST_CASE("FunctionType and CustomType with generics", "[ast]") {
    SourceSpan span{SourceLocation{1, 0, 1, 1}, SourceLocation{1, 20, 1, 21}};

    // async (x: int) => void
    std::vector<FuncTypeParam> params;
    params.push_back(FuncTypeParam{"x", std::make_unique<PrimitiveType>(span, PrimitiveTypeKind::Int), false});
    FunctionType fnT(span, std::move(params), std::make_unique<PrimitiveType>(span, PrimitiveTypeKind::Void), true);
    REQUIRE(fnT.isAsync());
    REQUIRE(fnT.getParams().size() == 1);
    REQUIRE(fnT.getParams()[0].name == "x");
    REQUIRE(fnT.getReturnType() != nullptr);

    // Custom generic: Node<T>
    std::vector<std::unique_ptr<TypeAnnotation>> typeArgs;
    typeArgs.push_back(std::make_unique<PrimitiveType>(span, PrimitiveTypeKind::Int));
    CustomType customT(span, "Node", std::move(typeArgs), "std");
    REQUIRE(customT.getName() == "Node");
    REQUIRE(customT.isGeneric());
    REQUIRE(customT.getTypeArgs().size() == 1);
    REQUIRE(customT.isQualified());
    REQUIRE(customT.getModuleQualifier() == "std");
}
