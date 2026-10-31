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

TypePtr Type::literalInt(int64_t value) {
    return std::make_shared<const LiteralType>(value);
}

TypePtr Type::literalFloat(double value) {
    return std::make_shared<const LiteralType>(value);
}

TypePtr Type::literalBool(bool value) {
    return std::make_shared<const LiteralType>(value);
}

TypePtr Type::literalChar(char value) {
    return std::make_shared<const LiteralType>(value);
}

TypePtr Type::literalString(std::string value) {
    return std::make_shared<const LiteralType>(std::move(value));
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
    return normalizeUnion(variants);
}

TypePtr Type::normalizeUnion(const std::vector<TypePtr>& variants) {
    // 1. Flattening: recursively unpack nested unions (Associativity)
    std::vector<TypePtr> flat;
    std::function<void(const TypePtr&)> flatten = [&](const TypePtr& t) {
        if (!t) return;
        if (t->isUnion()) {
            const auto& u = static_cast<const UnionType&>(*t);
            for (const auto& inner : u.getVariants()) {
                flatten(inner);
            }
        } else {
            flat.push_back(t);
        }
    };
    for (const auto& v : variants) {
        flatten(v);
    }

    // 2. Subtype & Literal absorption
    bool hasInt = false;
    bool hasFloat = false;
    bool hasBool = false;
    bool hasChar = false;
    bool hasString = false;
    bool hasTrueLit = false;
    bool hasFalseLit = false;

    for (const auto& t : flat) {
        if (t->isPrimitiveInt()) hasInt = true;
        else if (t->isPrimitiveFloat()) hasFloat = true;
        else if (t->isPrimitiveBool()) hasBool = true;
        else if (t->isPrimitiveChar()) hasChar = true;
        else if (t->isPrimitiveString()) hasString = true;
        else if (t->isLiteral()) {
            const auto& lit = static_cast<const LiteralType&>(*t);
            if (lit.getLiteralKind() == LiteralKind::Bool) {
                if (lit.getBoolValue()) hasTrueLit = true;
                else hasFalseLit = true;
            }
        }
    }

    // If both true and false literals are present, they combine into bool
    if (hasTrueLit && hasFalseLit) {
        hasBool = true;
    }

    std::vector<TypePtr> absorbed;
    for (const auto& t : flat) {
        if (t->isLiteral()) {
            const auto& lit = static_cast<const LiteralType&>(*t);
            if (lit.getLiteralKind() == LiteralKind::Int && hasInt) continue;
            if (lit.getLiteralKind() == LiteralKind::Float && hasFloat) continue;
            if (lit.getLiteralKind() == LiteralKind::Bool && hasBool) continue;
            if (lit.getLiteralKind() == LiteralKind::Char && hasChar) continue;
            if (lit.getLiteralKind() == LiteralKind::String && hasString) continue;
        }
        absorbed.push_back(t);
    }

    if (hasBool && (hasTrueLit || hasFalseLit)) {
        bool alreadyHasBool = false;
        for (const auto& t : absorbed) {
            if (t->isPrimitiveBool()) {
                alreadyHasBool = true;
                break;
            }
        }
        if (!alreadyHasBool) {
            absorbed.push_back(Type::boolType());
        }
    }

    // 3. Deduplication: remove duplicate variants
    std::vector<TypePtr> deduplicated;
    for (const auto& t : absorbed) {
        bool isDup = false;
        for (const auto& existing : deduplicated) {
            if (existing->equals(*t)) {
                isDup = true;
                break;
            }
        }
        if (!isDup) {
            deduplicated.push_back(t);
        }
    }

    // 4. Commutativity & Canonical Ordering
    auto typeCat = [](const Type& t) -> int {
        if (t.isPrimitiveInt()) return 1;
        if (t.isPrimitiveFloat()) return 2;
        if (t.isPrimitiveBool()) return 3;
        if (t.isPrimitiveChar()) return 4;
        if (t.isPrimitiveString()) return 5;
        if (t.isVoid()) return 6;
        if (t.isNull()) return 7;
        if (t.isError()) return 8;
        if (t.isLiteral()) return 9;
        if (t.isCustom()) return 10;
        if (t.isArray()) return 11;
        if (t.isMap()) return 12;
        if (t.isFunction()) return 13;
        return 14;
    };

    std::sort(deduplicated.begin(), deduplicated.end(), [&](const TypePtr& a, const TypePtr& b) {
        int catA = typeCat(*a);
        int catB = typeCat(*b);
        if (catA != catB) return catA < catB;
        if (a->isLiteral() && b->isLiteral()) {
            const auto& la = static_cast<const LiteralType&>(*a);
            const auto& lb = static_cast<const LiteralType&>(*b);
            if (la.getLiteralKind() != lb.getLiteralKind()) {
                return static_cast<int>(la.getLiteralKind()) < static_cast<int>(lb.getLiteralKind());
            }
            switch (la.getLiteralKind()) {
            case LiteralKind::Int: return la.getIntValue() < lb.getIntValue();
            case LiteralKind::Float: return la.getFloatValue() < lb.getFloatValue();
            case LiteralKind::Bool: return !la.getBoolValue() && lb.getBoolValue();
            case LiteralKind::Char: return la.getCharValue() < lb.getCharValue();
            case LiteralKind::String: return la.getStringValue() < lb.getStringValue();
            }
        }
        return a->toString() < b->toString();
    });

    // 5. Single-variant unwrapping
    if (deduplicated.empty()) {
        return unknownType();
    }
    if (deduplicated.size() == 1) {
        return deduplicated[0];
    }

    return std::make_shared<const UnionType>(std::move(deduplicated));
}

