#include <catch2/catch_test_macros.hpp>
#include <keet/std/ui.hpp>

using namespace keet::ui;

class WebCounterComponent : public Component {
public:
    int count{0};

    std::shared_ptr<UINode> mount() override {
        auto box = std::make_shared<BoxNode>();
        box->setId(1);
        box->setAttribute("class", "counter-card");

        textNode_ = std::make_shared<TextNode>("Count: " + std::to_string(count));
        textNode_->setId(2);
        box->appendChild(textNode_);

        btnNode_ = std::make_shared<ButtonNode>("Click Me");
        btnNode_->setId(3);
        btnNode_->on("click", [this]([[maybe_unused]] const std::string& _) {
            count++;
            invalidate(0x01);
        });
        box->appendChild(btnNode_);

        rootNode_ = box;
        return rootNode_;
    }

    void update([[maybe_unused]] uint32_t mask) override {
        textNode_->setContent("Count: " + std::to_string(count));
    }

private:
    std::shared_ptr<TextNode> textNode_;
    std::shared_ptr<ButtonNode> btnNode_;
};

TEST_CASE("Web VDOM Engine - VNode Tree & Serialization", "[ui][web][vdom]") {
    auto comp = std::make_shared<WebCounterComponent>();
    auto root = comp->mount();

    auto vnode = VNode::fromUINode(root);
    REQUIRE(vnode != nullptr);
    CHECK(vnode->tag == "div");
    CHECK(vnode->id == "1");
    CHECK(vnode->attributes["class"] == "counter-card");
    REQUIRE(vnode->children.size() == 2);

    CHECK(vnode->children[0]->tag == "span");
    CHECK(vnode->children[0]->id == "2");
    CHECK(vnode->children[0]->text == "Count: 0");

    CHECK(vnode->children[1]->tag == "button");
    CHECK(vnode->children[1]->id == "3");
    CHECK(vnode->children[1]->text == "Click Me");

    std::string json = vnode->toJson();
    CHECK(json.find("\"tag\":\"div\"") != std::string::npos);
    CHECK(json.find("\"class\":\"counter-card\"") != std::string::npos);
    CHECK(json.find("\"text\":\"Count: 0\"") != std::string::npos);
}

TEST_CASE("Web VDOM Engine - Diff Patch Generation", "[ui][web][vdom]") {
    auto comp = std::make_shared<WebCounterComponent>();
    auto root = comp->mount();

    auto vnodeOld = VNode::fromUINode(root);

    // Increment count & update
    comp->count = 5;
    comp->update(0x01);

    auto vnodeNew = VNode::fromUINode(root);

    auto patches = VDomDiffer::diff(vnodeOld, vnodeNew);
    REQUIRE(patches.size() == 1);
    CHECK(patches[0].op == VDomPatch::Op::ReplaceText);
    CHECK(patches[0].targetId == "2");
    CHECK(patches[0].value == "Count: 5");

    std::string patchesJson = VDomDiffer::patchesToJson(patches);
    CHECK(patchesJson.find("\"replaceText\"") != std::string::npos);
    CHECK(patchesJson.find("\"id\":\"2\"") != std::string::npos);
    CHECK(patchesJson.find("\"val\":\"Count: 5\"") != std::string::npos);
}

TEST_CASE("Web VDOM Session Backend - Multi-Client Isolation & Event Ingestion", "[ui][web][backend]") {
    WebVdomBackend backend([]() {
        return std::make_shared<WebCounterComponent>();
    });

    // Client 1
    auto s1 = backend.createSession("client-1");
    REQUIRE(s1 != nullptr);

    // Client 2 (Isolated)
    auto s2 = backend.createSession("client-2");
    REQUIRE(s2 != nullptr);
    CHECK(backend.sessionCount() == 2);

    // Client 1 receives full snapshot on sync
    std::string syncMsg = s1->handleClientMessage("{\"type\": \"sync\"}");
    CHECK(syncMsg.find("\"vdom_full\"") != std::string::npos);
    CHECK(syncMsg.find("Count: 0") != std::string::npos);

    // Client 1 sends click event
    std::string patchMsg = s1->handleClientMessage("{\"type\": \"event\", \"id\": \"3\", \"event\": \"click\", \"data\": {}}");
    CHECK(patchMsg.find("\"vdom_patch\"") != std::string::npos);
    CHECK(patchMsg.find("Count: 1") != std::string::npos);

    auto c1 = std::static_pointer_cast<WebCounterComponent>(s1->getComponent());
    auto c2 = std::static_pointer_cast<WebCounterComponent>(s2->getComponent());

    // State isolation check: Client 1 mutated, Client 2 unaffected
    CHECK(c1->count == 1);
    CHECK(c2->count == 0);

    // HTML Shell check
    std::string shell = WebVdomBackend::getHtmlShell("My Title", ".counter-card { color: red; }");
    CHECK(shell.find("<title>My Title</title>") != std::string::npos);
    CHECK(shell.find(".counter-card { color: red; }") != std::string::npos);
    CHECK(shell.find("keet-app") != std::string::npos);
    CHECK(shell.find("_keet_ws") != std::string::npos);

    backend.removeSession("client-1");
    CHECK(backend.sessionCount() == 1);
}

