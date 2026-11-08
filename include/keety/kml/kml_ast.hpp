#pragma once

#include <keety/ast/decl.hpp>
#include <keety/ast/expr.hpp>
#include <keety/ast/stmt.hpp>
#include <keety/diag/source_location.hpp>
#include <keety/kml/kml_token.hpp>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace keety::kml {

// ============================================================================
// Script AST Representation
// ============================================================================

struct KmlPropDecl {
    std::string name;
    std::string type; // Type name string or KeetScript type AST representation
    std::string defaultValueExpr; // Optional default expression
    diag::SourceSpan span;
};

struct KmlStateRune {
    enum class Kind {
        State,      // $state(...)
        Derived,    // $derived(...)
        Effect      // $effect(...)
    };

    Kind kind{Kind::State};
    std::string variableName;
    std::string initExpr;
    diag::SourceSpan span;
};

struct KmlScriptBlock {
    diag::SourceSpan span;
    std::string rawCode; // Raw code string parsed into KeetScript declarations
    std::vector<std::unique_ptr<ast::Decl>> declarations; // Parsed KeetScript AST declarations
    std::vector<KmlPropDecl> props;                       // Extracted props declarations
    std::vector<KmlStateRune> runes;                      // Extracted $state, $derived, $effect runes
};

// ============================================================================
// Template AST Representation
// ============================================================================

enum class KmlNodeKind {
    Element,
    Text,
    IfBlock,
    ForBlock,
    AwaitBlock,
    Slot
};

/// Base class for all nodes in a KML template tree
class KmlTemplateNode {
public:
    explicit KmlTemplateNode(KmlNodeKind kind, diag::SourceSpan span)
        : kind_(kind), span_(span) {}
    virtual ~KmlTemplateNode() = default;

    [[nodiscard]] KmlNodeKind getKind() const noexcept { return kind_; }
    [[nodiscard]] diag::SourceSpan getSpan() const noexcept { return span_; }

private:
    KmlNodeKind kind_;
    diag::SourceSpan span_;
};

/// Directives: @event, @event.modifier, $bind.property
struct KmlDirective {
    std::string name;             // e.g. "click", "keydown", "bind"
    std::vector<std::string> modifiers; // e.g. ["debounce(150)", "prevent"]
    std::string targetProperty;   // e.g. "value" for $bind.value
    std::string expression;       // handler or state expression
    diag::SourceSpan span;
};

/// Attributes: key="value" or key={expr}
struct KmlAttribute {
    std::string name;
    std::string value;            // Static string value or expression body
    bool isExpression{false};     // True if attribute is dynamic `{expr}`
    diag::SourceSpan span;
};

/// Element node: standard HTML/TUI tags (<box>, <vstack>, <button>, <input>) or custom component (<UserProfile>)
class KmlElementNode : public KmlTemplateNode {
public:
    KmlElementNode(std::string tagName, bool isComponent, bool selfClosing, diag::SourceSpan span)
        : KmlTemplateNode(KmlNodeKind::Element, span),
          tagName_(std::move(tagName)),
          isComponent_(isComponent),
          selfClosing_(selfClosing) {}

    [[nodiscard]] const std::string& getTagName() const noexcept { return tagName_; }
    [[nodiscard]] bool isComponent() const noexcept { return isComponent_; }
    [[nodiscard]] bool isSelfClosing() const noexcept { return selfClosing_; }

    void addAttribute(KmlAttribute attr) { attributes_.push_back(std::move(attr)); }
    [[nodiscard]] const std::vector<KmlAttribute>& getAttributes() const noexcept { return attributes_; }
    [[nodiscard]] std::vector<KmlAttribute>& getAttributes() noexcept { return attributes_; }

    void addDirective(KmlDirective dir) { directives_.push_back(std::move(dir)); }
    [[nodiscard]] const std::vector<KmlDirective>& getDirectives() const noexcept { return directives_; }
    [[nodiscard]] std::vector<KmlDirective>& getDirectives() noexcept { return directives_; }

