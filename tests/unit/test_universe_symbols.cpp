#include <catch2/catch_test_macros.hpp>
#include <keety/sema/scope.hpp>
#include <keety/sema/symbol.hpp>
#include <keety/sema/universe.hpp>

using namespace keety::sema;

TEST_CASE("Predeclared Universe types inspection", "[sema][universe]") {
    auto universe = Scope::createUniverse();
    REQUIRE(universe != nullptr);

    const std::vector<std::string> expectedTypes = {
        "bool", "int", "float", "char", "string", "void", "null", "error", "array", "map"
    };

    for (const auto& typeName : expectedTypes) {
        DYNAMIC_SECTION("Type: " << typeName) {
            CHECK(isUniverseType(typeName));
            CHECK(isUniverseSymbol(typeName));

            auto sym = universe->lookupCurrent(typeName);
            REQUIRE(sym != nullptr);
            CHECK(sym->getName() == typeName);
            CHECK(sym->isType());
            CHECK(sym->isBuiltin());
            CHECK(sym->isBuiltinType());
        }
    }

    CHECK_FALSE(isUniverseType("double"));
    CHECK_FALSE(isUniverseType("uint"));
    CHECK_FALSE(isUniverseType("any"));
    CHECK_FALSE(isUniverseType("CustomClass"));
}

TEST_CASE("Predeclared Universe constants inspection", "[sema][universe]") {
    auto universe = Scope::createUniverse();
    REQUIRE(universe != nullptr);

    SECTION("true constant") {
        CHECK(isUniverseConstant("true"));
        CHECK(isUniverseSymbol("true"));

        auto trueSym = universe->lookupCurrent("true");
        REQUIRE(trueSym != nullptr);
        CHECK(trueSym->isConstant());
        CHECK(trueSym->isBuiltinConstant());
        CHECK(trueSym->isBuiltin());
        CHECK_FALSE(trueSym->isMutable());
    }

    SECTION("false constant") {
        CHECK(isUniverseConstant("false"));
        CHECK(isUniverseSymbol("false"));

        auto falseSym = universe->lookupCurrent("false");
        REQUIRE(falseSym != nullptr);
        CHECK(falseSym->isConstant());
        CHECK(falseSym->isBuiltinConstant());
        CHECK(falseSym->isBuiltin());
        CHECK_FALSE(falseSym->isMutable());
    }

    SECTION("null is both constant and type") {
        CHECK(isUniverseConstant("null"));
        CHECK(isUniverseType("null"));
        CHECK(isUniverseSymbol("null"));

        auto nullSym = universe->lookupCurrent("null");
        REQUIRE(nullSym != nullptr);
        CHECK(nullSym->isConstant());
        CHECK(nullSym->isType());
        CHECK(nullSym->isBuiltin());
    }

    CHECK_FALSE(isUniverseConstant("undefined"));
    CHECK_FALSE(isUniverseConstant("nil"));
    CHECK_FALSE(isUniverseConstant("None"));
}

TEST_CASE("Predeclared Universe operators inspection", "[sema][universe]") {
    auto universe = Scope::createUniverse();
    REQUIRE(universe != nullptr);

    SECTION("typeof operator") {
        CHECK(isUniverseOperator("typeof"));
        CHECK(isUniverseSymbol("typeof"));

        auto typeofSym = universe->lookupCurrent("typeof");
        REQUIRE(typeofSym != nullptr);
        CHECK(typeofSym->isBuiltinOperator());
        CHECK(typeofSym->isBuiltin());
        CHECK(typeofSym->getKind() == SymbolKind::BuiltinOperator);
    }

    CHECK_FALSE(isUniverseOperator("sizeof"));
    CHECK_FALSE(isUniverseOperator("alignof"));
}

TEST_CASE("Universe catalog lists and queries", "[sema][universe]") {
    const auto& types = getUniverseTypeNames();
    CHECK(types.size() == 10);

    const auto& constants = getUniverseConstantNames();
    CHECK(constants.size() == 3);

    const auto& all = getAllUniverseSymbolNames();
    CHECK(all.size() == 13);

    auto universe = Scope::createUniverse();
    CHECK(universe->size() == 13);
}

