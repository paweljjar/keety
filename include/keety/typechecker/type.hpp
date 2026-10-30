#pragma once

#include <keety/ast/type.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace keety::typechecker {

/// Semantic classification of types in KeetScript
enum class TypeKind {
    Int,
    Float,
    Bool,
    Char,
    String,
    Void,
    Null,
    Error,
    Array,
    Map,
    Custom,
    Union,
    Function,
    Unknown
};

class Type;
using TypePtr = std::shared_ptr<const Type>;

/// Semantic type representation used during static type checking
class Type {
public:
    explicit Type(TypeKind kind) : kind_(kind) {}
    virtual ~Type() = default;

    [[nodiscard]] TypeKind getKind() const noexcept { return kind_; }

    [[nodiscard]] bool isInt() const noexcept { return kind_ == TypeKind::Int; }
    [[nodiscard]] bool isFloat() const noexcept { return kind_ == TypeKind::Float; }
    [[nodiscard]] bool isNumeric() const noexcept { return isInt() || isFloat(); }
    [[nodiscard]] bool isBool() const noexcept { return kind_ == TypeKind::Bool; }
    [[nodiscard]] bool isChar() const noexcept { return kind_ == TypeKind::Char; }
    [[nodiscard]] bool isString() const noexcept { return kind_ == TypeKind::String; }
    [[nodiscard]] bool isVoid() const noexcept { return kind_ == TypeKind::Void; }
    [[nodiscard]] bool isNull() const noexcept { return kind_ == TypeKind::Null; }
    [[nodiscard]] bool isError() const noexcept { return kind_ == TypeKind::Error; }
    [[nodiscard]] bool isArray() const noexcept { return kind_ == TypeKind::Array; }
    [[nodiscard]] bool isMap() const noexcept { return kind_ == TypeKind::Map; }
    [[nodiscard]] bool isCustom() const noexcept { return kind_ == TypeKind::Custom; }
    [[nodiscard]] bool isUnion() const noexcept { return kind_ == TypeKind::Union; }
    [[nodiscard]] bool isFunction() const noexcept { return kind_ == TypeKind::Function; }
    [[nodiscard]] bool isUnknown() const noexcept { return kind_ == TypeKind::Unknown; }

    [[nodiscard]] virtual std::string toString() const;
    [[nodiscard]] virtual bool equals(const Type& other) const;

    // --- Singleton / Factory Constructors ---
    static TypePtr intType();
    static TypePtr floatType();
    static TypePtr boolType();
    static TypePtr charType();
    static TypePtr stringType();
    static TypePtr voidType();
    static TypePtr nullType();
    static TypePtr errorType();
    static TypePtr unknownType();

    static TypePtr array(TypePtr elementType);
    static TypePtr map(TypePtr keyType, TypePtr valueType);
    static TypePtr custom(std::string name);
    static TypePtr unionOf(std::vector<TypePtr> variants);
    static TypePtr function(std::vector<TypePtr> params, TypePtr returnType, bool isAsync = false);

    /// Converts an AST TypeAnnotation into a semantic TypePtr
    static TypePtr fromAnnotation(const ast::TypeAnnotation* annot);

private:
    TypeKind kind_;
};

/// Array type representation: `array<T>`
class ArrayType : public Type {
public:
    explicit ArrayType(TypePtr elementType)
        : Type(TypeKind::Array), elementType_(std::move(elementType)) {}

    [[nodiscard]] const TypePtr& getElementType() const noexcept { return elementType_; }
    [[nodiscard]] std::string toString() const override;
    [[nodiscard]] bool equals(const Type& other) const override;

private:
    TypePtr elementType_;
};

/// Map type representation: `map<Key, Value>`
class MapType : public Type {
public:
    MapType(TypePtr keyType, TypePtr valueType)
        : Type(TypeKind::Map), keyType_(std::move(keyType)), valueType_(std::move(valueType)) {}

    [[nodiscard]] const TypePtr& getKeyType() const noexcept { return keyType_; }
    [[nodiscard]] const TypePtr& getValueType() const noexcept { return valueType_; }
    [[nodiscard]] std::string toString() const override;
    [[nodiscard]] bool equals(const Type& other) const override;

private:
    TypePtr keyType_;
    TypePtr valueType_;
};

/// Nominal custom or user-defined type: `User`, `Status`
class CustomType : public Type {
public:
    explicit CustomType(std::string name)
        : Type(TypeKind::Custom), name_(std::move(name)) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
    [[nodiscard]] std::string toString() const override { return name_; }
    [[nodiscard]] bool equals(const Type& other) const override;

private:
    std::string name_;
};

/// Union type representation: `T1 | T2 | T3`
class UnionType : public Type {
public:
    explicit UnionType(std::vector<TypePtr> variants)
        : Type(TypeKind::Union), variants_(std::move(variants)) {}

    [[nodiscard]] const std::vector<TypePtr>& getVariants() const noexcept { return variants_; }
    [[nodiscard]] std::string toString() const override;
    [[nodiscard]] bool equals(const Type& other) const override;

private:
    std::vector<TypePtr> variants_;
};

/// Function type representation: `(param1: T1, param2: T2) => Ret`
class FunctionType : public Type {
public:
    FunctionType(std::vector<TypePtr> paramTypes, TypePtr returnType, bool isAsync = false)
        : Type(TypeKind::Function), paramTypes_(std::move(paramTypes)),
          returnType_(std::move(returnType)), isAsync_(isAsync) {}

    [[nodiscard]] const std::vector<TypePtr>& getParamTypes() const noexcept { return paramTypes_; }
    [[nodiscard]] const TypePtr& getReturnType() const noexcept { return returnType_; }
    [[nodiscard]] bool isAsync() const noexcept { return isAsync_; }
    [[nodiscard]] std::string toString() const override;
    [[nodiscard]] bool equals(const Type& other) const override;

private:
    std::vector<TypePtr> paramTypes_;
    TypePtr returnType_;
    bool isAsync_;
};

} // namespace keety::typechecker