    void addChild(std::unique_ptr<KmlTemplateNode> child) { children_.push_back(std::move(child)); }
    [[nodiscard]] const std::vector<std::unique_ptr<KmlTemplateNode>>& getChildren() const noexcept { return children_; }
    [[nodiscard]] std::vector<std::unique_ptr<KmlTemplateNode>>& getChildren() noexcept { return children_; }

private:
    std::string tagName_;
    bool isComponent_{false};
    bool selfClosing_{false};
    std::vector<KmlAttribute> attributes_;
    std::vector<KmlDirective> directives_;
    std::vector<std::unique_ptr<KmlTemplateNode>> children_;
};

/// Static text runs and interpolated `{expr}` expressions
class KmlTextNode : public KmlTemplateNode {
public:
    KmlTextNode(std::string text, bool isInterpolated, diag::SourceSpan span)
        : KmlTemplateNode(KmlNodeKind::Text, span),
          text_(std::move(text)),
          isInterpolated_(isInterpolated) {}

    [[nodiscard]] const std::string& getText() const noexcept { return text_; }
    [[nodiscard]] bool isInterpolated() const noexcept { return isInterpolated_; }

private:
    std::string text_;
    bool isInterpolated_{false};
};

/// `{#if condition}` ... `{:else if other}` ... `{:else}` ... `{/if}`
class KmlIfBlock : public KmlTemplateNode {
public:
    struct ElseIfBranch {
        std::string condition;
        std::vector<std::unique_ptr<KmlTemplateNode>> body;
        diag::SourceSpan span;
    };

    KmlIfBlock(std::string condition, diag::SourceSpan span)
        : KmlTemplateNode(KmlNodeKind::IfBlock, span),
          condition_(std::move(condition)) {}

    [[nodiscard]] const std::string& getCondition() const noexcept { return condition_; }

    void addThenChild(std::unique_ptr<KmlTemplateNode> child) { thenBranch_.push_back(std::move(child)); }
    [[nodiscard]] const std::vector<std::unique_ptr<KmlTemplateNode>>& getThenBranch() const noexcept { return thenBranch_; }

    void addElseIfBranch(ElseIfBranch branch) { elseIfBranches_.push_back(std::move(branch)); }
    [[nodiscard]] const std::vector<ElseIfBranch>& getElseIfBranches() const noexcept { return elseIfBranches_; }

    void addElseChild(std::unique_ptr<KmlTemplateNode> child) { elseBranch_.push_back(std::move(child)); }
    [[nodiscard]] const std::vector<std::unique_ptr<KmlTemplateNode>>& getElseBranch() const noexcept { return elseBranch_; }

private:
    std::string condition_;
    std::vector<std::unique_ptr<KmlTemplateNode>> thenBranch_;
    std::vector<ElseIfBranch> elseIfBranches_;
    std::vector<std::unique_ptr<KmlTemplateNode>> elseBranch_;
};

/// `{#for (item, index in items; key=item.id)}` ... `{/for}`
class KmlForBlock : public KmlTemplateNode {
public:
    KmlForBlock(std::string itemVar,
                std::optional<std::string> indexVar,
                std::string iterableExpr,
                std::optional<std::string> keyExpr,
                diag::SourceSpan span)
        : KmlTemplateNode(KmlNodeKind::ForBlock, span),
          itemVar_(std::move(itemVar)),
          indexVar_(std::move(indexVar)),
          iterableExpr_(std::move(iterableExpr)),
          keyExpr_(std::move(keyExpr)) {}

    [[nodiscard]] const std::string& getItemVar() const noexcept { return itemVar_; }
    [[nodiscard]] const std::optional<std::string>& getIndexVar() const noexcept { return indexVar_; }
    [[nodiscard]] const std::string& getIterableExpr() const noexcept { return iterableExpr_; }
    [[nodiscard]] const std::optional<std::string>& getKeyExpr() const noexcept { return keyExpr_; }

