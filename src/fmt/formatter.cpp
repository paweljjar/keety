#include <keety/fmt/formatter.hpp>
#include <keety/fmt/comment_attacher.hpp>
#include <keety/fmt/import_sorter.hpp>
#include <keety/lexer/lexer.hpp>
#include <keety/parser/parser.hpp>

#include <algorithm>
#include <sstream>

namespace keety::fmt {

namespace {

[[nodiscard]] std::string_view getBinaryOpSpelling(lexer::TokenKind kind) noexcept {
    switch (kind) {
    case lexer::TokenKind::Plus:         return "+";
    case lexer::TokenKind::Minus:        return "-";
    case lexer::TokenKind::Star:         return "*";
    case lexer::TokenKind::Slash:        return "/";
    case lexer::TokenKind::Percent:      return "%";
    case lexer::TokenKind::EqualEqual:   return "==";
    case lexer::TokenKind::ExclaimEqual: return "!=";
    case lexer::TokenKind::Less:         return "<";
    case lexer::TokenKind::LessEqual:    return "<=";
    case lexer::TokenKind::Greater:      return ">";
    case lexer::TokenKind::GreaterEqual: return ">=";
    case lexer::TokenKind::AmpAmp:       return "&&";
    case lexer::TokenKind::PipePipe:     return "||";
    case lexer::TokenKind::Equal:        return "=";
    case lexer::TokenKind::PlusEqual:    return "+=";
    case lexer::TokenKind::MinusEqual:   return "-=";
    case lexer::TokenKind::StarEqual:    return "*=";
    case lexer::TokenKind::SlashEqual:   return "/=";
    case lexer::TokenKind::PercentEqual: return "%=";
    default:                             return "";
    }
}

[[nodiscard]] std::string_view getUnaryOpSpelling(lexer::TokenKind kind) noexcept {
    switch (kind) {
    case lexer::TokenKind::Exclaim:    return "!";
    case lexer::TokenKind::Minus:      return "-";
    case lexer::TokenKind::Plus:       return "+";
    case lexer::TokenKind::PlusPlus:   return "++";
    case lexer::TokenKind::MinusMinus: return "--";
    default:                           return "";
    }
}

[[nodiscard]] int getBinaryPrecedence(lexer::TokenKind kind) noexcept {
    switch (kind) {
    case lexer::TokenKind::Equal:
    case lexer::TokenKind::PlusEqual:
    case lexer::TokenKind::MinusEqual:
    case lexer::TokenKind::StarEqual:
    case lexer::TokenKind::SlashEqual:
    case lexer::TokenKind::PercentEqual:
        return 1;
    case lexer::TokenKind::Question:
        return 2;
    case lexer::TokenKind::KwZonk:
        return 3;
    case lexer::TokenKind::PipePipe:
        return 4;
    case lexer::TokenKind::AmpAmp:
        return 5;
    case lexer::TokenKind::EqualEqual:
    case lexer::TokenKind::ExclaimEqual:
        return 6;
    case lexer::TokenKind::Less:
    case lexer::TokenKind::LessEqual:
    case lexer::TokenKind::Greater:
    case lexer::TokenKind::GreaterEqual:
    case lexer::TokenKind::KwAs:
        return 7;
    case lexer::TokenKind::Plus:
    case lexer::TokenKind::Minus:
        return 8;
    case lexer::TokenKind::Star:
    case lexer::TokenKind::Slash:
    case lexer::TokenKind::Percent:
        return 9;
    default:
        return 0;
    }
}

[[nodiscard]] bool isRightAssociative(lexer::TokenKind kind) noexcept {
    switch (kind) {
    case lexer::TokenKind::Equal:
    case lexer::TokenKind::PlusEqual:
    case lexer::TokenKind::MinusEqual:
    case lexer::TokenKind::StarEqual:
    case lexer::TokenKind::SlashEqual:
    case lexer::TokenKind::PercentEqual:
    case lexer::TokenKind::Question:
        return true;
    default:
        return false;
    }
}

std::string formatImportPath(std::string_view path) {
    if (path.empty()) return "\"\"";
    if (path.front() == '"') return std::string(path);
    if (path.rfind("./", 0) == 0 || path.rfind("../", 0) == 0 ||
        path.rfind(".\\", 0) == 0 || path.rfind("..\\", 0) == 0 ||
        path.find('/') != std::string_view::npos) {
        return "\"" + std::string(path) + "\"";
    }
    return std::string(path);
}

class AstPrinter {
public:
    AstPrinter(const FormatterOptions& options, const CommentMap& commentMap)
        : options_(options), commentMap_(commentMap) {}

