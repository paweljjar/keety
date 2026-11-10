#include <catch2/catch_test_macros.hpp>
#include <keet/std/ui.hpp>

using namespace keet::ui;

TEST_CASE("Canvas2D - FrameBuffer Alpha Blending and Damage Tracking", "[ui][desktop][canvas]") {
    FrameBuffer fb(100, 100);
    CHECK(fb.getWidth() == 100);
    CHECK(fb.getHeight() == 100);

    // Initial background is opaque black
    RgbaColor p0 = fb.getPixel(0, 0);
    CHECK(p0.r == 0);
    CHECK(p0.g == 0);
    CHECK(p0.b == 0);
    CHECK(p0.a == 255);

    // Set pixel
    fb.setPixel(10, 10, RgbaColor{255, 0, 0, 255});
    RgbaColor pRed = fb.getPixel(10, 10);
    CHECK(pRed.r == 255);
    CHECK(pRed.g == 0);
    CHECK(pRed.b == 0);
    CHECK(pRed.a == 255);

    // Alpha blend 50% white over red
    fb.blendPixel(10, 10, RgbaColor{255, 255, 255, 128});
    RgbaColor pBlended = fb.getPixel(10, 10);
    // 255 * 128/255 + 255 * 127/255 = 255 red
    CHECK(pBlended.r == 255);
    // 255 * 128/255 + 0 * 127/255 ~= 128 green and blue
    CHECK(pBlended.g >= 126);
    CHECK(pBlended.g <= 129);
    CHECK(pBlended.b >= 126);
    CHECK(pBlended.b <= 129);

    // Damage tracking
    fb.addDamage(Rect{5, 5, 20, 20});
    CHECK(fb.getDamagedRegions().size() == 1);
    CHECK(fb.getDamagedRegions()[0] == Rect{5, 5, 20, 20});
    fb.clearDamagedRegions();
    CHECK(fb.getDamagedRegions().empty());
}

TEST_CASE("Canvas2D - Scissor Clipping and Drawing Primitives", "[ui][desktop][canvas]") {
    FrameBuffer fb(200, 200);
    Canvas2D canvas(fb);

    canvas.clear(RgbaColor{0, 0, 0, 255});

    // Scissor clip to [50, 50, 50, 50]
    canvas.pushClipRect(Rect{50, 50, 50, 50});

    // Draw full rect over entire canvas [0, 0, 200, 200] with Blue
    canvas.fillRect(Rect{0, 0, 200, 200}, RgbaColor{0, 0, 255, 255});

    // Pixels outside clip must remain black
    CHECK(fb.getPixel(10, 10) == RgbaColor{0, 0, 0, 255});
    CHECK(fb.getPixel(150, 150) == RgbaColor{0, 0, 0, 255});

    // Pixels inside clip must be blue
    CHECK(fb.getPixel(60, 60) == RgbaColor{0, 0, 255, 255});
    CHECK(fb.getPixel(90, 90) == RgbaColor{0, 0, 255, 255});

    canvas.popClipRect();

    // Now outside clip, draw green line
    canvas.drawLine(Point{0, 0}, Point{20, 0}, RgbaColor{0, 255, 0, 255});
    CHECK(fb.getPixel(0, 0) == RgbaColor{0, 255, 0, 255});
    CHECK(fb.getPixel(10, 0) == RgbaColor{0, 255, 0, 255});
    CHECK(fb.getPixel(20, 0) == RgbaColor{0, 255, 0, 255});

    // Anti-aliased rounded rect
    canvas.fillRoundedRect(Rect{100, 100, 40, 40}, 8, RgbaColor{255, 0, 0, 255});
    // Center is solid red
    CHECK(fb.getPixel(120, 120) == RgbaColor{255, 0, 0, 255});
    // Outer corner (100, 100) is outside the circle radius -> remains black
    CHECK(fb.getPixel(100, 100) == RgbaColor{0, 0, 0, 255});
}

