#pragma once

#include <keet/ui/canvas.hpp>
#include <keet/ui/component.hpp>
#include <keet/ui/flexbox.hpp>
#include <keet/ui/focus_manager.hpp>
#include <keet/ui/geometry.hpp>
#include <keet/ui/node.hpp>
#include <keet/ui/scheduler.hpp>
#include <keet/ui/types.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace keet::ui {

/// Spatial Hit-Testing Engine for desktop pointer interactions
class HitTester {
public:
    /// Traverses the scene graph in reverse Z-order to find the deepest visible node at Point p
    static std::shared_ptr<UINode> hitTest(const std::shared_ptr<UINode>& root, Point p) {
        if (!root || !root->isVisible()) return nullptr;
        return hitTestRecursive(root, p);
    }

    /// Finds the nearest enclosing ScrollNode ancestor for a given target node
    static std::shared_ptr<ScrollNode> findAncestorScrollNode(const std::shared_ptr<UINode>& node) {
        auto cur = node ? node->getParent() : nullptr;
        while (cur) {
            if (cur->getKind() == NodeKind::Scroll) {
                return std::static_pointer_cast<ScrollNode>(cur);
            }
            cur = cur->getParent();
        }
        return nullptr;
    }

private:
    static std::shared_ptr<UINode> hitTestRecursive(const std::shared_ptr<UINode>& node, Point p) {
        if (!node || !node->isVisible()) return nullptr;

        Rect bounds = node->getBounds();
        if (!bounds.contains(p)) return nullptr;

        bool isScroll = (node->getKind() == NodeKind::Scroll);
        Point childPoint = p;
        if (isScroll) {
            auto scrollNode = std::static_pointer_cast<ScrollNode>(node);
            childPoint.y += scrollNode->getScrollOffset();
        }

        // Test children in reverse order (top-most in Z-order wins)
        const auto& children = node->getChildren();
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            if (auto hit = hitTestRecursive(*it, childPoint)) {
                return hit;
            }
        }

        return node;
    }
};

/// High-performance desktop UI backend orchestrating layout, rasterization,
/// spatial pointer tracking, scroll dispatch, and keyboard state machine.
class DesktopBackend {
public:
    explicit DesktopBackend(std::shared_ptr<Component> rootComponent,
                            int width = 800,
                            int height = 600)
        : component_(std::move(rootComponent)),
          fb_(width, height),
          canvas_(fb_),
          focusMgr_() {

        metrics_.unitMode = LayoutUnitMode::DesktopPixels;
        metrics_.charWidthPx = 8;
        metrics_.lineHeightPx = 20;

        if (component_) {
            rootNode_ = component_->mount();
            component_->onMount();
            component_->flushUpdate();
            focusMgr_.setRoot(rootNode_);
        }

        relayout();
        renderFrame();
    }

    [[nodiscard]] int getWidth() const noexcept { return fb_.getWidth(); }
    [[nodiscard]] int getHeight() const noexcept { return fb_.getHeight(); }
    [[nodiscard]] FrameBuffer& getFrameBuffer() noexcept { return fb_; }
    [[nodiscard]] const FrameBuffer& getFrameBuffer() const noexcept { return fb_; }
    [[nodiscard]] std::shared_ptr<UINode> getRootNode() const noexcept { return rootNode_; }
    [[nodiscard]] FocusManager& getFocusManager() noexcept { return focusMgr_; }

    void resize(int width, int height) {
        fb_.resize(width, height);
        relayout();
        renderFrame();
    }

    void relayout() {
        if (!rootNode_) return;
        FlexboxSolver::computeLayout(rootNode_, Rect{0, 0, fb_.getWidth(), fb_.getHeight()}, metrics_);
    }

    void renderFrame() {
        canvas_.clear(RgbaColor{30, 30, 46, 255}); // Modern dark base theme
        if (rootNode_) {
            uint32_t focusedId = focusMgr_.getFocusedNodeId().value_or(0);
            SceneGraphRenderer::render(rootNode_, canvas_, focusedId);
        }
    }

    // --- Spatial Pointer & Mouse State Machine ---

    void dispatchMouseMove(int x, int y) {
        lastMousePos_ = Point{x, y};
        auto hit = HitTester::hitTest(rootNode_, lastMousePos_);
        uint32_t hitId = hit ? hit->getId() : 0;

        if (hitId != hoveredNodeId_.value_or(0)) {
            if (hoveredNodeId_.has_value()) {
                if (auto oldNode = findNodeById(*hoveredNodeId_)) {
                    oldNode->emit("mouseleave");
                    oldNode->setDirty(true);
                }
            }
            hoveredNodeId_ = hit ? std::optional<uint32_t>(hitId) : std::nullopt;
            if (hit) {
                hit->emit("mouseenter");
                hit->setDirty(true);
            }
        }

        if (hit) {
            hit->emit("mousemove", std::to_string(x) + "," + std::to_string(y));
        }

        flushState();
    }

