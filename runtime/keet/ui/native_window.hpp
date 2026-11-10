#pragma once

#include <keet/ui/canvas.hpp>
#include <keet/ui/desktop_backend.hpp>
#include <keet/ui/geometry.hpp>
#include <keet/ui/types.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#if defined(__APPLE__)
#include <objc/message.h>
#include <objc/runtime.h>
#define Point MacPoint
#define Rect MacRect
#define Size MacSize
#include <CoreGraphics/CoreGraphics.h>
#undef Point
#undef Rect
#undef Size
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#elif defined(__linux__)
#include <dlfcn.h>
#endif

namespace keet::ui {

/// Configuration parameters for native window creation
struct WindowConfig {
    std::string title{"Keet App"};
    int width{800};
    int height{600};
    bool resizable{true};
    bool headless{false};
    RgbaColor backgroundColor{30, 30, 46, 255};
};

/// Abstract base interface for cross-platform desktop windowing drivers
class NativeWindow {
public:
    virtual ~NativeWindow() = default;

    [[nodiscard]] virtual bool isOpen() const noexcept = 0;
    virtual void close() = 0;
    virtual void setTitle(const std::string& title) = 0;
    virtual void resize(int width, int height) = 0;
    [[nodiscard]] virtual int getWidth() const noexcept = 0;
    [[nodiscard]] virtual int getHeight() const noexcept = 0;
    [[nodiscard]] virtual const WindowConfig& getConfig() const noexcept = 0;

    /// Renders the frame buffer to the window display surface
    virtual void present(const FrameBuffer& fb) = 0;

    /// Polls and dispatches any pending OS events to DesktopBackend.
    /// Returns true if the window is still open and running, false if closed.
    virtual bool pollEvents(DesktopBackend& backend) = 0;

    /// Runs a blocking event loop at the specified target framerate (default 60 FPS)
    /// until the window is closed.
    virtual void runEventLoop(DesktopBackend& backend, int targetFps = 60) {
        if (targetFps <= 0) targetFps = 60;
        auto frameDuration = std::chrono::microseconds(1'000'000 / targetFps);

        while (isOpen()) {
            auto frameStart = std::chrono::steady_clock::now();

            if (!pollEvents(backend)) {
                break;
            }

            present(backend.getFrameBuffer());

            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - frameStart);
            if (elapsed < frameDuration) {
                std::this_thread::sleep_for(frameDuration - elapsed);
            }
        }
    }

    /// Factory method instantiating the appropriate OS driver or headless window
    static std::unique_ptr<NativeWindow> create(const WindowConfig& config = WindowConfig{});
};

/// Headless native window implementation for headless testing, CI pipelines, and offscreen rendering
class HeadlessNativeWindow : public NativeWindow {
public:
    explicit HeadlessNativeWindow(WindowConfig config)
        : config_(std::move(config)),
          width_(config_.width),
          height_(config_.height),
          open_(true) {}

    [[nodiscard]] bool isOpen() const noexcept override { return open_; }
    void close() override { open_ = false; }
    void setTitle(const std::string& title) override { config_.title = title; }
    void resize(int width, int height) override {
        width_ = width;
        height_ = height;
        config_.width = width;
        config_.height = height;
    }
    [[nodiscard]] int getWidth() const noexcept override { return width_; }
    [[nodiscard]] int getHeight() const noexcept override { return height_; }
    [[nodiscard]] const WindowConfig& getConfig() const noexcept override { return config_; }

    void present(const FrameBuffer& fb) override {
        lastPresentedFb_ = std::make_unique<FrameBuffer>(fb);
        presentedFrameCount_++;
    }

    bool pollEvents(DesktopBackend& /*backend*/) override {
        return open_;
    }

    [[nodiscard]] const FrameBuffer* getLastPresentedFrameBuffer() const noexcept {
        return lastPresentedFb_.get();
    }

    [[nodiscard]] size_t getPresentedFrameCount() const noexcept {
        return presentedFrameCount_;
    }

private:
    WindowConfig config_;
    int width_{800};
    int height_{600};
    bool open_{true};
    std::unique_ptr<FrameBuffer> lastPresentedFb_{nullptr};
    size_t presentedFrameCount_{0};
};

// ============================================================================
// macOS Cocoa / CoreGraphics Driver (Pure C Objective-C Runtime)
// ============================================================================
#if defined(__APPLE__)

namespace detail {

using NSInteger = intptr_t;
using NSUInteger = uintptr_t;

template <typename Ret, typename... Args>
inline Ret callMsg(id self, SEL sel, Args... args) {
    auto fn = reinterpret_cast<Ret (*)(id, SEL, Args...)>(objc_msgSend);
    return fn(self, sel, args...);
}

template <typename Ret, typename... Args>
inline Ret callClassMsg(Class cls, SEL sel, Args... args) {
    auto fn = reinterpret_cast<Ret (*)(Class, SEL, Args...)>(objc_msgSend);
    return fn(cls, sel, args...);
}

} // namespace detail