    std::string print(const parser::ModuleAST& moduleAst) {
        out_.clear();
        indentLevel_ = 0;

        // 1. File Header comments
        const auto& headers = commentMap_.getFileHeader();
        if (!headers.empty()) {
            for (const auto& c : headers) {
                out_ += c.text;
                out_ += "\n";
            }
            if (!moduleAst.declarations.empty()) {
                out_ += "\n";
            }
        }

        // 2. Separate imports from other declarations
        std::vector<const ast::ImportDecl*> imports;
        std::vector<const ast::Decl*> nonImports;

        for (const auto& decl : moduleAst.declarations) {
            if (const auto* imp = dynamic_cast<const ast::ImportDecl*>(decl.get())) {
                imports.push_back(imp);
            } else {
                nonImports.push_back(decl.get());
            }
        }

        // 3. Print imports
        if (!imports.empty()) {
            if (options_.sortImports) {
                auto groups = ImportSorter::groupAndSort(imports);
                bool printedAnyGroup = false;
                for (const auto& grp : groups) {
                    if (grp.empty()) continue;
                    if (printedAnyGroup) {
                        out_ += "\n"; // Blank line separator between import tiers
                    }
                    for (const auto* imp : grp) {
                        emitLeadingComments(imp);
                        emitIndent();
                        printImportDecl(imp);
                        emitTrailingComments(imp);
                        out_ += "\n";
                    }
                    printedAnyGroup = true;
                }
            } else {
                for (const auto* imp : imports) {
                    emitLeadingComments(imp);
                    emitIndent();
                    printImportDecl(imp);
                    emitTrailingComments(imp);
                    out_ += "\n";
                }
            }

            if (!nonImports.empty()) {
                out_ += "\n"; // Blank line between imports and rest of code
            }
        }

        // 4. Print non-import declarations
        for (size_t i = 0; i < nonImports.size(); ++i) {
            const auto* decl = nonImports[i];
            if (i > 0) {
                const auto* prevDecl = nonImports[i - 1];
                bool isFuncOrType = dynamic_cast<const ast::FnDecl*>(decl) ||
                                    dynamic_cast<const ast::ClassDecl*>(decl) ||
                                    dynamic_cast<const ast::EnumDecl*>(decl) ||
                                    dynamic_cast<const ast::ErrTypeDecl*>(decl);
                bool prevIsFuncOrType = dynamic_cast<const ast::FnDecl*>(prevDecl) ||
                                        dynamic_cast<const ast::ClassDecl*>(prevDecl) ||
                                        dynamic_cast<const ast::EnumDecl*>(prevDecl) ||
                                        dynamic_cast<const ast::ErrTypeDecl*>(prevDecl);
                if (isFuncOrType || prevIsFuncOrType) {
                    ensureBlankLine();
                } else if (decl->getSpan().isValid() && prevDecl->getSpan().isValid() &&
                           decl->getSpan().start.line > prevDecl->getSpan().end.line + 1) {
                    ensureBlankLine();
                }
            }
            emitLeadingComments(decl);
            emitIndent();
            printDecl(decl);
            emitTrailingComments(decl);
            out_ += "\n";
        }

        // 5. File Footer comments
        const auto& footers = commentMap_.getFileFooter();
        if (!footers.empty()) {
            if (!out_.empty() && !out_.ends_with("\n\n")) {
                ensureBlankLine();
            }
            for (const auto& c : footers) {
                out_ += c.text;
                out_ += "\n";
            }
        }

        ensureNewline();
        return out_;
    }

private:
    void indent() noexcept { ++indentLevel_; }
    void dedent() noexcept { if (indentLevel_ > 0) --indentLevel_; }

    void emitIndent() {
        out_.append(indentLevel_ * options_.indentWidth, ' ');
    }

    void ensureNewline() {
        if (!out_.empty() && out_.back() != '\n') {
            out_ += "\n";
        }
    }

    void ensureBlankLine() {
        if (out_.empty()) return;
        if (!out_.ends_with("\n")) {
            out_ += "\n\n";
        } else if (!out_.ends_with("\n\n")) {
            out_ += "\n";
        }
    }

    void emitLeadingComments(const void* nodeKey) {
        const auto& comments = commentMap_.getLeading(nodeKey);
        for (const auto& c : comments) {
            if (c.hasPrecedingBlankLine) {
                ensureBlankLine();
            }
            emitIndent();
            out_ += c.text;
            out_ += "\n";
        }
    }

    void emitTrailingComments(const void* nodeKey) {
        const auto& comments = commentMap_.getTrailing(nodeKey);
        for (const auto& c : comments) {
            out_ += " ";
            out_ += c.text;
        }
    }

    void emitDanglingComments(const void* nodeKey) {
        const auto& comments = commentMap_.getDangling(nodeKey);
        for (const auto& c : comments) {
            if (c.hasPrecedingBlankLine) {
                ensureBlankLine();
            }
            emitIndent();
            out_ += c.text;
            out_ += "\n";
        }
    }

    void printImportDecl(const ast::ImportDecl* imp) {
        if (imp->isReExported()) {
            out_ += "export ";
        }
        if (imp->isSelective()) {
            out_ += "from ";
            out_ += formatImportPath(imp->getModulePath());
            out_ += " import ";
            const auto& syms = imp->getSymbols();
            for (size_t i = 0; i < syms.size(); ++i) {
                if (i > 0) out_ += ", ";
                out_ += syms[i].originalName;
                if (!syms[i].alias.empty()) {
                    out_ += " as ";
                    out_ += syms[i].alias;
                }
            }
            out_ += ";";
        } else {
            out_ += "import ";
            out_ += formatImportPath(imp->getModulePath());
            if (!imp->getModuleAlias().empty()) {
                out_ += " as ";
                out_ += imp->getModuleAlias();
            }
            out_ += ";";
        }
    }

