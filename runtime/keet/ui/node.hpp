#pragma once

#include <keet/ui/geometry.hpp>
#include <keet/ui/types.hpp>

#include <algorithm>
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
    AlignItems alignItems{AlignItems::FlexStart};

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

    bool overflowScrollY{false};
    bool overflowScrollX{false};
};

using EventCallback = std::function<void(const std::string& arg)>;

/// Universal UI Node base class
class UINode : public std::enable_shared_from_this<UINode> {
public:
    explicit UINode(NodeKind kind = NodeKind::Box) : kind_(kind) {}
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
    InputNode(std::string val = "") : UINode(NodeKind::Input), value_(std::move(val)) {}

    void setValue(std::string val) {
        value_ = std::move(val);
        setDirty(true);
        emit("input", value_);
    }
    [[nodiscard]] const std::string& getValue() const noexcept { return value_; }

    void setCursorPos(int pos) noexcept { cursorPos_ = pos; }
    [[nodiscard]] int getCursorPos() const noexcept { return cursorPos_; }

private:
    std::string value_;
    int cursorPos_{0};
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

