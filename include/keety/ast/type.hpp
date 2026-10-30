#pragma once

#include <keety/ast/base.hpp>
#include <keety/lexer/token.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace keety::ast {

/// Primitive scalar or special type kind
enum class PrimitiveTypeKind {
    Bool,
    Int,
    Float,
    Char,
    String,
    Void,
    Error,
    Null
};

/// Returns string representation of PrimitiveTypeKind
[[nodiscard]] constexpr std::string_view getPrimitiveTypeName(PrimitiveTypeKind kind) noexcept {
    switch (kind) {
    case PrimitiveTypeKind::Bool:   return "bool";
    case PrimitiveTypeKind::Int:    return "int";
    case PrimitiveTypeKind::Float:  return "float";
    case PrimitiveTypeKind::Char:   return "char";
    case PrimitiveTypeKind::String: return "string";
    case PrimitiveTypeKind::Void:   return "void";
    case PrimitiveTypeKind::Error:  return "error";
    case PrimitiveTypeKind::Null:   return "null";
    }
    return "unknown";
}

/// Primitive type node: `bool`, `int`, `float`, `char`, `string`, `void`, `error`, `null`
class PrimitiveType : public TypeAnnotation {
public:
    PrimitiveType(diag::SourceSpan span, PrimitiveTypeKind kind)
        : TypeAnnotation(span), kind_(kind) {}

    [[nodiscard]] PrimitiveTypeKind getKind() const noexcept { return kind_; }
    [[nodiscard]] std::string_view getTypeName() const noexcept { return getPrimitiveTypeName(kind_); }

private:
    PrimitiveTypeKind kind_;
};

/// Array type node: `array<T>`
class ArrayType : public TypeAnnotation {
public:
    ArrayType(diag::SourceSpan span, std::unique_ptr<TypeAnnotation> elementType)
        : TypeAnnotation(span), elementType_(std::move(elementType)) {}

    [[nodiscard]] const TypeAnnotation* getElementType() const noexcept { return elementType_.get(); }

private:
    std::unique_ptr<TypeAnnotation> elementType_;
};

/// Map type node: `map<KeyType, ValueType>`
class MapType : public TypeAnnotation {
public:
    MapType(diag::SourceSpan span, std::unique_ptr<TypeAnnotation> keyType,
            std::unique_ptr<TypeAnnotation> valueType)
        : TypeAnnotation(span), keyType_(std::move(keyType)), valueType_(std::move(valueType)) {}

    [[nodiscard]] const TypeAnnotation* getKeyType() const noexcept { return keyType_.get(); }
    [[nodiscard]] const TypeAnnotation* getValueType() const noexcept { return valueType_.get(); }

private:
    std::unique_ptr<TypeAnnotation> keyType_;
    std::unique_ptr<TypeAnnotation> valueType_;
};

/// Union type node: `int | string | null`
class UnionType : public TypeAnnotation {
public:
    UnionType(diag::SourceSpan span, std::vector<std::unique_ptr<TypeAnnotation>> variants)
        : TypeAnnotation(span), variants_(std::move(variants)) {}

    [[nodiscard]] const std::vector<std::unique_ptr<TypeAnnotation>>& getVariants() const noexcept {
        return variants_;
    }

private:
    std::vector<std::unique_ptr<TypeAnnotation>> variants_;
};

/// Function type parameter
struct FuncTypeParam {
    std::string name; // Optional parameter name: `(x: int) => int`
    std::unique_ptr<TypeAnnotation> type;
    bool isMut{false};
};

/// Function type node: `(x: int, y: int) => int` or `async () => void`
class FunctionType : public TypeAnnotation {
public:
    FunctionType(diag::SourceSpan span, std::vector<FuncTypeParam> params,
                 std::unique_ptr<TypeAnnotation> returnType, bool isAsync = false)
        : TypeAnnotation(span), params_(std::move(params)),
          returnType_(std::move(returnType)), isAsync_(isAsync) {}

    [[nodiscard]] const std::vector<FuncTypeParam>& getParams() const noexcept { return params_; }
    [[nodiscard]] const TypeAnnotation* getReturnType() const noexcept { return returnType_.get(); }
    [[nodiscard]] bool isAsync() const noexcept { return isAsync_; }

private:
    std::vector<FuncTypeParam> params_;
    std::unique_ptr<TypeAnnotation> returnType_;
    bool isAsync_{false};
};

/// Custom nominal type or generic instantiation: `Player`, `MyType<int>`, `module.Type`
class CustomType : public TypeAnnotation {
public:
    CustomType(diag::SourceSpan span, std::string name,
               std::vector<std::unique_ptr<TypeAnnotation>> typeArgs = {},
               std::string moduleQualifier = "")
        : TypeAnnotation(span), name_(std::move(name)),
          typeArgs_(std::move(typeArgs)), moduleQualifier_(std::move(moduleQualifier)) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
    [[nodiscard]] const std::vector<std::unique_ptr<TypeAnnotation>>& getTypeArgs() const noexcept { return typeArgs_; }
    [[nodiscard]] bool isGeneric() const noexcept { return !typeArgs_.empty(); }
    [[nodiscard]] const std::string& getModuleQualifier() const noexcept { return moduleQualifier_; }
    [[nodiscard]] bool isQualified() const noexcept { return !moduleQualifier_.empty(); }

private:
    std::string name_;
    std::vector<std::unique_ptr<TypeAnnotation>> typeArgs_;
    std::string moduleQualifier_;
};

} // namespace keety::ast