TEST_CASE("Canvas2D - Glyph Font Rendering", "[ui][desktop][canvas]") {
    FrameBuffer fb(200, 100);
    Canvas2D canvas(fb);
    canvas.clear(RgbaColor{0, 0, 0, 255});

    Size sz = Canvas2D::measureText("Hello", 14);
    CHECK(sz.width == 40);
    CHECK(sz.height == 20);

    canvas.drawText("A", Point{10, 10}, RgbaColor{255, 255, 255, 255}, 14);

    // Letter 'A' should draw white pixels within [10, 10, 18, 26]
    bool foundWhite = false;
    for (int y = 10; y < 30; ++y) {
        for (int x = 10; x < 20; ++x) {
            if (fb.getPixel(x, y).r > 200) {
                foundWhite = true;
                break;
            }
        }
        if (foundWhite) break;
    }
    CHECK(foundWhite);
}

TEST_CASE("SceneGraphRenderer - Renders UINode Hierarchy to FrameBuffer", "[ui][desktop][renderer]") {
    FrameBuffer fb(400, 300);
    Canvas2D canvas(fb);
    canvas.clear(RgbaColor{30, 30, 46, 255}); // Dark Catppuccin Base

    auto root = std::make_shared<BoxNode>();
    root->setId(1);
    root->setBounds(Rect{0, 0, 400, 300});

    auto btn = std::make_shared<ButtonNode>("Click Me");
    btn->setId(2);
    btn->setBounds(Rect{50, 50, 120, 36});
    btn->getStyle().backgroundColorRgba = RgbaColor{137, 180, 250, 255}; // Accent Blue
    root->appendChild(btn);

    auto input = std::make_shared<InputNode>("Type here");
    input->setId(3);
    input->setBounds(Rect{50, 100, 200, 32});
    root->appendChild(input);

    SceneGraphRenderer::render(root, canvas, 2); // Focused on button

    // Button interior pixel should have button background color
    RgbaColor btnPixel = fb.getPixel(60, 60);
    CHECK(btnPixel.r == 137);
    CHECK(btnPixel.g == 180);
    CHECK(btnPixel.b == 250);

    // Input interior pixel away from text glyphs should have input background color (24, 24, 37)
    RgbaColor inputPixel = fb.getPixel(180, 116);
    CHECK(inputPixel.r == 24);
    CHECK(inputPixel.g == 24);
    CHECK(inputPixel.b == 37);
}

// Test component for DesktopBackend integration
class TestDesktopApp : public Component {
public:
    int counter{0};
    std::string textValue{"initial"};
    std::string hoverStatus{"none"};
    bool enterPressed{false};

    std::shared_ptr<UINode> mount() override {
        auto box = std::make_shared<BoxNode>();
        box->setId(1);
        box->getStyle().flexDirection = FlexDirection::Column;
        box->getStyle().padding = Edges::All(20);
        box->getStyle().gap = 10;

        auto label = std::make_shared<TextNode>("Count: " + std::to_string(counter));
        label->setId(2);
        labelNode_ = label;
        box->appendChild(label);

        auto btn = std::make_shared<ButtonNode>("Increment");
        btn->setId(3);
        btn->getStyle().width = Dimension::Pixels(120);
        btn->getStyle().height = Dimension::Pixels(36);
        btn->on("click", [this](const std::string&) {
            counter++;
            invalidate(0x01);
        });
        btn->on("mouseenter", [this](const std::string&) {
            hoverStatus = "btn_hovered";
        });
        btn->on("mouseleave", [this](const std::string&) {
            hoverStatus = "btn_left";
        });
        box->appendChild(btn);

        auto input = std::make_shared<InputNode>(textValue);
        input->setId(4);
        input->getStyle().width = Dimension::Pixels(200);
        input->getStyle().height = Dimension::Pixels(32);
        input->on("input", [this](const std::string& v) {
            textValue = v;
            invalidate(0x02);
        });
        input->on("keydown", [this](const std::string& key) {
            if (key == "Enter") enterPressed = true;
        });
        inputNode_ = input;
        box->appendChild(input);

        // Scrollable feed
        auto scroll = std::make_shared<ScrollNode>();
        scroll->setId(5);
        scroll->getStyle().width = Dimension::Pixels(300);
        scroll->getStyle().height = Dimension::Pixels(100);
        scroll->getStyle().flexDirection = FlexDirection::Column;

        for (int i = 0; i < 10; ++i) {
            auto item = std::make_shared<TextNode>("Item " + std::to_string(i));
            item->setId(10 + i);
            item->getStyle().height = Dimension::Pixels(25);
            scroll->appendChild(item);
        }
        scrollNode_ = scroll;
        box->appendChild(scroll);

        rootNode_ = box;
        return rootNode_;
    }