    void addChild(std::unique_ptr<KmlTemplateNode> child) { body_.push_back(std::move(child)); }
    [[nodiscard]] const std::vector<std::unique_ptr<KmlTemplateNode>>& getBody() const noexcept { return body_; }

private:
    std::string itemVar_;
    std::optional<std::string> indexVar_;
    std::string iterableExpr_;
    std::optional<std::string> keyExpr_;
    std::vector<std::unique_ptr<KmlTemplateNode>> body_;
};

/// `{#await promise}` ... `{:then result}` ... `{:catch error}` ... `{/await}`
class KmlAwaitBlock : public KmlTemplateNode {
public:
    KmlAwaitBlock(std::string promiseExpr, diag::SourceSpan span)
        : KmlTemplateNode(KmlNodeKind::AwaitBlock, span),
          promiseExpr_(std::move(promiseExpr)) {}

    [[nodiscard]] const std::string& getPromiseExpr() const noexcept { return promiseExpr_; }

    void addPendingChild(std::unique_ptr<KmlTemplateNode> child) { pendingBranch_.push_back(std::move(child)); }
    [[nodiscard]] const std::vector<std::unique_ptr<KmlTemplateNode>>& getPendingBranch() const noexcept { return pendingBranch_; }

    void setThenVariable(std::string var) { thenVar_ = std::move(var); }
    [[nodiscard]] const std::optional<std::string>& getThenVar() const noexcept { return thenVar_; }
    void addThenChild(std::unique_ptr<KmlTemplateNode> child) { thenBranch_.push_back(std::move(child)); }
    [[nodiscard]] const std::vector<std::unique_ptr<KmlTemplateNode>>& getThenBranch() const noexcept { return thenBranch_; }

    void setCatchVariable(std::string var) { catchVar_ = std::move(var); }
    [[nodiscard]] const std::optional<std::string>& getCatchVar() const noexcept { return catchVar_; }
    void addCatchChild(std::unique_ptr<KmlTemplateNode> child) { catchBranch_.push_back(std::move(child)); }
    [[nodiscard]] const std::vector<std::unique_ptr<KmlTemplateNode>>& getCatchBranch() const noexcept { return catchBranch_; }

private:
    std::string promiseExpr_;
    std::vector<std::unique_ptr<KmlTemplateNode>> pendingBranch_;
    std::optional<std::string> thenVar_;
    std::vector<std::unique_ptr<KmlTemplateNode>> thenBranch_;
    std::optional<std::string> catchVar_;
    std::vector<std::unique_ptr<KmlTemplateNode>> catchBranch_;
};

/// `<slot>` projection point: default and named slots
class KmlSlotNode : public KmlTemplateNode {
public:
    KmlSlotNode(std::optional<std::string> slotName, diag::SourceSpan span)
        : KmlTemplateNode(KmlNodeKind::Slot, span),
          slotName_(std::move(slotName)) {}

    [[nodiscard]] const std::optional<std::string>& getSlotName() const noexcept { return slotName_; }

    void addFallbackChild(std::unique_ptr<KmlTemplateNode> child) { fallback_.push_back(std::move(child)); }
    [[nodiscard]] const std::vector<std::unique_ptr<KmlTemplateNode>>& getFallback() const noexcept { return fallback_; }

private:
    std::optional<std::string> slotName_;
    std::vector<std::unique_ptr<KmlTemplateNode>> fallback_;
};

// ============================================================================
// Style AST Representation
// ============================================================================

struct KmlStyleDeclaration {
    std::string property;
    std::string value;
    diag::SourceSpan span;
};

struct KmlStyleRule {
    std::string selector;
    std::vector<KmlStyleDeclaration> declarations;
    diag::SourceSpan span;
};

struct KmlStyleBlock {
    diag::SourceSpan span;
    std::string rawCss;
    std::vector<KmlStyleRule> rules;
};

// ============================================================================
// Document Representation
// ============================================================================

/// Top-level KML Single-File Component Document AST
struct KmlDocument {
    diag::FileId fileId{diag::INVALID_FILE_ID};
    std::string componentName;
    std::optional<KmlScriptBlock> script;
    std::vector<std::unique_ptr<KmlTemplateNode>> templateNodes;
    std::optional<KmlStyleBlock> style;
};

} // namespace keety::kml

