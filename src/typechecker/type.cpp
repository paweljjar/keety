#include <keety/typechecker/type.hpp>

#include <algorithm>
#include <sstream>

namespace keety::typechecker {

TypePtr Type::intType() {
    static const auto t = std::make_shared<const Type>(TypeKind::Int);
    return t;
}

TypePtr Type::floatType() {
    static const auto t = std::make_shared<const Type>(TypeKind::Float);
    return t;
}

TypePtr Type::boolType() {
    static const auto t = std::make_shared<const Type>(TypeKind::Bool);
    return t;
}

TypePtr Type::charType() {
    static const auto t = std::make_shared<const Type>(TypeKind::Char);
    return t;
}

TypePtr Type::stringType() {
    static const auto t = std::make_shared<const Type>(TypeKind::String);
    return t;
}

TypePtr Type::voidType() {
    static const auto t = std::make_shared<const Type>(TypeKind::Void);
    return t;
}

TypePtr Type::nullType() {
    static const auto t = std::make_shared<const Type>(TypeKind::Null);
    return t;
}

TypePtr Type::errorType() {
    static const auto t = std::make_shared<const Type>(TypeKind::Error);
    return t;
}

TypePtr Type::unknownType() {
    static const auto t = std::make_shared<const Type>(TypeKind::Unknown);
    return t;
}

TypePtr Type::array(TypePtr elementType) {
    return std::make_shared<const ArrayType>(std::move(elementType));
}

TypePtr Type::map(TypePtr keyType, TypePtr valueType) {
    return std::make_shared<const MapType>(std::move(keyType), std::move(valueType));
}

TypePtr Type::custom(std::string name) {
    if (name == "int") return intType();
    if (name == "float") return floatType();
    if (name == "bool") return boolType();
    if (name == "char") return charType();
    if (name == "string") return stringType();
    if (name == "void") return voidType();
    if (name == "null") return nullType();
    if (name == "error") return errorType();
    return std::make_shared<const CustomType>(std::move(name));
}

TypePtr Type::unionOf(std::vector<TypePtr> variants) {
    return std::make_shared<const UnionType>(std::move(variants));
}

TypePtr Type::function(std::vector<TypePtr> params, TypePtr returnType, bool isAsync) {
    return std::make_shared<const FunctionType>(std::move(params), std::move(returnType), isAsync);
}

std::string Type::toString() const {
    switch (kind_) {
    case TypeKind::Int: return "int";
    case TypeKind::Float: return "float";
    case TypeKind::Bool: return "bool";
    case TypeKind::Char: return "char";
    case TypeKind::String: return "string";
    case TypeKind::Void: return "void";
    case TypeKind::Null: return "null";
    case TypeKind::Error: return "error";
    case TypeKind::Array: return "array";
    case TypeKind::Map: return "map";
    case TypeKind::Custom: return "custom";
    case TypeKind::Union: return "union";
    case TypeKind::Function: return "function";
    case TypeKind::Unknown: return "<unknown>";
    }
    return "<unknown>";
}

bool Type::equals(const Type& other) const {
    if (this == &other) return true;
    return kind_ == other.kind_;
}

TypePtr Type::fromAnnotation(const ast::TypeAnnotation* annot) {
    if (!annot) {
        return unknownType();
    }

    if (const auto* prim = dynamic_cast<const ast::PrimitiveType*>(annot)) {
        switch (prim->getKind()) {
        case ast::PrimitiveTypeKind::Int: return intType();
        case ast::PrimitiveTypeKind::Float: return floatType();
        case ast::PrimitiveTypeKind::Bool: return boolType();
        case ast::PrimitiveTypeKind::Char: return charType();
        case ast::PrimitiveTypeKind::String: return stringType();
        case ast::PrimitiveTypeKind::Void: return voidType();
        case ast::PrimitiveTypeKind::Null: return nullType();
        case ast::PrimitiveTypeKind::Error: return errorType();
        }
    } else if (const auto* arr = dynamic_cast<const ast::ArrayType*>(annot)) {
        return array(fromAnnotation(arr->getElementType()));
    } else if (const auto* m = dynamic_cast<const ast::MapType*>(annot)) {
        return map(fromAnnotation(m->getKeyType()), fromAnnotation(m->getValueType()));
    } else if (const auto* customT = dynamic_cast<const ast::CustomType*>(annot)) {
        return custom(customT->getName());
    } else if (const auto* unionT = dynamic_cast<const ast::UnionType*>(annot)) {
        std::vector<TypePtr> variants;
        for (const auto& v : unionT->getVariants()) {
            variants.push_back(fromAnnotation(v.get()));
        }
        return unionOf(std::move(variants));
    } else if (const auto* fnT = dynamic_cast<const ast::FunctionType*>(annot)) {
        std::vector<TypePtr> params;
        for (const auto& p : fnT->getParams()) {
            params.push_back(fromAnnotation(p.type.get()));
        }
        return function(std::move(params), fromAnnotation(fnT->getReturnType()), fnT->isAsync());
    }

    return unknownType();
}

std::string ArrayType::toString() const {
    return "array<" + (elementType_ ? elementType_->toString() : "<unknown>") + ">";
}

bool ArrayType::equals(const Type& other) const {
    if (this == &other) return true;
    if (other.getKind() != TypeKind::Array) return false;
    const auto& o = static_cast<const ArrayType&>(other);
    if (!elementType_ || !o.elementType_) return elementType_ == o.elementType_;
    return elementType_->equals(*o.elementType_);
}

std::string MapType::toString() const {
    std::string k = keyType_ ? keyType_->toString() : "<unknown>";
    std::string v = valueType_ ? valueType_->toString() : "<unknown>";
    return "map<" + k + ", " + v + ">";
}

bool MapType::equals(const Type& other) const {
    if (this == &other) return true;
    if (other.getKind() != TypeKind::Map) return false;
    const auto& o = static_cast<const MapType&>(other);
    bool kEq = (!keyType_ || !o.keyType_) ? (keyType_ == o.keyType_) : keyType_->equals(*o.keyType_);
    bool vEq = (!valueType_ || !o.valueType_) ? (valueType_ == o.valueType_) : valueType_->equals(*o.valueType_);
    return kEq && vEq;
}

bool CustomType::equals(const Type& other) const {
    if (this == &other) return true;
    if (other.getKind() != TypeKind::Custom) return false;
    const auto& o = static_cast<const CustomType&>(other);
    return name_ == o.name_;
}

std::string UnionType::toString() const {
    std::ostringstream ss;
    for (size_t i = 0; i < variants_.size(); ++i) {
        if (i > 0) ss << " | ";
        ss << (variants_[i] ? variants_[i]->toString() : "<unknown>");
    }
    return ss.str();
}

bool UnionType::equals(const Type& other) const {
    if (this == &other) return true;
    if (other.getKind() != TypeKind::Union) return false;
    const auto& o = static_cast<const UnionType&>(other);
    if (variants_.size() != o.variants_.size()) return false;
    for (size_t i = 0; i < variants_.size(); ++i) {
        if (!variants_[i]->equals(*o.variants_[i])) return false;
    }
    return true;
}

std::string FunctionType::toString() const {
    std::ostringstream ss;
    if (isAsync_) ss << "async ";
    ss << "(";
    for (size_t i = 0; i < paramTypes_.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << (paramTypes_[i] ? paramTypes_[i]->toString() : "<unknown>");
    }
    ss << ") => " << (returnType_ ? returnType_->toString() : "<unknown>");
    return ss.str();
}

bool FunctionType::equals(const Type& other) const {
    if (this == &other) return true;
    if (other.getKind() != TypeKind::Function) return false;
    const auto& o = static_cast<const FunctionType&>(other);
    if (isAsync_ != o.isAsync_) return false;
    if (paramTypes_.size() != o.paramTypes_.size()) return false;
    for (size_t i = 0; i < paramTypes_.size(); ++i) {
        if (!paramTypes_[i]->equals(*o.paramTypes_[i])) return false;
    }
    if (!returnType_ || !o.returnType_) return returnType_ == o.returnType_;
    return returnType_->equals(*o.returnType_);
}

} // namespace keety::typechecker