    void printDecl(const ast::Decl* decl) {
        if (!decl) return;

        if (const auto* fn = dynamic_cast<const ast::FnDecl*>(decl)) {
            printFnDecl(fn);
            return;
        }
        if (const auto* var = dynamic_cast<const ast::VarDecl*>(decl)) {
            printVarDecl(var);
            return;
        }
        if (const auto* cst = dynamic_cast<const ast::ConstDecl*>(decl)) {
            printConstDecl(cst);
            return;
        }
        if (const auto* def = dynamic_cast<const ast::DefineDecl*>(decl)) {
            printDefineDecl(def);
            return;
        }
        if (const auto* cls = dynamic_cast<const ast::ClassDecl*>(decl)) {
            printClassDecl(cls);
            return;
        }
        if (const auto* enm = dynamic_cast<const ast::EnumDecl*>(decl)) {
            printEnumDecl(enm);
            return;
        }
        if (const auto* err = dynamic_cast<const ast::ErrTypeDecl*>(decl)) {
            printErrTypeDecl(err);
            return;
        }
        if (const auto* exp = dynamic_cast<const ast::ExportDecl*>(decl)) {
            printExportDecl(exp);
            return;
        }
    }

    void printVarDecl(const ast::VarDecl* var) {
        if (var->isExported()) out_ += "export ";
        out_ += "let ";
        out_ += var->getName();
        if (var->getType()) {
            out_ += ": ";
            out_ += formatType(var->getType());
        }
        if (var->hasInitializer()) {
            out_ += " = ";
            out_ += formatExpr(var->getInitializer());
        }
        out_ += ";";
    }

    void printConstDecl(const ast::ConstDecl* cst) {
        if (cst->isExported()) out_ += "export ";
        out_ += "const ";
        out_ += cst->getName();
        if (cst->getType()) {
            out_ += ": ";
            out_ += formatType(cst->getType());
        }
        out_ += " = ";
        out_ += formatExpr(cst->getInitializer());
        out_ += ";";
    }

    void printDefineDecl(const ast::DefineDecl* def) {
        if (def->isExported()) out_ += "export ";
        out_ += "define ";
        out_ += def->getName();
        if (def->getType()) {
            out_ += ": ";
            out_ += formatType(def->getType());
        }
        out_ += " = ";
        out_ += formatExpr(def->getValue());
        out_ += ";";
    }

    void printExportDecl(const ast::ExportDecl* exp) {
        out_ += "export { ";
        const auto& names = exp->getExportedNames();
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) out_ += ", ";
            out_ += names[i];
        }
        out_ += " };";
    }

    void printFnDecl(const ast::FnDecl* fn, bool isMethod = false) {
        if (fn->isExported() && !isMethod) {
            out_ += "export ";
        }
        if (isMethod) {
            if (fn->getVisibility() == ast::Visibility::Pub) {
                out_ += "pub ";
            } else if (fn->getVisibility() == ast::Visibility::Priv) {
                out_ += "priv ";
            }
        }
        if (fn->isAsync()) {
            out_ += "async ";
        }
        out_ += "fn ";
        out_ += fn->getName();
        if (fn->isGeneric()) {
            out_ += "<";
            const auto& tparams = fn->getTypeParams();
            for (size_t i = 0; i < tparams.size(); ++i) {
                if (i > 0) out_ += ", ";
                out_ += tparams[i];
            }
            out_ += ">";
        }
        out_ += "(";
        printParams(fn->getParams());
        out_ += ")";
        if (fn->getReturnType()) {
            out_ += ": ";
            out_ += formatType(fn->getReturnType());
        }
        if (fn->getBody()) {
            out_ += " ";
            printBlock(dynamic_cast<const ast::BlockStmt*>(fn->getBody()));
        } else {
            out_ += ";";
        }
    }

    void printParams(const std::vector<ast::Param>& params) {
        for (size_t i = 0; i < params.size(); ++i) {
            if (i > 0) out_ += ", ";
            const auto& p = params[i];
            if (p.isMut) out_ += "mut ";
            out_ += p.name;
            if (p.type) {
                out_ += ": ";
                out_ += formatType(p.type.get());
            }
            if (p.defaultValue) {
                out_ += " = ";
                out_ += formatExpr(p.defaultValue.get());
            }
        }
    }

