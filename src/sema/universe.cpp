#include <keety/sema/universe.hpp>

namespace keety::sema {

namespace {

const std::vector<std::string_view> UNIVERSE_TYPES = {
    "bool", "int", "float", "char", "string", "void", "null", "error", "array", "map"
};

const std::vector<std::string_view> UNIVERSE_CONSTANTS = {
    "true", "false", "null"
};

const std::vector<std::string_view> UNIVERSE_OPERATORS = {
    "typeof"
};

} // namespace

const std::vector<std::string_view>& getUniverseTypeNames() noexcept {
    return UNIVERSE_TYPES;
}

const std::vector<std::string_view>& getUniverseConstantNames() noexcept {
    return UNIVERSE_CONSTANTS;
}

const std::vector<std::string_view>& getAllUniverseSymbolNames() noexcept {
    static const std::vector<std::string_view> ALL = {
        "bool", "int", "float", "char", "string", "void", "null", "error", "array", "map",
        "true", "false", "typeof"
    };
    return ALL;
}

bool isUniverseType(std::string_view name) noexcept {
    for (auto t : UNIVERSE_TYPES) {
        if (t == name) {
            return true;
        }
    }
    return false;
}

bool isUniverseConstant(std::string_view name) noexcept {
    for (auto c : UNIVERSE_CONSTANTS) {
        if (c == name) {
            return true;
        }
    }
    return false;
}

bool isUniverseOperator(std::string_view name) noexcept {
    for (auto o : UNIVERSE_OPERATORS) {
        if (o == name) {
            return true;
        }
    }
    return false;
}

bool isUniverseSymbol(std::string_view name) noexcept {
    return isUniverseType(name) || isUniverseConstant(name) || isUniverseOperator(name);
}

void registerUniverseSymbols(Scope& universe) {
    diag::SourceSpan builtinSpan{};

    // 1. Predeclared Types: bool, int, float, char, string, void, error, array, map, never
    for (auto typeName : UNIVERSE_TYPES) {
        if (typeName == "null") {
            // "null" is registered below as a combined constant and type
            continue;
        }
        auto sym = std::make_shared<Symbol>(std::string(typeName), SymbolKind::BuiltinType, builtinSpan);
        universe.define(sym);
    }

    // 2. Predeclared Constants: true, false
    auto trueSym = std::make_shared<Symbol>("true", SymbolKind::BuiltinConstant, builtinSpan);
    trueSym->setConst(true);
    universe.define(trueSym);

    auto falseSym = std::make_shared<Symbol>("false", SymbolKind::BuiltinConstant, builtinSpan);
    falseSym->setConst(true);
    universe.define(falseSym);

    // 3. Predeclared "null" (both type and constant keyword)
    auto nullSym = std::make_shared<Symbol>("null", SymbolKind::BuiltinConstant, builtinSpan);
    nullSym->setConst(true);
    nullSym->setIsType(true);
    universe.define(nullSym);

    // 4. Predeclared Operators / Built-ins: typeof
    auto typeofSym = std::make_shared<Symbol>("typeof", SymbolKind::BuiltinOperator, builtinSpan);
    universe.define(typeofSym);
}

bool isStdlibModule(std::string_view name) noexcept {
    return name == "io" || name == "std.io" || name == "keet.io" ||
           name == "fs" || name == "std.fs" || name == "keet.fs";
}

void populateIoModuleScope(Scope& ioScope) {
    diag::SourceSpan span{};

    auto makeFn = [&](const char* name) {
        auto sym = std::make_shared<Symbol>(name, SymbolKind::Function, span);
        sym->setExported(true);
        sym->setConst(true);
        ioScope.define(sym, false);
    };

    makeFn("print");
    makeFn("println");
    makeFn("eprint");
    makeFn("eprintln");
    makeFn("readln");
    makeFn("readLine");
    makeFn("read_line");
    makeFn("input");
    makeFn("eof");
    makeFn("red");
    makeFn("green");
    makeFn("yellow");
    makeFn("blue");
    makeFn("magenta");
    makeFn("cyan");
    makeFn("white");
    makeFn("gray");
    makeFn("bold");
    makeFn("flush");

    // io.Color enum
    auto colorEnum = std::make_shared<Symbol>("Color", SymbolKind::Enum, span);
    colorEnum->setExported(true);
    colorEnum->setIsType(true);
    colorEnum->setConst(true);
    ioScope.define(colorEnum, false);

    const std::vector<const char*> colorMembers = {
        "Default", "Red", "Green", "Yellow", "Blue", "Magenta", "Cyan", "White", "Gray"
    };
    for (const char* memberName : colorMembers) {
        auto valSym = std::make_shared<Symbol>(memberName, SymbolKind::EnumValue, span);
        valSym->setExported(true);
        valSym->setConst(true);
        ioScope.define(valSym, false);
    }
}

void populateFsModuleScope(Scope& fsScope) {
    diag::SourceSpan span{};

    auto makeFn = [&](const char* name) {
        auto sym = std::make_shared<Symbol>(name, SymbolKind::Function, span);
        sym->setExported(true);
        sym->setConst(true);
        fsScope.define(sym, false);
    };

    makeFn("read_text");
    makeFn("read_lines");
    makeFn("write_text");
    makeFn("append_text");
    makeFn("exists");
    makeFn("is_file");
    makeFn("is_dir");
    makeFn("file_size");
    makeFn("mkdir");
    makeFn("remove");
    makeFn("remove_all");
    makeFn("copy");
    makeFn("rename");
    makeFn("list_dir");
    makeFn("cwd");
    makeFn("join");
    makeFn("filename");
    makeFn("extension");
    makeFn("parent_dir");
}

std::shared_ptr<Scope> createStdlibModuleScope(std::string_view moduleName, const std::shared_ptr<Scope>& universe) {
    auto scope = std::make_shared<Scope>(ScopeKind::Module, universe);
    if (moduleName == "io" || moduleName == "std.io" || moduleName == "keet.io") {
        populateIoModuleScope(*scope);
    } else if (moduleName == "fs" || moduleName == "std.fs" || moduleName == "keet.fs") {
        populateFsModuleScope(*scope);
    }
    return scope;
}

} // namespace keety::sema
