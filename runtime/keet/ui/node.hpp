#pragma once

#include <keet/ui/geometry.hpp>
#include <keet/ui/types.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace keet::ui {

enum class NodeKind {
    Box,
    Text,
    Button,
    Input,
    Stack,
    Scroll,
    Custom
};

enum class FlexDirection {
    Column,
    Row
};

enum class JustifyContent {
    FlexStart,
    Center,
    FlexEnd,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly
};

enum class AlignItems {
    FlexStart,
    Center,
    FlexEnd,
    Stretch
};

struct NodeStyle {
    Dimension width{Dimension::Auto()};
    Dimension height{Dimension::Auto()};
    int flexGrow{0};
    int flexShrink{1};

    FlexDirection flexDirection{FlexDirection::Column};
    JustifyContent justifyContent{JustifyContent::FlexStart};
    AlignItems alignItems{AlignItems::Stretch};

    Edges padding{0, 0, 0, 0};
    Edges margin{0, 0, 0, 0};
    Edges border{0, 0, 0, 0};

    BorderStyle borderStyle{BorderStyle::None};
    Color borderColor{Color::Default};
    Color color{Color::Default};
    Color backgroundColor{Color::Default};
    FontStyle fontStyle{FontStyle::Normal};

    // Desktop RGBA overrides & rich styling
    std::optional<RgbaColor> textColorRgba;
    std::optional<RgbaColor> backgroundColorRgba;
    std::optional<RgbaColor> borderColorRgba;

    int borderRadius{0};
    int fontSize{14};
    int gap{0};
    std::string fontFamily;

    bool overflowScrollY{false};
    bool overflowScrollX{false};
};

using EventCallback = std::function<void(const std::string& arg)>;

inline uint32_t nextUniqueNodeId() noexcept {
    static std::atomic<uint32_t> s_idCounter{100};
    return s_idCounter.fetch_add(1, std::memory_order_relaxed);
}

/// Universal UI Node base class
class UINode : public std::enable_shared_from_this<UINode> {
public:
    explicit UINode(NodeKind kind = NodeKind::Box) : kind_(kind), id_(nextUniqueNodeId()) {}
    virtual ~UINode() = default;

    [[nodiscard]] NodeKind getKind() const noexcept { return kind_; }

    void setId(uint32_t id) noexcept { id_ = id; }
    [[nodiscard]] uint32_t getId() const noexcept { return id_; }

    void setParent(std::weak_ptr<UINode> parent) noexcept { parent_ = parent; }
    [[nodiscard]] std::shared_ptr<UINode> getParent() const noexcept { return parent_.lock(); }

    void setBounds(Rect bounds) noexcept { bounds_ = bounds; }
    [[nodiscard]] Rect getBounds() const noexcept { return bounds_; }

    void setVisible(bool v) noexcept { visible_ = v; }
    [[nodiscard]] bool isVisible() const noexcept { return visible_; }

    void setDirty(bool d) noexcept { dirty_ = d; }
    [[nodiscard]] bool isDirty() const noexcept { return dirty_; }

    [[nodiscard]] NodeStyle& getStyle() noexcept { return style_; }
    [[nodiscard]] const NodeStyle& getStyle() const noexcept { return style_; }

    // Child management
    void appendChild(std::shared_ptr<UINode> child) {
        if (!child) return;
        child->setParent(weak_from_this());
        children_.push_back(std::move(child));
        setDirty(true);
    }

    void removeChild(const std::shared_ptr<UINode>& child) {
        auto it = std::remove(children_.begin(), children_.end(), child);
        if (it != children_.end()) {
            children_.erase(it, children_.end());
            setDirty(true);
        }
    }

    void clearChildren() {
        children_.clear();
        setDirty(true);
    }

    [[nodiscard]] const std::vector<std::shared_ptr<UINode>>& getChildren() const noexcept {
        return children_;
    }

    // Attributes
    void setAttribute(const std::string& key, std::string val) {
        attributes_[key] = std::move(val);
        setDirty(true);
    }

    [[nodiscard]] std::string getAttribute(const std::string& key) const {
        auto it = attributes_.find(key);
        return it != attributes_.end() ? it->second : "";
    }

    [[nodiscard]] bool hasAttribute(const std::string& key) const {
        return attributes_.find(key) != attributes_.end();
    }

