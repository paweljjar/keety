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
    Literal,
    Unknown
};

/// Classification of literal type constants
enum class LiteralKind {
    Int,
    Float,
    Bool,
    Char,
    String
};

class Type;
using TypePtr = std::shared_ptr<const Type>;

/// Semantic type representation used during static type checking
class Type : public std::enable_shared_from_this<Type> {
public:
    explicit Type(TypeKind kind) : kind_(kind) {}
    virtual ~Type() = default;

    [[nodiscard]] TypeKind getKind() const noexcept { return kind_; }

    [[nodiscard]] bool isPrimitiveInt() const noexcept { return kind_ == TypeKind::Int; }
    [[nodiscard]] bool isPrimitiveFloat() const noexcept { return kind_ == TypeKind::Float; }
    [[nodiscard]] bool isPrimitiveBool() const noexcept { return kind_ == TypeKind::Bool; }
    [[nodiscard]] bool isPrimitiveChar() const noexcept { return kind_ == TypeKind::Char; }
    [[nodiscard]] bool isPrimitiveString() const noexcept { return kind_ == TypeKind::String; }
    [[nodiscard]] bool isLiteral() const noexcept { return kind_ == TypeKind::Literal; }

    [[nodiscard]] virtual bool isInt() const noexcept { return kind_ == TypeKind::Int; }
    [[nodiscard]] virtual bool isFloat() const noexcept { return kind_ == TypeKind::Float; }
    [[nodiscard]] virtual bool isNumeric() const noexcept { return isInt() || isFloat(); }
    [[nodiscard]] virtual bool isBool() const noexcept { return kind_ == TypeKind::Bool; }
    [[nodiscard]] virtual bool isChar() const noexcept { return kind_ == TypeKind::Char; }
    [[nodiscard]] virtual bool isString() const noexcept { return kind_ == TypeKind::String; }
    [[nodiscard]] bool isVoid() const noexcept { return kind_ == TypeKind::Void; }
    [[nodiscard]] bool isNull() const noexcept { return kind_ == TypeKind::Null; }
    [[nodiscard]] bool isError() const noexcept { return kind_ == TypeKind::Error; }
    [[nodiscard]] bool isArray() const noexcept { return kind_ == TypeKind::Array; }
    [[nodiscard]] bool isMap() const noexcept { return kind_ == TypeKind::Map; }
    [[nodiscard]] bool isCustom() const noexcept { return kind_ == TypeKind::Custom; }
    [[nodiscard]] bool isUnion() const noexcept { return kind_ == TypeKind::Union; }
    [[nodiscard]] bool isFunction() const noexcept { return kind_ == TypeKind::Function; }
    [[nodiscard]] bool isUnknown() const noexcept { return kind_ == TypeKind::Unknown; }

    [[nodiscard]] virtual TypePtr getBaseType() const;
    [[nodiscard]] virtual std::string toString() const;
    [[nodiscard]] virtual bool equals(const Type& other) const;
    [[nodiscard]] virtual bool isAssignableTo(const Type& target) const;

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

    static TypePtr literalInt(int64_t value);
    static TypePtr literalFloat(double value);
    static TypePtr literalBool(bool value);
    static TypePtr literalChar(char value);
    static TypePtr literalString(std::string value);

    static TypePtr array(TypePtr elementType);
    static TypePtr map(TypePtr keyType, TypePtr valueType);
    static TypePtr custom(std::string name);
    static TypePtr unionOf(std::vector<TypePtr> variants);
    static TypePtr function(std::vector<TypePtr> params, TypePtr returnType, bool isAsync = false);

    /// Normalizes a collection of union variants according to canonical union algebra:
    /// 1. Flattening: A | (B | C) => A | B | C
    /// 2. Subtype & Literal absorption: 200 | 404 | int => int, true | false => bool
    /// 3. Deduplication & Commutativity: A | B | A => A | B
    /// 4. Single-variant unwrapping: returns T directly if only one variant remains
    static TypePtr normalizeUnion(const std::vector<TypePtr>& variants);

    /// Converts an AST TypeAnnotation into a semantic TypePtr
    static TypePtr fromAnnotation(const ast::TypeAnnotation* annot);

private:
    TypeKind kind_;
};

/// Literal type representation: `200`, `404`, `'y'`, `"read"`, `true`
class LiteralType : public Type {
public:
    explicit LiteralType(int64_t value)
        : Type(TypeKind::Literal), litKind_(LiteralKind::Int), intVal_(value) {}
    explicit LiteralType(double value)
        : Type(TypeKind::Literal), litKind_(LiteralKind::Float), floatVal_(value) {}
    explicit LiteralType(bool value)
        : Type(TypeKind::Literal), litKind_(LiteralKind::Bool), boolVal_(value) {}
    explicit LiteralType(char value)
        : Type(TypeKind::Literal), litKind_(LiteralKind::Char), charVal_(value) {}
    explicit LiteralType(std::string value)
        : Type(TypeKind::Literal), litKind_(LiteralKind::String), stringVal_(std::move(value)) {}

    [[nodiscard]] LiteralKind getLiteralKind() const noexcept { return litKind_; }
    [[nodiscard]] int64_t getIntValue() const noexcept { return intVal_; }
    [[nodiscard]] double getFloatValue() const noexcept { return floatVal_; }
    [[nodiscard]] bool getBoolValue() const noexcept { return boolVal_; }
    [[nodiscard]] char getCharValue() const noexcept { return charVal_; }
    [[nodiscard]] const std::string& getStringValue() const noexcept { return stringVal_; }

    [[nodiscard]] bool isInt() const noexcept override { return litKind_ == LiteralKind::Int; }
    [[nodiscard]] bool isFloat() const noexcept override { return litKind_ == LiteralKind::Float; }
    [[nodiscard]] bool isBool() const noexcept override { return litKind_ == LiteralKind::Bool; }
    [[nodiscard]] bool isChar() const noexcept override { return litKind_ == LiteralKind::Char; }
    [[nodiscard]] bool isString() const noexcept override { return litKind_ == LiteralKind::String; }

    [[nodiscard]] TypePtr getBaseType() const override;
    [[nodiscard]] std::string toString() const override;
    [[nodiscard]] bool equals(const Type& other) const override;
    [[nodiscard]] bool isAssignableTo(const Type& target) const override;

private:
    LiteralKind litKind_;
    int64_t intVal_{0};
    double floatVal_{0.0};
    bool boolVal_{false};
    char charVal_{'\0'};
    std::string stringVal_;
};

/// Array type representation: `array<T>`
class ArrayType : public Type {
public:
    explicit ArrayType(TypePtr elementType)
        : Type(TypeKind::Array), elementType_(std::move(elementType)) {}

    [[nodiscard]] const TypePtr& getElementType() const noexcept { return elementType_; }
    [[nodiscard]] std::string toString() const override;
    [[nodiscard]] bool equals(const Type& other) const override;
    [[nodiscard]] bool isAssignableTo(const Type& target) const override;

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
    [[nodiscard]] bool isAssignableTo(const Type& target) const override;

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
    [[nodiscard]] bool isAssignableTo(const Type& target) const override;

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
    [[nodiscard]] bool isAssignableTo(const Type& target) const override;

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
    [[nodiscard]] bool isAssignableTo(const Type& target) const override;

private:
    std::vector<TypePtr> paramTypes_;
    TypePtr returnType_;
    bool isAsync_;
};

} // namespace keety::typechecker

