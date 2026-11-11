#pragma once

#include <keet/ui/geometry.hpp>
#include <keet/ui/node.hpp>
#include <keet/ui/types.hpp>

#include <algorithm>
#include <cmath>
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
    float dpiScale{1.0f};

    // Desktop pixel metrics
    int charWidthPx{8};
    int lineHeightPx{20};
    int buttonPaddingX{12};
    int buttonPaddingY{6};
    int inputHeightPx{32};
    int defaultBoxWidthPx{80};
    int defaultBoxHeightPx{40};

    [[nodiscard]] inline int scale(int val) const noexcept {
        if (dpiScale <= 1.001f && dpiScale >= 0.999f) return val;
        return static_cast<int>(std::round(val * dpiScale));
    }

    [[nodiscard]] inline Edges scaleEdges(const Edges& e) const noexcept {
        if (dpiScale <= 1.001f && dpiScale >= 0.999f) return e;
        return Edges{scale(e.top), scale(e.right), scale(e.bottom), scale(e.left)};
    }

    using TextMeasureFn = std::function<Size(std::string_view text, int fontSize, const std::string& fontFamily)>;
    TextMeasureFn textMeasureFn;

    [[nodiscard]] Size measureText(std::string_view text, int fontSize = 14, const std::string& fontFamily = "") const {
        if (textMeasureFn) {
            return textMeasureFn(text, fontSize, fontFamily);
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
    static int estimateIntrinsicWidth(const std::shared_ptr<UINode>& node, const LayoutMetrics& metrics, const std::string& inheritedFontFamily = "") {
        if (!node) return 0;
        const auto& cStyle = node->getStyle();
        std::string effectiveFont = !cStyle.fontFamily.empty() ? cStyle.fontFamily : inheritedFontFamily;
        if (cStyle.width.mode == SizeMode::Fixed || cStyle.width.mode == SizeMode::Pixels) {
            return metrics.scale(cStyle.width.value);
        }
        bool isPixelsMode = (metrics.unitMode == LayoutUnitMode::DesktopPixels);
        int scaledFontSize = metrics.scale(cStyle.fontSize);
        if (node->getKind() == NodeKind::Text) {
            auto t = std::static_pointer_cast<TextNode>(node);
            return metrics.measureText(t->getContent(), scaledFontSize, effectiveFont).width;
        }
        if (node->getKind() == NodeKind::Button) {
            auto b = std::static_pointer_cast<ButtonNode>(node);
            int padX = isPixelsMode ? metrics.scale(metrics.buttonPaddingX * 2) : 2;
            return metrics.measureText(b->getLabel(), scaledFontSize, effectiveFont).width + padX;
        }
        if (node->getKind() == NodeKind::Input) {
            return isPixelsMode ? metrics.scale(140) : 12;
        }

        const auto& children = node->getChildren();
        if (children.empty()) {
            return isPixelsMode ? metrics.scale(metrics.defaultBoxWidthPx) : 4;
        }

        int totalWidth = 0;
        int maxChildWidth = 0;
        int visibleCount = 0;
        bool childIsRow = (cStyle.flexDirection == FlexDirection::Row);

        for (const auto& ch : children) {
            if (!ch || !ch->isVisible()) continue;
            visibleCount++;
            int w = estimateIntrinsicWidth(ch, metrics, effectiveFont) + metrics.scaleEdges(ch->getStyle().margin).horizontal();
            totalWidth += w;
            maxChildWidth = std::max(maxChildWidth, w);
        }

        int gapTotal = (visibleCount > 1 && cStyle.gap > 0) ? (metrics.scale(cStyle.gap) * (visibleCount - 1)) : 0;
        int contentW = childIsRow ? (totalWidth + gapTotal) : maxChildWidth;
        return contentW + metrics.scaleEdges(cStyle.padding).horizontal() + metrics.scaleEdges(cStyle.border).horizontal();
    }

    static int estimateIntrinsicHeight(const std::shared_ptr<UINode>& node, const LayoutMetrics& metrics, const std::string& inheritedFontFamily = "") {
        if (!node) return 0;
        const auto& cStyle = node->getStyle();
        std::string effectiveFont = !cStyle.fontFamily.empty() ? cStyle.fontFamily : inheritedFontFamily;
        if (cStyle.height.mode == SizeMode::Fixed || cStyle.height.mode == SizeMode::Pixels) {
            return metrics.scale(cStyle.height.value);
        }
        bool isPixelsMode = (metrics.unitMode == LayoutUnitMode::DesktopPixels);
        int scaledFontSize = metrics.scale(cStyle.fontSize);
        if (node->getKind() == NodeKind::Text) {
            auto t = std::static_pointer_cast<TextNode>(node);
            return metrics.measureText(t->getContent(), scaledFontSize, effectiveFont).height;
        }
        if (node->getKind() == NodeKind::Button) {
            int padY = isPixelsMode ? metrics.scale(metrics.buttonPaddingY * 2) : 0;
            return metrics.measureText("", scaledFontSize, effectiveFont).height + padY;
        }
        if (node->getKind() == NodeKind::Input) {
            return isPixelsMode ? metrics.scale(metrics.inputHeightPx) : 1;
        }

        const auto& children = node->getChildren();
        if (children.empty()) {
            return isPixelsMode ? metrics.scale(metrics.defaultBoxHeightPx) : 3;
        }

        int totalHeight = 0;
        int maxChildHeight = 0;
        int visibleCount = 0;
        bool childIsRow = (cStyle.flexDirection == FlexDirection::Row);

        for (const auto& ch : children) {
            if (!ch || !ch->isVisible()) continue;
            visibleCount++;
            int h = estimateIntrinsicHeight(ch, metrics, effectiveFont) + metrics.scaleEdges(ch->getStyle().margin).vertical();
            totalHeight += h;
            maxChildHeight = std::max(maxChildHeight, h);
        }

        int gapTotal = (visibleCount > 1 && cStyle.gap > 0) ? (metrics.scale(cStyle.gap) * (visibleCount - 1)) : 0;
        int contentH = childIsRow ? maxChildHeight : (totalHeight + gapTotal);
        return contentH + metrics.scaleEdges(cStyle.padding).vertical() + metrics.scaleEdges(cStyle.border).vertical();
    }

    static void computeNode(const std::shared_ptr<UINode>& node,
                            Rect available,
                            const LayoutMetrics& metrics,
                            bool isRoot = false) {
        const auto& style = node->getStyle();
        Edges pad = metrics.scaleEdges(style.padding);
        Edges bord = metrics.scaleEdges(style.border);
        Edges marg = metrics.scaleEdges(style.margin);
        int scaledGap = metrics.scale(style.gap);

        // 1. Compute node width and height
        int width = 0;
        if (!isRoot) {
            width = available.width;
        } else if (style.width.mode == SizeMode::Fixed || style.width.mode == SizeMode::Pixels) {
            width = metrics.scale(style.width.value);
        } else if (style.width.mode == SizeMode::Percent) {
            width = (available.width * style.width.value) / 100;
        } else {
            // Auto
            width = available.width - marg.horizontal();
        }

        int height = 0;
        if (!isRoot) {
            height = available.height;
        } else if (style.height.mode == SizeMode::Fixed || style.height.mode == SizeMode::Pixels) {
            height = metrics.scale(style.height.value);
        } else if (style.height.mode == SizeMode::Percent) {
            height = (available.height * style.height.value) / 100;
        } else {
            // Auto
            height = available.height - marg.vertical();
        }

        width = std::max(0, width);
        height = std::max(0, height);

        int nodeX = available.x + (isRoot ? marg.left : 0);
        int nodeY = available.y + (isRoot ? marg.top : 0);

        node->setBounds(Rect{nodeX, nodeY, width, height});

        // If leaf or has no children, done
        const auto& children = node->getChildren();
        if (children.empty()) return;

        // Content area inside padding + border
        int contentX = nodeX + pad.left + bord.left;
        int contentY = nodeY + pad.top + bord.top;
        int contentWidth = std::max(0, width - pad.horizontal() - bord.horizontal());
        int contentHeight = std::max(0, height - pad.vertical() - bord.vertical());

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
            Edges cMarg = metrics.scaleEdges(cStyle.margin);
            ChildMeasure m;
            m.node = child;
            m.flexGrow = cStyle.flexGrow;
            m.flexShrink = std::max(0, cStyle.flexShrink);

            if (isRow) {
                // Main axis = width, Cross axis = height
                if (cStyle.width.mode == SizeMode::Fixed || cStyle.width.mode == SizeMode::Pixels) {
                    m.baseMainSize = metrics.scale(cStyle.width.value);
                } else if (cStyle.width.mode == SizeMode::Percent) {
                    m.baseMainSize = (contentWidth * cStyle.width.value) / 100;
                } else {
                    m.baseMainSize = estimateIntrinsicWidth(child, metrics, style.fontFamily);
                }

                if (cStyle.height.mode == SizeMode::Fixed || cStyle.height.mode == SizeMode::Pixels) {
                    m.crossSize = metrics.scale(cStyle.height.value);
                } else if (cStyle.height.mode == SizeMode::Percent) {
                    m.crossSize = (contentHeight * cStyle.height.value) / 100;
                } else {
                    m.crossSize = (style.alignItems == AlignItems::Stretch)
                                      ? contentHeight
                                      : estimateIntrinsicHeight(child, metrics, style.fontFamily);
                }
            } else {
                // Main axis = height, Cross axis = width
                if (cStyle.height.mode == SizeMode::Fixed || cStyle.height.mode == SizeMode::Pixels) {
                    m.baseMainSize = metrics.scale(cStyle.height.value);
                } else if (cStyle.height.mode == SizeMode::Percent) {
                    m.baseMainSize = (contentHeight * cStyle.height.value) / 100;
                } else {
                    m.baseMainSize = estimateIntrinsicHeight(child, metrics, style.fontFamily);
                }

                if (cStyle.width.mode == SizeMode::Fixed || cStyle.width.mode == SizeMode::Pixels) {
                    m.crossSize = metrics.scale(cStyle.width.value);
                } else if (cStyle.width.mode == SizeMode::Percent) {
                    m.crossSize = (contentWidth * cStyle.width.value) / 100;
                } else {
                    m.crossSize = (style.alignItems == AlignItems::Stretch)
                                      ? contentWidth
                                      : estimateIntrinsicWidth(child, metrics, style.fontFamily);
                }
            }

            if (m.flexGrow > 0) {
                if (isRow && cStyle.width.mode == SizeMode::Auto) {
                    m.baseMainSize = 0;
                } else if (!isRow && cStyle.height.mode == SizeMode::Auto) {
                    m.baseMainSize = 0;
                }
            }

            m.baseMainSize += isRow ? cMarg.horizontal() : cMarg.vertical();
            m.finalMainSize = m.baseMainSize;
            totalBaseMain += m.baseMainSize;
            totalGrow += m.flexGrow;
            totalShrink += m.flexShrink;
            measures.push_back(m);
        }

        // Account for scaledGap between children
        int totalGap = (visibleCount > 1 && style.gap > 0) ? (scaledGap * (visibleCount - 1)) : 0;
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
        int spaceGap = scaledGap;

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
            Edges cMarg = metrics.scaleEdges(cStyle.margin);
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
                childBounds.x = contentX + currentPos + cMarg.left;
                childBounds.y = contentY + crossPos + cMarg.top;
                childBounds.width = std::max(0, m.finalMainSize - cMarg.horizontal());
                childBounds.height = (style.alignItems == AlignItems::Stretch)
                                         ? std::max(0, contentHeight - cMarg.vertical())
                                         : m.crossSize;
                currentPos += m.finalMainSize + spaceGap;
            } else {
                childBounds.x = contentX + crossPos + cMarg.left;
                childBounds.y = contentY + currentPos + cMarg.top;
                childBounds.width = (style.alignItems == AlignItems::Stretch)
                                         ? std::max(0, contentWidth - cMarg.horizontal())
                                         : m.crossSize;
                childBounds.height = std::max(0, m.finalMainSize - cMarg.vertical());
                currentPos += m.finalMainSize + spaceGap;
            }

            maxChildRight = std::max(maxChildRight, childBounds.x + childBounds.width - contentX);
            maxChildBottom = std::max(maxChildBottom, childBounds.y + childBounds.height - contentY);

            computeNode(m.node, childBounds, metrics, false);
        }

        // 6. Update ScrollNode content dimensions if applicable
        if (node->getKind() == NodeKind::Scroll) {
            auto scrollNode = std::static_pointer_cast<ScrollNode>(node);
            int totalContentH = maxChildBottom > 0 ? (maxChildBottom + pad.vertical() + bord.vertical()) : 0;
            int totalContentW = maxChildRight > 0 ? (maxChildRight + pad.horizontal() + bord.horizontal()) : 0;
            scrollNode->setContentHeight(totalContentH);
            scrollNode->setContentWidth(totalContentW);
            int maxOffset = std::max(0, totalContentH - height);
            if (scrollNode->getScrollOffset() > maxOffset) {
                scrollNode->setScrollOffset(maxOffset);
            }
        }
    }
};

} // namespace keet::ui
