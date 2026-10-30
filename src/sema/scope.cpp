#include <keety/sema/scope.hpp>
#include <keety/sema/universe.hpp>

namespace keety::sema {

Scope::Scope(ScopeKind kind, std::shared_ptr<Scope> parent)
    : kind_(kind), parent_(parent), depth_(parent ? parent->getDepth() + 1 : 0) {}

std::shared_ptr<Scope> Scope::createUniverse(bool populatePredeclared) {
    auto scope = std::make_shared<Scope>(ScopeKind::Universe, nullptr);
    if (populatePredeclared) {
        registerUniverseSymbols(*scope);
    }
    return scope;
}

std::shared_ptr<Scope> Scope::createChild(ScopeKind kind) {
    auto child = std::make_shared<Scope>(kind, shared_from_this());
    children_.push_back(child);
    return child;
}

std::shared_ptr<Scope> Scope::getEnclosingModule() const {
    auto curr = parent_.lock();
    while (curr) {
        if (curr->isModule()) {
            return curr;
        }
        curr = curr->parent_.lock();
    }
    return nullptr;
}

std::shared_ptr<Scope> Scope::getEnclosingFunction() const {
    auto curr = parent_.lock();
    while (curr) {
        if (curr->isFunction()) {
            return curr;
        }
        curr = curr->parent_.lock();
    }
    return nullptr;
}

std::shared_ptr<Scope> Scope::getEnclosingClass() const {
    auto curr = parent_.lock();
    while (curr) {
        if (curr->isClass()) {
            return curr;
        }
        curr = curr->parent_.lock();
    }
    return nullptr;
}

DefineResult Scope::define(std::shared_ptr<Symbol> symbol, bool enforceNoShadowing) {
    if (!symbol) {
        return DefineResult::duplicate(nullptr, "cannot define null symbol");
    }

    // Blank identifier `_` is an anonymous discard placeholder.
    // It does not introduce a binding into any scope and can be repeated freely.
    if (symbol->getName() == "_") {
        return DefineResult::blankIgnored();
    }

    // Check for duplicate declarations in the current scope
    auto it = symbols_.find(symbol->getName());
    if (it != symbols_.end()) {
        auto existing = it->second;
        // Function and method overloading: functions or methods with the same name
        // in the same scope are accumulated into an overload set.
        if ((existing->isFunction() && symbol->isFunction()) ||
            (existing->isMethod() && symbol->isMethod())) {
            existing->addOverload(symbol);
            return DefineResult::ok();
        }

        return DefineResult::duplicate(
            existing,
            "redeclaration of identifier '" + symbol->getName() + "' in the same scope");
    }

    // Strict No-Shadowing check across enclosing lexical scopes
    if (enforceNoShadowing) {
        // Class fields and methods do not shadow outer bindings
        if (!isClass() || (!symbol->isField() && !symbol->isMethod())) {
            if (auto outer = lookupParentForShadowing(symbol->getName())) {
                return DefineResult::shadowed(
                    outer,
                    "identifier '" + symbol->getName() + "' shadows previous declaration in outer scope");
            }
        }
    }

    symbols_.emplace(symbol->getName(), symbol);
    return DefineResult::ok();
}

std::shared_ptr<Symbol> Scope::lookupCurrent(std::string_view name) const {
    auto it = symbols_.find(std::string(name));
    if (it != symbols_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Symbol> Scope::lookup(std::string_view name) const {
    if (auto sym = lookupCurrent(name)) {
        return sym;
    }
    if (auto p = parent_.lock()) {
        return p->lookup(name);
    }
    return nullptr;
}

std::shared_ptr<Symbol> Scope::lookupParent(std::string_view name) const {
    if (auto p = parent_.lock()) {
        return p->lookup(name);
    }
    return nullptr;
}

std::shared_ptr<Symbol> Scope::lookupParentForShadowing(std::string_view name) const {
    auto curr = parent_.lock();
    while (curr) {
        if (curr->isClass()) {
            // Per Language Spec Section 741: Inside class methods, instance fields and methods
            // require `this.` access. Method parameters and locals may share names with fields/methods
            // without violating the no-shadowing rule.
            if (auto sym = curr->lookupCurrent(name)) {
                if (!sym->isField() && !sym->isMethod()) {
                    return sym;
                }
            }
        } else {
            if (auto sym = curr->lookupCurrent(name)) {
                return sym;
            }
        }
        curr = curr->parent_.lock();
    }
    return nullptr;
}

std::shared_ptr<const typechecker::Type> Scope::getNarrowedType(std::string_view name) const {
    auto it = narrowedTypes_.find(std::string(name));
    if (it != narrowedTypes_.end()) {
        return it->second;
    }
    if (auto p = parent_.lock()) {
        return p->getNarrowedType(name);
    }
    return nullptr;
}

void Scope::setNarrowedType(std::string name, std::shared_ptr<const typechecker::Type> type) {
    narrowedTypes_[std::move(name)] = std::move(type);
}

void Scope::resetNarrowedType(std::string_view name) {
    narrowedTypes_.erase(std::string(name));
    if (auto p = parent_.lock()) {
        p->resetNarrowedType(name);
    }
}

} // namespace keety::sema

