#pragma once

#include <keet/ui/node.hpp>
#include <keet/ui/scheduler.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace keet::ui {

/// Base class for all reactive UI components
class Component : public std::enable_shared_from_this<Component> {
public:
    Component() = default;
    virtual ~Component() = default;

    /// Lifecycle hooks
    virtual void onMount() {}
    virtual void beforeUpdate([[maybe_unused]] uint32_t dirtyMask) {}
    virtual void afterUpdate([[maybe_unused]] uint32_t dirtyMask) {}
    virtual void onDestroy() {}

    /// Core component lifecycle implementation
    virtual std::shared_ptr<UINode> mount() { return rootNode_; }
    virtual void update([[maybe_unused]] uint32_t dirtyMask) {}
    virtual void destroy() {
        onDestroy();
        if (rootNode_) {
            rootNode_->setDirty(true);
        }
    }

    /// Marks the component dirty with specific bits and schedules a batched update
    void invalidate(uint32_t dirtyBits) {
        bool wasClean = (dirtyMask_ == 0);
        dirtyMask_ |= dirtyBits;

        if (wasClean) {
            std::weak_ptr<Component> weakSelf = weak_from_this();
            MicrotaskScheduler::instance().enqueue([weakSelf]() {
                if (auto self = weakSelf.lock()) {
                    self->flushUpdate();
                }
            });
        }
    }

    /// Immediate synchronous flush of dirty mask
    void flushUpdate() {
        if (dirtyMask_ != 0) {
            uint32_t currentMask = dirtyMask_;
            beforeUpdate(currentMask);
            update(currentMask);
            afterUpdate(currentMask);
            dirtyMask_ = 0;
        }
    }

    [[nodiscard]] uint32_t getDirtyMask() const noexcept { return dirtyMask_; }
    [[nodiscard]] bool isDirty() const noexcept { return dirtyMask_ != 0; }

    [[nodiscard]] std::shared_ptr<UINode> getRootNode() const noexcept { return rootNode_; }

    std::shared_ptr<UINode> findNodeById(uint32_t id) {
        return rootNode_ ? rootNode_->findById(id) : nullptr;
    }

    void registerChild(uint32_t childId, std::shared_ptr<Component> child) {
        children_[childId] = std::move(child);
    }

    std::shared_ptr<Component> findChildById(uint32_t childId) {
        auto it = children_.find(childId);
        return it != children_.end() ? it->second : nullptr;
    }

protected:
    uint32_t dirtyMask_{0xFFFFFFFF}; // Initial mount is DIRTY_ALL
    std::shared_ptr<UINode> rootNode_{nullptr};
    std::unordered_map<uint32_t, std::shared_ptr<Component>> children_;
};

/// Built-in reactive counter component demonstrating live VDOM diffing and WebSocket event dispatch
class ReactiveCounter : public Component {
public:
    int64_t count{0};
    std::shared_ptr<TextNode> countText_;

    std::shared_ptr<UINode> mount() override {
        auto card = std::make_shared<BoxNode>();
        card->setId(1);
        card->setAttribute("class", "counter-card");

        auto title = std::make_shared<TextNode>("KeetScript Reactive UI Engine");
        title->setId(2);
        title->setAttribute("class", "title");
        card->appendChild(title);

        countText_ = std::make_shared<TextNode>("Current Count: " + std::to_string(count));
        countText_->setId(3);
        countText_->setAttribute("class", "count-display");
        card->appendChild(countText_);

        auto btnRow = std::make_shared<BoxNode>();
        btnRow->setId(4);
        btnRow->setAttribute("class", "button-row");

        auto decBtn = std::make_shared<ButtonNode>("- Decrement");
        decBtn->setId(5);
        decBtn->setAttribute("class", "btn btn-secondary");
        decBtn->on("click", [this](const std::string&) {
            count--;
            invalidate(0x01);
        });
        btnRow->appendChild(decBtn);

        auto incBtn = std::make_shared<ButtonNode>("+ Increment");
        incBtn->setId(6);
        incBtn->setAttribute("class", "btn btn-primary");
        incBtn->on("click", [this](const std::string&) {
            count++;
            invalidate(0x01);
        });
        btnRow->appendChild(incBtn);

        auto resetBtn = std::make_shared<ButtonNode>("Reset");
        resetBtn->setId(7);
        resetBtn->setAttribute("class", "btn btn-outline");
        resetBtn->on("click", [this](const std::string&) {
            count = 0;
            invalidate(0x01);
        });
        btnRow->appendChild(resetBtn);

        card->appendChild(btnRow);

        rootNode_ = card;
        return rootNode_;
    }

    void update(uint32_t dirtyMask) override {
        if ((dirtyMask & 0x01) && countText_) {
            countText_->setContent("Current Count: " + std::to_string(count));
        }
    }
};

} // namespace keet::ui

