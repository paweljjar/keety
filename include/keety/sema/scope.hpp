#pragma once

#include <keety/diag/error_code.hpp>
#include <keety/sema/symbol.hpp>
#include <keety/typechecker/type.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace keety::sema {

/// Identifies the hierarchical kind of a lexical scope.
enum class ScopeKind {
    Universe,  ///< Root global scope containing predeclared types, constants, and built-ins
    Module,    ///< File/module-level scope containing top-level declarations and imports
    Function,  ///< Function, method, or constructor parameter & body scope
    Block,     ///< Lexical block scope (`{ ... }`, loop body, branch, zonk recovery)
    Class      ///< Class definition body containing fields and methods
};

/// Returns a human-readable string representation of a scope kind.
[[nodiscard]] constexpr std::string_view scopeKindToString(ScopeKind kind) noexcept {
    switch (kind) {
    case ScopeKind::Universe:
        return "Universe";
    case ScopeKind::Module:
        return "Module";
    case ScopeKind::Function:
        return "Function";
    case ScopeKind::Block:
        return "Block";
    case ScopeKind::Class:
        return "Class";
    }
    return "Unknown";
}

/// Result returned when attempting to define a symbol in a scope.
struct DefineResult {
    bool success{false};
    std::optional<diag::ErrorCode> errorCode{std::nullopt};
    std::shared_ptr<Symbol> existingSymbol{nullptr};
    std::string errorMessage{};

    [[nodiscard]] static DefineResult ok() {
        return {true, std::nullopt, nullptr, ""};
    }

    [[nodiscard]] static DefineResult blankIgnored() {
        return {true, std::nullopt, nullptr, ""};
    }

    [[nodiscard]] static DefineResult duplicate(std::shared_ptr<Symbol> existing, std::string msg) {
        return {false, diag::ErrorCode::RedeclarationInSameScope, std::move(existing), std::move(msg)};
    }

    [[nodiscard]] static DefineResult shadowed(std::shared_ptr<Symbol> outer, std::string msg) {
        return {false, diag::ErrorCode::IdentifierShadowed, std::move(outer), std::move(msg)};
    }
};

/// Represents a hierarchical lexical scope in the KeetScript compilation unit.
class Scope : public std::enable_shared_from_this<Scope> {
public:
    explicit Scope(ScopeKind kind, std::shared_ptr<Scope> parent = nullptr);

    /// Creates the root Universe scope (depth 0, no parent).
    /// If populatePredeclared is true, registers all built-in types, constants, and operators.
    [[nodiscard]] static std::shared_ptr<Scope> createUniverse(bool populatePredeclared = true);

    /// Creates a child scope of the specified kind nested within this scope.
    [[nodiscard]] std::shared_ptr<Scope> createChild(ScopeKind kind);

    // --- Identification and Hierarchy ---
    [[nodiscard]] ScopeKind getKind() const noexcept { return kind_; }
    [[nodiscard]] std::string_view getKindName() const noexcept { return scopeKindToString(kind_); }
    [[nodiscard]] std::shared_ptr<Scope> getParent() const noexcept { return parent_.lock(); }
    [[nodiscard]] size_t getDepth() const noexcept { return depth_; }

    [[nodiscard]] bool isUniverse() const noexcept { return kind_ == ScopeKind::Universe; }
    [[nodiscard]] bool isModule() const noexcept { return kind_ == ScopeKind::Module; }
    [[nodiscard]] bool isFunction() const noexcept { return kind_ == ScopeKind::Function; }
    [[nodiscard]] bool isBlock() const noexcept { return kind_ == ScopeKind::Block; }
    [[nodiscard]] bool isClass() const noexcept { return kind_ == ScopeKind::Class; }

    /// Finds the nearest enclosing Module scope, or nullptr if none exists.
    [[nodiscard]] std::shared_ptr<Scope> getEnclosingModule() const;

    /// Finds the nearest enclosing Function scope, or nullptr if none exists.
    [[nodiscard]] std::shared_ptr<Scope> getEnclosingFunction() const;

    /// Finds the nearest enclosing Class scope, or nullptr if none exists.
    [[nodiscard]] std::shared_ptr<Scope> getEnclosingClass() const;

    // --- Symbol Registration ---

    /// Defines a symbol in this scope.
    ///
    /// - If the symbol name is the blank identifier `_`, it is discarded without binding
    ///   and returns success (can be declared repeatedly).
    /// - If the symbol name already exists in this scope:
    ///   - If both are functions/methods (overloads), the new symbol is appended to the
    ///     existing symbol's overload set.
    ///   - Otherwise, returns `RedeclarationInSameScope` (E2004).
    /// - If `enforceNoShadowing` is true and an outer declaration is shadowed:
    ///   - Returns `IdentifierShadowed` (E2001).
    DefineResult define(std::shared_ptr<Symbol> symbol, bool enforceNoShadowing = true);

    // --- Lookups ---

    /// Searches for a symbol by name in this scope, recursively traversing parent scopes
    /// up to the Universe scope. Returns nullptr if not found.
    [[nodiscard]] std::shared_ptr<Symbol> lookup(std::string_view name) const;

    /// Searches for a symbol by name strictly in this scope (non-recursive).
    [[nodiscard]] std::shared_ptr<Symbol> lookupCurrent(std::string_view name) const;

    /// Searches for a symbol by name in ancestor scopes only (excluding this scope).
    [[nodiscard]] std::shared_ptr<Symbol> lookupParent(std::string_view name) const;

    /// Searches for an outer declaration that would be shadowed by a local value binding.
    ///
    /// Per Language Spec Section 741, method parameters and local variables inside a method
    /// may share the same name as class fields (since class fields require `this.` access).
    /// Therefore, class fields in an enclosing Class scope are not treated as shadowing targets.
    [[nodiscard]] std::shared_ptr<Symbol> lookupParentForShadowing(std::string_view name) const;

    // --- Direct Access ---
    [[nodiscard]] const std::unordered_map<std::string, std::shared_ptr<Symbol>>& getSymbols() const noexcept {
        return symbols_;
    }

    [[nodiscard]] size_t size() const noexcept { return symbols_.size(); }
    [[nodiscard]] bool empty() const noexcept { return symbols_.empty(); }

    [[nodiscard]] const std::vector<std::shared_ptr<Scope>>& getChildren() const noexcept {
        return children_;
    }

    // --- Control-Flow Analysis & Type Narrowing ---

    /// Gets the narrowed static type for a variable name in this scope or ancestor scopes.
    /// Returns nullptr if the variable's type is not narrowed.
    [[nodiscard]] std::shared_ptr<const typechecker::Type> getNarrowedType(std::string_view name) const;

    /// Sets the narrowed static type for a variable name in this specific scope.
    void setNarrowedType(std::string name, std::shared_ptr<const typechecker::Type> type);

    /// Invalidates / clears any narrowed type for a variable name in this scope and ancestor scopes.
    void resetNarrowedType(std::string_view name);

    [[nodiscard]] const std::unordered_map<std::string, std::shared_ptr<const typechecker::Type>>& getNarrowedTypes() const noexcept {
        return narrowedTypes_;
    }

private:
    ScopeKind kind_;
    std::weak_ptr<Scope> parent_;
    size_t depth_{0};
    std::unordered_map<std::string, std::shared_ptr<Symbol>> symbols_;
    std::vector<std::shared_ptr<Scope>> children_;
    std::unordered_map<std::string, std::shared_ptr<const typechecker::Type>> narrowedTypes_;
};

} // namespace keety::sema