    void printClassDecl(const ast::ClassDecl* cls) {
        if (cls->isExported()) out_ += "export ";
        out_ += "class ";
        out_ += cls->getName();
        if (cls->hasBaseClass()) {
            out_ += " inherits ";
            out_ += cls->getBaseClass();
        }
        out_ += " {";

        bool hasMembers = !cls->getFields().empty() || !cls->getMethods().empty();
        if (!hasMembers) {
            const auto& dangling = commentMap_.getDangling(cls);
            if (!dangling.empty()) {
                out_ += "\n";
                indent();
                emitDanglingComments(cls);
                dedent();
                emitIndent();
            }
            out_ += "}";
            return;
        }

        out_ += "\n";
        indent();

        // Fields
        for (const auto& field : cls->getFields()) {
            emitLeadingComments(&field);
            emitIndent();
            if (field.visibility == ast::Visibility::Pub) {
                out_ += "pub ";
            } else if (field.visibility == ast::Visibility::Priv) {
                out_ += "priv ";
            }
            if (field.isConst) {
                out_ += "const ";
            } else {
                out_ += "let ";
            }
            out_ += field.name;
            if (field.type) {
                out_ += ": ";
                out_ += formatType(field.type.get());
            }
            if (field.initializer) {
                out_ += " = ";
                out_ += formatExpr(field.initializer.get());
            }
            out_ += ";";
            emitTrailingComments(&field);
            out_ += "\n";
        }

        // Methods
        for (size_t i = 0; i < cls->getMethods().size(); ++i) {
            const auto* method = cls->getMethods()[i].get();
            if (!cls->getFields().empty() || i > 0) {
                ensureBlankLine();
            }
            emitLeadingComments(method);
            emitIndent();
            printFnDecl(method, true);
            emitTrailingComments(method);
            out_ += "\n";
        }

        emitDanglingComments(cls);
        dedent();
        emitIndent();
        out_ += "}";
    }

    void printEnumDecl(const ast::EnumDecl* enm) {
        if (enm->isExported()) out_ += "export ";
        out_ += "enum ";
        out_ += enm->getName();
        if (enm->getUnderlyingType()) {
            out_ += ": ";
            out_ += formatType(enm->getUnderlyingType());
        }
        out_ += " {";

        if (enm->getMembers().empty()) {
            const auto& dangling = commentMap_.getDangling(enm);
            if (!dangling.empty()) {
                out_ += "\n";
                indent();
                emitDanglingComments(enm);
                dedent();
                emitIndent();
            }
            out_ += "}";
            return;
        }

        out_ += "\n";
        indent();

        for (const auto& member : enm->getMembers()) {
            emitLeadingComments(&member);
            emitIndent();
            out_ += member.name;
            if (member.value) {
                out_ += " = ";
                out_ += formatExpr(member.value.get());
            }
            out_ += ",";
            emitTrailingComments(&member);
            out_ += "\n";
        }

        emitDanglingComments(enm);
        dedent();
        emitIndent();
        out_ += "}";
    }

    void printErrTypeDecl(const ast::ErrTypeDecl* err) {
        if (err->isExported()) out_ += "export ";
        out_ += "errtype ";
        out_ += err->getName();
        if (!err->getBaseError().empty() && err->getBaseError() != "error") {
            out_ += " inherits ";
            out_ += err->getBaseError();
        }
        out_ += " {";

        if (err->getFields().empty()) {
            emitDanglingComments(err);
            out_ += "}";
            return;
        }

        out_ += "\n";
        indent();
        for (const auto& field : err->getFields()) {
            emitLeadingComments(&field);
            emitIndent();
            if (field.visibility == ast::Visibility::Pub) {
                out_ += "pub ";
            } else if (field.visibility == ast::Visibility::Priv) {
                out_ += "priv ";
            }
            if (field.isConst) {
                out_ += "const ";
            } else {
                out_ += "let ";
            }
            out_ += field.name;
            if (field.type) {
                out_ += ": ";
                out_ += formatType(field.type.get());
            }
            if (field.initializer) {
                out_ += " = ";
                out_ += formatExpr(field.initializer.get());
            }
            out_ += ";";
            emitTrailingComments(&field);
            out_ += "\n";
        }
        emitDanglingComments(err);
        dedent();
        emitIndent();
        out_ += "}";
    }

    void printBlock(const ast::BlockStmt* block) {
        if (!block) {
            out_ += "{}";
            return;
        }
        out_ += "{";

        if (block->getStatements().empty()) {
            const auto& dangling = commentMap_.getDangling(block);
            if (!dangling.empty()) {
                out_ += "\n";
                indent();
                emitDanglingComments(block);
                dedent();
                emitIndent();
            }
            out_ += "}";
            return;
        }

        out_ += "\n";
        indent();

        for (const auto& stmt : block->getStatements()) {
            emitLeadingComments(stmt.get());
            emitIndent();
            printStmt(stmt.get());
            emitTrailingComments(stmt.get());
            out_ += "\n";
        }

        emitDanglingComments(block);
        dedent();
        emitIndent();
        out_ += "}";
    }