    void update(uint32_t mask) override {
        if ((mask & 0x01) && labelNode_) {
            labelNode_->setContent("Count: " + std::to_string(counter));
        }
        if ((mask & 0x02) && labelNode_) {
            labelNode_->setContent("Input: " + textValue);
        }
    }

    std::shared_ptr<TextNode> labelNode_;
    std::shared_ptr<InputNode> inputNode_;
    std::shared_ptr<ScrollNode> scrollNode_;
};

TEST_CASE("DesktopBackend - Spatial Hit Testing, Click, Hover, Key and Scroll Dispatch", "[ui][desktop][backend]") {
    auto app = std::make_shared<TestDesktopApp>();
    DesktopBackend backend(app, 800, 600);

    CHECK(backend.getWidth() == 800);
    CHECK(backend.getHeight() == 600);

    // Initial state
    CHECK(app->counter == 0);
    auto btn = backend.findNodeById(3);
    REQUIRE(btn != nullptr);
    Rect btnBounds = btn->getBounds();

    // 1. Mouse move into button emits mouseenter
    backend.dispatchMouseMove(btnBounds.x + 5, btnBounds.y + 5);
    CHECK(app->hoverStatus == "btn_hovered");

    // Mouse move away emits mouseleave
    backend.dispatchMouseMove(1, 1);
    CHECK(app->hoverStatus == "btn_left");

    // 2. Click button triggers click listener and reactive update
    bool clicked = backend.dispatchClick(btnBounds.x + 10, btnBounds.y + 10);
    CHECK(clicked);
    CHECK(app->counter == 1);
    CHECK(app->labelNode_->getContent() == "Count: 1");

    // 3. Click input sets focus (click towards end of "initial" -> x + 80)
    auto input = backend.findNodeById(4);
    REQUIRE(input != nullptr);
    Rect inputBounds = input->getBounds();

    backend.dispatchClick(inputBounds.x + 80, inputBounds.y + 10);
    CHECK(backend.getFocusManager().getFocusedNodeId() == 4);

    // 4. Keyboard dispatch types into input
    backend.dispatchKey("!");
    CHECK(app->inputNode_->getValue() == "initial!");
    CHECK(app->textValue == "initial!");

    backend.dispatchKey("Enter");
    CHECK(app->enterPressed);

    // Cursor navigation
    backend.dispatchKey("Home");
    CHECK(app->inputNode_->getCursorPos() == 0);
    backend.dispatchKey("ArrowRight");
    CHECK(app->inputNode_->getCursorPos() == 1);

    // 5. Scroll dispatch on ScrollNode
    auto scroll = backend.findNodeById(5);
    REQUIRE(scroll != nullptr);
    Rect scrollBounds = scroll->getBounds();

    CHECK(app->scrollNode_->getScrollOffset() == 0);
    bool scrolled = backend.dispatchScroll(scrollBounds.x + 10, scrollBounds.y + 10, 40);
    CHECK(scrolled);
    CHECK(app->scrollNode_->getScrollOffset() == 40);

    // Snapshot check
    std::string snap = backend.snapshot();
    CHECK(snap.find("<button id=\"3\"") != std::string::npos);
    CHECK(snap.find("<input id=\"4\"") != std::string::npos);
}

TEST_CASE("NativeWindow - WindowConfig Defaults and Headless Driver", "[ui][desktop][window]") {
    WindowConfig config;
    CHECK(config.title == "Keet App");
    CHECK(config.width == 800);
    CHECK(config.height == 600);
    CHECK(config.resizable == true);
    CHECK(config.headless == false);
    CHECK(config.backgroundColor == RgbaColor{30, 30, 46, 255});

    config.title = "Custom Keet Window";
    config.width = 1024;
    config.height = 768;
    config.headless = true;

    auto window = NativeWindow::create(config);
    REQUIRE(window != nullptr);
    CHECK(window->isOpen());
    CHECK(window->getWidth() == 1024);
    CHECK(window->getHeight() == 768);
    CHECK(window->getConfig().title == "Custom Keet Window");

    // Title & resize mutation
    window->setTitle("Renamed Window");
    CHECK(window->getConfig().title == "Renamed Window");

    window->resize(1280, 720);
    CHECK(window->getWidth() == 1280);
    CHECK(window->getHeight() == 720);

    // Present frame buffer
    FrameBuffer fb(1280, 720);
    fb.setPixel(10, 10, RgbaColor{255, 100, 50, 255});
    window->present(fb);

    auto* headlessWin = dynamic_cast<HeadlessNativeWindow*>(window.get());
    REQUIRE(headlessWin != nullptr);
    CHECK(headlessWin->getPresentedFrameCount() == 1);
    REQUIRE(headlessWin->getLastPresentedFrameBuffer() != nullptr);
    CHECK(headlessWin->getLastPresentedFrameBuffer()->getPixel(10, 10).r == 255);

    // Close window
    window->close();
    CHECK(!window->isOpen());
}

