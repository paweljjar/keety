#pragma once

#include <keety/sema/scope.hpp>
#include <keety/sema/symbol.hpp>

#include <string_view>
#include <vector>

namespace keety::sema {

/// Populates the given Universe scope with all predeclared KeetScript symbols:
/// - Predeclared types: bool, int, float, char, string, void, null, error, array, map, never
/// - Predeclared constants: true, false, null
/// - Predeclared operators/builtins: typeof
void registerUniverseSymbols(Scope& universe);

/// Checks if an identifier name is a predeclared universe type.
[[nodiscard]] bool isUniverseType(std::string_view name) noexcept;

/// Checks if an identifier name is a predeclared universe constant.
[[nodiscard]] bool isUniverseConstant(std::string_view name) noexcept;

/// Checks if an identifier name is a predeclared universe operator or builtin.
[[nodiscard]] bool isUniverseOperator(std::string_view name) noexcept;

/// Checks if an identifier name is any predeclared universe symbol.
[[nodiscard]] bool isUniverseSymbol(std::string_view name) noexcept;

/// Returns the complete list of predeclared type names.
[[nodiscard]] const std::vector<std::string_view>& getUniverseTypeNames() noexcept;

/// Returns the complete list of predeclared constant names.
[[nodiscard]] const std::vector<std::string_view>& getUniverseConstantNames() noexcept;

/// Returns the complete list of all predeclared symbol names.
[[nodiscard]] const std::vector<std::string_view>& getAllUniverseSymbolNames() noexcept;

/// Checks if an identifier or import path is a recognized standard library module.
[[nodiscard]] bool isStdlibModule(std::string_view name) noexcept;

/// Creates a Scope populated with all exported symbols of a standard library module.
[[nodiscard]] std::shared_ptr<Scope> createStdlibModuleScope(std::string_view moduleName, const std::shared_ptr<Scope>& universe = nullptr);

/// Populates an existing Scope with all exported symbols of the 'io' standard library module.
void populateIoModuleScope(Scope& ioScope);

/// Populates an existing Scope with all exported symbols of the 'fs' standard library module.
void populateFsModuleScope(Scope& fsScope);

/// Populates an existing Scope with all exported symbols of the 'sys' standard library module.
void populateSysModuleScope(Scope& sysScope);

/// Populates an existing Scope with all exported symbols of the 'math' standard library module.
void populateMathModuleScope(Scope& mathScope);

/// Populates an existing Scope with all exported symbols of the 'time' standard library module.
void populateTimeModuleScope(Scope& timeScope);

/// Populates an existing Scope with all exported symbols of the 'json' standard library module.
void populateJsonModuleScope(Scope& jsonScope);

/// Populates an existing Scope with all exported symbols of the 'net' standard library module.
void populateNetModuleScope(Scope& netScope);

/// Populates an existing Scope with all exported symbols of the 'test' standard library module.
void populateTestModuleScope(Scope& testScope);

} // namespace keety::sema
