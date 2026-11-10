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

/// Loads a vector font (.ttf or .otf) from file and registers it under the given family name
inline bool loadFont(const std::string& family, const std::string& path) {
    return FontManager::instance().registerFont(family, path, true);
}

/// Mounts a UI component onto a desktop raster backend
template <typename T>
inline DesktopBackend mountDesktop(T&& root, int width = 800, int height = 600, float dpiScale = 1.0f) {
    if constexpr (std::is_convertible_v<T, std::shared_ptr<Component>>) {
        return DesktopBackend(std::forward<T>(root), width, height, dpiScale);
    } else if constexpr (std::is_base_of_v<Component, std::decay_t<T>>) {
        return DesktopBackend(std::make_shared<std::decay_t<T>>(std::forward<T>(root)), width, height, dpiScale);
    } else {
        return DesktopBackend(nullptr, width, height, dpiScale);
    }
}

/// Mounts a UI component onto a desktop raster backend (heap pointer)
inline std::shared_ptr<DesktopBackend> mountDesktopPtr(std::shared_ptr<Component> root, int width = 800, int height = 600, float dpiScale = 1.0f) {
    return std::make_shared<DesktopBackend>(std::move(root), width, height, dpiScale);
}

/// Renders a UI component into a native OS window with event loop execution
template <typename T>
inline void renderDesktop(T&& root, const WindowConfig& config = WindowConfig{}) {
    auto window = NativeWindow::create(config);
    if (!window) return;
    float scale = window->getBackingScaleFactor();
    auto backend = mountDesktop(std::forward<T>(root), config.width, config.height, scale);
    window->present(backend.getFrameBuffer());
    window->runEventLoop(backend);
}

/// Renders a UI component into a native OS window by title and dimensions
template <typename T>
inline void renderDesktop(T&& root, const std::string& title, int width = 800, int height = 600) {
    WindowConfig config;
    config.title = title;
    config.width = width;
    config.height = height;
    renderDesktop(std::forward<T>(root), config);
}

} // namespace keet::ui

namespace keet_module_ui {
    using namespace ::keet::ui;
}

