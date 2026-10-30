#pragma once

#include <keety/ast/ast_node.hpp>
#include <keety/ast/base.hpp>
#include <keety/lexer/token.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace keety::ast {

/// Visibility modifier for declarations and class members
enum class Visibility {
    Default,  // Private for class members, module-local for top-level declarations
    Pub,      // Public / exported
    Priv      // Explicitly private
};

/// Base class for all Declaration AST nodes
class Decl : public AstNode {
public:
    using AstNode::AstNode;
};

/// Represents a variable declaration: `let x: int = 10;` or `export let count: int;`
class VarDecl : public Decl {
public:
    VarDecl(diag::SourceSpan span, std::string name, std::unique_ptr<TypeAnnotation> type,
            std::unique_ptr<Expr> initializer = nullptr, bool isExported = false)
        : Decl(span), name_(std::move(name)), type_(std::move(type)),
          initializer_(std::move(initializer)), isExported_(isExported) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
    [[nodiscard]] const TypeAnnotation* getType() const noexcept { return type_.get(); }
    [[nodiscard]] const Expr* getInitializer() const noexcept { return initializer_.get(); }
    [[nodiscard]] bool hasInitializer() const noexcept { return initializer_ != nullptr; }
    [[nodiscard]] bool isExported() const noexcept { return isExported_; }

private:
    std::string name_;
    std::unique_ptr<TypeAnnotation> type_;
    std::unique_ptr<Expr> initializer_;
    bool isExported_{false};
};

/// Represents an immutable runtime constant: `const MAX_RETRIES: int = 5;`
class ConstDecl : public Decl {
public:
    ConstDecl(diag::SourceSpan span, std::string name, std::unique_ptr<TypeAnnotation> type,
              std::unique_ptr<Expr> initializer, bool isExported = false)
        : Decl(span), name_(std::move(name)), type_(std::move(type)),
          initializer_(std::move(initializer)), isExported_(isExported) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
    [[nodiscard]] const TypeAnnotation* getType() const noexcept { return type_.get(); }
    [[nodiscard]] const Expr* getInitializer() const noexcept { return initializer_.get(); }
    [[nodiscard]] bool isExported() const noexcept { return isExported_; }

private:
    std::string name_;
    std::unique_ptr<TypeAnnotation> type_;
    std::unique_ptr<Expr> initializer_;
    bool isExported_{false};
};

/// Represents a compile-time constant: `define BUFFER_SIZE: int = 1024;`
class DefineDecl : public Decl {
public:
    DefineDecl(diag::SourceSpan span, std::string name, std::unique_ptr<TypeAnnotation> type,
               std::unique_ptr<Expr> value, bool isExported = false)
        : Decl(span), name_(std::move(name)), type_(std::move(type)),
          value_(std::move(value)), isExported_(isExported) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
    [[nodiscard]] const TypeAnnotation* getType() const noexcept { return type_.get(); }
    [[nodiscard]] const Expr* getValue() const noexcept { return value_.get(); }
    [[nodiscard]] bool isExported() const noexcept { return isExported_; }

private:
    std::string name_;
    std::unique_ptr<TypeAnnotation> type_;
    std::unique_ptr<Expr> value_;
    bool isExported_{false};
};

/// Represents a parameter in a function or method signature
struct Param {
    diag::SourceSpan span{};
    std::string name;
    std::unique_ptr<TypeAnnotation> type;
    bool isMut{false};
    std::unique_ptr<Expr> defaultValue{nullptr};
};

/// Represents a function declaration: `fn add(a: int, b: int): int { return a + b; }`
class FnDecl : public Decl {
public:
    FnDecl(diag::SourceSpan span, std::string name, std::vector<Param> params,
           std::unique_ptr<TypeAnnotation> returnType, std::unique_ptr<Stmt> body,
           bool isAsync = false, bool isExported = false, Visibility vis = Visibility::Default)
        : Decl(span), name_(std::move(name)), params_(std::move(params)),
          returnType_(std::move(returnType)), body_(std::move(body)),
          isAsync_(isAsync), isExported_(isExported), visibility_(vis) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
    [[nodiscard]] const std::vector<Param>& getParams() const noexcept { return params_; }
    [[nodiscard]] const TypeAnnotation* getReturnType() const noexcept { return returnType_.get(); }
    [[nodiscard]] const Stmt* getBody() const noexcept { return body_.get(); }
    [[nodiscard]] bool isAsync() const noexcept { return isAsync_; }
    [[nodiscard]] bool isExported() const noexcept { return isExported_; }
    [[nodiscard]] Visibility getVisibility() const noexcept { return visibility_; }

private:
    std::string name_;
    std::vector<Param> params_;
    std::unique_ptr<TypeAnnotation> returnType_;
    std::unique_ptr<Stmt> body_;
    bool isAsync_{false};
    bool isExported_{false};
    Visibility visibility_{Visibility::Default};
};

/// Represents a class member field: `pub let name: string;`
struct ClassField {
    diag::SourceSpan span{};
    Visibility visibility{Visibility::Default};
    bool isConst{false};
    std::string name;
    std::unique_ptr<TypeAnnotation> type;
    std::unique_ptr<Expr> initializer{nullptr};
};

