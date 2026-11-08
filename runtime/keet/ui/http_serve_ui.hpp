#pragma once

#include <keet/http.hpp>
#include <keet/ui/vdom_backend.hpp>

#include <string>
#include <string_view>

namespace keet::ui {

struct UiServeOptions {
    std::string title{"Keet App"};
    std::string scopedCss{""};
};

/// High-level extension helper that connects an HttpServer to a reactive KML/std.ui component factory.
inline void registerUiRoutes(net::http::HttpServer& server,
                             std::string_view baseRoute,
                             ComponentFactory factory,
                             UiServeOptions options = {}) {
    std::string route = std::string(baseRoute);
    if (route.empty()) route = "/";
    if (route.back() == '/' && route.size() > 1) {
        route.pop_back();
    }

    auto backend = std::make_shared<WebVdomBackend>(std::move(factory));

    // 1. Client runtime asset endpoint: GET <route>/_keet/vdom.js
    std::string scriptPath = (route == "/") ? "/_keet/vdom.js" : (route + "/_keet/vdom.js");
    server.get(scriptPath, [](net::http::Request&, net::http::Response& res) {
        res.type("application/javascript");
        res.setHeader("Cache-Control", "public, max-age=3600");
        res.send(WebVdomBackend::getClientScript());
    });

    // 2. HTML shell host: GET <route>
    server.get(route, [options, scriptPath](net::http::Request&, net::http::Response& res) {
        res.type("text/html; charset=UTF-8");
        std::string css = options.scopedCss;
        if (css.empty()) {
            css = R"css(
* { box-sizing: border-box; }
body {
    margin: 0;
    padding: 0;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
    background-color: #0f172a;
    color: #f8fafc;
    display: flex;
    justify-content: center;
    align-items: center;
    min-height: 100vh;
}
#keet-app {
    display: flex;
    justify-content: center;
    align-items: center;
    width: 100%;
}
.counter-card {
    background: #1e293b;
    border: 1px solid #334155;
    border-radius: 16px;
    padding: 36px 48px;
    box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.5), 0 8px 10px -6px rgba(0, 0, 0, 0.5);
    text-align: center;
    min-width: 380px;
}
.title {
    font-size: 24px;
    font-weight: 700;
    color: #38bdf8;
    margin-bottom: 8px;
    display: block;
}
.count-display {
    font-size: 36px;
    font-weight: 800;
    color: #f1f5f9;
    margin: 24px 0;
    display: block;
    letter-spacing: -0.5px;
}
.button-row {
    display: flex;
    gap: 12px;
    justify-content: center;
    margin-top: 16px;
}
button {
    font-size: 15px;
    font-weight: 600;
    padding: 10px 20px;
    border-radius: 8px;
    border: none;
    cursor: pointer;
    transition: all 0.15s ease;
}
button:active {
    transform: scale(0.96);
}
.btn-primary {
    background: #2563eb;
    color: white;
}
.btn-primary:hover {
    background: #1d4ed8;
}
.btn-secondary {
    background: #475569;
    color: white;
}
.btn-secondary:hover {
    background: #334155;
}
.btn-outline {
    background: transparent;
    border: 1px solid #64748b;
    color: #cbd5e1;
}
.btn-outline:hover {
    background: #334155;
}
.badge {
    margin-top: 24px;
    padding: 10px 16px;
    border-radius: 8px;
    font-size: 14px;
    font-weight: 600;
}
.badge-info {
    background: rgba(56, 189, 248, 0.15);
    color: #38bdf8;
    border: 1px solid rgba(56, 189, 248, 0.3);
}
.badge-success {
    background: rgba(34, 197, 94, 0.15);
    color: #4ade80;
    border: 1px solid rgba(34, 197, 94, 0.3);
}
.badge-warning {
    background: rgba(245, 158, 11, 0.15);
    color: #fbbf24;
    border: 1px solid rgba(245, 158, 11, 0.3);
}
            )css";
        }

        std::stringstream ss;
        ss << "<!DOCTYPE html>\n"
           << "<html lang=\"en\">\n"
           << "<head>\n"
           << "    <meta charset=\"UTF-8\">\n"
           << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
           << "    <title>" << options.title << "</title>\n"
           << "    <style>\n" << css << "\n    </style>\n"
           << "</head>\n"
           << "<body>\n"
           << "    <div id=\"keet-app\"></div>\n"
           << "    <script src=\"" << scriptPath << "\"></script>\n"
           << "</body>\n"
           << "</html>\n";
        res.send(ss.str());
    });

    // 3. WebSocket bridge endpoint: WS /_keet_ws or <route>/_ws
    std::string wsPath = "/_keet_ws";
    server.ws(wsPath, [backend](std::shared_ptr<net::http::WebSocket> ws) {
        if (!ws) return;

        static std::atomic<uint64_t> s_sessionSeq{1};
        std::string sessionId = "session-" + std::to_string(s_sessionSeq++);

        auto session = backend->createSession(sessionId);
        if (!session) return;

        ws->onMessage([ws, session](const std::string& msg) {
            std::string reply = session->handleClientMessage(msg);
            if (!reply.empty()) {
                ws->send(reply);
            }
        });

        ws->onClose([backend, sessionId]([[maybe_unused]] uint16_t code, [[maybe_unused]] const std::string& reason) {
            backend->removeSession(sessionId);
        });

        // Send initial full VDOM snapshot
        std::string initialSync = session->getFullSyncMessage();
        ws->send(initialSync);
    });
}

} // namespace keet::ui