TEST_CASE("Universe symbols propagate down through child scopes", "[sema][universe]") {
    auto universe = Scope::createUniverse();
    auto moduleScope = universe->createChild(ScopeKind::Module);
    auto fnScope = moduleScope->createChild(ScopeKind::Function);
    auto blockScope = fnScope->createChild(ScopeKind::Block);

    SECTION("Resolving built-ins from innermost block scope") {
        auto intSym = blockScope->lookup("int");
        REQUIRE(intSym != nullptr);
        CHECK(intSym->isBuiltinType());

        auto trueSym = blockScope->lookup("true");
        REQUIRE(trueSym != nullptr);
        CHECK(trueSym->isBuiltinConstant());

        auto typeofSym = blockScope->lookup("typeof");
        REQUIRE(typeofSym != nullptr);
        CHECK(typeofSym->isBuiltinOperator());

        // Not present in local block scope directly
        CHECK(blockScope->lookupCurrent("int") == nullptr);
        CHECK(blockScope->lookupParent("int") == intSym);
    }

    SECTION("Universe symbols are detected as shadowing targets") {
        auto candidateInt = fnScope->lookupParentForShadowing("int");
        REQUIRE(candidateInt != nullptr);
        CHECK(candidateInt->getName() == "int");

        auto candidateTypeof = blockScope->lookupParentForShadowing("typeof");
        REQUIRE(candidateTypeof != nullptr);
        CHECK(candidateTypeof->getName() == "typeof");

        auto candidateTrue = moduleScope->lookupParentForShadowing("true");
        REQUIRE(candidateTrue != nullptr);
        CHECK(candidateTrue->getName() == "true");
    }

    SECTION("Empty universe without predeclared symbols when requested") {
        auto emptyUniverse = Scope::createUniverse(/*populatePredeclared=*/false);
        CHECK(emptyUniverse->empty());
        CHECK(emptyUniverse->lookup("int") == nullptr);
    }
}

TEST_CASE("Standard library json module scope population", "[sema][universe][json]") {
    auto universe = Scope::createUniverse();
    REQUIRE(universe != nullptr);

    CHECK(isStdlibModule("json"));
    auto jsonScope = createStdlibModuleScope("json", universe);
    REQUIRE(jsonScope != nullptr);

    const std::vector<std::string> expectedFunctions = {
        "isValid", "typeOf", "isObject", "isArray", "size", "keys",
        "hasKey", "isNull", "getString", "getInt", "getFloat", "getBool", "getArray", "getObject",
        "getStringOr", "getIntOr", "getFloatOr", "getBoolOr",
        "parseMap", "parseArray", "parseIntArray", "parseFloatArray", "parseBoolArray",
        "emptyObject", "emptyArray", "setString", "setInt", "setFloat", "setBool", "setNull", "setRaw", "removeKey",
        "stringify", "format", "minify", "escape", "unescape",
        "readFile", "writeFile"
    };

    CHECK(jsonScope->size() == expectedFunctions.size());
    for (const auto& fnName : expectedFunctions) {
        auto sym = jsonScope->lookupCurrent(fnName);
        REQUIRE(sym != nullptr);
        CHECK(sym->getName() == fnName);
        CHECK(sym->getKind() == SymbolKind::Function);
        CHECK(sym->isExported());
    }

    CHECK(jsonScope->lookupCurrent("is_valid") == nullptr);
    CHECK(jsonScope->lookupCurrent("get_string") == nullptr);
}