TypePtr Type::function(std::vector<TypePtr> params, TypePtr returnType, bool isAsync) {
    return std::make_shared<const FunctionType>(std::move(params), std::move(returnType), isAsync);
}

TypePtr Type::getBaseType() const {
    return shared_from_this();
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
    case TypeKind::Literal: return "<literal>";
    case TypeKind::Unknown: return "<unknown>";
    }
    return "<unknown>";
}

bool Type::equals(const Type& other) const {
    if (this == &other) return true;
    return kind_ == other.kind_;
}

bool Type::isAssignableTo(const Type& target) const {
    if (this == &target || equals(target)) {
        return true;
    }
    if (isUnknown() || target.isUnknown()) {
        return true;
    }
    // Literal assignment to base type (e.g. 200 <: int)
    if (isLiteral()) {
        if (getBaseType()->equals(target)) {
            return true;
        }
    }
    // Target is a union: source must be assignable to at least one variant
    if (target.isUnion()) {
        const auto& tgtU = static_cast<const UnionType&>(target);
        if (isUnion()) {
            // Union to union: every source variant must be assignable to target union
            const auto& srcU = static_cast<const UnionType&>(*this);
            for (const auto& sv : srcU.getVariants()) {
                if (!sv || !sv->isAssignableTo(target)) {
                    return false;
                }
            }
            return true;
        } else {
            for (const auto& tv : tgtU.getVariants()) {
                if (tv && isAssignableTo(*tv)) {
                    return true;
                }
            }
            return false;
        }
    }
    // Source is a union, target is not: every source variant must be assignable to target
    if (isUnion()) {
        const auto& srcU = static_cast<const UnionType&>(*this);
        for (const auto& sv : srcU.getVariants()) {
            if (!sv || !sv->isAssignableTo(target)) {
                return false;
            }
        }
        return true;
    }
    // Array compatibility
    if (isArray() && target.isArray()) {
        const auto& a1 = static_cast<const ArrayType&>(*this);
        const auto& a2 = static_cast<const ArrayType&>(target);
        if (a1.getElementType() && a2.getElementType()) {
            return a1.getElementType()->isAssignableTo(*a2.getElementType());
        }
    }
    // Map compatibility
    if (isMap() && target.isMap()) {
        const auto& m1 = static_cast<const MapType&>(*this);
        const auto& m2 = static_cast<const MapType&>(target);
        if (m1.getKeyType() && m2.getKeyType() && m1.getValueType() && m2.getValueType()) {
            return m1.getKeyType()->isAssignableTo(*m2.getKeyType()) &&
                   m1.getValueType()->isAssignableTo(*m2.getValueType());
        }
    }
    return false;
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
    } else if (const auto* lit = dynamic_cast<const ast::LiteralType*>(annot)) {
        switch (lit->getLiteralKind()) {
        case ast::LiteralTypeKind::Int:
            return literalInt(lit->getIntValue());
        case ast::LiteralTypeKind::Float:
            return literalFloat(lit->getFloatValue());
        case ast::LiteralTypeKind::Bool:
            return literalBool(lit->getBoolValue());
        case ast::LiteralTypeKind::Char:
            return literalChar(lit->getCharValue());
        case ast::LiteralTypeKind::String:
            return literalString(lit->getStringValue());
        }
    } else if (const auto* arr = dynamic_cast<const ast::ArrayType*>(annot)) {
        return array(fromAnnotation(arr->getElementType()));
    } else if (const auto* m = dynamic_cast<const ast::MapType*>(annot)) {
        return map(fromAnnotation(m->getKeyType()), fromAnnotation(m->getValueType()));
    } else if (const auto* customT = dynamic_cast<const ast::CustomType*>(annot)) {
        if (customT->isQualified()) {
            return custom(customT->getModuleQualifier() + "." + customT->getName());
        }
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

TypePtr LiteralType::getBaseType() const {
    switch (litKind_) {
    case LiteralKind::Int: return Type::intType();
    case LiteralKind::Float: return Type::floatType();
    case LiteralKind::Bool: return Type::boolType();
    case LiteralKind::Char: return Type::charType();
    case LiteralKind::String: return Type::stringType();
    }
    return Type::unknownType();
}

std::string LiteralType::toString() const {
    switch (litKind_) {
    case LiteralKind::Int:
        return std::to_string(intVal_);
    case LiteralKind::Float: {
        std::ostringstream ss;
        ss << floatVal_;
        std::string s = ss.str();
        if (s.find('.') == std::string::npos && s.find('e') == std::string::npos) {
            s += ".0";
        }
        return s;
    }
    case LiteralKind::Bool:
        return boolVal_ ? "true" : "false";
    case LiteralKind::Char:
        return std::string("'") + charVal_ + "'";
    case LiteralKind::String:
        return "\"" + stringVal_ + "\"";
    }
    return "<literal>";
}

bool LiteralType::equals(const Type& other) const {
    if (this == &other) return true;
    if (other.getKind() != TypeKind::Literal) return false;
    const auto& o = static_cast<const LiteralType&>(other);
    if (litKind_ != o.litKind_) return false;
    switch (litKind_) {
    case LiteralKind::Int: return intVal_ == o.intVal_;
    case LiteralKind::Float: return floatVal_ == o.floatVal_;
    case LiteralKind::Bool: return boolVal_ == o.boolVal_;
    case LiteralKind::Char: return charVal_ == o.charVal_;
    case LiteralKind::String: return stringVal_ == o.stringVal_;
    }
    return false;
}

bool LiteralType::isAssignableTo(const Type& target) const {
    if (this == &target || equals(target)) return true;
    if (getBaseType()->equals(target)) return true;
    return Type::isAssignableTo(target);
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

bool ArrayType::isAssignableTo(const Type& target) const {
    return Type::isAssignableTo(target);
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

bool MapType::isAssignableTo(const Type& target) const {
    return Type::isAssignableTo(target);
}

bool CustomType::equals(const Type& other) const {
    if (this == &other) return true;
    if (other.getKind() != TypeKind::Custom) return false;
    const auto& o = static_cast<const CustomType&>(other);
    if (name_ == o.name_) return true;
    if ((name_ == "Color" && o.name_ == "io.Color") ||
        (name_ == "io.Color" && o.name_ == "Color")) {
        return true;
    }
    return false;
}

bool CustomType::isAssignableTo(const Type& target) const {
    return Type::isAssignableTo(target);
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

bool UnionType::isAssignableTo(const Type& target) const {
    return Type::isAssignableTo(target);
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

bool FunctionType::isAssignableTo(const Type& target) const {
    if (equals(target)) return true;
    if (target.isUnion()) return Type::isAssignableTo(target);
    return false;
}

} // namespace keety::typechecker