    void printStmt(const ast::Stmt* stmt) {
        if (!stmt) return;

        if (const auto* exprStmt = dynamic_cast<const ast::ExprStmt*>(stmt)) {
            out_ += formatExpr(exprStmt->getExpression());
            out_ += ";";
            return;
        }
        if (const auto* declStmt = dynamic_cast<const ast::DeclStmt*>(stmt)) {
            printDecl(declStmt->getDeclaration());
            return;
        }
        if (const auto* ret = dynamic_cast<const ast::ReturnStmt*>(stmt)) {
            out_ += "return";
            if (ret->hasValue()) {
                out_ += " ";
                out_ += formatExpr(ret->getValue());
            }
            out_ += ";";
            return;
        }
        if (const auto* ifStmt = dynamic_cast<const ast::IfStmt*>(stmt)) {
            printIfStmt(ifStmt);
            return;
        }
        if (const auto* whileStmt = dynamic_cast<const ast::WhileStmt*>(stmt)) {
            printWhileStmt(whileStmt);
            return;
        }
        if (const auto* forStmt = dynamic_cast<const ast::ForStmt*>(stmt)) {
            printForStmt(forStmt);
            return;
        }
        if (const auto* forIn = dynamic_cast<const ast::ForInStmt*>(stmt)) {
            printForInStmt(forIn);
            return;
        }
        if (const auto* sw = dynamic_cast<const ast::SwitchStmt*>(stmt)) {
            printSwitchStmt(sw);
            return;
        }
        if (const auto* brk = dynamic_cast<const ast::BreakStmt*>(stmt)) {
            out_ += "break";
            if (brk->hasTargetLabel()) {
                out_ += " ";
                out_ += brk->getTargetLabel();
            }
            out_ += ";";
            return;
        }
        if (const auto* cont = dynamic_cast<const ast::ContinueStmt*>(stmt)) {
            out_ += "continue";
            if (cont->hasTargetLabel()) {
                out_ += " ";
                out_ += cont->getTargetLabel();
            }
            out_ += ";";
            return;
        }
        if (const auto* yld = dynamic_cast<const ast::YieldStmt*>(stmt)) {
            out_ += "yield ";
            out_ += formatExpr(yld->getValue());
            out_ += ";";
            return;
        }
        if (const auto* znk = dynamic_cast<const ast::ZonkStmt*>(stmt)) {
            out_ += "zonk ";
            out_ += formatExpr(znk->getErrorExpr());
            out_ += ";";
            return;
        }
        if (const auto* blk = dynamic_cast<const ast::BlockStmt*>(stmt)) {
            printBlock(blk);
            return;
        }
    }

    void printIfStmt(const ast::IfStmt* ifStmt) {
        out_ += "if (";
        if (ifStmt->hasInit()) {
            printInitStmt(ifStmt->getInit());
            out_ += " ";
        }
        out_ += formatExpr(ifStmt->getCondition());
        out_ += ") ";
        printBlock(ifStmt->getThenBranch());

        const ast::Stmt* elseBranch = ifStmt->getElseBranch();
        while (elseBranch) {
            if (const auto* elif = dynamic_cast<const ast::IfStmt*>(elseBranch)) {
                out_ += " elif (";
                if (elif->hasInit()) {
                    printInitStmt(elif->getInit());
                    out_ += " ";
                }
                out_ += formatExpr(elif->getCondition());
                out_ += ") ";
                printBlock(elif->getThenBranch());
                elseBranch = elif->getElseBranch();
            } else if (const auto* block = dynamic_cast<const ast::BlockStmt*>(elseBranch)) {
                out_ += " else ";
                printBlock(block);
                break;
            } else {
                break;
            }
        }
    }

    void printWhileStmt(const ast::WhileStmt* whileStmt) {
        out_ += "while (";
        if (whileStmt->hasInit()) {
            printInitStmt(whileStmt->getInit());
            out_ += " ";
        }
        out_ += formatExpr(whileStmt->getCondition());
        out_ += ")";
        if (whileStmt->hasLabel()) {
            out_ += " as ";
            out_ += whileStmt->getLabel();
        }
        out_ += " ";
        printBlock(whileStmt->getBody());
    }

    void printForStmt(const ast::ForStmt* forStmt) {
        out_ += "for (";
        if (forStmt->getInit()) {
            printInitStmt(forStmt->getInit());
            out_ += " ";
        } else {
            out_ += "; ";
        }
        if (forStmt->getCondition()) {
            out_ += formatExpr(forStmt->getCondition());
        }
        out_ += ";";
        if (forStmt->getPost()) {
            out_ += " ";
            if (const auto* es = dynamic_cast<const ast::ExprStmt*>(forStmt->getPost())) {
                out_ += formatExpr(es->getExpression());
            } else {
                printInitStmt(forStmt->getPost(), false);
            }
        }
        out_ += ")";
        if (forStmt->hasLabel()) {
            out_ += " as ";
            out_ += forStmt->getLabel();
        }
        out_ += " ";
        printBlock(forStmt->getBody());
    }

    void printForInStmt(const ast::ForInStmt* forIn) {
        out_ += "for (";
        out_ += forIn->isConst() ? "const " : "let ";
        if (forIn->isKeyValue()) {
            out_ += forIn->getKeyName();
            out_ += ", ";
            out_ += forIn->getValueName();
        } else {
            out_ += forIn->getValueName();
        }
        out_ += " in ";
        out_ += formatExpr(forIn->getCollection());
        out_ += ")";
        if (forIn->hasLabel()) {
            out_ += " as ";
            out_ += forIn->getLabel();
        }
        out_ += " ";
        printBlock(forIn->getBody());
    }

