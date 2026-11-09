#pragma once

#include <keet/ui/geometry.hpp>
#include <keet/ui/node.hpp>
#include <keet/ui/types.hpp>

#include <algorithm>
#include <memory>
#include <vector>

namespace keet::ui {

/// Integer Flexbox Layout Solver for Terminal character cells.
/// Computes discrete integer bounds for UI nodes based on flex-direction,
/// justify-content, align-items, margins, paddings, and borders.
class FlexboxSolver {
public:
    /// Solves layout recursively starting from root node within available bounds
    static void computeLayout(const std::shared_ptr<UINode>& root, Rect availableBounds) {
        if (!root) return;
        computeNode(root, availableBounds);
    }

private:
    static void computeNode(const std::shared_ptr<UINode>& node, Rect available) {
        const auto& style = node->getStyle();

        // 1. Compute node width and height
        int width = 0;
        if (style.width.mode == SizeMode::Fixed) {
            width = style.width.value;
        } else if (style.width.mode == SizeMode::Percent) {
            width = (available.width * style.width.value) / 100;
        } else {
            // Auto
            width = available.width - style.margin.horizontal();
        }

        int height = 0;
        if (style.height.mode == SizeMode::Fixed) {
            height = style.height.value;
        } else if (style.height.mode == SizeMode::Percent) {
            height = (available.height * style.height.value) / 100;
        } else {
            // Auto: default to available height unless children demand specific size
            height = available.height - style.margin.vertical();
        }

        width = std::max(0, width);
        height = std::max(0, height);

        int nodeX = available.x + style.margin.left;
        int nodeY = available.y + style.margin.top;

        node->setBounds(Rect{nodeX, nodeY, width, height});

        // If leaf or has no children, done
        const auto& children = node->getChildren();
        if (children.empty()) return;

        // Content area inside padding + border
        int contentX = nodeX + style.padding.left + style.border.left;
        int contentY = nodeY + style.padding.top + style.border.top;
        int contentWidth = std::max(0, width - style.padding.horizontal() - style.border.horizontal());
        int contentHeight = std::max(0, height - style.padding.vertical() - style.border.vertical());

        bool isRow = (style.flexDirection == FlexDirection::Row);

        // 2. Measure children
        struct ChildMeasure {
            std::shared_ptr<UINode> node;
            int baseMainSize{0};
            int crossSize{0};
            int flexGrow{0};
            int flexShrink{1};
            int finalMainSize{0};
        };

        std::vector<ChildMeasure> measures;
        measures.reserve(children.size());

        int totalBaseMain = 0;
        int totalGrow = 0;

        for (const auto& child : children) {
            if (!child->isVisible()) continue;
            const auto& cStyle = child->getStyle();
            ChildMeasure m;
            m.node = child;
            m.flexGrow = cStyle.flexGrow;
            m.flexShrink = cStyle.flexShrink;

            if (isRow) {
                // Main axis = width, Cross axis = height
                if (cStyle.width.mode == SizeMode::Fixed) {
                    m.baseMainSize = cStyle.width.value;
                } else if (cStyle.width.mode == SizeMode::Percent) {
                    m.baseMainSize = (contentWidth * cStyle.width.value) / 100;
                } else {
                    // Auto width: estimate from content or default min size
                    if (child->getKind() == NodeKind::Text) {
                        auto t = std::static_pointer_cast<TextNode>(child);
                        m.baseMainSize = static_cast<int>(t->getContent().size());
                    } else if (child->getKind() == NodeKind::Button) {
                        auto b = std::static_pointer_cast<ButtonNode>(child);
                        m.baseMainSize = static_cast<int>(b->getLabel().size()) + 2; // [ label ]
                    } else if (child->getKind() == NodeKind::Input) {
                        m.baseMainSize = 12; // default input width
                    } else {
                        m.baseMainSize = 4;
                    }
                }

                if (cStyle.height.mode == SizeMode::Fixed) {
                    m.crossSize = cStyle.height.value;
                } else if (cStyle.height.mode == SizeMode::Percent) {
                    m.crossSize = (contentHeight * cStyle.height.value) / 100;
                } else {
                    m.crossSize = (cStyle.alignItems == AlignItems::Stretch) ? contentHeight : 1;
                }
            } else {
                // Main axis = height, Cross axis = width
                if (cStyle.height.mode == SizeMode::Fixed) {
                    m.baseMainSize = cStyle.height.value;
                } else if (cStyle.height.mode == SizeMode::Percent) {
                    m.baseMainSize = (contentHeight * cStyle.height.value) / 100;
                } else {
                    // Auto height
                    m.baseMainSize = (child->getKind() == NodeKind::Text || child->getKind() == NodeKind::Button || child->getKind() == NodeKind::Input) ? 1 : 3;
                }

                if (cStyle.width.mode == SizeMode::Fixed) {
                    m.crossSize = cStyle.width.value;
                } else if (cStyle.width.mode == SizeMode::Percent) {
                    m.crossSize = (contentWidth * cStyle.width.value) / 100;
                } else {
                    m.crossSize = (style.alignItems == AlignItems::Stretch) ? contentWidth : contentWidth;
                }
            }

            m.baseMainSize += isRow ? cStyle.margin.horizontal() : cStyle.margin.vertical();
            m.finalMainSize = m.baseMainSize;
            totalBaseMain += m.baseMainSize;
            totalGrow += m.flexGrow;
            measures.push_back(m);
        }

        // 3. Resolve flex grow or shrink
        int availableMain = isRow ? contentWidth : contentHeight;
        int remainingMain = availableMain - totalBaseMain;

        if (remainingMain > 0 && totalGrow > 0) {
            for (auto& m : measures) {
                if (m.flexGrow > 0) {
                    int extra = (remainingMain * m.flexGrow) / totalGrow;
                    m.finalMainSize += extra;
                }
            }
        }

        // 4. Align items & Justify content
        int startOffset = 0;
        int gap = 0;

        if (remainingMain > 0 && totalGrow == 0) {
            switch (style.justifyContent) {
                case JustifyContent::FlexStart:
                    startOffset = 0;
                    break;
                case JustifyContent::Center:
                    startOffset = remainingMain / 2;
                    break;
                case JustifyContent::FlexEnd:
                    startOffset = remainingMain;
                    break;
                case JustifyContent::SpaceBetween:
                    if (measures.size() > 1) {
                        gap = remainingMain / static_cast<int>(measures.size() - 1);
                    }
                    break;
                case JustifyContent::SpaceAround:
                    if (!measures.empty()) {
                        gap = remainingMain / static_cast<int>(measures.size());
                        startOffset = gap / 2;
                    }
                    break;
                case JustifyContent::SpaceEvenly:
                    if (!measures.empty()) {
                        gap = remainingMain / static_cast<int>(measures.size() + 1);
                        startOffset = gap;
                    }
                    break;
            }
        }

        // 5. Position each child and compute recursively
        int currentPos = startOffset;
        for (const auto& m : measures) {
            const auto& cStyle = m.node->getStyle();
            Rect childBounds{0, 0, 0, 0};

            int crossPos = 0;
            switch (style.alignItems) {
                case AlignItems::FlexStart:
                    crossPos = 0;
                    break;
                case AlignItems::Center:
                    crossPos = std::max(0, ((isRow ? contentHeight : contentWidth) - m.crossSize) / 2);
                    break;
                case AlignItems::FlexEnd:
                    crossPos = std::max(0, (isRow ? contentHeight : contentWidth) - m.crossSize);
                    break;
                case AlignItems::Stretch:
                    crossPos = 0;
                    break;
            }

            if (isRow) {
                childBounds.x = contentX + currentPos + cStyle.margin.left;
                childBounds.y = contentY + crossPos + cStyle.margin.top;
                childBounds.width = std::max(0, m.finalMainSize - cStyle.margin.horizontal());
                childBounds.height = (style.alignItems == AlignItems::Stretch)
                                         ? std::max(0, contentHeight - cStyle.margin.vertical())
                                         : m.crossSize;
                currentPos += m.finalMainSize + gap;
            } else {
                childBounds.x = contentX + crossPos + cStyle.margin.left;
                childBounds.y = contentY + currentPos + cStyle.margin.top;
                childBounds.width = (style.alignItems == AlignItems::Stretch)
                                        ? std::max(0, contentWidth - cStyle.margin.horizontal())
                                        : m.crossSize;
                childBounds.height = std::max(0, m.finalMainSize - cStyle.margin.vertical());
                currentPos += m.finalMainSize + gap;
            }

            computeNode(m.node, childBounds);
        }
    }
};

} // namespace keet::ui