    bool dispatchMouseDown(int x, int y, int button = 0) {
        lastMousePos_ = Point{x, y};
        auto hit = HitTester::hitTest(rootNode_, lastMousePos_);
        if (!hit) {
            pressedNodeId_ = std::nullopt;
            return false;
        }

        pressedNodeId_ = hit->getId();

        // Update focus
        if (hit->getKind() == NodeKind::Button || hit->getKind() == NodeKind::Input || hit->hasAttribute("tabindex")) {
            focusMgr_.setFocus(hit->getId());
        }

        // Input cursor positioning based on click coordinate
        if (hit->getKind() == NodeKind::Input) {
            auto input = std::static_pointer_cast<InputNode>(hit);
            int relX = std::max(0, x - hit->getBounds().x - 8);
            int charW = std::max(1, (8 * hit->getStyle().fontSize) / 14);
            int clickedChar = std::min(static_cast<int>(input->getValue().size()), (relX + charW / 2) / charW);
            input->setCursorPos(clickedChar);
        }

        hit->emit("mousedown", std::to_string(button));
        flushState();
        return true;
    }

    bool dispatchMouseUp(int x, int y, int button = 0) {
        lastMousePos_ = Point{x, y};
        auto hit = HitTester::hitTest(rootNode_, lastMousePos_);

        if (hit) {
            hit->emit("mouseup", std::to_string(button));
        }

        bool clickTriggered = false;
        if (pressedNodeId_.has_value()) {
            uint32_t pId = *pressedNodeId_;
            if (hit && (hit->getId() == pId || isAncestor(pId, hit))) {
                if (auto target = findNodeById(pId)) {
                    target->emit("click");
                    clickTriggered = true;
                }
            }
            pressedNodeId_ = std::nullopt;
        }

        flushState();
        return clickTriggered;
    }

    bool dispatchClick(int x, int y, int button = 0) {
        dispatchMouseDown(x, y, button);
        return dispatchMouseUp(x, y, button);
    }

    bool dispatchScroll(int x, int y, int deltaY) {
        lastMousePos_ = Point{x, y};
        auto hit = HitTester::hitTest(rootNode_, lastMousePos_);
        if (!hit) return false;

        std::shared_ptr<ScrollNode> scroll = (hit->getKind() == NodeKind::Scroll)
                                                 ? std::static_pointer_cast<ScrollNode>(hit)
                                                 : HitTester::findAncestorScrollNode(hit);

        if (!scroll) return false;

        int curOffset = scroll->getScrollOffset();
        int maxOffset = std::max(0, scroll->getContentHeight() - scroll->getBounds().height);
        int newOffset = std::clamp(curOffset + deltaY, 0, maxOffset);

        if (newOffset != curOffset) {
            scroll->setScrollOffset(newOffset);
            scroll->emit("scroll", std::to_string(newOffset));
            flushState();
            return true;
        }
        return false;
    }

    bool dispatchKey(const std::string& key) {
        bool handled = focusMgr_.handleKey(key);
        flushState();
        return handled;
    }

    std::shared_ptr<UINode> findNodeById(uint32_t id) const {
        return rootNode_ ? rootNode_->findById(id) : nullptr;
    }

    /// Snapshot serializes current tree to XML for headless inspection
    [[nodiscard]] std::string snapshot() const {
        std::stringstream ss;
        if (rootNode_) serializeNodeSnapshot(rootNode_, ss, 0);
        return ss.str();
    }

private:
    static bool isAncestor(uint32_t ancestorId, const std::shared_ptr<UINode>& node) {
        auto cur = node ? node->getParent() : nullptr;
        while (cur) {
            if (cur->getId() == ancestorId) return true;
            cur = cur->getParent();
        }
        return false;
    }

    void flushState() {
        if (component_) {
            component_->flushUpdate();
        }
        MicrotaskScheduler::instance().tick();
        relayout();
        renderFrame();
    }

    static void serializeNodeSnapshot(const std::shared_ptr<UINode>& node, std::stringstream& ss, int indent) {
        if (!node) return;
        std::string ind(indent * 2, ' ');
        Rect b = node->getBounds();

        std::string tag = "node";
        switch (node->getKind()) {
            case NodeKind::Box: tag = "box"; break;
            case NodeKind::Text: tag = "text"; break;
            case NodeKind::Button: tag = "button"; break;
            case NodeKind::Input: tag = "input"; break;
            case NodeKind::Stack: tag = "stack"; break;
            case NodeKind::Scroll: tag = "scroll"; break;
            default: break;
        }

        ss << ind << "<" << tag << " id=\"" << node->getId() << "\" bounds=\""
           << b.x << "," << b.y << "," << b.width << "," << b.height << "\"";

        if (node->getKind() == NodeKind::Input) {
            auto inp = std::static_pointer_cast<InputNode>(node);
            ss << " value=\"" << inp->getValue() << "\"";
        }
        ss << ">";

        if (node->getKind() == NodeKind::Text) {
            auto t = std::static_pointer_cast<TextNode>(node);
            ss << t->getContent();
        } else if (node->getKind() == NodeKind::Button) {
            auto btn = std::static_pointer_cast<ButtonNode>(node);
            ss << btn->getLabel();
        }

        const auto& children = node->getChildren();
        if (!children.empty()) {
            ss << "\n";
            for (const auto& ch : children) {
                serializeNodeSnapshot(ch, ss, indent + 1);
            }
            ss << ind;
        }

        ss << "</" << tag << ">\n";
    }

    std::shared_ptr<Component> component_;
    std::shared_ptr<UINode> rootNode_;
    FrameBuffer fb_;
    Canvas2D canvas_;
    FocusManager focusMgr_;
    LayoutMetrics metrics_;

    Point lastMousePos_{0, 0};
    std::optional<uint32_t> hoveredNodeId_{std::nullopt};
    std::optional<uint32_t> pressedNodeId_{std::nullopt};
};

} // namespace keet::ui