    void printSwitchStmt(const ast::SwitchStmt* sw) {
        out_ += "switch (";
        if (sw->hasInit()) {
            printInitStmt(sw->getInit());
            out_ += " ";
        }
        out_ += formatExpr(sw->getDiscriminant());
        out_ += ")";
        if (sw->hasLabel()) {
            out_ += " as ";
            out_ += sw->getLabel();
        }
        out_ += " {";

        if (sw->getCases().empty()) {
            emitDanglingComments(sw);
            out_ += "}";
            return;
        }

        out_ += "\n";
        indent();

        for (const auto& c : sw->getCases()) {
            emitLeadingComments(&c);
            emitIndent();
            if (c.isDefault()) {
                out_ += "default ";
            } else {
                out_ += "case (";
                for (size_t i = 0; i < c.values.size(); ++i) {
                    if (i > 0) out_ += ", ";
                    out_ += formatExpr(c.values[i].get());
                }
                out_ += ") ";
            }
            printBlock(c.body.get());
            emitTrailingComments(&c);
            out_ += "\n";
        }

        emitDanglingComments(sw);
        dedent();
        emitIndent();
        out_ += "}";
    }

    void printInitStmt(const ast::Stmt* stmt, bool includeSemicolon = true) {
        if (!stmt) return;
        if (const auto* declStmt = dynamic_cast<const ast::DeclStmt*>(stmt)) {
            if (const auto* var = dynamic_cast<const ast::VarDecl*>(declStmt->getDeclaration())) {
                out_ += "let ";
                out_ += var->getName();
                if (var->getType()) {
                    out_ += ": ";
                    out_ += formatType(var->getType());
                }
                if (var->hasInitializer()) {
                    out_ += " = ";
                    out_ += formatExpr(var->getInitializer());
                }
                if (includeSemicolon) out_ += ";";
                return;
            }
        }
        if (const auto* exprStmt = dynamic_cast<const ast::ExprStmt*>(stmt)) {
            out_ += formatExpr(exprStmt->getExpression());
            if (includeSemicolon) out_ += ";";
            return;
        }
    }

