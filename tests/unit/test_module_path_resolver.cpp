#include <catch2/catch_test_macros.hpp>
#include <keety/diag/diagnostic_emitter.hpp>
#include <keety/diag/source_manager.hpp>
#include <keety/resolver/module_path_resolver.hpp>

#include <filesystem>
#include <fstream>

using namespace keety::resolver;
using namespace keety::diag;

namespace fs = std::filesystem;

/// RAII helper to create and automatically remove a temporary test directory
struct TempDirectory {
    fs::path path;

    explicit TempDirectory(const std::string& prefix) {
        path = fs::temp_directory_path() / (prefix + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
        fs::create_directories(path);
    }

    ~TempDirectory() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    void createFile(const fs::path& relPath, const std::string& content = "") {
        fs::path fullPath = path / relPath;
        fs::create_directories(fullPath.parent_path());
        std::ofstream ofs(fullPath);
        ofs << content;
    }
};

TEST_CASE("ModulePathResolver resolves standard library modules", "[resolver]") {
    SourceManager sm;
    DiagnosticEmitter emitter(sm, ColorMode::Never);
    ModulePathResolver resolver(fs::current_path(), &emitter);

    SECTION("Built-in virtual standard library modules") {
        auto ioMod = resolver.resolve("src/main.keet", "io");
        REQUIRE(ioMod.has_value());
        REQUIRE(ioMod->isStandardLibrary());
        REQUIRE(ioMod->moduleName == "io");
        REQUIRE_FALSE(ioMod->isNativeHeader);

        auto mathMod = resolver.resolve("src/main.keet", "math");
        REQUIRE(mathMod.has_value());
        REQUIRE(mathMod->isStandardLibrary());
        REQUIRE(mathMod->moduleName == "math");

        for (const char* pkg : {"crypto", "fs", "http", "io", "json", "math", "net", "sys", "test", "time", "ui"}) {
            auto mod = resolver.resolve("src/main.keet", pkg);
            REQUIRE(mod.has_value());
            REQUIRE(mod->isStandardLibrary());
            REQUIRE(mod->moduleName == pkg);
        }

        auto netHttpMod = resolver.resolve("src/main.keet", "net.http");
        REQUIRE(netHttpMod.has_value());
        REQUIRE(netHttpMod->isStandardLibrary());
        REQUIRE(netHttpMod->moduleName == "net.http");
    }

    SECTION("Standard library with physical stdlib directory") {
        TempDirectory temp("keety_stdlib_test");
        temp.createFile("io.keet", "export fn println(s: string): void {}");
        temp.createFile("net/http.keet", "export class Client {}");

        resolver.setStdlibDirectory(temp.path);

        auto ioMod = resolver.resolve("src/main.keet", "io");
        REQUIRE(ioMod.has_value());
        REQUIRE(ioMod->isStandardLibrary());
        REQUIRE(fs::exists(ioMod->canonicalPath));

        auto httpMod = resolver.resolve("src/main.keet", "net.http");
        REQUIRE(httpMod.has_value());
        REQUIRE(httpMod->isStandardLibrary());
        REQUIRE(fs::exists(httpMod->canonicalPath));
    }
}

TEST_CASE("ModulePathResolver resolves local relative imports with canonical deduplication", "[resolver]") {
    TempDirectory temp("keety_local_test");
    temp.createFile("src/main.keet", "import \"./config\";\nimport \"./utils/helpers\";");
    temp.createFile("src/config.keet", "export const PORT: int = 8080;");
    temp.createFile("src/utils/helpers.keet", "export fn helper(): void {}");
    temp.createFile("src/models/index.keet", "export class User {}");
    temp.createFile("src/native/fast_hash.hpp", "#pragma once\n");

    SourceManager sm;
    DiagnosticEmitter emitter(sm, ColorMode::Never);
    ModulePathResolver resolver(temp.path, &emitter);

    fs::path mainPath = temp.path / "src" / "main.keet";

    SECTION("Relative module without .keet extension") {
        auto configMod = resolver.resolve(mainPath, "./config");
        REQUIRE(configMod.has_value());
        REQUIRE(configMod->isLocal());
        REQUIRE(configMod->moduleName == "config");
        REQUIRE(configMod->canonicalPath == fs::canonical(temp.path / "src" / "config.keet"));
    }

    SECTION("Relative module with explicit .keet extension") {
        auto configMod = resolver.resolve(mainPath, "./config.keet");
        REQUIRE(configMod.has_value());
        REQUIRE(configMod->isLocal());
        REQUIRE(configMod->canonicalPath == fs::canonical(temp.path / "src" / "config.keet"));
    }

    SECTION("Relative module inside directory via index.keet") {
        auto modelsMod = resolver.resolve(mainPath, "./models");
        REQUIRE(modelsMod.has_value());
        REQUIRE(modelsMod->isLocal());
        REQUIRE(modelsMod->canonicalPath == fs::canonical(temp.path / "src" / "models" / "index.keet"));
    }

    SECTION("Native C++ header import") {
        auto nativeMod = resolver.resolve(mainPath, "./native/fast_hash.hpp");
        REQUIRE(nativeMod.has_value());
        REQUIRE(nativeMod->isNativeHeader);
        REQUIRE(nativeMod->canonicalPath == fs::canonical(temp.path / "src" / "native" / "fast_hash.hpp"));
    }

    SECTION("Canonical deduplication between different relative paths") {
        // From main: "./config"
        // From utils/helpers: "../config"
        fs::path helpersPath = temp.path / "src" / "utils" / "helpers.keet";

        auto mod1 = resolver.resolve(mainPath, "./config");
        auto mod2 = resolver.resolve(helpersPath, "../config");

        REQUIRE(mod1.has_value());
        REQUIRE(mod2.has_value());
        REQUIRE(mod1->canonicalPath == mod2->canonicalPath);
    }
}

TEST_CASE("ModulePathResolver resolves third-party packages in keet_modules/", "[resolver]") {
    TempDirectory temp("keety_pkg_test");
    temp.createFile("keet_modules/vendor/json.keet", "export class JsonParser {}");
    temp.createFile("keet_modules/sqlite/index.keet", "export class Database {}");

    SourceManager sm;
    DiagnosticEmitter emitter(sm, ColorMode::Never);
    ModulePathResolver resolver(temp.path, &emitter);

    fs::path mainPath = temp.path / "main.keet";

    SECTION("Dot-separated package path: vendor.json") {
        auto jsonMod = resolver.resolve(mainPath, "vendor.json");
        REQUIRE(jsonMod.has_value());
        REQUIRE(jsonMod->isPackage());
        REQUIRE(jsonMod->moduleName == "json");
        REQUIRE(jsonMod->canonicalPath == fs::canonical(temp.path / "keet_modules" / "vendor" / "json.keet"));
    }

    SECTION("Single package name with index.keet: sqlite") {
        auto sqliteMod = resolver.resolve(mainPath, "sqlite");
        REQUIRE(sqliteMod.has_value());
        REQUIRE(sqliteMod->isPackage());
        REQUIRE(sqliteMod->canonicalPath == fs::canonical(temp.path / "keet_modules" / "sqlite" / "index.keet"));
    }
}

TEST_CASE("ModulePathResolver error reporting: ModuleNotFound (E1002) and CannotImportSelf (E1003)", "[resolver]") {
    TempDirectory temp("keety_err_test");
    temp.createFile("src/self.keet", "import \"./self.keet\";");

    SourceManager sm;
    DiagnosticEmitter emitter(sm, ColorMode::Never);
    ModulePathResolver resolver(temp.path, &emitter);

    fs::path selfPath = temp.path / "src" / "self.keet";

    SECTION("CannotImportSelf (E1003) when file imports itself") {
        auto result = resolver.resolve(selfPath, "./self");
        REQUIRE_FALSE(result.has_value());
        REQUIRE(emitter.hasErrors());
        REQUIRE(emitter.getErrorCount() > 0);
    }

    SECTION("ModuleNotFound (E1002) when local file does not exist") {
        SourceManager sm2;
        DiagnosticEmitter emitter2(sm2, ColorMode::Never);
        ModulePathResolver resolver2(temp.path, &emitter2);

        auto result = resolver2.resolve(selfPath, "./non_existent_module");
        REQUIRE_FALSE(result.has_value());
        REQUIRE(emitter2.hasErrors());
    }

    SECTION("ModuleNotFound (E1002) when package is not in keet_modules/") {
        SourceManager sm3;
        DiagnosticEmitter emitter3(sm3, ColorMode::Never);
        ModulePathResolver resolver3(temp.path, &emitter3);

        auto result = resolver3.resolve(selfPath, "unknown.pkg");
        REQUIRE_FALSE(result.has_value());
        REQUIRE(emitter3.hasErrors());
    }
}
