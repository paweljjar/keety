#pragma once

#include <keet/http.hpp>
#include <keet/ui/http_serve_ui.hpp>

namespace keet::net::http {

inline HttpServer& HttpServer::serveUi(std::string_view route,
                                       std::function<std::shared_ptr<void>()> factory,
                                       std::string title,
                                       std::string scopedCss) {
    ui::UiServeOptions opts;
    opts.title = std::move(title);
    opts.scopedCss = std::move(scopedCss);

    ui::ComponentFactory compFactory = [f = std::move(factory)]() -> std::shared_ptr<ui::Component> {
        auto raw = f();
        return std::static_pointer_cast<ui::Component>(raw);
    };

    ui::registerUiRoutes(*this, route, std::move(compFactory), opts);
    return *this;
}

} // namespace keet::net::http