    std::string formatExpr(const ast::Expr* expr, int parentPrecedence = 0, bool isRightSide = false) {
        if (!expr) return "";

        if (const auto* lit = dynamic_cast<const ast::LiteralExpr*>(expr)) {
            return lit->getRawText();
        }
        if (const auto* id = dynamic_cast<const ast::IdentExpr*>(expr)) {
            return id->getName();
        }
        if (const auto* bin = dynamic_cast<const ast::BinaryExpr*>(expr)) {
            return formatBinaryExpr(bin, parentPrecedence, isRightSide);
        }
        if (const auto* un = dynamic_cast<const ast::UnaryExpr*>(expr)) {
            std::string opStr = std::string(getUnaryOpSpelling(un->getOp()));
            if (un->isPrefix()) {
                return opStr + formatExpr(un->getOperand(), 15);
            }
            return formatExpr(un->getOperand(), 15) + opStr;
        }
        if (const auto* call = dynamic_cast<const ast::CallExpr*>(expr)) {
            std::string res = formatExpr(call->getCallee(), 15);
            if (!call->getTypeArgs().empty()) {
                res += "<";
                for (size_t i = 0; i < call->getTypeArgs().size(); ++i) {
                    if (i > 0) res += ", ";
                    res += formatType(call->getTypeArgs()[i].get());
                }
                res += ">";
            }
            res += "(";
            const auto& args = call->getArgs();
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) res += ", ";
                if (args[i].isMut) res += "mut ";
                res += formatExpr(args[i].value.get());
            }
            res += ")";
            return res;
        }
        if (const auto* idx = dynamic_cast<const ast::IndexExpr*>(expr)) {
            return formatExpr(idx->getContainer(), 15) + "[" + formatExpr(idx->getIndex()) + "]";
        }
        if (const auto* mem = dynamic_cast<const ast::MemberExpr*>(expr)) {
            return formatExpr(mem->getObject(), 15) + "." + mem->getMemberName();
        }
        if (const auto* znk = dynamic_cast<const ast::ZonkExpr*>(expr)) {
            if (znk->hasRecoveryBlock()) {
                AstPrinter subPrinter(options_, commentMap_);
                subPrinter.indentLevel_ = indentLevel_;
                std::string res = formatExpr(znk->getPrimary(), 3) + " zonk (" + znk->getErrorVarName() + ") ";
                subPrinter.printBlock(znk->getRecoveryBlock());
                return res + subPrinter.out_;
            }
            return formatExpr(znk->getPrimary(), 3) + " zonk (" + formatExpr(znk->getFallback()) + ")";
        }
        if (const auto* tern = dynamic_cast<const ast::TernaryExpr*>(expr)) {
            std::string res = formatExpr(tern->getCondition(), 3) + " ? " +
                              formatExpr(tern->getThenExpr()) + " : " +
                              formatExpr(tern->getElseExpr(), 2, true);
            if (parentPrecedence > 2) {
                res = "(" + res + ")";
            }
            return res;
        }
        if (const auto* cast = dynamic_cast<const ast::TypeCastExpr*>(expr)) {
            if (cast->isAssertion()) {
                std::string res = formatExpr(cast->getExpr(), 7) + " as " + formatType(cast->getTargetType());
                if (parentPrecedence > 7) {
                    res = "(" + res + ")";
                }
                return res;
            }
            return formatType(cast->getTargetType()) + "(" + formatExpr(cast->getExpr()) + ")";
        }
        if (const auto* tof = dynamic_cast<const ast::TypeofExpr*>(expr)) {
            return "typeof(" + formatExpr(tof->getExpr()) + ")";
        }
        if (const auto* arr = dynamic_cast<const ast::ArrayLitExpr*>(expr)) {
            return formatArrayLit(arr);
        }
        if (const auto* mp = dynamic_cast<const ast::MapLitExpr*>(expr)) {
            return formatMapLit(mp);
        }
        if (const auto* cls = dynamic_cast<const ast::ClosureExpr*>(expr)) {
            AstPrinter subPrinter(options_, commentMap_);
            subPrinter.indentLevel_ = indentLevel_;
            std::string res;
            if (cls->isAsync()) res += "async ";
            res += "fn(";
            for (size_t i = 0; i < cls->getParams().size(); ++i) {
                if (i > 0) res += ", ";
                const auto& p = cls->getParams()[i];
                if (p.isMut) res += "mut ";
                res += p.name;
                if (p.type) {
                    res += ": ";
                    res += formatType(p.type.get());
                }
            }
            res += ")";
            if (cls->getReturnType()) {
                res += ": ";
                res += formatType(cls->getReturnType());
            }
            res += " ";
            subPrinter.printBlock(cls->getBody());
            return res + subPrinter.out_;
        }
        if (const auto* awt = dynamic_cast<const ast::AwaitExpr*>(expr)) {
            return "await " + formatExpr(awt->getTarget(), 15);
        }
        if (const auto* tpl = dynamic_cast<const ast::TemplateLitExpr*>(expr)) {
            std::string res = "`";
            for (const auto& part : tpl->getParts()) {
                if (part.isExpr) {
                    res += "${" + formatExpr(part.expr.get()) + "}";
                } else {
                    res += part.text;
                }
            }
            res += "`";
            return res;
        }

        return "";
    }

    std::string formatBinaryExpr(const ast::BinaryExpr* bin, int parentPrecedence, bool isRightSide) {
        int prec = getBinaryPrecedence(bin->getOp());
        std::string_view opSpelling = getBinaryOpSpelling(bin->getOp());

        bool needParens = false;
        if (parentPrecedence > 0) {
            if (prec < parentPrecedence) {
                needParens = true;
            } else if (prec == parentPrecedence) {
                if (isRightSide && !isRightAssociative(bin->getOp())) {
                    needParens = true;
                }
            }
        }

        std::string leftStr = formatExpr(bin->getLeft(), prec, false);
        std::string rightStr = formatExpr(bin->getRight(), prec, true);

        std::string res = leftStr + " " + std::string(opSpelling) + " " + rightStr;
        if (needParens) {
            res = "(" + res + ")";
        }
        return res;
    }

    std::string formatArrayLit(const ast::ArrayLitExpr* arr) {
        const auto& elems = arr->getElements();
        if (elems.empty()) return "[]";

        std::string single = "[";
        for (size_t i = 0; i < elems.size(); ++i) {
            if (i > 0) single += ", ";
            single += formatExpr(elems[i].get());
        }
        single += "]";

        if (single.find('\n') == std::string::npos &&
            (indentLevel_ * options_.indentWidth + single.size()) <= options_.maxLineWidth) {
            return single;
        }

        // Multi-line formatting
        std::string multi = "[\n";
        int innerIndent = indentLevel_ + 1;
        std::string pad(innerIndent * options_.indentWidth, ' ');
        for (const auto& elem : elems) {
            multi += pad + formatExpr(elem.get()) + ",\n";
        }
        multi += std::string(indentLevel_ * options_.indentWidth, ' ') + "]";
        return multi;
    }

    std::string formatMapLit(const ast::MapLitExpr* mp) {
        const auto& entries = mp->getEntries();
        if (entries.empty()) return "{}";

        std::string single = "{";
        for (size_t i = 0; i < entries.size(); ++i) {
            if (i > 0) single += ", ";
            single += formatExpr(entries[i].key.get()) + ": " + formatExpr(entries[i].value.get());
        }
        single += "}";

        if (single.find('\n') == std::string::npos &&
            (indentLevel_ * options_.indentWidth + single.size()) <= options_.maxLineWidth) {
            return single;
        }

        // Multi-line formatting
        std::string multi = "{\n";
        int innerIndent = indentLevel_ + 1;
        std::string pad(innerIndent * options_.indentWidth, ' ');
        for (const auto& entry : entries) {
            multi += pad + formatExpr(entry.key.get()) + ": " + formatExpr(entry.value.get()) + ",\n";
        }
        multi += std::string(indentLevel_ * options_.indentWidth, ' ') + "}";
        return multi;
    }

    std::string formatType(const ast::TypeAnnotation* type) {
        if (!type) return "void";

        if (const auto* prim = dynamic_cast<const ast::PrimitiveType*>(type)) {
            return std::string(prim->getTypeName());
        }
        if (const auto* arr = dynamic_cast<const ast::ArrayType*>(type)) {
            return "array<" + formatType(arr->getElementType()) + ">";
        }
        if (const auto* mp = dynamic_cast<const ast::MapType*>(type)) {
            return "map<" + formatType(mp->getKeyType()) + ", " + formatType(mp->getValueType()) + ">";
        }
        if (const auto* un = dynamic_cast<const ast::UnionType*>(type)) {
            std::string res;
            for (size_t i = 0; i < un->getVariants().size(); ++i) {
                if (i > 0) res += " | ";
                res += formatType(un->getVariants()[i].get());
            }
            return res;
        }
        if (const auto* fn = dynamic_cast<const ast::FunctionType*>(type)) {
            std::string res;
            if (fn->isAsync()) res += "async ";
            res += "(";
            for (size_t i = 0; i < fn->getParams().size(); ++i) {
                if (i > 0) res += ", ";
                const auto& p = fn->getParams()[i];
                if (p.isMut) res += "mut ";
                if (!p.name.empty()) res += p.name + ": ";
                res += formatType(p.type.get());
            }
            res += ") => " + formatType(fn->getReturnType());
            return res;
        }
        if (const auto* cust = dynamic_cast<const ast::CustomType*>(type)) {
            std::string res;
            if (cust->isQualified()) {
                res += cust->getModuleQualifier() + ".";
            }
            res += cust->getName();
            if (cust->isGeneric()) {
                res += "<";
                for (size_t i = 0; i < cust->getTypeArgs().size(); ++i) {
                    if (i > 0) res += ", ";
                    res += formatType(cust->getTypeArgs()[i].get());
                }
                res += ">";
            }
            return res;
        }
        if (const auto* lit = dynamic_cast<const ast::LiteralType*>(type)) {
            return lit->getRawText();
        }

        return "unknown";
    }

    const FormatterOptions& options_;
    const CommentMap& commentMap_;
    std::string out_;
    int indentLevel_{0};
};

} // namespace

