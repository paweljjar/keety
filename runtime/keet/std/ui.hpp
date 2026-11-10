#pragma once

#include <keet/ui/types.hpp>
#include <keet/ui/geometry.hpp>
#include <keet/ui/node.hpp>
#include <keet/ui/flexbox.hpp>
#include <keet/ui/focus_manager.hpp>
#include <keet/ui/tui_backend.hpp>
#include <keet/ui/vdom.hpp>
#include <keet/ui/vdom_backend.hpp>
#include <keet/ui/scheduler.hpp>
#include <keet/ui/component.hpp>
#include <keet/ui/headless_backend.hpp>
#include <keet/ui/canvas.hpp>
#include <keet/ui/desktop_backend.hpp>
#include <keet/ui/native_window.hpp>

namespace keet::ui {

/// Mounts a UI component onto a desktop raster backend
inline std::shared_ptr<DesktopBackend> mountDesktop(std::shared_ptr<Component> root, int width = 800, int height = 600) {
    return std::make_shared<DesktopBackend>(std::move(root), width, height);
}

/// Renders a UI component into a native OS window with event loop execution
inline void renderDesktop(std::shared_ptr<Component> root, const WindowConfig& config = WindowConfig{}) {
    auto backend = mountDesktop(root, config.width, config.height);
    auto window = NativeWindow::create(config);
    if (!window) return;
    window->present(backend->getFrameBuffer());
    window->runEventLoop(*backend);
}

/// Renders a UI component into a native OS window by title and dimensions
inline void renderDesktop(std::shared_ptr<Component> root, const std::string& title, int width = 800, int height = 600) {
    WindowConfig config;
    config.title = title;
    config.width = width;
    config.height = height;
    renderDesktop(std::move(root), config);
}

} // namespace keet::ui

namespace keet_module_ui {
    using namespace ::keet::ui;
}