class CocoaNativeWindow : public NativeWindow {
public:
    explicit CocoaNativeWindow(WindowConfig config)
        : config_(std::move(config)),
          width_(config_.width),
          height_(config_.height),
          open_(false) {
        initWindow();
    }

    ~CocoaNativeWindow() override {
        close();
    }

    [[nodiscard]] bool isOpen() const noexcept override { return open_; }

    void close() override {
        if (!open_) return;
        open_ = false;
        if (window_) {
            detail::callMsg<void>(window_, sel_registerName("close"));
            window_ = nullptr;
            view_ = nullptr;
        }
    }

    void setTitle(const std::string& title) override {
        config_.title = title;
        if (window_) {
            Class NSStringClass = objc_getClass("NSString");
            id str = detail::callClassMsg<id>(NSStringClass, sel_registerName("stringWithUTF8String:"), title.c_str());
            detail::callMsg<void, id>(window_, sel_registerName("setTitle:"), str);
        }
    }

    void resize(int width, int height) override {
        width_ = width;
        height_ = height;
        config_.width = width;
        config_.height = height;
        if (window_) {
            CGRect frame = detail::callMsg<CGRect>(window_, sel_registerName("frame"));
            frame.size.width = static_cast<double>(width);
            frame.size.height = static_cast<double>(height);
            detail::callMsg<void, CGRect, BOOL>(window_, sel_registerName("setFrame:display:"), frame, YES);
        }
    }

    [[nodiscard]] int getWidth() const noexcept override { return width_; }
    [[nodiscard]] int getHeight() const noexcept override { return height_; }
    [[nodiscard]] const WindowConfig& getConfig() const noexcept override { return config_; }

    void present(const FrameBuffer& fb) override {
        if (!open_ || !view_) return;

        fbWidth_ = fb.getWidth();
        fbHeight_ = fb.getHeight();
        const auto& pixels = fb.getPixels();
        currentFbPixels_.assign(pixels.begin(), pixels.end());

        detail::callMsg<void, BOOL>(view_, sel_registerName("setNeedsDisplay:"), YES);
    }

    bool pollEvents(DesktopBackend& backend) override {
        if (!open_ || !window_) return false;

        BOOL isVis = detail::callMsg<BOOL>(window_, sel_registerName("isVisible"));
        if (!isVis) {
            open_ = false;
            return false;
        }

        Class NSApplicationClass = objc_getClass("NSApplication");
        id app = detail::callClassMsg<id>(NSApplicationClass, sel_registerName("sharedApplication"));
        SEL nextEventSel = sel_registerName("nextEventMatchingMask:untilDate:inMode:dequeue:");
        SEL sendEventSel = sel_registerName("sendEvent:");

        Class NSStringClass = objc_getClass("NSString");
        id defaultMode = detail::callClassMsg<id>(NSStringClass, sel_registerName("stringWithUTF8String:"), "kCFRunLoopDefaultMode");

        while (true) {
            id event = detail::callMsg<id, detail::NSUInteger, id, id, BOOL>(
                app,
                nextEventSel,
                0xFFFFFFFFFFFFFFFFULL, // NSEventMaskAny
                nullptr,               // non-blocking
                defaultMode,
                YES                    // dequeue: YES
            );

            if (!event) break;

            detail::NSUInteger type = detail::callMsg<detail::NSUInteger>(event, sel_registerName("type"));
            CGPoint loc = detail::callMsg<CGPoint>(event, sel_registerName("locationInWindow"));
            int mouseX = static_cast<int>(loc.x);
            int mouseY = static_cast<int>(height_ - loc.y); // Flipped coordinate

            switch (type) {
                case 1: { // NSEventTypeLeftMouseDown
                    backend.dispatchMouseDown(mouseX, mouseY, 0);
                    break;
                }
                case 2: { // NSEventTypeLeftMouseUp
                    backend.dispatchMouseUp(mouseX, mouseY, 0);
                    break;
                }
                case 3: { // NSEventTypeRightMouseDown
                    backend.dispatchMouseDown(mouseX, mouseY, 1);
                    break;
                }
                case 4: { // NSEventTypeRightMouseUp
                    backend.dispatchMouseUp(mouseX, mouseY, 1);
                    break;
                }
                case 5:   // NSEventTypeMouseMoved
                case 6: { // NSEventTypeLeftMouseDragged
                    backend.dispatchMouseMove(mouseX, mouseY);
                    break;
                }
                case 22: { // NSEventTypeScrollWheel
                    double deltaY = detail::callMsg<double>(event, sel_registerName("scrollingDeltaY"));
                    int scrollStep = static_cast<int>(-deltaY * 3.0);
                    if (scrollStep != 0) {
                        backend.dispatchScroll(mouseX, mouseY, scrollStep);
                    }
                    break;
                }
                case 10: { // NSEventTypeKeyDown
                    id charsStr = detail::callMsg<id>(event, sel_registerName("characters"));
                    if (charsStr) {
                        const char* utf8 = detail::callMsg<const char*>(charsStr, sel_registerName("UTF8String"));
                        if (utf8 && utf8[0] != '\0') {
                            std::string key = mapCocoaKey(utf8, event);
                            if (!key.empty()) {
                                backend.dispatchKey(key);
                            }
                        }
                    }
                    break;
                }
                default:
                    break;
            }

            detail::callMsg<void, id>(app, sendEventSel, event);
        }

        // Check if window view size changed
        CGRect bounds = detail::callMsg<CGRect>(view_, sel_registerName("bounds"));
        int curW = static_cast<int>(bounds.size.width);
        int curH = static_cast<int>(bounds.size.height);
        if (curW > 0 && curH > 0 && (curW != width_ || curH != height_)) {
            width_ = curW;
            height_ = curH;
            backend.resize(curW, curH);
        }

        detail::callMsg<void>(app, sel_registerName("updateWindows"));
        return open_;
    }

