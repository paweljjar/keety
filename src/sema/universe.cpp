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
           name == "fs" || name == "std.fs" || name == "keet.fs" ||
           name == "sys" || name == "std.sys" || name == "keet.sys" ||
           name == "math" || name == "std.math" || name == "keet.math" ||
           name == "time" || name == "std.time" || name == "keet.time" ||
           name == "json" || name == "std.json" || name == "keet.json" ||
           name == "net" || name == "std.net" || name == "keet.net";
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

    // Standard camelCase library functions
    makeFn("readText");
    makeFn("readLines");
    makeFn("writeText");
    makeFn("appendText");
    makeFn("exists");
    makeFn("isFile");
    makeFn("isDir");
    makeFn("fileSize");
    makeFn("mkdir");
    makeFn("remove");
    makeFn("removeAll");
    makeFn("copy");
    makeFn("rename");
    makeFn("listDir");
    makeFn("cwd");
    makeFn("join");
    makeFn("fileName");
    makeFn("extension");
    makeFn("parentDir");
}

void populateSysModuleScope(Scope& sysScope) {
    diag::SourceSpan span{};

    auto makeFn = [&](const char* name) {
        auto sym = std::make_shared<Symbol>(name, SymbolKind::Function, span);
        sym->setExported(true);
        sym->setConst(true);
        sysScope.define(sym, false);
    };

    makeFn("args");
    makeFn("exit");
    makeFn("pid");
    makeFn("getEnv");
    makeFn("getEnvOr");
    makeFn("setEnv");
    makeFn("unsetEnv");
    makeFn("hasEnv");
    makeFn("platform");
    makeFn("arch");
    makeFn("cpuCount");
    makeFn("cwd");
    makeFn("chdir");
    makeFn("homeDir");
    makeFn("tempDir");
    makeFn("exec");
    makeFn("execOutput");
}

void populateMathModuleScope(Scope& mathScope) {
    diag::SourceSpan span{};

    auto makeConst = [&](const char* name) {
        auto sym = std::make_shared<Symbol>(name, SymbolKind::Constant, span);
        sym->setExported(true);
        sym->setConst(true);
        mathScope.define(sym, false);
    };

    auto makeFn = [&](const char* name) {
        auto sym = std::make_shared<Symbol>(name, SymbolKind::Function, span);
        sym->setExported(true);
        sym->setConst(true);
        mathScope.define(sym, false);
    };

    // Constants
    makeConst("PI");
    makeConst("E");
    makeConst("TAU");
    makeConst("INF");
    makeConst("NAN");

    // Basic & Rounding Operations
    makeFn("abs");
    makeFn("absInt");
    makeFn("min");
    makeFn("minInt");
    makeFn("max");
    makeFn("maxInt");
    makeFn("clamp");
    makeFn("clampInt");
    makeFn("floor");
    makeFn("ceil");
    makeFn("round");
    makeFn("trunc");
    makeFn("sign");
    makeFn("signInt");

    // Powers, Roots, Exponential & Logarithmic Functions
    makeFn("sqrt");
    makeFn("cbrt");
    makeFn("pow");
    makeFn("hypot");
    makeFn("exp");
    makeFn("log");
    makeFn("log2");
    makeFn("log10");

    // Trigonometric & Hyperbolic Functions
    makeFn("sin");
    makeFn("cos");
    makeFn("tan");
    makeFn("asin");
    makeFn("acos");
    makeFn("atan");
    makeFn("atan2");
    makeFn("sinh");
    makeFn("cosh");
    makeFn("tanh");
    makeFn("toRadians");
    makeFn("toDegrees");

    // Floating-Point Classification & Utility Functions
    makeFn("isNan");
    makeFn("isInf");
    makeFn("isFinite");
    makeFn("lerp");
    makeFn("fmod");

    // Pseudo-Random Number Generation
    makeFn("random");
    makeFn("randomInt");
    makeFn("randomFloat");
    makeFn("seed");
}

void populateTimeModuleScope(Scope& timeScope) {
    diag::SourceSpan span{};

    auto makeConst = [&](const char* name) {
        auto sym = std::make_shared<Symbol>(name, SymbolKind::Constant, span);
        sym->setExported(true);
        sym->setConst(true);
        timeScope.define(sym, false);
    };

    auto makeFn = [&](const char* name) {
        auto sym = std::make_shared<Symbol>(name, SymbolKind::Function, span);
        sym->setExported(true);
        sym->setConst(true);
        timeScope.define(sym, false);
    };

    // Time Unit Conversion Constants
    makeConst("NS_PER_US");
    makeConst("NS_PER_MS");
    makeConst("NS_PER_SEC");
    makeConst("US_PER_MS");
    makeConst("US_PER_SEC");
    makeConst("MS_PER_SEC");
    makeConst("SEC_PER_MIN");
    makeConst("SEC_PER_HOUR");
    makeConst("SEC_PER_DAY");

    // Wall-Clock Time Queries
    makeFn("now");
    makeFn("nowSec");
    makeFn("nowMs");
    makeFn("nowUs");
    makeFn("nowNs");

    // Monotonic High-Resolution Timers
    makeFn("monotonicMs");
    makeFn("monotonicUs");
    makeFn("monotonicNs");
    makeFn("monotonicSec");
    makeFn("elapsedMs");
    makeFn("elapsedSec");

    // Thread Sleep Operations
    makeFn("sleep");
    makeFn("sleepMs");
    makeFn("sleepUs");
    makeFn("sleepSec");

    // Timestamp Formatting & Calendar Extraction
    makeFn("formatIso8601");
    makeFn("formatUtc");
    makeFn("formatLocal");
    makeFn("yearUtc");
    makeFn("monthUtc");
    makeFn("dayUtc");
    makeFn("hourUtc");
    makeFn("minuteUtc");
    makeFn("secondUtc");
}

