#pragma once

#include <keet/ui/component.hpp>
#include <keet/ui/node.hpp>

#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace keet::ui {

/// Dual-layer headless test driver for querying layout trees, inspecting DOM states,
/// and dispatching synthetic UI events in unit tests.
class HeadlessBackend {
public:
    explicit HeadlessBackend(std::shared_ptr<Component> rootComponent)
        : component_(std::move(rootComponent)) {
        if (component_) {
            rootNode_ = component_->mount();
            component_->onMount();
            component_->flushUpdate(); // flush initial dirtyMask
        }
    }

    /// Mounts and returns root node
    [[nodiscard]] std::shared_ptr<UINode> getRootNode() const noexcept {
        return rootNode_;
    }

    // --- Query Engine ---

    /// Find node by unique numeric ID
    std::shared_ptr<UINode> findById(uint32_t id) const {
        return rootNode_ ? rootNode_->findById(id) : nullptr;
    }

    /// Find first node containing specific text (searches TextNode or ButtonNode)
    std::shared_ptr<UINode> findByText(const std::string& text) const {
        return searchByTextRecursive(rootNode_, text);
    }

    /// Find node by tag or attribute selector (e.g. "button", "input", ".class", "#id")
    std::shared_ptr<UINode> find(const std::string& selector) const {
        return searchBySelectorRecursive(rootNode_, selector);
    }

    /// Find all nodes matching selector
    std::vector<std::shared_ptr<UINode>> findAll(const std::string& selector) const {
        std::vector<std::shared_ptr<UINode>> results;
        searchAllRecursive(rootNode_, selector, results);
        return results;
    }

    // --- Synthetic Event Simulator ---

    /// Simulate user click on target node ID (triggers "@click")
    bool simulateClick(uint32_t nodeId) {
        auto node = findById(nodeId);
        if (!node) return false;
        node->emit("click");
        tick(); // Flush microtask updates
        return true;
    }

    /// Simulate user typing into InputNode (updates value and triggers "@input" / "@change")
    bool simulateType(uint32_t nodeId, const std::string& text) {
        auto node = findById(nodeId);
        if (!node) return false;
        if (node->getKind() == NodeKind::Input) {
            auto inputNode = std::static_pointer_cast<InputNode>(node);
            inputNode->setValue(text);
        } else {
            node->emit("input", text);
        }
        tick();
        return true;
    }

    /// Simulate key press (e.g. "Enter", "Tab", "Escape")
    bool simulateKey(uint32_t nodeId, const std::string& key) {
        auto node = findById(nodeId);
        if (!node) return false;
        node->emit("keydown", key);
        if (key == "Enter" && (node->getKind() == NodeKind::Button)) {
            node->emit("click");
        }
        tick();
        return true;
    }

    // --- Snapshot Testing ---

    /// Generates structured ASCII/indented snapshot string of current DOM tree
    std::string snapshot() const {
        std::stringstream ss;
        renderSnapshotRecursive(rootNode_, ss, 0);
        return ss.str();
    }

private:
    static std::shared_ptr<UINode> searchByTextRecursive(const std::shared_ptr<UINode>& node, const std::string& text) {
        if (!node) return nullptr;

        if (node->getKind() == NodeKind::Text) {
            auto t = std::static_pointer_cast<TextNode>(node);
            if (t->getContent().find(text) != std::string::npos) return node;
        } else if (node->getKind() == NodeKind::Button) {
            auto b = std::static_pointer_cast<ButtonNode>(node);
            if (b->getLabel().find(text) != std::string::npos) return node;
        }

        for (const auto& child : node->getChildren()) {
            if (auto found = searchByTextRecursive(child, text)) return found;
        }
        return nullptr;
    }

    static bool matchesSelector(const std::shared_ptr<UINode>& node, const std::string& selector) {
        if (!node || selector.empty()) return false;

        // Class selector: .btn
        if (selector.front() == '.') {
            std::string cls = selector.substr(1);
            return node->getAttribute("class") == cls;
        }

        // Id selector: #123
        if (selector.front() == '#') {
            std::string idStr = selector.substr(1);
            return std::to_string(node->getId()) == idStr;
        }

        // Tag name selector
        switch (node->getKind()) {
            case NodeKind::Box: return selector == "box" || selector == "div";
            case NodeKind::Text: return selector == "text" || selector == "span" || selector == "p";
            case NodeKind::Button: return selector == "button";
            case NodeKind::Input: return selector == "input";
            case NodeKind::Scroll: return selector == "scroll";
            case NodeKind::Stack: return selector == "stack" || selector == "vstack" || selector == "hstack";
            case NodeKind::Custom: return true;
        }
        return false;
    }

    static std::shared_ptr<UINode> searchBySelectorRecursive(const std::shared_ptr<UINode>& node, const std::string& selector) {
        if (!node) return nullptr;
        if (matchesSelector(node, selector)) return node;

        for (const auto& child : node->getChildren()) {
            if (auto found = searchBySelectorRecursive(child, selector)) return found;
        }
        return nullptr;
    }

    static void searchAllRecursive(const std::shared_ptr<UINode>& node, const std::string& selector, std::vector<std::shared_ptr<UINode>>& out) {
        if (!node) return;
        if (matchesSelector(node, selector)) {
            out.push_back(node);
        }
        for (const auto& child : node->getChildren()) {
            searchAllRecursive(child, selector, out);
        }
    }

    static void renderSnapshotRecursive(const std::shared_ptr<UINode>& node, std::stringstream& ss, int indent) {
        if (!node) return;
        std::string spaces(indent * 2, ' ');
        ss << spaces << "<";

        switch (node->getKind()) {
            case NodeKind::Box: ss << "box"; break;
            case NodeKind::Text: ss << "text"; break;
            case NodeKind::Button: ss << "button"; break;
            case NodeKind::Input: ss << "input"; break;
            case NodeKind::Stack: ss << "stack"; break;
            case NodeKind::Scroll: ss << "scroll"; break;
            case NodeKind::Custom: ss << "custom"; break;
        }

        if (node->getId() > 0) ss << " id=\"" << node->getId() << "\"";
        if (node->hasAttribute("class")) ss << " class=\"" << node->getAttribute("class") << "\"";

        if (node->getKind() == NodeKind::Text) {
            auto t = std::static_pointer_cast<TextNode>(node);
            ss << ">" << t->getContent() << "</text>\n";
            return;
        }

        if (node->getKind() == NodeKind::Button) {
            auto b = std::static_pointer_cast<ButtonNode>(node);
            if (!b->getLabel().empty()) {
                ss << ">" << b->getLabel() << "</button>\n";
                return;
            }
        }

        if (node->getKind() == NodeKind::Input) {
            auto inp = std::static_pointer_cast<InputNode>(node);
            ss << " value=\"" << inp->getValue() << "\" />\n";
            return;
        }

        if (node->getChildren().empty()) {
            ss << " />\n";
            return;
        }

        ss << ">\n";
        for (const auto& child : node->getChildren()) {
            renderSnapshotRecursive(child, ss, indent + 1);
        }
        ss << spaces << "</" << (node->getKind() == NodeKind::Box ? "box" : "container") << ">\n";
    }

    std::shared_ptr<Component> component_;
    std::shared_ptr<UINode> rootNode_;
};

} // namespace keet::ui
