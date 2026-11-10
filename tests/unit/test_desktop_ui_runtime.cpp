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