    // Event hooks
    void on(const std::string& event, EventCallback cb) {
        listeners_[event].push_back(std::move(cb));
    }

    void emit(const std::string& event, const std::string& arg = "") {
        auto it = listeners_.find(event);
        if (it != listeners_.end()) {
            for (const auto& cb : it->second) {
                if (cb) cb(arg);
            }
        }
    }

    // Node lookup
    std::shared_ptr<UINode> findById(uint32_t targetId) {
        if (id_ == targetId) return shared_from_this();
        for (auto& child : children_) {
            if (auto found = child->findById(targetId)) return found;
        }
        return nullptr;
    }

protected:
    NodeKind kind_{NodeKind::Box};
    uint32_t id_{0};
    std::weak_ptr<UINode> parent_;
    std::vector<std::shared_ptr<UINode>> children_;
    Rect bounds_{0, 0, 0, 0};
    NodeStyle style_{};
    bool visible_{true};
    bool dirty_{true};

    std::unordered_map<std::string, std::string> attributes_;
    std::unordered_map<std::string, std::vector<EventCallback>> listeners_;
};

/// Specific layout & widget node implementations
class BoxNode : public UINode {
public:
    BoxNode() : UINode(NodeKind::Box) {}
};

class TextNode : public UINode {
public:
    TextNode(std::string text = "") : UINode(NodeKind::Text), content_(std::move(text)) {}

    void setContent(std::string text) {
        content_ = std::move(text);
        setDirty(true);
    }
    [[nodiscard]] const std::string& getContent() const noexcept { return content_; }

private:
    std::string content_;
};

class ButtonNode : public UINode {
public:
    ButtonNode(std::string label = "") : UINode(NodeKind::Button), label_(std::move(label)) {}

    void setLabel(std::string label) {
        label_ = std::move(label);
        setDirty(true);
    }
    [[nodiscard]] const std::string& getLabel() const noexcept { return label_; }

private:
    std::string label_;
};

class InputNode : public UINode {
public:
    InputNode(std::string val = "") : UINode(NodeKind::Input), value_(std::move(val)) {
        cursorPos_ = static_cast<int>(value_.size());
    }

    void setValue(std::string val) {
        pushUndo();
        value_ = std::move(val);
        cursorPos_ = std::min(cursorPos_, static_cast<int>(value_.size()));
        clearSelection();
        setDirty(true);
        emit("input", value_);
    }

    void setValueSilent(std::string val) {
        value_ = std::move(val);
        cursorPos_ = std::min(cursorPos_, static_cast<int>(value_.size()));
        clearSelection();
        setDirty(true);
    }
    [[nodiscard]] const std::string& getValue() const noexcept { return value_; }

    void setCursorPos(int pos) noexcept {
        cursorPos_ = pos;
        clearSelection();
    }
    [[nodiscard]] int getCursorPos() const noexcept { return cursorPos_; }

    void selectAll() noexcept {
        selectionStart_ = 0;
        selectionEnd_ = static_cast<int>(value_.size());
        cursorPos_ = selectionEnd_;
        setDirty(true);
    }

    void clearSelection() noexcept {
        selectionStart_ = -1;
        selectionEnd_ = -1;
    }

    [[nodiscard]] bool hasSelection() const noexcept {
        return selectionStart_ >= 0 && selectionEnd_ > selectionStart_;
    }

    [[nodiscard]] int getSelectionStart() const noexcept { return selectionStart_; }
    [[nodiscard]] int getSelectionEnd() const noexcept { return selectionEnd_; }

    void pushUndo() {
        undoStack_.push_back(value_);
        if (undoStack_.size() > 50) undoStack_.erase(undoStack_.begin());
        redoStack_.clear();
    }

    void undo() {
        if (undoStack_.empty()) return;
        redoStack_.push_back(value_);
        value_ = undoStack_.back();
        undoStack_.pop_back();
        cursorPos_ = static_cast<int>(value_.size());
        clearSelection();
        setDirty(true);
        emit("input", value_);
    }

    void redo() {
        if (redoStack_.empty()) return;
        undoStack_.push_back(value_);
        value_ = redoStack_.back();
        redoStack_.pop_back();
        cursorPos_ = static_cast<int>(value_.size());
        clearSelection();
        setDirty(true);
        emit("input", value_);
    }