    void onDrawRect(id /*view*/) {
        if (currentFbPixels_.empty() || fbWidth_ <= 0 || fbHeight_ <= 0) return;

        Class NSGraphicsContextClass = objc_getClass("NSGraphicsContext");
        id curContext = detail::callClassMsg<id>(NSGraphicsContextClass, sel_registerName("currentContext"));
        if (!curContext) return;
        CGContextRef ctx = detail::callMsg<CGContextRef>(curContext, sel_registerName("CGContext"));
        if (!ctx) return;

        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGDataProviderRef provider = CGDataProviderCreateWithData(
            nullptr,
            currentFbPixels_.data(),
            currentFbPixels_.size() * sizeof(uint32_t),
            nullptr
        );

        // ARGB32 little-endian memory layout corresponds to BGRA byte order
        CGBitmapInfo bitmapInfo = static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst) |
                                  static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little);

        CGImageRef image = CGImageCreate(
            fbWidth_,
            fbHeight_,
            8,
            32,
            fbWidth_ * sizeof(uint32_t),
            colorSpace,
            bitmapInfo,
            provider,
            nullptr,
            false,
            kCGRenderingIntentDefault
        );

        if (image) {
            CGContextSaveGState(ctx);
            CGContextTranslateCTM(ctx, 0, height_);
            CGContextScaleCTM(ctx, 1.0, -1.0);
            CGRect destRect = CGRectMake(0, 0, width_, height_);
            CGContextDrawImage(ctx, destRect, image);
            CGContextRestoreGState(ctx);
            CGImageRelease(image);
        }

        CGDataProviderRelease(provider);
        CGColorSpaceRelease(colorSpace);
    }

private:
    static Class getOrCreateViewClass() {
        static Class viewCls = nullptr;
        if (viewCls) return viewCls;

        Class existing = objc_getClass("KeetCocoaContentView");
        if (existing) {
            viewCls = existing;
            return viewCls;
        }

        Class superCls = objc_getClass("NSView");
        viewCls = objc_allocateClassPair(superCls, "KeetCocoaContentView", 0);
        class_addIvar(viewCls, "_nativeWindowPtr", sizeof(void*), alignof(void*), "^v");

        auto drawRectImp = [](id self, SEL /*cmd*/, CGRect /*dirtyRect*/) {
            void* ptr = nullptr;
            object_getInstanceVariable(self, "_nativeWindowPtr", &ptr);
            if (ptr) {
                auto* win = static_cast<CocoaNativeWindow*>(ptr);
                win->onDrawRect(self);
            }
        };
        class_addMethod(viewCls, sel_registerName("drawRect:"), reinterpret_cast<IMP>(+drawRectImp), "v@:{CGRect={CGPoint=dd}{CGSize=dd}}");

        auto acceptsFirstRespImp = [](id /*self*/, SEL /*cmd*/) -> BOOL {
            return YES;
        };
        class_addMethod(viewCls, sel_registerName("acceptsFirstResponder"), reinterpret_cast<IMP>(+acceptsFirstRespImp), "c@:");

        auto isFlippedImp = [](id /*self*/, SEL /*cmd*/) -> BOOL {
            return YES; // Top-left origin
        };
        class_addMethod(viewCls, sel_registerName("isFlipped"), reinterpret_cast<IMP>(+isFlippedImp), "c@:");

        objc_registerClassPair(viewCls);
        return viewCls;
    }

    void initWindow() {
        Class NSApplicationClass = objc_getClass("NSApplication");
        if (!NSApplicationClass) return;

        id app = detail::callClassMsg<id>(NSApplicationClass, sel_registerName("sharedApplication"));
        if (!app) return;

        detail::callMsg<void, detail::NSInteger>(app, sel_registerName("setActivationPolicy:"), 0); // Regular app
        detail::callMsg<void>(app, sel_registerName("finishLaunching"));

        Class NSWindowClass = objc_getClass("NSWindow");

        detail::NSUInteger styleMask = (1 << 0) | (1 << 1) | (1 << 2); // Titled | Closable | Miniaturizable
        if (config_.resizable) {
            styleMask |= (1 << 3); // Resizable
        }

        CGRect rect = CGRectMake(100.0, 100.0, static_cast<double>(width_), static_cast<double>(height_));
        id allocWindow = detail::callClassMsg<id>(NSWindowClass, sel_registerName("alloc"));
        window_ = detail::callMsg<id, CGRect, detail::NSUInteger, detail::NSUInteger, BOOL>(
            allocWindow,
            sel_registerName("initWithContentRect:styleMask:backing:defer:"),
            rect,
            styleMask,
            2, // NSBackingStoreBuffered
            NO
        );

        if (!window_) return;

        setTitle(config_.title);

        Class viewClass = getOrCreateViewClass();
        id allocView = detail::callClassMsg<id>(viewClass, sel_registerName("alloc"));
        view_ = detail::callMsg<id, CGRect>(allocView, sel_registerName("initWithFrame:"), rect);

        void* thisPtr = this;
        object_setInstanceVariable(view_, "_nativeWindowPtr", thisPtr);

        detail::callMsg<void, id>(window_, sel_registerName("setContentView:"), view_);
        detail::callMsg<void, id>(window_, sel_registerName("makeKeyAndOrderFront:"), nullptr);
        detail::callMsg<void, BOOL>(app, sel_registerName("activateIgnoringOtherApps:"), YES);

        open_ = true;
    }

    static std::string mapCocoaKey(const char* utf8, id event) {
        if (!utf8 || utf8[0] == '\0') return "";

        unsigned char c0 = static_cast<unsigned char>(utf8[0]);
        if (c0 == '\r' || c0 == '\n') return "Enter";
        if (c0 == '\t') return "Tab";
        if (c0 == 0x7F || c0 == 0x08) return "Backspace";
        if (c0 == 0x1B) return "Escape";

        unsigned short keyCode = detail::callMsg<unsigned short>(event, sel_registerName("keyCode"));
        switch (keyCode) {
            case 123: return "ArrowLeft";
            case 124: return "ArrowRight";
            case 125: return "ArrowDown";
            case 126: return "ArrowUp";
            case 115: return "Home";
            case 119: return "End";
            case 117: return "Delete";
            case 116: return "PageUp";
            case 121: return "PageDown";
            default: break;
        }

        if (c0 >= 32 && c0 <= 126) {
            return std::string(utf8);
        }
        if (c0 > 127) {
            return std::string(utf8);
        }
        return "";
    }

    WindowConfig config_;
    int width_{800};
    int height_{600};
    bool open_{false};
    id window_{nullptr};
    id view_{nullptr};

    int fbWidth_{0};
    int fbHeight_{0};
    std::vector<uint32_t> currentFbPixels_;
};