/// Represents a class declaration: `class Player inherits Entity { ... }`
class ClassDecl : public Decl {
public:
    ClassDecl(diag::SourceSpan span, std::string name, std::string baseClass,
              std::vector<ClassField> fields, std::vector<std::unique_ptr<FnDecl>> methods,
              bool isExported = false)
        : Decl(span), name_(std::move(name)), baseClass_(std::move(baseClass)),
          fields_(std::move(fields)), methods_(std::move(methods)), isExported_(isExported) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
    [[nodiscard]] const std::string& getBaseClass() const noexcept { return baseClass_; }
    [[nodiscard]] bool hasBaseClass() const noexcept { return !baseClass_.empty(); }
    [[nodiscard]] const std::vector<ClassField>& getFields() const noexcept { return fields_; }
    [[nodiscard]] const std::vector<std::unique_ptr<FnDecl>>& getMethods() const noexcept { return methods_; }
    [[nodiscard]] bool isExported() const noexcept { return isExported_; }

private:
    std::string name_;
    std::string baseClass_;
    std::vector<ClassField> fields_;
    std::vector<std::unique_ptr<FnDecl>> methods_;
    bool isExported_{false};
};

/// Represents an enumerator member: `OK = 200`
struct EnumMember {
    diag::SourceSpan span{};
    std::string name;
    std::unique_ptr<Expr> value{nullptr};
};

/// Represents an enum declaration: `enum Status: int { OK = 200, NotFound = 404 }`
class EnumDecl : public Decl {
public:
    EnumDecl(diag::SourceSpan span, std::string name, std::unique_ptr<TypeAnnotation> underlyingType,
             std::vector<EnumMember> members, bool isExported = false)
        : Decl(span), name_(std::move(name)), underlyingType_(std::move(underlyingType)),
          members_(std::move(members)), isExported_(isExported) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
    [[nodiscard]] const TypeAnnotation* getUnderlyingType() const noexcept { return underlyingType_.get(); }
    [[nodiscard]] const std::vector<EnumMember>& getMembers() const noexcept { return members_; }
    [[nodiscard]] bool isExported() const noexcept { return isExported_; }

private:
    std::string name_;
    std::unique_ptr<TypeAnnotation> underlyingType_;
    std::vector<EnumMember> members_;
    bool isExported_{false};
};

/// Represents a structured error type: `errtype NetworkTimeout { pub let retryAfter: int; }`
class ErrTypeDecl : public Decl {
public:
    ErrTypeDecl(diag::SourceSpan span, std::string name, std::string baseError,
                std::vector<ClassField> fields, bool isExported = false)
        : Decl(span), name_(std::move(name)), baseError_(std::move(baseError)),
          fields_(std::move(fields)), isExported_(isExported) {}

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
    [[nodiscard]] const std::string& getBaseError() const noexcept { return baseError_; }
    [[nodiscard]] const std::vector<ClassField>& getFields() const noexcept { return fields_; }
    [[nodiscard]] bool isExported() const noexcept { return isExported_; }

private:
    std::string name_;
    std::string baseError_;
    std::vector<ClassField> fields_;
    bool isExported_{false};
};

/// Represents an imported symbol in selective imports: `from "./math" import add, sub as subtract;`
struct ImportedSymbol {
    std::string originalName;
    std::string alias; // Empty if not aliased
    diag::SourceSpan span{};

    [[nodiscard]] const std::string& getEffectiveName() const noexcept {
        return alias.empty() ? originalName : alias;
    }
};

/// Represents an import declaration: `import io;` or `from "./helper" import foo;`
class ImportDecl : public Decl {
public:
    ImportDecl(diag::SourceSpan span, std::string modulePath, std::string moduleAlias = "",
               std::vector<ImportedSymbol> symbols = {}, bool isReExported = false)
        : Decl(span), modulePath_(std::move(modulePath)), moduleAlias_(std::move(moduleAlias)),
          symbols_(std::move(symbols)), isReExported_(isReExported) {}

    [[nodiscard]] const std::string& getModulePath() const noexcept { return modulePath_; }
    [[nodiscard]] const std::string& getModuleAlias() const noexcept { return moduleAlias_; }
    [[nodiscard]] const std::vector<ImportedSymbol>& getSymbols() const noexcept { return symbols_; }
    [[nodiscard]] bool isSelective() const noexcept { return !symbols_.empty(); }
    [[nodiscard]] bool isReExported() const noexcept { return isReExported_; }

private:
    std::string modulePath_;
    std::string moduleAlias_;
    std::vector<ImportedSymbol> symbols_;
    bool isReExported_{false};
};

/// Represents an export statement: `export { foo, bar };`
class ExportDecl : public Decl {
public:
    ExportDecl(diag::SourceSpan span, std::vector<std::string> exportedNames)
        : Decl(span), exportedNames_(std::move(exportedNames)) {}

    [[nodiscard]] const std::vector<std::string>& getExportedNames() const noexcept {
        return exportedNames_;
    }

private:
    std::vector<std::string> exportedNames_;
};

} // namespace keety::ast