TEST_CASE("NativeWindow - Desktop Event Loop and Backend Integration", "[ui][desktop][window]") {
    auto app = std::make_shared<TestDesktopApp>();
    auto backend = mountDesktop(app, 640, 480);
    CHECK(backend.getWidth() == 640);
    CHECK(backend.getHeight() == 480);

    WindowConfig config;
    config.title = "Test App";
    config.width = 640;
    config.height = 480;
    config.headless = true;

    auto window = NativeWindow::create(config);
    REQUIRE(window != nullptr);

    // Initial frame presentation
    window->present(backend.getFrameBuffer());

    auto* headlessWin = dynamic_cast<HeadlessNativeWindow*>(window.get());
    REQUIRE(headlessWin != nullptr);
    CHECK(headlessWin->getPresentedFrameCount() == 1);

    // Event poll on headless window returns open status
    bool stillOpen = window->pollEvents(backend);
    CHECK(stillOpen);

    // Interact with backend and present updated frame
    backend.dispatchClick(30, 60); // Click button
    CHECK(app->counter == 1);

    window->present(backend.getFrameBuffer());
    CHECK(headlessWin->getPresentedFrameCount() == 2);

    window->close();
    CHECK(!window->isOpen());
    CHECK(!window->pollEvents(backend));
}

#if defined(__APPLE__)
TEST_CASE("NativeWindow - Cocoa Driver Instantiation and Surface", "[ui][desktop][window][cocoa]") {
    WindowConfig config;
    config.title = "Keet Cocoa Integration Test";
    config.width = 400;
    config.height = 300;
    config.headless = false;

    auto window = NativeWindow::create(config);
    REQUIRE(window != nullptr);
    CHECK(window->isOpen());
    CHECK(window->getWidth() == 400);
    CHECK(window->getHeight() == 300);

    auto app = std::make_shared<TestDesktopApp>();
    DesktopBackend backend(app, 400, 300);

    // Present frame to Cocoa view
    window->present(backend.getFrameBuffer());

    // Non-blocking event poll
    bool open = window->pollEvents(backend);
    CHECK(open);

    // Close window
    window->close();
    CHECK(!window->isOpen());
}
#endif