#endif // __APPLE__

// ============================================================================
// Windows Win32 Driver (GDI / StretchDIBits)
// ============================================================================
#if defined(_WIN32)

class Win32NativeWindow : public NativeWindow {
public:
    explicit Win32NativeWindow(WindowConfig config)
        : config_(std::move(config)),
          width_(config_.width),
          height_(config_.height),
          open_(false) {
        initWindow();
    }

    ~Win32NativeWindow() override {
        close();
    }

    [[nodiscard]] bool isOpen() const noexcept override { return open_; }

    void close() override {
        if (!open_) return;
        open_ = false;
        if (hwnd_) {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
    }

    void setTitle(const std::string& title) override {
        config_.title = title;
        if (hwnd_) {
            SetWindowTextA(hwnd_, title.c_str());
        }
    }

    void resize(int width, int height) override {
        width_ = width;
        height_ = height;
        config_.width = width;
        config_.height = height;
        if (hwnd_) {
            SetWindowPos(hwnd_, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
        }
    }

    [[nodiscard]] int getWidth() const noexcept override { return width_; }
    [[nodiscard]] int getHeight() const noexcept override { return height_; }
    [[nodiscard]] const WindowConfig& getConfig() const noexcept override { return config_; }

    void present(const FrameBuffer& fb) override {
        if (!open_ || !hwnd_) return;

        fbWidth_ = fb.getWidth();
        fbHeight_ = fb.getHeight();
        const auto& pixels = fb.getPixels();
        currentFbPixels_.assign(pixels.begin(), pixels.end());

        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    bool pollEvents(DesktopBackend& backend) override {
        if (!open_ || !hwnd_) return false;

        backendPtr_ = &backend;

        MSG msg;
        while (PeekMessageW(&msg, hwnd_, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                open_ = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        backendPtr_ = nullptr;
        return open_;
    }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        auto* self = reinterpret_cast<Win32NativeWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!self) {
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }

        switch (uMsg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                if (!self->currentFbPixels_.empty() && self->fbWidth_ > 0 && self->fbHeight_ > 0) {
                    BITMAPINFO bmi{};
                    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    bmi.bmiHeader.biWidth = self->fbWidth_;
                    bmi.bmiHeader.biHeight = -self->fbHeight_; // Top-down DIB
                    bmi.bmiHeader.biPlanes = 1;
                    bmi.bmiHeader.biBitCount = 32;
                    bmi.bmiHeader.biCompression = BI_RGB;

                    StretchDIBits(
                        hdc,
                        0, 0, self->width_, self->height_,
                        0, 0, self->fbWidth_, self->fbHeight_,
                        self->currentFbPixels_.data(),
                        &bmi,
                        DIB_RGB_COLORS,
                        SRCCOPY
                    );
                }
                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_MOUSEMOVE: {
                if (self->backendPtr_) {
                    int x = GET_X_LPARAM(lParam);
                    int y = GET_Y_LPARAM(lParam);
                    self->backendPtr_->dispatchMouseMove(x, y);
                }
                return 0;
            }
            case WM_LBUTTONDOWN: {
                if (self->backendPtr_) {
                    SetCapture(hwnd);
                    int x = GET_X_LPARAM(lParam);
                    int y = GET_Y_LPARAM(lParam);
                    self->backendPtr_->dispatchMouseDown(x, y, 0);
                }
                return 0;
            }
            case WM_LBUTTONUP: {
                if (self->backendPtr_) {
                    ReleaseCapture();
                    int x = GET_X_LPARAM(lParam);
                    int y = GET_Y_LPARAM(lParam);
                    self->backendPtr_->dispatchMouseUp(x, y, 0);
                }
                return 0;
            }
            case WM_RBUTTONDOWN: {
                if (self->backendPtr_) {
                    int x = GET_X_LPARAM(lParam);
                    int y = GET_Y_LPARAM(lParam);
                    self->backendPtr_->dispatchMouseDown(x, y, 1);
                }
                return 0;
            }
            case WM_RBUTTONUP: {
                if (self->backendPtr_) {
                    int x = GET_X_LPARAM(lParam);
                    int y = GET_Y_LPARAM(lParam);
                    self->backendPtr_->dispatchMouseUp(x, y, 1);
                }
                return 0;
            }
            case WM_MOUSEWHEEL: {
                if (self->backendPtr_) {
                    POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                    ScreenToClient(hwnd, &pt);
                    short delta = GET_WHEEL_DELTA_WPARAM(wParam);
                    int step = -(delta / 4);
                    self->backendPtr_->dispatchScroll(pt.x, pt.y, step);
                }
                return 0;
            }
            case WM_KEYDOWN: {
                if (self->backendPtr_) {
                    std::string key;
                    switch (wParam) {
                        case VK_LEFT: key = "ArrowLeft"; break;
                        case VK_RIGHT: key = "ArrowRight"; break;
                        case VK_UP: key = "ArrowUp"; break;
                        case VK_DOWN: key = "ArrowDown"; break;
                        case VK_HOME: key = "Home"; break;
                        case VK_END: key = "End"; break;
                        case VK_BACK: key = "Backspace"; break;
                        case VK_DELETE: key = "Delete"; break;
                        case VK_RETURN: key = "Enter"; break;
                        case VK_TAB: key = "Tab"; break;
                        case VK_ESCAPE: key = "Escape"; break;
                        default: break;
                    }
                    if (!key.empty()) {
                        self->backendPtr_->dispatchKey(key);
                    }
                }
                return 0;
            }
            case WM_CHAR: {
                if (self->backendPtr_ && wParam >= 32 && wParam != 127) {
                    char c = static_cast<char>(wParam);
                    self->backendPtr_->dispatchKey(std::string(1, c));
                }
                return 0;
            }
            case WM_SIZE: {
                int newW = LOWORD(lParam);
                int newH = HIWORD(lParam);
                if (newW > 0 && newH > 0 && (newW != self->width_ || newH != self->height_)) {
                    self->width_ = newW;
                    self->height_ = newH;
                    if (self->backendPtr_) {
                        self->backendPtr_->resize(newW, newH);
                    }
                }
                return 0;
            }
            case WM_CLOSE: {
                self->open_ = false;
                DestroyWindow(hwnd);
                return 0;
            }
            case WM_DESTROY: {
                self->open_ = false;
                self->hwnd_ = nullptr;
                return 0;
            }
            default:
                break;
        }

        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    void initWindow() {
        HINSTANCE hInstance = GetModuleHandleW(nullptr);

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"KeetWindowClass";

        RegisterClassExW(&wc);

        DWORD style = WS_OVERLAPPEDWINDOW;
        if (!config_.resizable) {
            style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
        }

        RECT rc{0, 0, width_, height_};
        AdjustWindowRect(&rc, style, FALSE);

        hwnd_ = CreateWindowExW(
            0,
            L"KeetWindowClass",
            L"Keet App",
            style | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top,
            nullptr, nullptr, hInstance, nullptr
        );

        if (!hwnd_) return;

        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        setTitle(config_.title);
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);

        open_ = true;
    }

    WindowConfig config_;
    int width_{800};
    int height_{600};
    bool open_{false};
    HWND hwnd_{nullptr};
    DesktopBackend* backendPtr_{nullptr};

    int fbWidth_{0};
    int fbHeight_{0};
    std::vector<uint32_t> currentFbPixels_;
};

#endif // _WIN32

// ============================================================================
// Linux X11 Dynamic Driver (Dynamic dlopen of libX11.so.6)
// ============================================================================
#if defined(__linux__) && !defined(__APPLE__)

class X11NativeWindow : public NativeWindow {
public:
    explicit X11NativeWindow(WindowConfig config)
        : config_(std::move(config)),
          width_(config_.width),
          height_(config_.height) {
        initDriver();
    }

    ~X11NativeWindow() override {
        close();
        if (x11Lib_) {
            dlclose(x11Lib_);
            x11Lib_ = nullptr;
        }
    }

    [[nodiscard]] bool isValid() const noexcept { return display_ != nullptr && window_ != 0; }
    [[nodiscard]] bool isOpen() const noexcept override { return open_; }

    void close() override {
        if (!open_) return;
        open_ = false;
        if (display_ && window_) {
            fnXDestroyWindow_(display_, window_);
            window_ = 0;
        }
        if (display_ && gc_) {
            fnXFreeGC_(display_, gc_);
            gc_ = nullptr;
        }
        if (display_) {
            fnXCloseDisplay_(display_);
            display_ = nullptr;
        }
    }

    void setTitle(const std::string& title) override {
        config_.title = title;
        if (display_ && window_ && fnXStoreName_) {
            fnXStoreName_(display_, window_, title.c_str());
        }
    }

    void resize(int width, int height) override {
        width_ = width;
        height_ = height;
        config_.width = width;
        config_.height = height;
    }

    [[nodiscard]] int getWidth() const noexcept override { return width_; }
    [[nodiscard]] int getHeight() const noexcept override { return height_; }
    [[nodiscard]] const WindowConfig& getConfig() const noexcept override { return config_; }

    void present(const FrameBuffer& fb) override {
        if (!open_ || !display_ || !window_ || !gc_) return;

        fbWidth_ = fb.getWidth();
        fbHeight_ = fb.getHeight();
        const auto& pixels = fb.getPixels();
        currentFbPixels_.assign(pixels.begin(), pixels.end());

        void* visual = fnXDefaultVisual_(display_, screen_);
        int depth = fnXDefaultDepth_(display_, screen_);

        void* ximage = fnXCreateImage_(
            display_, visual, depth, 2 /* ZPixmap */, 0,
            reinterpret_cast<char*>(currentFbPixels_.data()),
            fbWidth_, fbHeight_, 32, 0
        );

        if (ximage) {
            fnXPutImage_(display_, window_, gc_, ximage, 0, 0, 0, 0, fbWidth_, fbHeight_);
            // XDestroyImage also tries to free data if not detached, so zero data pointer before destroying
            struct XImageStruct {
                int width, height;
                int xoffset;
                int format;
                char* data;
            };
            auto* imgPtr = reinterpret_cast<XImageStruct*>(ximage);
            imgPtr->data = nullptr;
            fnXDestroyImage_(ximage);
        }

        fnXFlush_(display_);
    }

    bool pollEvents(DesktopBackend& backend) override {
        if (!open_ || !display_ || !window_) return false;

        while (fnXPending_(display_) > 0) {
            alignas(8) char eventBuf[192];
            fnXNextEvent_(display_, eventBuf);

            int type = *reinterpret_cast<int*>(eventBuf);

            // MotionNotify = 6
            if (type == 6) {
                struct XMotionEvent {
                    int type; unsigned long serial; int send_event; void* display;
                    unsigned long window; unsigned long root; unsigned long subwindow;
                    unsigned long time; int x, y;
                };
                auto* motion = reinterpret_cast<XMotionEvent*>(eventBuf);
                backend.dispatchMouseMove(motion->x, motion->y);
            }
            // ButtonPress = 4
            else if (type == 4) {
                struct XButtonEvent {
                    int type; unsigned long serial; int send_event; void* display;
                    unsigned long window; unsigned long root; unsigned long subwindow;
                    unsigned long time; int x, y; int x_root, y_root;
                    unsigned int state; unsigned int button;
                };
                auto* btn = reinterpret_cast<XButtonEvent*>(eventBuf);
                if (btn->button == 4) {
                    backend.dispatchScroll(btn->x, btn->y, -30);
                } else if (btn->button == 5) {
                    backend.dispatchScroll(btn->x, btn->y, 30);
                } else {
                    int b = (btn->button == 1) ? 0 : 1;
                    backend.dispatchMouseDown(btn->x, btn->y, b);
                }
            }
            // ButtonRelease = 5
            else if (type == 5) {
                struct XButtonEvent {
                    int type; unsigned long serial; int send_event; void* display;
                    unsigned long window; unsigned long root; unsigned long subwindow;
                    unsigned long time; int x, y; int x_root, y_root;
                    unsigned int state; unsigned int button;
                };
                auto* btn = reinterpret_cast<XButtonEvent*>(eventBuf);
                if (btn->button != 4 && btn->button != 5) {
                    int b = (btn->button == 1) ? 0 : 1;
                    backend.dispatchMouseUp(btn->x, btn->y, b);
                }
            }
            // KeyPress = 2
            else if (type == 2) {
                char str[32]{};
                unsigned long keysym = 0;
                fnXLookupString_(eventBuf, str, sizeof(str) - 1, &keysym, nullptr);
                std::string key = mapX11Key(keysym, str);
                if (!key.empty()) {
                    backend.dispatchKey(key);
                }
            }
            // ConfigureNotify = 22
            else if (type == 22) {
                struct XConfigureEvent {
                    int type; unsigned long serial; int send_event; void* display;
                    unsigned long event; unsigned long window;
                    int x, y; int width, height; int border_width;
                };
                auto* conf = reinterpret_cast<XConfigureEvent*>(eventBuf);
                if (conf->width > 0 && conf->height > 0 &&
                    (conf->width != width_ || conf->height != height_)) {
                    width_ = conf->width;
                    height_ = conf->height;
                    backend.resize(width_, height_);
                }
            }
            // ClientMessage = 33
            else if (type == 33) {
                struct XClientMessageEvent {
                    int type; unsigned long serial; int send_event; void* display;
                    unsigned long window; unsigned long message_type; int format;
                    long data[5];
                };
                auto* cmsg = reinterpret_cast<XClientMessageEvent*>(eventBuf);
                if (wmDeleteMessage_ && static_cast<unsigned long>(cmsg->data[0]) == wmDeleteMessage_) {
                    open_ = false;
                    return false;
                }
            }
        }

        return open_;
    }

private:
    void initDriver() {
        x11Lib_ = dlopen("libX11.so.6", RTLD_LAZY | RTLD_GLOBAL);
        if (!x11Lib_) return;

        fnXOpenDisplay_ = reinterpret_cast<void* (*)(const char*)>(dlsym(x11Lib_, "XOpenDisplay"));
        fnXCloseDisplay_ = reinterpret_cast<int (*)(void*)>(dlsym(x11Lib_, "XCloseDisplay"));
        fnXDefaultScreen_ = reinterpret_cast<int (*)(void*)>(dlsym(x11Lib_, "XDefaultScreen"));
        fnXRootWindow_ = reinterpret_cast<unsigned long (*)(void*, int)>(dlsym(x11Lib_, "XRootWindow"));
        fnXCreateSimpleWindow_ = reinterpret_cast<unsigned long (*)(void*, unsigned long, int, int, unsigned int, unsigned int, unsigned int, unsigned long, unsigned long)>(dlsym(x11Lib_, "XCreateSimpleWindow"));
        fnXSelectInput_ = reinterpret_cast<int (*)(void*, unsigned long, long)>(dlsym(x11Lib_, "XSelectInput"));
        fnXMapWindow_ = reinterpret_cast<int (*)(void*, unsigned long)>(dlsym(x11Lib_, "XMapWindow"));
        fnXStoreName_ = reinterpret_cast<int (*)(void*, unsigned long, const char*)>(dlsym(x11Lib_, "XStoreName"));
        fnXCreateGC_ = reinterpret_cast<void* (*)(void*, unsigned long, unsigned long, void*)>(dlsym(x11Lib_, "XCreateGC"));
        fnXFreeGC_ = reinterpret_cast<int (*)(void*, void*)>(dlsym(x11Lib_, "XFreeGC"));
        fnXDestroyWindow_ = reinterpret_cast<int (*)(void*, unsigned long)>(dlsym(x11Lib_, "XDestroyWindow"));
        fnXDefaultVisual_ = reinterpret_cast<void* (*)(void*, int)>(dlsym(x11Lib_, "XDefaultVisual"));
        fnXDefaultDepth_ = reinterpret_cast<int (*)(void*, int)>(dlsym(x11Lib_, "XDefaultDepth"));
        fnXCreateImage_ = reinterpret_cast<void* (*)(void*, void*, unsigned int, int, int, char*, unsigned int, unsigned int, int, int)>(dlsym(x11Lib_, "XCreateImage"));
        fnXDestroyImage_ = reinterpret_cast<int (*)(void*)>(dlsym(x11Lib_, "XDestroyImage"));
        fnXPutImage_ = reinterpret_cast<int (*)(void*, unsigned long, void*, void*, int, int, int, int, unsigned int, unsigned int)>(dlsym(x11Lib_, "XPutImage"));
        fnXPending_ = reinterpret_cast<int (*)(void*)>(dlsym(x11Lib_, "XPending"));
        fnXNextEvent_ = reinterpret_cast<int (*)(void*, void*)>(dlsym(x11Lib_, "XNextEvent"));
        fnXLookupString_ = reinterpret_cast<int (*)(void*, char*, int, unsigned long*, void*)>(dlsym(x11Lib_, "XLookupString"));
        fnXInternAtom_ = reinterpret_cast<unsigned long (*)(void*, const char*, int)>(dlsym(x11Lib_, "XInternAtom"));
        fnXSetWMProtocols_ = reinterpret_cast<int (*)(void*, unsigned long, unsigned long*, int)>(dlsym(x11Lib_, "XSetWMProtocols"));
        fnXFlush_ = reinterpret_cast<int (*)(void*)>(dlsym(x11Lib_, "XFlush"));

        if (!fnXOpenDisplay_ || !fnXCreateSimpleWindow_) return;

        display_ = fnXOpenDisplay_(nullptr);
        if (!display_) return;

        screen_ = fnXDefaultScreen_(display_);
        unsigned long root = fnXRootWindow_(display_, screen_);

        window_ = fnXCreateSimpleWindow_(
            display_, root,
            100, 100, width_, height_,
            1, 0, 0
        );

        if (!window_) {
            fnXCloseDisplay_(display_);
            display_ = nullptr;
            return;
        }

        // KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask
        long eventMask = (1L << 0) | (1L << 2) | (1L << 3) | (1L << 6) | (1L << 17);
        fnXSelectInput_(display_, window_, eventMask);

        gc_ = fnXCreateGC_(display_, window_, 0, nullptr);

        if (fnXInternAtom_ && fnXSetWMProtocols_) {
            wmDeleteMessage_ = fnXInternAtom_(display_, "WM_DELETE_WINDOW", 0);
            if (wmDeleteMessage_) {
                fnXSetWMProtocols_(display_, window_, &wmDeleteMessage_, 1);
            }
        }

        setTitle(config_.title);
        fnXMapWindow_(display_, window_);
        fnXFlush_(display_);

        open_ = true;
    }

    static std::string mapX11Key(unsigned long keysym, const char* str) {
        switch (keysym) {
            case 0xFF51: return "ArrowLeft";
            case 0xFF52: return "ArrowUp";
            case 0xFF53: return "ArrowRight";
            case 0xFF54: return "ArrowDown";
            case 0xFF50: return "Home";
            case 0xFF57: return "End";
            case 0xFF08: return "Backspace";
            case 0xFFFF: return "Delete";
            case 0xFF0D: return "Enter";
            case 0xFF09: return "Tab";
            case 0xFF1B: return "Escape";
            default: break;
        }

        if (str && str[0] != '\0') {
            return std::string(str);
        }
        return "";
    }

    WindowConfig config_;
    int width_{800};
    int height_{600};
    bool open_{false};

    void* x11Lib_{nullptr};
    void* display_{nullptr};
    int screen_{0};
    unsigned long window_{0};
    void* gc_{nullptr};
    unsigned long wmDeleteMessage_{0};

    int fbWidth_{0};
    int fbHeight_{0};
    std::vector<uint32_t> currentFbPixels_;

    void* (*fnXOpenDisplay_)(const char*){nullptr};
    int (*fnXCloseDisplay_)(void*){nullptr};
    int (*fnXDefaultScreen_)(void*){nullptr};
    unsigned long (*fnXRootWindow_)(void*, int){nullptr};
    unsigned long (*fnXCreateSimpleWindow_)(void*, unsigned long, int, int, unsigned int, unsigned int, unsigned int, unsigned long, unsigned long){nullptr};
    int (*fnXSelectInput_)(void*, unsigned long, long){nullptr};
    int (*fnXMapWindow_)(void*, unsigned long){nullptr};
    int (*fnXStoreName_)(void*, unsigned long, const char*){nullptr};
    void* (*fnXCreateGC_)(void*, unsigned long, unsigned long, void*){nullptr};
    int (*fnXFreeGC_)(void*, void*){nullptr};
    int (*fnXDestroyWindow_)(void*, unsigned long){nullptr};
    void* (*fnXDefaultVisual_)(void*, int){nullptr};
    int (*fnXDefaultDepth_)(void*, int){nullptr};
    void* (*fnXCreateImage_)(void*, void*, unsigned int, int, int, char*, unsigned int, unsigned int, int, int){nullptr};
    int (*fnXDestroyImage_)(void*){nullptr};
    int (*fnXPutImage_)(void*, unsigned long, void*, void*, int, int, int, int, unsigned int, unsigned int){nullptr};
    int (*fnXPending_)(void*){nullptr};
    int (*fnXNextEvent_)(void*, void*){nullptr};
    int (*fnXLookupString_)(void*, char*, int, unsigned long*, void*){nullptr};
    unsigned long (*fnXInternAtom_)(void*, const char*, int){nullptr};
    int (*fnXSetWMProtocols_)(void*, unsigned long, unsigned long*, int){nullptr};
    int (*fnXFlush_)(void*){nullptr};
};

#endif // __linux__ && !__APPLE__

// ============================================================================
// Factory Implementation
// ============================================================================
inline std::unique_ptr<NativeWindow> NativeWindow::create(const WindowConfig& config) {
    if (config.headless) {
        return std::make_unique<HeadlessNativeWindow>(config);
    }

#if defined(__APPLE__)
    try {
        auto win = std::make_unique<CocoaNativeWindow>(config);
        if (win->isOpen()) {
            return win;
        }
    } catch (...) {}
    return std::make_unique<HeadlessNativeWindow>(config);

#elif defined(_WIN32)
    try {
        auto win = std::make_unique<Win32NativeWindow>(config);
        if (win->isOpen()) {
            return win;
        }
    } catch (...) {}
    return std::make_unique<HeadlessNativeWindow>(config);

#elif defined(__linux__)
    const char* disp = std::getenv("DISPLAY");
    if (!disp || disp[0] == '\0') {
        return std::make_unique<HeadlessNativeWindow>(config);
    }
    try {
        auto win = std::make_unique<X11NativeWindow>(config);
        if (win->isValid()) {
            return win;
        }
    } catch (...) {}
    return std::make_unique<HeadlessNativeWindow>(config);

#else
    return std::make_unique<HeadlessNativeWindow>(config);
#endif
}

} // namespace keet::ui