TEST_CASE("Standard library net module scope population", "[sema][universe][net]") {
    auto universe = Scope::createUniverse();
    REQUIRE(universe != nullptr);

    CHECK(isStdlibModule("net"));
    auto netScope = createStdlibModuleScope("net", universe);
    REQUIRE(netScope != nullptr);

    const std::vector<std::string> expectedFunctions = {
        "urlEncode", "urlDecode", "parseUrl", "urlScheme", "urlHost", "urlPort", "urlPath", "urlQuery", "buildQuery", "parseQuery",
        "resolveHost", "resolveIpv4", "isIpv4", "isIpv6", "isIp", "isLoopback", "hostname",
        "get", "getStatus", "post", "postJson", "put", "deleteReq", "request", "download",
        "tcpConnect", "tcpConnectTimeout", "tcpListen", "tcpAccept", "tcpLocalPort", "tcpPeerAddr", "tcpSend", "tcpRecv", "tcpRecvLine", "tcpSetTimeout", "tcpClose",
        "udpBind", "udpLocalPort", "udpSendTo", "udpRecvFrom", "udpClose"
    };

    CHECK(netScope->size() == expectedFunctions.size());
    for (const auto& fnName : expectedFunctions) {
        auto sym = netScope->lookupCurrent(fnName);
        REQUIRE(sym != nullptr);
        CHECK(sym->getName() == fnName);
        CHECK(sym->getKind() == SymbolKind::Function);
        CHECK(sym->isExported());
    }

    CHECK(netScope->lookupCurrent("url_encode") == nullptr);
    CHECK(netScope->lookupCurrent("tcp_connect") == nullptr);
}

TEST_CASE("Standard library test module scope population", "[sema][universe][test]") {
    auto universe = Scope::createUniverse();
    REQUIRE(universe != nullptr);

    CHECK(isStdlibModule("test"));
    CHECK(isStdlibModule("std.test"));
    CHECK(isStdlibModule("keet.test"));
    auto testScope = createStdlibModuleScope("test", universe);
    REQUIRE(testScope != nullptr);

    const std::vector<std::string> expectedFunctions = {
        "case", "suite", "assertEq", "assertNe", "assertTrue", "assertFalse", "assertZonks", "fail", "run"
    };

    CHECK(testScope->size() == expectedFunctions.size());
    for (const auto& fnName : expectedFunctions) {
        auto sym = testScope->lookupCurrent(fnName);
        REQUIRE(sym != nullptr);
        CHECK(sym->getName() == fnName);
        CHECK(sym->getKind() == SymbolKind::Function);
        CHECK(sym->isExported());
    }

    CHECK(testScope->lookupCurrent("assert_eq") == nullptr);
    CHECK(testScope->lookupCurrent("unknown") == nullptr);
}

TEST_CASE("Standard library crypto module scope population", "[sema][universe][crypto]") {
    auto universe = Scope::createUniverse();
    REQUIRE(universe != nullptr);

    CHECK(isStdlibModule("crypto"));
    CHECK(isStdlibModule("std.crypto"));
    CHECK(isStdlibModule("keet.crypto"));
    CHECK(isStdlibModule("crypto.hash"));
    auto cryptoScope = createStdlibModuleScope("crypto", universe);
    REQUIRE(cryptoScope != nullptr);

    const std::vector<std::string> expectedFunctions = {
        "toHex", "fromHex", "toBase64", "fromBase64", "toBase64Url", "fromBase64Url",
        "randomBytes", "randomHex", "randomInt", "randomUuid", "timingSafeEqual",
        "sha256", "sha512", "sha3_256", "sha3_512", "blake3", "createHash",
        "hmacSha256", "hmacSha512", "createHmac",
        "encryptChaCha20Poly1305", "decryptChaCha20Poly1305",
        "encryptAesGcm", "decryptAesGcm",
        "encodeCode128", "encodeCode128Svg", "encodeCode128Ascii", "decodeCode128",
        "generateQr", "decodeQr"
    };

    for (const auto& fnName : expectedFunctions) {
        auto sym = cryptoScope->lookupCurrent(fnName);
        REQUIRE(sym != nullptr);
        CHECK(sym->getName() == fnName);
        CHECK(sym->getKind() == SymbolKind::Function);
        CHECK(sym->isExported());
    }

    const std::vector<std::string> expectedTypes = {
        "EncryptedPayload", "QrCode", "Hasher", "Hmac"
    };

    for (const auto& typeName : expectedTypes) {
        auto sym = cryptoScope->lookupCurrent(typeName);
        REQUIRE(sym != nullptr);
        CHECK(sym->getName() == typeName);
        CHECK(sym->getKind() == SymbolKind::Class);
        CHECK(sym->isType());
        CHECK(sym->isExported());
    }

    CHECK(cryptoScope->size() == expectedFunctions.size() + expectedTypes.size());
    CHECK(cryptoScope->lookupCurrent("unknown_crypto") == nullptr);
}