TEST_CASE("FlexboxSolver - Desktop Pixel Dual-Unit Flexbox Layout", "[ui][desktop][layout]") {
    LayoutMetrics metrics;
    metrics.unitMode = LayoutUnitMode::DesktopPixels;
    metrics.charWidthPx = 8;
    metrics.lineHeightPx = 20;

    SECTION("Nested StackNode (Row / Column), gap, padding, margin and flexGrow") {
        auto root = std::make_shared<StackNode>(FlexDirection::Column);
        root->getStyle().padding = Edges::All(10);
        root->getStyle().gap = 12;

        auto header = std::make_shared<BoxNode>();
        header->getStyle().height = Dimension::Pixels(40);
        root->appendChild(header);

        auto contentRow = std::make_shared<StackNode>(FlexDirection::Row);
        contentRow->getStyle().flexGrow = 1.0f;
        contentRow->getStyle().gap = 8;
        contentRow->getStyle().alignItems = AlignItems::Stretch;

        auto sidebar = std::make_shared<BoxNode>();
        sidebar->getStyle().width = Dimension::Pixels(180);
        contentRow->appendChild(sidebar);

        auto mainFeed = std::make_shared<ScrollNode>();
        mainFeed->getStyle().flexGrow = 1.0f;
        mainFeed->getStyle().overflowScrollY = true;

        for (int i = 0; i < 20; ++i) {
            auto msg = std::make_shared<TextNode>("Message " + std::to_string(i));
            msg->getStyle().height = Dimension::Pixels(30);
            mainFeed->appendChild(msg);
        }
        contentRow->appendChild(mainFeed);
        root->appendChild(contentRow);

        FlexboxSolver::computeLayout(root, Rect{0, 0, 800, 600}, metrics);

        // Root is [0, 0, 800, 600]
        CHECK(root->getBounds() == Rect{0, 0, 800, 600});

        // Header starts at padding (10, 10), height 40, width 800 - 20 = 780
        CHECK(header->getBounds().x == 10);
        CHECK(header->getBounds().y == 10);
        CHECK(header->getBounds().height == 40);
        CHECK(header->getBounds().width == 780);

        // ContentRow starts at y = 10 + 40 + gap(12) = 62, remaining height = 600 - 20 - 40 - 12 = 528
        CHECK(contentRow->getBounds().x == 10);
        CHECK(contentRow->getBounds().y == 62);
        CHECK(contentRow->getBounds().height == 528);
        CHECK(contentRow->getBounds().width == 780);

        // Sidebar is width 180, starts at x = 10, y = 62
        CHECK(sidebar->getBounds().x == 10);
        CHECK(sidebar->getBounds().y == 62);
        CHECK(sidebar->getBounds().width == 180);
        CHECK(sidebar->getBounds().height == 528);

        // MainFeed starts at x = 10 + 180 + gap(8) = 198, width = 780 - 180 - 8 = 592
        CHECK(mainFeed->getBounds().x == 198);
        CHECK(mainFeed->getBounds().y == 62);
        CHECK(mainFeed->getBounds().width == 592);
        CHECK(mainFeed->getBounds().height == 528);

        // ScrollNode content height: 20 items * 30px = 600px > 528px (overflows!)
        CHECK(mainFeed->getContentHeight() >= 600);
    }
}

TEST_CASE("DesktopBackend - Comprehensive Keyboard Shortcuts, Clipboard & Editing", "[ui][desktop][shortcuts]") {
    auto app = std::make_shared<TestDesktopApp>();
    DesktopBackend backend(app, 800, 600);

    std::string fakeClipboard;
    backend.setClipboardProvider(
        [&fakeClipboard](const std::string& t) { fakeClipboard = t; },
        [&fakeClipboard]() -> std::string { return fakeClipboard; }
    );

    // Focus input node (id 4)
    backend.getFocusManager().setFocus(4);
    REQUIRE(backend.getFocusManager().getFocusedNodeId() == 4);
    auto input = app->inputNode_;
    REQUIRE(input != nullptr);

    // Initial value is "initial"
    CHECK(input->getValue() == "initial");

    // 1. Select All (Cmd+A)
    backend.dispatchKey("SelectAll");
    CHECK(input->hasSelection());
    CHECK(input->getSelectionStart() == 0);
    CHECK(input->getSelectionEnd() == 7);

    // 2. Copy (Cmd+C)
    backend.dispatchKey("Copy");
    CHECK(fakeClipboard == "initial");

    // 3. Cut (Cmd+X)
    backend.dispatchKey("Cut");
    CHECK(input->getValue() == "");
    CHECK(fakeClipboard == "initial");

    // 4. Paste (Cmd+V)
    backend.dispatchKey("Paste");
    CHECK(input->getValue() == "initial");

    // 5. Undo (Cmd+Z)
    backend.dispatchKey("Undo");
    CHECK(input->getValue() == "");

    // 6. Redo (Cmd+Shift+Z)
    backend.dispatchKey("Redo");
    CHECK(input->getValue() == "initial");

    // 7. Type extra words
    input->setCursorPos(7);
    backend.dispatchKey(" ");
    backend.dispatchKey("w");
    backend.dispatchKey("o");
    backend.dispatchKey("r");
    backend.dispatchKey("l");
    backend.dispatchKey("d");
    CHECK(input->getValue() == "initial world");

    // 8. Word navigation (Alt+ArrowLeft)
    backend.dispatchKey("Alt+ArrowLeft");
    CHECK(input->getCursorPos() == 8);

    backend.dispatchKey("Alt+ArrowRight");
    CHECK(input->getCursorPos() == 13);

    // 9. Word delete (Alt+Backspace)
    backend.dispatchKey("Alt+Backspace");
    CHECK(input->getValue() == "initial ");

    // 10. Delete to line start (Cmd+Backspace)
    backend.dispatchKey("Cmd+Backspace");
    CHECK(input->getValue() == "");
}