void populateJsonModuleScope(Scope& jsonScope) {
    diag::SourceSpan span{};

    auto makeFn = [&](const char* name) {
        auto sym = std::make_shared<Symbol>(name, SymbolKind::Function, span);
        sym->setExported(true);
        sym->setConst(true);
        jsonScope.define(sym, false);
    };

    // Validation & Type Inspection
    makeFn("isValid");
    makeFn("typeOf");
    makeFn("isObject");
    makeFn("isArray");
    makeFn("size");
    makeFn("keys");

    // Dot-Path Queries & Typed Extraction
    makeFn("hasKey");
    makeFn("isNull");
    makeFn("getString");
    makeFn("getInt");
    makeFn("getFloat");
    makeFn("getBool");
    makeFn("getArray");
    makeFn("getObject");

    // Non-Throwing Fallback Extractors (*Or)
    makeFn("getStringOr");
    makeFn("getIntOr");
    makeFn("getFloatOr");
    makeFn("getBoolOr");

    // Typed Collection Parsers
    makeFn("parseMap");
    makeFn("parseArray");
    makeFn("parseIntArray");
    makeFn("parseFloatArray");
    makeFn("parseBoolArray");

    // Immutable Object & Array Builders / Mutation Helpers
    makeFn("emptyObject");
    makeFn("emptyArray");
    makeFn("setString");
    makeFn("setInt");
    makeFn("setFloat");
    makeFn("setBool");
    makeFn("setNull");
    makeFn("setRaw");
    makeFn("removeKey");

    // Polymorphic Serialization, Formatting, Escaping & File I/O
    makeFn("stringify");
    makeFn("format");
    makeFn("minify");
    makeFn("escape");
    makeFn("unescape");
    makeFn("readFile");
    makeFn("writeFile");
}

void populateNetModuleScope(Scope& netScope) {
    diag::SourceSpan span{};

    auto makeFn = [&](const char* name) {
        auto sym = std::make_shared<Symbol>(name, SymbolKind::Function, span);
        sym->setExported(true);
        sym->setConst(true);
        netScope.define(sym, false);
    };

    // URL Parsing, Encoding & Query String Utilities
    makeFn("urlEncode");
    makeFn("urlDecode");
    makeFn("parseUrl");
    makeFn("urlScheme");
    makeFn("urlHost");
    makeFn("urlPort");
    makeFn("urlPath");
    makeFn("urlQuery");
    makeFn("buildQuery");
    makeFn("parseQuery");

    // DNS Resolution & IP Address Utilities
    makeFn("resolveHost");
    makeFn("resolveIpv4");
    makeFn("isIpv4");
    makeFn("isIpv6");
    makeFn("isIp");
    makeFn("isLoopback");
    makeFn("hostname");

    // HTTP/1.1 & HTTPS Client
    makeFn("get");
    makeFn("getStatus");
    makeFn("post");
    makeFn("postJson");
    makeFn("put");
    makeFn("deleteReq");
    makeFn("request");
    makeFn("download");

    // Handle-Based TCP Sockets
    makeFn("tcpConnect");
    makeFn("tcpConnectTimeout");
    makeFn("tcpListen");
    makeFn("tcpAccept");
    makeFn("tcpLocalPort");
    makeFn("tcpPeerAddr");
    makeFn("tcpSend");
    makeFn("tcpRecv");
    makeFn("tcpRecvLine");
    makeFn("tcpSetTimeout");
    makeFn("tcpClose");

    // Handle-Based UDP Sockets
    makeFn("udpBind");
    makeFn("udpLocalPort");
    makeFn("udpSendTo");
    makeFn("udpRecvFrom");
    makeFn("udpClose");
}

std::shared_ptr<Scope> createStdlibModuleScope(std::string_view moduleName, const std::shared_ptr<Scope>& universe) {
    auto scope = std::make_shared<Scope>(ScopeKind::Module, universe);
    if (moduleName == "io" || moduleName == "std.io" || moduleName == "keet.io") {
        populateIoModuleScope(*scope);
    } else if (moduleName == "fs" || moduleName == "std.fs" || moduleName == "keet.fs") {
        populateFsModuleScope(*scope);
    } else if (moduleName == "sys" || moduleName == "std.sys" || moduleName == "keet.sys") {
        populateSysModuleScope(*scope);
    } else if (moduleName == "math" || moduleName == "std.math" || moduleName == "keet.math") {
        populateMathModuleScope(*scope);
    } else if (moduleName == "time" || moduleName == "std.time" || moduleName == "keet.time") {
        populateTimeModuleScope(*scope);
    } else if (moduleName == "json" || moduleName == "std.json" || moduleName == "keet.json") {
        populateJsonModuleScope(*scope);
    } else if (moduleName == "net" || moduleName == "std.net" || moduleName == "keet.net") {
        populateNetModuleScope(*scope);
    }
    return scope;
}

} // namespace keety::sema
