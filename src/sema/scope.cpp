#include <keety/sema/scope.hpp>

namespace keety::sema {

Scope::Scope(ScopeKind kind, std::shared_ptr<Scope> parent)
    : kind_(kind), parent_(parent), depth_(parent ? parent->getDepth() + 1 : 0) {}

std::shared_ptr<Scope> Scope::createUniverse() {
    return std::make_shared<Scope>(ScopeKind::Universe, nullptr);
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

DefineResult Scope::define(std::shared_ptr<Symbol> symbol) {
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
            // Per Language Spec Section 741: Inside class methods, instance fields require
            // `this.` access. Method parameters and locals may share names with fields
            // without violating the no-shadowing rule.
            if (auto sym = curr->lookupCurrent(name)) {
                if (!sym->isField()) {
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

} // namespace keety::sema
