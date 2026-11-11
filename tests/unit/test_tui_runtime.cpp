#include <catch2/catch_test_macros.hpp>
#include <keet/std/ui.hpp>

using namespace keet::ui;

TEST_CASE("TUI Runtime - Integer Flexbox Layout Solver", "[ui][tui][flexbox]") {
    auto root = std::make_shared<BoxNode>();
    root->setId(1);
    root->getStyle().flexDirection = FlexDirection::Column;
    root->getStyle().padding = Edges::All(1);

    auto header = std::make_shared<TextNode>("Title");
    header->setId(2);
    header->getStyle().height = Dimension::Cells(1);
    root->appendChild(header);

    auto row = std::make_shared<BoxNode>();
    row->setId(3);
    row->getStyle().flexDirection = FlexDirection::Row;
    row->getStyle().height = Dimension::Cells(1);

    auto btn1 = std::make_shared<ButtonNode>("OK");
    btn1->setId(4);
    btn1->getStyle().width = Dimension::Cells(6);
    row->appendChild(btn1);

    auto btn2 = std::make_shared<ButtonNode>("Cancel");
    btn2->setId(5);
    btn2->getStyle().width = Dimension::Cells(10);
    row->appendChild(btn2);

    root->appendChild(row);

    Rect screenBounds{0, 0, 80, 24};
    FlexboxSolver::computeLayout(root, screenBounds);

    CHECK(root->getBounds().width == 80);
    CHECK(root->getBounds().height == 24);

    // Header inside padding of 1
    CHECK(header->getBounds().x == 1);
    CHECK(header->getBounds().y == 1);

    // Row below header (y = 1 + 1 = 2)
    CHECK(row->getBounds().x == 1);
    CHECK(row->getBounds().y == 2);

    // btn1 and btn2 laid out horizontally inside row
    CHECK(btn1->getBounds().x == 1);
    CHECK(btn1->getBounds().width == 6);
    CHECK(btn2->getBounds().x == 7);
    CHECK(btn2->getBounds().width == 10);
}

TEST_CASE("Desktop Flexbox Layout Solver - Pixels, Gap, FlexShrink & ScrollNode", "[ui][desktop][flexbox]") {
    LayoutMetrics metrics;
    metrics.unitMode = LayoutUnitMode::DesktopPixels;
    metrics.charWidthPx = 8;
    metrics.lineHeightPx = 20;

    // 1. Row layout with gap and explicit pixel sizes
    auto root = std::make_shared<BoxNode>();
    root->setId(1);
    root->getStyle().flexDirection = FlexDirection::Row;
    root->getStyle().padding = Edges::All(10);
    root->getStyle().gap = 15;

    auto colA = std::make_shared<BoxNode>();
    colA->setId(2);
    colA->getStyle().width = Dimension::Pixels(200);
    colA->getStyle().height = Dimension::Pixels(100);
    root->appendChild(colA);

    auto colB = std::make_shared<BoxNode>();
    colB->setId(3);
    colB->getStyle().width = Dimension::Pixels(300);
    colB->getStyle().height = Dimension::Pixels(100);
    root->appendChild(colB);

    Rect screenBounds{0, 0, 800, 600};
    FlexboxSolver::computeLayout(root, screenBounds, metrics);

    CHECK(root->getBounds().width == 800);
    CHECK(root->getBounds().height == 600);

    // colA starts at padding 10
    CHECK(colA->getBounds().x == 10);
    CHECK(colA->getBounds().y == 10);
    CHECK(colA->getBounds().width == 200);

    // colB starts after colA + gap (10 + 200 + 15 = 225)
    CHECK(colB->getBounds().x == 225);
    CHECK(colB->getBounds().y == 10);
    CHECK(colB->getBounds().width == 300);

    // 2. Proportional flex-shrink under constrained space
    auto shrinkContainer = std::make_shared<BoxNode>();
    shrinkContainer->setId(10);
    shrinkContainer->getStyle().flexDirection = FlexDirection::Row;
    shrinkContainer->getStyle().width = Dimension::Pixels(400);

    auto item1 = std::make_shared<BoxNode>();
    item1->setId(11);
    item1->getStyle().width = Dimension::Pixels(300);
    item1->getStyle().flexShrink = 1;
    shrinkContainer->appendChild(item1);

    auto item2 = std::make_shared<BoxNode>();
    item2->setId(12);
    item2->getStyle().width = Dimension::Pixels(300);
    item2->getStyle().flexShrink = 1;
    shrinkContainer->appendChild(item2);

    FlexboxSolver::computeLayout(shrinkContainer, Rect{0, 0, 400, 200}, metrics);
    // Total required = 600, available = 400, debt = 200. Each item shrinks by 100 -> width 200
    CHECK(item1->getBounds().width == 200);
    CHECK(item2->getBounds().width == 200);

    // 3. ScrollNode computes contentHeight and clamps scrollOffset
    auto scroll = std::make_shared<ScrollNode>();
    scroll->setId(20);
    scroll->getStyle().width = Dimension::Pixels(400);
    scroll->getStyle().height = Dimension::Pixels(300);
    scroll->getStyle().flexDirection = FlexDirection::Column;
    scroll->getStyle().gap = 10;

    for (int i = 0; i < 5; ++i) {
        auto card = std::make_shared<BoxNode>();
        card->setId(21 + i);
        card->getStyle().height = Dimension::Pixels(100);
        scroll->appendChild(card);
    }

    scroll->setScrollOffset(1000); // Intentionally out of bounds
    FlexboxSolver::computeLayout(scroll, Rect{0, 0, 400, 300}, metrics);

    // 5 cards * 100 + 4 gaps * 10 = 540 contentHeight
    CHECK(scroll->getContentHeight() == 540);
    // Max offset = 540 - 300 = 240. Scroll offset should be clamped to 240
    CHECK(scroll->getScrollOffset() == 240);
}

