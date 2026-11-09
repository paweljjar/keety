#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <keet/json.hpp>
#include <keet/ui/node.hpp>

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace keet::ui {

/// Virtual DOM Node representation serialized as RFC 8259 JSON
struct VNode {
    std::string tag{"div"};
    std::string id;
    std::string key;
    std::string text;
    std::unordered_map<std::string, std::string> attributes;
    std::unordered_map<std::string, std::string> styles;
    std::vector<std::string> events;
    std::vector<std::shared_ptr<VNode>> children;

    /// Converts UINode hierarchy into Virtual DOM tree
    static std::shared_ptr<VNode> fromUINode(const std::shared_ptr<UINode>& node) {
        if (!node) return nullptr;

        auto vnode = std::make_shared<VNode>();
        vnode->id = std::to_string(node->getId());

        switch (node->getKind()) {
            case NodeKind::Box:
                vnode->tag = "div";
                break;
            case NodeKind::Text: {
                vnode->tag = "span";
                auto t = std::static_pointer_cast<TextNode>(node);
                vnode->text = t->getContent();
                break;
            }
            case NodeKind::Button: {
                vnode->tag = "button";
                auto b = std::static_pointer_cast<ButtonNode>(node);
                vnode->text = b->getLabel();
                break;
            }
            case NodeKind::Input: {
                vnode->tag = "input";
                auto inp = std::static_pointer_cast<InputNode>(node);
                vnode->attributes["value"] = inp->getValue();
                break;
            }
            case NodeKind::Stack:
                vnode->tag = "div";
                break;
            case NodeKind::Scroll:
                vnode->tag = "div";
                vnode->styles["overflow"] = "auto";
                break;
            case NodeKind::Custom:
                vnode->tag = "div";
                break;
        }

        // Copy classes & attributes
        if (node->hasAttribute("class")) {
            vnode->attributes["class"] = node->getAttribute("class");
        }
        if (node->hasAttribute("key")) {
            vnode->key = node->getAttribute("key");
        }

        // Copy children
        for (const auto& child : node->getChildren()) {
            if (auto vChild = fromUINode(child)) {
                vnode->children.push_back(std::move(vChild));
            }
        }

        return vnode;
    }

    /// Serializes VNode tree to JSON string
    [[nodiscard]] std::string toJson() const {
        std::stringstream ss;
        serializeToJson(ss);
        return ss.str();
    }

private:
    void serializeToJson(std::stringstream& ss) const {
        ss << "{";
        ss << "\"tag\":\"" << escapeJson(tag) << "\"";
        if (!id.empty()) {
            ss << ",\"id\":\"" << escapeJson(id) << "\"";
        }
        if (!key.empty()) {
            ss << ",\"key\":\"" << escapeJson(key) << "\"";
        }
        if (!text.empty()) {
            ss << ",\"text\":\"" << escapeJson(text) << "\"";
        }

        if (!attributes.empty()) {
            ss << ",\"attrs\":{";
            bool first = true;
            for (const auto& [k, v] : attributes) {
                if (!first) ss << ",";
                first = false;
                ss << "\"" << escapeJson(k) << "\":\"" << escapeJson(v) << "\"";
            }
            ss << "}";
        }

        if (!styles.empty()) {
            ss << ",\"styles\":{";
            bool first = true;
            for (const auto& [k, v] : styles) {
                if (!first) ss << ",";
                first = false;
                ss << "\"" << escapeJson(k) << "\":\"" << escapeJson(v) << "\"";
            }
            ss << "}";
        }

        if (!children.empty()) {
            ss << ",\"children\":[";
            for (size_t i = 0; i < children.size(); ++i) {
                if (i > 0) ss << ",";
                children[i]->serializeToJson(ss);
            }
            ss << "]";
        }

        ss << "}";
    }

    static std::string escapeJson(const std::string& str) {
        std::string out;
        out.reserve(str.size());
        for (char c : str) {
            if (c == '"') out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\b') out += "\\b";
            else if (c == '\f') out += "\\f";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else if (c == '\t') out += "\\t";
            else out.push_back(c);
        }
        return out;
    }
};

/// RFC 6902 / Opcode Diff Patch
struct VDomPatch {
    enum class Op {
        ReplaceText,
        SetAttribute,
        RemoveAttribute,
        AppendChild,
        RemoveChild
    };

    Op op{Op::ReplaceText};
    std::string targetId;
    std::string key;
    std::string value;

    [[nodiscard]] std::string toJson() const {
        std::stringstream ss;
        ss << "{\"op\":";
        switch (op) {
            case Op::ReplaceText: ss << "\"replaceText\""; break;
            case Op::SetAttribute: ss << "\"setAttr\""; break;
            case Op::RemoveAttribute: ss << "\"removeAttr\""; break;
            case Op::AppendChild: ss << "\"appendChild\""; break;
            case Op::RemoveChild: ss << "\"removeChild\""; break;
        }
        ss << ",\"id\":\"" << targetId << "\"";
        if (!key.empty()) ss << ",\"key\":\"" << key << "\"";
        if (!value.empty()) ss << ",\"val\":\"" << value << "\"";
        ss << "}";
        return ss.str();
    }
};

/// Computes minimal diff patches between old and new VNode trees
class VDomDiffer {
public:
    static std::vector<VDomPatch> diff(const std::shared_ptr<VNode>& oldTree,
                                       const std::shared_ptr<VNode>& newTree) {
        std::vector<VDomPatch> patches;
        diffRecursive(oldTree, newTree, patches);
        return patches;
    }

    static std::string patchesToJson(const std::vector<VDomPatch>& patches) {
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < patches.size(); ++i) {
            if (i > 0) ss << ",";
            ss << patches[i].toJson();
        }
        ss << "]";
        return ss.str();
    }

private:
    static void diffRecursive(const std::shared_ptr<VNode>& oldNode,
                              const std::shared_ptr<VNode>& newNode,
                              std::vector<VDomPatch>& patches) {
        if (!oldNode || !newNode) return;

        // Text change
        if (oldNode->text != newNode->text && !newNode->id.empty()) {
            VDomPatch p;
            p.op = VDomPatch::Op::ReplaceText;
            p.targetId = newNode->id;
            p.value = newNode->text;
            patches.push_back(p);
        }

        // Attribute changes
        for (const auto& [k, v] : newNode->attributes) {
            auto it = oldNode->attributes.find(k);
            if (it == oldNode->attributes.end() || it->second != v) {
                VDomPatch p;
                p.op = VDomPatch::Op::SetAttribute;
                p.targetId = newNode->id;
                p.key = k;
                p.value = v;
                patches.push_back(p);
            }
        }

        // Compare children
        size_t minCount = (std::min)(oldNode->children.size(), newNode->children.size());
        for (size_t i = 0; i < minCount; ++i) {
            diffRecursive(oldNode->children[i], newNode->children[i], patches);
        }
    }
};

} // namespace keet::ui