std::string Formatter::format(
    std::string_view source,
    std::string_view filename,
    diag::DiagnosticEmitter* emitter
) {
    diag::SourceManager sm;
    diag::FileId fileId = sm.addBuffer(std::string(filename), std::string(source));

    lexer::Lexer lexer(fileId, source, emitter);
    lexer.setCaptureComments(true);

    parser::Parser parser(lexer, emitter);
    parser::ModuleAST moduleAst = parser.parseModule(std::string(filename));

    const auto& comments = lexer.getCapturedComments();
    CommentMap commentMap = CommentAttacher::attach(moduleAst, comments);

    return formatModule(moduleAst, commentMap);
}

std::string Formatter::formatModule(
    const parser::ModuleAST& moduleAst,
    const CommentMap& commentMap
) {
    AstPrinter printer(options_, commentMap);
    return printer.print(moduleAst);
}

std::string Formatter::generateDiff(
    std::string_view original,
    std::string_view formatted,
    std::string_view filepath
) {
    if (original == formatted) return "";

    std::vector<std::string> origLines;
    std::vector<std::string> fmtLines;

    auto splitLines = [](std::string_view text) {
        std::vector<std::string> lines;
        std::string current;
        for (char c : text) {
            if (c == '\r') continue;
            if (c == '\n') {
                lines.push_back(std::move(current));
                current.clear();
            } else {
                current += c;
            }
        }
        if (!current.empty() || (!text.empty() && text.back() != '\n')) {
            lines.push_back(std::move(current));
        }
        return lines;
    };

    origLines = splitLines(original);
    fmtLines = splitLines(formatted);

    std::ostringstream diff;
    diff << "--- a/" << filepath << "\n";
    diff << "+++ b/" << filepath << "\n";
    diff << "@@ -1," << origLines.size() << " +1," << fmtLines.size() << " @@\n";

    // Simple line diff
    size_t i = 0;
    size_t j = 0;
    while (i < origLines.size() && j < fmtLines.size()) {
        if (origLines[i] == fmtLines[j]) {
            diff << " " << origLines[i] << "\n";
            ++i;
            ++j;
        } else {
            diff << "-" << origLines[i] << "\n";
            diff << "+" << fmtLines[j] << "\n";
            ++i;
            ++j;
        }
    }
    while (i < origLines.size()) {
        diff << "-" << origLines[i++] << "\n";
    }
    while (j < fmtLines.size()) {
        diff << "+" << fmtLines[j++] << "\n";
    }

    return diff.str();
}

} // namespace keety::fmt