TEST_CASE("TUI Runtime - Focus Manager Tab Navigation", "[ui][tui][focus]") {
    auto root = std::make_shared<BoxNode>();
    root->setId(1);

    auto btnA = std::make_shared<ButtonNode>("A");
    btnA->setId(10);
    root->appendChild(btnA);

    auto inputB = std::make_shared<InputNode>("test");
    inputB->setId(20);
    root->appendChild(inputB);

    auto btnC = std::make_shared<ButtonNode>("C");
    btnC->setId(30);
    root->appendChild(btnC);

    FocusManager focusMgr(root);

    // Initial focus on first focusable element
    CHECK(focusMgr.getFocusedNodeId() == 10);

    // Tab -> next element
    focusMgr.handleKey("Tab");
    CHECK(focusMgr.getFocusedNodeId() == 20);

    // Tab -> next element
    focusMgr.handleKey("Tab");
    CHECK(focusMgr.getFocusedNodeId() == 30);

    // Tab wraps around to first element
    focusMgr.handleKey("Tab");
    CHECK(focusMgr.getFocusedNodeId() == 10);

    // ShiftTab -> cycles backwards
    focusMgr.handleKey("ShiftTab");
    CHECK(focusMgr.getFocusedNodeId() == 30);

    // Activation test: space on button triggers click
    bool clicked = false;
    btnC->on("click", [&]([[maybe_unused]] const std::string& _) {
        clicked = true;
    });

    focusMgr.handleKey(" ");
    CHECK(clicked);

    // Typing into focused input
    focusMgr.setFocus(20);
    focusMgr.handleKey("1");
    focusMgr.handleKey("2");
    CHECK(inputB->getValue() == "test12");
}

TEST_CASE("TUI Runtime - Double-Buffered Differential Terminal Renderer", "[ui][tui][renderer]") {
    TuiBackend tui(Size{40, 10});

    auto root = std::make_shared<BoxNode>();
    root->setId(1);
    root->setBounds(Rect{0, 0, 40, 10});

    auto text = std::make_shared<TextNode>("Hello Keet TUI");
    text->setId(2);
    text->setBounds(Rect{2, 2, 14, 1});
    root->appendChild(text);

    tui.renderTree(root);
    std::string firstDiff = tui.flushToString();

    // First flush contains positioning and full text
    CHECK(firstDiff.find("Hello Keet TUI") != std::string::npos);

    // Second flush without modification should be completely empty (differential rendering)
    std::string secondDiff = tui.flushToString();
    CHECK(secondDiff.empty());

    // Modify text node
    text->setContent("Hello World!!");
    tui.renderTree(root);
    std::string thirdDiff = tui.flushToString();

    // Only differential changes flushed
    CHECK(thirdDiff.find("World!!") != std::string::npos);
}

