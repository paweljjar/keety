#pragma once

#include <keet/ui/node.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

namespace keet::ui {

/// Focus navigation and active element manager for keyboard-driven TUI interactions.
/// Collects focusable nodes (buttons, inputs, or explicit tabindex >= 0) in depth-first order,
/// and handles Tab / Shift-Tab cycling and Space / Enter activation.
class FocusManager {
public:
    explicit FocusManager(std::shared_ptr<UINode> root = nullptr)
        : root_(std::move(root)) {
        refreshFocusableList();
    }

    void setRoot(std::shared_ptr<UINode> root) {
        root_ = std::move(root);
        refreshFocusableList();
    }

    /// Re-scans the DOM tree in depth-first order to find focusable elements
    void refreshFocusableList() {
        focusableNodes_.clear();
        if (root_) {
            collectFocusableRecursive(root_);
        }

        // Validate current focused ID
        if (focusedNodeId_.has_value()) {
            bool found = false;
            for (const auto& node : focusableNodes_) {
                if (node->getId() == *focusedNodeId_) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                focusedNodeId_ = focusableNodes_.empty() ? std::nullopt : std::optional<uint32_t>(focusableNodes_[0]->getId());
            }
        } else if (!focusableNodes_.empty()) {
            focusedNodeId_ = focusableNodes_[0]->getId();
        }
    }

    [[nodiscard]] std::optional<uint32_t> getFocusedNodeId() const noexcept {
        return focusedNodeId_;
    }

    std::shared_ptr<UINode> getFocusedNode() const {
        if (!focusedNodeId_) return nullptr;
        for (const auto& node : focusableNodes_) {
            if (node->getId() == *focusedNodeId_) return node;
        }
        return nullptr;
    }

    void setFocus(uint32_t id) {
        for (const auto& node : focusableNodes_) {
            if (node->getId() == id) {
                focusedNodeId_ = id;
                return;
            }
        }
    }

    /// Advances focus to the next focusable element (Tab)
    void focusNext() {
        if (focusableNodes_.empty()) return;
        if (!focusedNodeId_) {
            focusedNodeId_ = focusableNodes_[0]->getId();
            return;
        }

        for (size_t i = 0; i < focusableNodes_.size(); ++i) {
            if (focusableNodes_[i]->getId() == *focusedNodeId_) {
                size_t nextIdx = (i + 1) % focusableNodes_.size();
                focusedNodeId_ = focusableNodes_[nextIdx]->getId();
                return;
            }
        }
    }

    /// Moves focus to the previous focusable element (Shift-Tab)
    void focusPrevious() {
        if (focusableNodes_.empty()) return;
        if (!focusedNodeId_) {
            focusedNodeId_ = focusableNodes_.back()->getId();
            return;
        }

        for (size_t i = 0; i < focusableNodes_.size(); ++i) {
            if (focusableNodes_[i]->getId() == *focusedNodeId_) {
                size_t prevIdx = (i == 0) ? (focusableNodes_.size() - 1) : (i - 1);
                focusedNodeId_ = focusableNodes_[prevIdx]->getId();
                return;
            }
        }
    }

    /// Handles keyboard activation (e.g. Space/Enter on Button, typing in Input)
    bool handleKey(const std::string& key) {
        auto node = getFocusedNode();
        if (!node) return false;

        if (key == "Tab") {
            focusNext();
            return true;
        }
        if (key == "BackTab" || key == "ShiftTab") {
            focusPrevious();
            return true;
        }

        if (node->getKind() == NodeKind::Button) {
            if (key == " " || key == "Enter") {
                node->emit("click");
                node->emit("keydown", key);
                return true;
            }
        } else if (node->getKind() == NodeKind::Input) {
            auto input = std::static_pointer_cast<InputNode>(node);
            int cursor = std::clamp(input->getCursorPos(), 0, static_cast<int>(input->getValue().size()));
            std::string val = input->getValue();

            if (key == "SelectAll" || key == "Cmd+A" || key == "Ctrl+A" || key == "Cmd+a" || key == "Ctrl+a") {
                input->selectAll();
                node->emit("keydown", key);
                return true;
            }
            if (key == "ArrowLeft") {
                if (cursor > 0) input->setCursorPos(cursor - 1);
                node->emit("keydown", key);
                return true;
            }
            if (key == "ArrowRight") {
                if (cursor < static_cast<int>(val.size())) input->setCursorPos(cursor + 1);
                node->emit("keydown", key);
                return true;
            }
            if (key == "Home") {
                input->setCursorPos(0);
                node->emit("keydown", key);
                return true;
            }
            if (key == "End") {
                input->setCursorPos(static_cast<int>(val.size()));
                node->emit("keydown", key);
                return true;
            }
            if (key == "Backspace" || key == "Delete") {
                if (input->hasSelection()) {
                    input->setValue("");
                    input->clearSelection();
                } else if (key == "Backspace" && cursor > 0 && !val.empty()) {
                    val.erase(cursor - 1, 1);
                    input->setValue(val);
                    input->setCursorPos(cursor - 1);
                } else if (key == "Delete" && cursor < static_cast<int>(val.size())) {
                    val.erase(cursor, 1);
                    input->setValue(val);
                    input->setCursorPos(cursor);
                }
                node->emit("keydown", key);
                return true;
            }
            if (key.size() == 1 && std::isprint(static_cast<unsigned char>(key[0]))) {
                if (input->hasSelection()) {
                    input->setValue(key);
                    input->clearSelection();
                    input->setCursorPos(1);
                } else {
                    val.insert(cursor, key);
                    input->setValue(val);
                    input->setCursorPos(cursor + 1);
                }
                node->emit("keydown", key);
                return true;
            }
        }

        node->emit("keydown", key);
        return false;
    }

    [[nodiscard]] const std::vector<std::shared_ptr<UINode>>& getFocusableNodes() const noexcept {
        return focusableNodes_;
    }

private:
    void collectFocusableRecursive(const std::shared_ptr<UINode>& node) {
        if (!node || !node->isVisible()) return;

        bool focusable = false;
        if (node->getKind() == NodeKind::Button || node->getKind() == NodeKind::Input) {
            focusable = true;
        } else if (node->hasAttribute("tabindex")) {
            int tabIndex = std::stoi(node->getAttribute("tabindex"));
            if (tabIndex >= 0) focusable = true;
        }

        if (focusable) {
            focusableNodes_.push_back(node);
        }

        for (const auto& child : node->getChildren()) {
            collectFocusableRecursive(child);
        }
    }

    std::shared_ptr<UINode> root_;
    std::vector<std::shared_ptr<UINode>> focusableNodes_;
    std::optional<uint32_t> focusedNodeId_{std::nullopt};
};

} // namespace keet::ui

