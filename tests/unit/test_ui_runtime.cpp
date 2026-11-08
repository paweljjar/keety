#include <catch2/catch_test_macros.hpp>
#include <keet/std/ui.hpp>

using namespace keet::ui;

// Concrete test component simulating compiled KML component
class CounterComponent : public Component {
public:
    int count{0};
    std::string textPrefix{"Current: "};

    static constexpr uint32_t DIRTY_COUNT = 0x01;
    static constexpr uint32_t DIRTY_PREFIX = 0x02;

    std::shared_ptr<UINode> mount() override {
        auto box = std::make_shared<BoxNode>();
        box->setId(1);
        box->setAttribute("class", "counter-card");

        textNode_ = std::make_shared<TextNode>(textPrefix + std::to_string(count));
        textNode_->setId(2);
        box->appendChild(textNode_);

        btnNode_ = std::make_shared<ButtonNode>("Increment");
        btnNode_->setId(3);
        btnNode_->on("click", [this]([[maybe_unused]] const std::string& _) {
            count++;
            invalidate(DIRTY_COUNT);
        });
        box->appendChild(btnNode_);

        inputNode_ = std::make_shared<InputNode>("hello");
        inputNode_->setId(4);
        inputNode_->on("input", [this](const std::string& val) {
            textPrefix = val + ": ";
            invalidate(DIRTY_PREFIX);
        });
        box->appendChild(inputNode_);

        rootNode_ = box;
        return rootNode_;
    }

    void update(uint32_t mask) override {
        if ((mask & DIRTY_COUNT) || (mask & DIRTY_PREFIX)) {
            textNode_->setContent(textPrefix + std::to_string(count));
        }
    }

private:
    std::shared_ptr<TextNode> textNode_;
    std::shared_ptr<ButtonNode> btnNode_;
    std::shared_ptr<InputNode> inputNode_;
};

TEST_CASE("std.ui Universal Runtime - Primitives & Geometry", "[ui][geometry]") {
    Point p{10, 20};
    Size s{100, 50};
    CHECK(s.width == 100);
    CHECK(s.height == 50);
    Rect r{10, 20, 100, 50};

    CHECK(r.contains(p));
    CHECK(r.contains(15, 25));
    CHECK(!r.contains(5, 5));
    CHECK(!r.contains(120, 80));

    Edges padding = Edges::Symmetric(2, 4);
    CHECK(padding.vertical() == 4);
    CHECK(padding.horizontal() == 8);

    Dimension dAuto = Dimension::Auto();
    CHECK(dAuto.mode == SizeMode::Auto);
    Dimension dCells = Dimension::Cells(20);
    CHECK(dCells.value == 20);
}

TEST_CASE("std.ui Universal Runtime - Node Hierarchy & Events", "[ui][node]") {
    auto box = std::make_shared<BoxNode>();
    box->setId(10);
    box->setAttribute("class", "container");

    auto btn = std::make_shared<ButtonNode>("Submit");
    btn->setId(11);

    bool clicked = false;
    btn->on("click", [&]([[maybe_unused]] const std::string& _) {
        clicked = true;
    });

    box->appendChild(btn);

    CHECK(box->getChildren().size() == 1);
    CHECK(btn->getParent() == box);
    CHECK(box->findById(11) == btn);

    btn->emit("click");
    CHECK(clicked);
}

TEST_CASE("std.ui Universal Runtime - Microtask Scheduler & Reactive Updates", "[ui][scheduler]") {
    auto comp = std::make_shared<CounterComponent>();
    HeadlessBackend backend(comp);

    CHECK(comp->count == 0);
    auto text = std::static_pointer_cast<TextNode>(backend.findById(2));
    REQUIRE(text != nullptr);
    CHECK(text->getContent() == "Current: 0");

    // Click button via synthetic event simulator
    backend.simulateClick(3);

    // After simulateClick, microtask scheduler has ticked
    CHECK(comp->count == 1);
    CHECK(text->getContent() == "Current: 1");

    // Type new prefix
    backend.simulateType(4, "Val");
    CHECK(comp->textPrefix == "Val: ");
    CHECK(text->getContent() == "Val: 1");
}

TEST_CASE("std.ui Headless Driver - Queries and Snapshot", "[ui][headless]") {
    auto comp = std::make_shared<CounterComponent>();
    HeadlessBackend backend(comp);

    // Query tests
    auto byId = backend.findById(3);
    REQUIRE(byId != nullptr);
    CHECK(byId->getKind() == NodeKind::Button);

    auto byText = backend.findByText("Current: 0");
    REQUIRE(byText != nullptr);
    CHECK(byText->getId() == 2);

    auto bySelector = backend.find(".counter-card");
    REQUIRE(bySelector != nullptr);
    CHECK(bySelector->getId() == 1);

    auto btnByTag = backend.find("button");
    REQUIRE(btnByTag != nullptr);
    CHECK(btnByTag->getId() == 3);

    // Snapshot test
    std::string snap = backend.snapshot();
    CHECK(snap.find("<box id=\"1\" class=\"counter-card\">") != std::string::npos);
    CHECK(snap.find("<text id=\"2\">Current: 0</text>") != std::string::npos);
    CHECK(snap.find("<button id=\"3\">Increment</button>") != std::string::npos);
}
