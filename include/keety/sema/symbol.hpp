#pragma once

#include <keety/ast/base.hpp>
#include <keety/diag/source_location.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace keety::sema {

/// Identifies the semantic kind of a declared symbol.
enum class SymbolKind {
    Variable,          ///< Local or module-level variable (`let`)
    Constant,          ///< Immutable runtime constant (`const`)
    Define,            ///< Compile-time constant (`define`)
    Function,          ///< Function declaration (`fn`)
    Parameter,         ///< Function/method parameter
    TypeAlias,         ///< Type alias (`type`)
    Class,             ///< Class declaration (`class`)
    Enum,              ///< Enum declaration (`enum`)
    EnumValue,         ///< Enumeration member
    ErrType,           ///< Error type declaration (`errtype`)
    Module,            ///< Imported namespace module (`import ... as mod;`)
    ImportedSymbol,    ///< Named imported symbol (`import { x } from ...;`)
    Field,             ///< Class field member
    Method,            ///< Class method member
    Label,             ///< Loop or switch named label (`as labelName`)
    BuiltinType,       ///< Universe built-in type (`int`, `float`, `string`, etc.)
    BuiltinConstant,   ///< Universe built-in constant (`true`, `false`, `null`)
    BuiltinOperator,   ///< Universe built-in operator (`typeof`)
};

/// Returns a human-readable string representation of a symbol kind.
[[nodiscard]] constexpr std::string_view symbolKindToString(SymbolKind kind) noexcept {
    switch (kind) {
    case SymbolKind::Variable:
        return "variable";
    case SymbolKind::Constant:
        return "constant";
    case SymbolKind::Define:
        return "compile-time constant";
    case SymbolKind::Function:
        return "function";
    case SymbolKind::Parameter:
        return "parameter";
    case SymbolKind::TypeAlias:
        return "type alias";
    case SymbolKind::Class:
        return "class";
    case SymbolKind::Enum:
        return "enum";
    case SymbolKind::EnumValue:
        return "enum member";
    case SymbolKind::ErrType:
        return "error type";
    case SymbolKind::Module:
        return "module namespace";
    case SymbolKind::ImportedSymbol:
        return "imported symbol";
    case SymbolKind::Field:
        return "class field";
    case SymbolKind::Method:
        return "class method";
    case SymbolKind::Label:
        return "label";
    case SymbolKind::BuiltinType:
        return "built-in type";
    case SymbolKind::BuiltinConstant:
        return "built-in constant";
    case SymbolKind::BuiltinOperator:
        return "built-in operator";
    }
    return "unknown";
}

/// Represents a declared identifier in the KeetScript symbol table.
class Symbol {
public:
    Symbol(std::string name, SymbolKind kind, diag::SourceSpan span)
        : name_(std::move(name)), kind_(kind), span_(span) {}

    // --- Identification ---
    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
    [[nodiscard]] SymbolKind getKind() const noexcept { return kind_; }
    [[nodiscard]] diag::SourceSpan getSpan() const noexcept { return span_; }
    [[nodiscard]] std::string_view getKindName() const noexcept { return symbolKindToString(kind_); }

    // --- Predicates ---
    [[nodiscard]] bool isVariable() const noexcept { return kind_ == SymbolKind::Variable; }
    [[nodiscard]] bool isConstant() const noexcept {
        return kind_ == SymbolKind::Constant || kind_ == SymbolKind::Define ||
               kind_ == SymbolKind::EnumValue || kind_ == SymbolKind::BuiltinConstant || isConst_;
    }
    [[nodiscard]] bool isDefine() const noexcept { return kind_ == SymbolKind::Define; }
    [[nodiscard]] bool isFunction() const noexcept { return kind_ == SymbolKind::Function; }
    [[nodiscard]] bool isParameter() const noexcept { return kind_ == SymbolKind::Parameter; }
    [[nodiscard]] bool isType() const noexcept {
        return kind_ == SymbolKind::TypeAlias || kind_ == SymbolKind::Class ||
               kind_ == SymbolKind::Enum || kind_ == SymbolKind::ErrType ||
               kind_ == SymbolKind::BuiltinType || isType_;
    }
    [[nodiscard]] bool isClass() const noexcept { return kind_ == SymbolKind::Class; }
    [[nodiscard]] bool isEnum() const noexcept { return kind_ == SymbolKind::Enum; }
    [[nodiscard]] bool isEnumValue() const noexcept { return kind_ == SymbolKind::EnumValue; }
    [[nodiscard]] bool isErrType() const noexcept { return kind_ == SymbolKind::ErrType; }
    [[nodiscard]] bool isModule() const noexcept { return kind_ == SymbolKind::Module; }
    [[nodiscard]] bool isImportedSymbol() const noexcept { return kind_ == SymbolKind::ImportedSymbol; }
    [[nodiscard]] bool isField() const noexcept { return kind_ == SymbolKind::Field; }
    [[nodiscard]] bool isMethod() const noexcept { return kind_ == SymbolKind::Method; }
    [[nodiscard]] bool isLabel() const noexcept { return kind_ == SymbolKind::Label; }
    [[nodiscard]] bool isBuiltin() const noexcept {
        return kind_ == SymbolKind::BuiltinType || kind_ == SymbolKind::BuiltinConstant ||
               kind_ == SymbolKind::BuiltinOperator;
    }
    [[nodiscard]] bool isBuiltinType() const noexcept {
        return kind_ == SymbolKind::BuiltinType || (isType_ && isBuiltin());
    }
    [[nodiscard]] bool isBuiltinConstant() const noexcept {
        return kind_ == SymbolKind::BuiltinConstant;
    }
    [[nodiscard]] bool isBuiltinOperator() const noexcept {
        return kind_ == SymbolKind::BuiltinOperator;
    }

