#include <keety/ir/module.hpp>

#include <iostream>
#include <sstream>

namespace keety::ir {

std::string GlobalVariable::to_string() const {
    std::ostringstream ss;
    if (is_export) {
        ss << "export ";
    }
    ss << (is_const ? "const " : "let ") << "@" << name << ": "
       << (type ? type->toString() : "void");
    if (initial_value.has_value()) {
        ss << " = " << initial_value->to_string();
    }
    return ss.str();
}

const GlobalVariable* Module::find_global(const std::string& name) const noexcept {
    for (const auto& global : globals_) {
        if (global.name == name) {
            return &global;
        }
    }
    return nullptr;
}

Function* Module::find_function(const std::string& name) const noexcept {
    for (const auto& fn : functions_) {
        if (fn && fn->get_name() == name) {
            return fn.get();
        }
    }
    return nullptr;
}

void Module::recompute_cfg() {
    for (auto& fn : functions_) {
        if (fn) {
            fn->recompute_cfg_edges();
        }
    }
}

bool Module::validate(std::string& error_message) const {
    for (const auto& fn : functions_) {
        if (fn && !fn->validate_cfg(error_message)) {
            return false;
        }
    }
    return true;
}

std::string Module::to_string() const {
    std::ostringstream ss;
    ss << "module \"" << name_ << "\" {\n\n";

    if (!globals_.empty()) {
        ss << "; Global Variables\n";
        for (const auto& g : globals_) {
            ss << g.to_string() << ";\n";
        }
        ss << "\n";
    }

    for (const auto& fn : functions_) {
        if (fn) {
            ss << fn->to_string() << "\n";
        }
    }

    ss << "}\n";
    return ss.str();
}

void Module::dump(std::ostream& os) const {
    os << to_string();
}

} // namespace keety::ir