    [[nodiscard]] std::string getSelectedOrFullText() const {
        if (hasSelection()) {
            int start = std::max(0, selectionStart_);
            int len = std::max(0, selectionEnd_ - selectionStart_);
            return value_.substr(start, len);
        }
        return value_;
    }

    void deleteSelectionOrAll() {
        pushUndo();
        if (hasSelection()) {
            int start = std::max(0, selectionStart_);
            int len = std::max(0, selectionEnd_ - selectionStart_);
            value_.erase(start, len);
            cursorPos_ = start;
            clearSelection();
        } else {
            value_.clear();
            cursorPos_ = 0;
        }
        setDirty(true);
        emit("input", value_);
    }

    void pasteText(const std::string& text) {
        if (text.empty()) return;
        pushUndo();
        if (hasSelection()) {
            int start = std::max(0, selectionStart_);
            int len = std::max(0, selectionEnd_ - selectionStart_);
            value_.replace(start, len, text);
            cursorPos_ = start + static_cast<int>(text.size());
            clearSelection();
        } else {
            int pos = std::clamp(cursorPos_, 0, static_cast<int>(value_.size()));
            value_.insert(pos, text);
            cursorPos_ = pos + static_cast<int>(text.size());
        }
        setDirty(true);
        emit("input", value_);
    }

    void deleteToStartOfLine() {
        pushUndo();
        if (hasSelection()) {
            deleteSelectionOrAll();
            return;
        }
        int pos = std::clamp(cursorPos_, 0, static_cast<int>(value_.size()));
        if (pos > 0) {
            value_.erase(0, pos);
            cursorPos_ = 0;
            setDirty(true);
            emit("input", value_);
        }
    }

    void deleteWordLeft() {
        pushUndo();
        if (hasSelection()) {
            deleteSelectionOrAll();
            return;
        }
        int pos = std::clamp(cursorPos_, 0, static_cast<int>(value_.size()));
        if (pos == 0) return;
        int i = pos;
        while (i > 0 && std::isspace(static_cast<unsigned char>(value_[i - 1]))) i--;
        while (i > 0 && !std::isspace(static_cast<unsigned char>(value_[i - 1]))) i--;
        value_.erase(i, pos - i);
        cursorPos_ = i;
        setDirty(true);
        emit("input", value_);
    }

    void moveWordLeft() noexcept {
        clearSelection();
        int pos = std::clamp(cursorPos_, 0, static_cast<int>(value_.size()));
        int i = pos;
        while (i > 0 && std::isspace(static_cast<unsigned char>(value_[i - 1]))) i--;
        while (i > 0 && !std::isspace(static_cast<unsigned char>(value_[i - 1]))) i--;
        cursorPos_ = i;
        setDirty(true);
    }

    void moveWordRight() noexcept {
        clearSelection();
        int n = static_cast<int>(value_.size());
        int pos = std::clamp(cursorPos_, 0, n);
        int i = pos;
        while (i < n && !std::isspace(static_cast<unsigned char>(value_[i]))) i++;
        while (i < n && std::isspace(static_cast<unsigned char>(value_[i]))) i++;
        cursorPos_ = i;
        setDirty(true);
    }

private:
    std::string value_;
    int cursorPos_{0};
    int selectionStart_{-1};
    int selectionEnd_{-1};
    std::vector<std::string> undoStack_;
    std::vector<std::string> redoStack_;
};

class StackNode : public UINode {
public:
    explicit StackNode(FlexDirection dir = FlexDirection::Column) : UINode(NodeKind::Stack) {
        style_.flexDirection = dir;
    }
};

class ScrollNode : public UINode {
public:
    ScrollNode() : UINode(NodeKind::Scroll) {
        style_.overflowScrollY = true;
    }

    void setScrollOffset(int off) noexcept { scrollOffset_ = off; setDirty(true); }
    [[nodiscard]] int getScrollOffset() const noexcept { return scrollOffset_; }

    void setContentHeight(int h) noexcept { contentHeight_ = h; }
    [[nodiscard]] int getContentHeight() const noexcept { return contentHeight_; }

    void setContentWidth(int w) noexcept { contentWidth_ = w; }
    [[nodiscard]] int getContentWidth() const noexcept { return contentWidth_; }

private:
    int scrollOffset_{0};
    int contentHeight_{0};
    int contentWidth_{0};
};

} // namespace keet::ui

