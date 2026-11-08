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

