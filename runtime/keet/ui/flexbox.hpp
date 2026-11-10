#pragma once

#include <keet/ui/geometry.hpp>
#include <keet/ui/node.hpp>
#include <keet/ui/types.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

namespace keet::ui {

/// Layout unit coordinate space
enum class LayoutUnitMode : uint8_t {
    TerminalCells,
    DesktopPixels
};

/// Metrics and sizing parameters for Flexbox calculation
struct LayoutMetrics {
    LayoutUnitMode unitMode{LayoutUnitMode::TerminalCells};

    // Desktop pixel metrics
    int charWidthPx{8};
    int lineHeightPx{20};
    int buttonPaddingX{12};
    int buttonPaddingY{6};
    int inputHeightPx{32};
    int defaultBoxWidthPx{80};
    int defaultBoxHeightPx{40};

    using TextMeasureFn = std::function<Size(std::string_view text, int fontSize)>;
    TextMeasureFn textMeasureFn;

    [[nodiscard]] Size measureText(std::string_view text, int fontSize = 14) const {
        if (textMeasureFn) {
            return textMeasureFn(text, fontSize);
        }
        if (unitMode == LayoutUnitMode::DesktopPixels) {
            // Estimate based on monospace font metrics
            int charW = std::max(1, (charWidthPx * fontSize) / 14);
            int lineH = std::max(1, (lineHeightPx * fontSize) / 14);
            return Size{static_cast<int>(text.size()) * charW, lineH};
        }
        // Terminal cells
        return Size{static_cast<int>(text.size()), 1};
    }
};

/// Integer & Pixel Flexbox Layout Solver.
/// Computes discrete bounds for UI nodes based on flex-direction,
/// justify-content, align-items, margins, paddings, borders, gap, flexGrow, and flexShrink.
class FlexboxSolver {
public:
    /// Solves layout recursively starting from root node within available bounds
    static void computeLayout(const std::shared_ptr<UINode>& root,
                              Rect availableBounds,
                              const LayoutMetrics& metrics = LayoutMetrics{}) {
        if (!root) return;
        computeNode(root, availableBounds, metrics, true);
    }

private:
    static void computeNode(const std::shared_ptr<UINode>& node,
                            Rect available,
                            const LayoutMetrics& metrics,
                            bool isRoot = false) {
        const auto& style = node->getStyle();

        // 1. Compute node width and height
        int width = 0;
        if (!isRoot) {
            width = available.width;
        } else if (style.width.mode == SizeMode::Fixed || style.width.mode == SizeMode::Pixels) {
            width = style.width.value;
        } else if (style.width.mode == SizeMode::Percent) {
            width = (available.width * style.width.value) / 100;
        } else {
            // Auto
            width = available.width - style.margin.horizontal();
        }

        int height = 0;
        if (!isRoot) {
            height = available.height;
        } else if (style.height.mode == SizeMode::Fixed || style.height.mode == SizeMode::Pixels) {
            height = style.height.value;
        } else if (style.height.mode == SizeMode::Percent) {
            height = (available.height * style.height.value) / 100;
        } else {
            // Auto
            height = available.height - style.margin.vertical();
        }

        width = std::max(0, width);
        height = std::max(0, height);

        int nodeX = available.x + (isRoot ? style.margin.left : 0);
        int nodeY = available.y + (isRoot ? style.margin.top : 0);

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
        int totalShrink = 0;
        int visibleCount = 0;

        for (const auto& child : children) {
            if (!child->isVisible()) continue;
            visibleCount++;
            const auto& cStyle = child->getStyle();
            ChildMeasure m;
            m.node = child;
            m.flexGrow = cStyle.flexGrow;
            m.flexShrink = std::max(0, cStyle.flexShrink);

            bool isPixelsMode = (metrics.unitMode == LayoutUnitMode::DesktopPixels);

            if (isRow) {
                // Main axis = width, Cross axis = height
                if (cStyle.width.mode == SizeMode::Fixed || cStyle.width.mode == SizeMode::Pixels) {
                    m.baseMainSize = cStyle.width.value;
                } else if (cStyle.width.mode == SizeMode::Percent) {
                    m.baseMainSize = (contentWidth * cStyle.width.value) / 100;
                } else {
                    // Auto width: estimate from content or default min size
                    if (child->getKind() == NodeKind::Text) {
                        auto t = std::static_pointer_cast<TextNode>(child);
                        Size textSize = metrics.measureText(t->getContent(), cStyle.fontSize);
                        m.baseMainSize = textSize.width;
                    } else if (child->getKind() == NodeKind::Button) {
                        auto b = std::static_pointer_cast<ButtonNode>(child);
                        Size textSize = metrics.measureText(b->getLabel(), cStyle.fontSize);
                        int padX = isPixelsMode ? (metrics.buttonPaddingX * 2) : 2;
                        m.baseMainSize = textSize.width + padX;
                    } else if (child->getKind() == NodeKind::Input) {
                        m.baseMainSize = isPixelsMode ? 140 : 12;
                    } else {
                        m.baseMainSize = isPixelsMode ? metrics.defaultBoxWidthPx : 4;
                    }
                }

                if (cStyle.height.mode == SizeMode::Fixed || cStyle.height.mode == SizeMode::Pixels) {
                    m.crossSize = cStyle.height.value;
                } else if (cStyle.height.mode == SizeMode::Percent) {
                    m.crossSize = (contentHeight * cStyle.height.value) / 100;
                } else {
                    if (child->getKind() == NodeKind::Button && isPixelsMode) {
                        Size textSize = metrics.measureText("", cStyle.fontSize);
                        m.crossSize = textSize.height + (metrics.buttonPaddingY * 2);
                    } else if (child->getKind() == NodeKind::Input && isPixelsMode) {
                        m.crossSize = metrics.inputHeightPx;
                    } else {
                        m.crossSize = (cStyle.alignItems == AlignItems::Stretch) ? contentHeight : (isPixelsMode ? metrics.lineHeightPx : 1);
                    }
                }
            } else {
                // Main axis = height, Cross axis = width
                if (cStyle.height.mode == SizeMode::Fixed || cStyle.height.mode == SizeMode::Pixels) {
                    m.baseMainSize = cStyle.height.value;
                } else if (cStyle.height.mode == SizeMode::Percent) {
                    m.baseMainSize = (contentHeight * cStyle.height.value) / 100;
                } else {
                    // Auto height
                    if (child->getKind() == NodeKind::Text) {
                        auto t = std::static_pointer_cast<TextNode>(child);
                        Size textSize = metrics.measureText(t->getContent(), cStyle.fontSize);
                        m.baseMainSize = textSize.height;
                    } else if (child->getKind() == NodeKind::Button) {
                        if (isPixelsMode) {
                            Size textSize = metrics.measureText("", cStyle.fontSize);
                            m.baseMainSize = textSize.height + (metrics.buttonPaddingY * 2);
                        } else {
                            m.baseMainSize = 1;
                        }
                    } else if (child->getKind() == NodeKind::Input) {
                        m.baseMainSize = isPixelsMode ? metrics.inputHeightPx : 1;
                    } else {
                        m.baseMainSize = isPixelsMode ? metrics.defaultBoxHeightPx : 3;
                    }
                }

                if (cStyle.width.mode == SizeMode::Fixed || cStyle.width.mode == SizeMode::Pixels) {
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
            totalShrink += m.flexShrink;
            measures.push_back(m);
        }

        // Account for style.gap between children
        int totalGap = (visibleCount > 1 && style.gap > 0) ? (style.gap * (visibleCount - 1)) : 0;
        int totalRequiredMain = totalBaseMain + totalGap;

        // 3. Resolve flex grow or shrink
        bool isScroll = (node->getKind() == NodeKind::Scroll);
        int availableMain = isRow ? contentWidth : contentHeight;
        int remainingMain = availableMain - totalRequiredMain;

        if (isScroll && !isRow) {
            // For vertical scroll container, children do not shrink below intrinsic height;
            // content expands freely to totalRequiredMain.
        } else if (remainingMain > 0 && totalGrow > 0) {
            for (auto& m : measures) {
                if (m.flexGrow > 0) {
                    int extra = (remainingMain * m.flexGrow) / totalGrow;
                    m.finalMainSize += extra;
                }
            }
        } else if (remainingMain < 0 && totalShrink > 0) {
            int shrinkDebt = -remainingMain;
            for (auto& m : measures) {
                if (m.flexShrink > 0) {
                    int reduce = (shrinkDebt * m.flexShrink) / totalShrink;
                    m.finalMainSize = std::max(0, m.finalMainSize - reduce);
                }
            }
        }

        // 4. Align items & Justify content
        int startOffset = 0;
        int spaceGap = style.gap;

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
                        spaceGap += remainingMain / static_cast<int>(measures.size() - 1);
                    }
                    break;
                case JustifyContent::SpaceAround:
                    if (!measures.empty()) {
                        int extraGap = remainingMain / static_cast<int>(measures.size());
                        spaceGap += extraGap;
                        startOffset = extraGap / 2;
                    }
                    break;
                case JustifyContent::SpaceEvenly:
                    if (!measures.empty()) {
                        int extraGap = remainingMain / static_cast<int>(measures.size() + 1);
                        spaceGap += extraGap;
                        startOffset = extraGap;
                    }
                    break;
            }
        }