    /// Returns true if this symbol binds a runtime or compile-time value.
    [[nodiscard]] bool isValueBinding() const noexcept {
        return kind_ == SymbolKind::Variable || kind_ == SymbolKind::Constant ||
               kind_ == SymbolKind::Define || kind_ == SymbolKind::Parameter ||
               kind_ == SymbolKind::Field || kind_ == SymbolKind::EnumValue;
    }

    // --- Mutability and Export Modifiers ---
    [[nodiscard]] bool isExported() const noexcept { return isExported_; }
    void setExported(bool exported) noexcept { isExported_ = exported; }

    [[nodiscard]] bool isMutable() const noexcept { return isMutable_; }
    void setMutable(bool mut) noexcept { isMutable_ = mut; }

    [[nodiscard]] bool isConst() const noexcept { return isConst_; }
    void setConst(bool c) noexcept { isConst_ = c; }

    [[nodiscard]] bool isConstView() const noexcept { return isConstView_; }
    void setConstView(bool cv) noexcept { isConstView_ = cv; }

    /// Returns true if this symbol can be mutated in place.
    [[nodiscard]] bool canMutate() const noexcept {
        return isMutable_ && !isConst_ && !isConstView_;
    }

    /// Returns true if this symbol can be an assignment target (l-value).
    [[nodiscard]] bool isAssignable() const noexcept {
        return canMutate() &&
               (kind_ == SymbolKind::Variable || kind_ == SymbolKind::Field || kind_ == SymbolKind::Parameter);
    }

    // Linkage to original exported symbol (for imported const-views)
    [[nodiscard]] std::shared_ptr<Symbol> getOriginalExportedSymbol() const noexcept {
        return originalExportedSymbol_;
    }
    void setOriginalExportedSymbol(std::shared_ptr<Symbol> orig) noexcept {
        originalExportedSymbol_ = std::move(orig);
    }

    /// Factory method to create an imported const-view of an exported variable.
    [[nodiscard]] static std::shared_ptr<Symbol> createImportedConstView(
        const std::shared_ptr<Symbol>& original,
        diag::SourceSpan importSpan) {
        auto imported = std::make_shared<Symbol>(
            original->getName(), SymbolKind::ImportedSymbol, importSpan);
        imported->setConstView(true);
        imported->setMutable(false);
        imported->setConst(true);
        imported->setAstNode(original->getAstNode());
        imported->setTypeAnnotation(original->getTypeAnnotation());
        imported->setOriginalExportedSymbol(original);
        return imported;
    }

    void setIsType(bool t) noexcept { isType_ = t; }

    // --- AST Associations ---
    [[nodiscard]] const ast::AstNode* getAstNode() const noexcept { return astNode_; }
    void setAstNode(const ast::AstNode* node) noexcept { astNode_ = node; }

    [[nodiscard]] const ast::TypeAnnotation* getTypeAnnotation() const noexcept { return typeAnnotation_; }
    void setTypeAnnotation(const ast::TypeAnnotation* type) noexcept { typeAnnotation_ = type; }

    // --- Function Overloading ---
    [[nodiscard]] bool hasOverloads() const noexcept { return !overloads_.empty(); }
    [[nodiscard]] const std::vector<std::shared_ptr<Symbol>>& getOverloads() const noexcept { return overloads_; }
    void addOverload(std::shared_ptr<Symbol> overload) {
        overloads_.push_back(std::move(overload));
    }

private:
    std::string name_;
    SymbolKind kind_;
    diag::SourceSpan span_;
    const ast::AstNode* astNode_{nullptr};
    const ast::TypeAnnotation* typeAnnotation_{nullptr};
    bool isExported_{false};
    bool isMutable_{false};
    bool isConst_{false};
    bool isConstView_{false};
    bool isType_{false};
    std::vector<std::shared_ptr<Symbol>> overloads_;
    std::shared_ptr<Symbol> originalExportedSymbol_{nullptr};
};

} // namespace keety::sema
