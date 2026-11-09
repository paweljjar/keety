#include <catch2/catch_test_macros.hpp>
#include <keet/std/http.hpp>
#include <keet/std/ui.hpp>

using namespace keet::net::http;
using namespace keet::ui;

class HttpTestCounter : public Component {
public:
    int count{0};

    std::shared_ptr<UINode> mount() override {
        auto box = std::make_shared<BoxNode>();
        box->setId(1);
        auto text = std::make_shared<TextNode>("Count: " + std::to_string(count));
        text->setId(2);
        box->appendChild(text);
        rootNode_ = box;
        return rootNode_;
    }
};

TEST_CASE("HttpServer.serveUi - Route Registration & Assets", "[http][ui]") {
    HttpServer server;
    server.serveUi("/", []() {
        return std::make_shared<HttpTestCounter>();
    }, "My Keet UI App", ".box { color: red; }");

    // Verify HTML host endpoint via internal router lookup
    auto& r = server.router();
    auto htmlHandler = r.match("GET", "/");
    CHECK(htmlHandler.status == RouteMatchStatus::Found);

    HttpRequestMessage rawReq;
    auto msg = htmlHandler.handler(rawReq);

    CHECK(msg.headers.get("Content-Type") == "text/html; charset=UTF-8");
    CHECK(msg.body.find("<title>My Keet UI App</title>") != std::string::npos);
    CHECK(msg.body.find(".box { color: red; }") != std::string::npos);
    CHECK(msg.body.find("<div id=\"keet-app\"></div>") != std::string::npos);
    CHECK(msg.body.find("src=\"/_keet/vdom.js\"") != std::string::npos);

    // Verify vdom.js asset endpoint
    auto jsHandler = r.match("GET", "/_keet/vdom.js");
    CHECK(jsHandler.status == RouteMatchStatus::Found);

    HttpRequestMessage jsReq;
    auto jsMsg = jsHandler.handler(jsReq);

    CHECK(jsMsg.headers.get("Content-Type") == "application/javascript");
    CHECK(jsMsg.body.find("keet-vdom") != std::string::npos);
    CHECK(jsMsg.body.find("WebSocket") != std::string::npos);
}