        // 5. Position each child and compute recursively
        int currentPos = startOffset;
        int maxChildBottom = 0;
        int maxChildRight = 0;

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
                currentPos += m.finalMainSize + spaceGap;
            } else {
                childBounds.x = contentX + crossPos + cStyle.margin.left;
                childBounds.y = contentY + currentPos + cStyle.margin.top;
                childBounds.width = (style.alignItems == AlignItems::Stretch)
                                         ? std::max(0, contentWidth - cStyle.margin.horizontal())
                                         : m.crossSize;
                childBounds.height = std::max(0, m.finalMainSize - cStyle.margin.vertical());
                currentPos += m.finalMainSize + spaceGap;
            }

            maxChildRight = std::max(maxChildRight, childBounds.x + childBounds.width - contentX);
            maxChildBottom = std::max(maxChildBottom, childBounds.y + childBounds.height - contentY);

            computeNode(m.node, childBounds, metrics, false);
        }

        // 6. Update ScrollNode content dimensions if applicable
        if (node->getKind() == NodeKind::Scroll) {
            auto scrollNode = std::static_pointer_cast<ScrollNode>(node);
            scrollNode->setContentHeight(maxChildBottom);
            scrollNode->setContentWidth(maxChildRight);
            int maxOffset = std::max(0, maxChildBottom - contentHeight);
            if (scrollNode->getScrollOffset() > maxOffset) {
                scrollNode->setScrollOffset(maxOffset);
            }
        }
    }
};

} // namespace keet::ui
