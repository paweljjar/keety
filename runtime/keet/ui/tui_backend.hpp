#pragma once

#include <keet/ui/flexbox.hpp>
#include <keet/ui/focus_manager.hpp>
#include <keet/ui/geometry.hpp>
#include <keet/ui/node.hpp>
#include <keet/ui/types.hpp>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace keet::ui {

/// Individual cell on terminal character grid
struct TerminalCell {
    char character{'\0'};
    Color fgColor{Color::Default};
    Color bgColor{Color::Default};
    FontStyle fontStyle{FontStyle::Normal};

    bool operator==(const TerminalCell&) const noexcept = default;
};

/// Structured events decoded from terminal stream
struct KeyEvent {
    std::string key; // "Enter", "Tab", "Escape", "ArrowUp", "a", etc.
    bool ctrl{false};
    bool alt{false};
    bool shift{false};
};

struct MouseEvent {
    enum class Action { Click, Drag, ScrollUp, ScrollDown };
    Action action{Action::Click};
    Point position{0, 0};
    int button{0};
};

struct ResizeEvent {
    Size newSize{80, 24};
};

/// RAII Terminal Session Guard.
/// Restores original terminal state, cursor visibility, and exits alternate buffer
/// automatically on normal exit, exception, panic, or OS signal (SIGINT, SIGTERM, SIGSEGV, SIGABRT).
class TerminalSession {
public:
    TerminalSession() {
        initSession();
    }

    ~TerminalSession() {
        restoreSession();
    }

    TerminalSession(const TerminalSession&) = delete;
    TerminalSession& operator=(const TerminalSession&) = delete;

    static TerminalSession& instance() {
        static TerminalSession s_session;
        return s_session;
    }

    static void signalHandler(int sig) {
        instance().restoreSession();
        std::signal(sig, SIG_DFL);
        std::raise(sig);
    }

    Size getTerminalSize() const {
#if defined(_WIN32)
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
            int cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
            return Size{cols, rows};
        }
#else
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
            return Size{ws.ws_col, ws.ws_row};
        }
#endif
        return Size{80, 24};
    }

private:
    void initSession() {
        if (active_) return;
#if defined(_WIN32)
        hIn_ = GetStdHandle(STD_INPUT_HANDLE);
        hOut_ = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleMode(hIn_, &origInMode_);
        GetConsoleMode(hOut_, &origOutMode_);

        // Enable VT processing & disable line input / echo
        DWORD inMode = origInMode_ & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
        inMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
        SetConsoleMode(hIn_, inMode);

        DWORD outMode = origOutMode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut_, outMode);
#else
        tcgetattr(STDIN_FILENO, &origTermios_);
        struct termios raw = origTermios_;
        cfmakeraw(&raw);
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
#endif
        // Switch to alternate screen buffer, hide cursor
        std::cout << "\x1b[?1049h\x1b[?25l" << std::flush;

        // Install signal handlers & atexit
        std::signal(SIGINT, TerminalSession::signalHandler);
        std::signal(SIGTERM, TerminalSession::signalHandler);
        std::signal(SIGSEGV, TerminalSession::signalHandler);
        std::signal(SIGABRT, TerminalSession::signalHandler);

        std::atexit([]() {
            TerminalSession::instance().restoreSession();
        });

        active_ = true;
    }

    void restoreSession() {
        if (!active_) return;
        // Restore cursor, exit alternate buffer
        std::cout << "\x1b[?25h\x1b[?1049l" << std::flush;
#if defined(_WIN32)
        SetConsoleMode(hIn_, origInMode_);
        SetConsoleMode(hOut_, origOutMode_);
#else
        tcsetattr(STDIN_FILENO, TCSANOW, &origTermios_);
#endif
        active_ = false;
    }

    bool active_{false};
#if defined(_WIN32)
    HANDLE hIn_{nullptr};
    HANDLE hOut_{nullptr};
    DWORD origInMode_{0};
    DWORD origOutMode_{0};
#else
    struct termios origTermios_{};
#endif
};

/// ANSI SGR Escape Code Helper
inline std::string colorToAnsiFg(Color c) {
    switch (c) {
        case Color::Black: return "\x1b[30m";
        case Color::Red: return "\x1b[31m";
        case Color::Green: return "\x1b[32m";
        case Color::Yellow: return "\x1b[33m";
        case Color::Blue: return "\x1b[34m";
        case Color::Magenta: return "\x1b[35m";
        case Color::Cyan: return "\x1b[36m";
        case Color::White: return "\x1b[37m";
        case Color::BrightBlack: return "\x1b[90m";
        case Color::BrightRed: return "\x1b[91m";
        case Color::BrightGreen: return "\x1b[92m";
        case Color::BrightYellow: return "\x1b[93m";
        case Color::BrightBlue: return "\x1b[94m";
        case Color::BrightMagenta: return "\x1b[95m";
        case Color::BrightCyan: return "\x1b[96m";
        case Color::BrightWhite: return "\x1b[97m";
        case Color::Default: return "\x1b[39m";
    }
    return "\x1b[39m";
}

inline std::string colorToAnsiBg(Color c) {
    switch (c) {
        case Color::Black: return "\x1b[40m";
        case Color::Red: return "\x1b[41m";
        case Color::Green: return "\x1b[42m";
        case Color::Yellow: return "\x1b[43m";
        case Color::Blue: return "\x1b[44m";
        case Color::Magenta: return "\x1b[45m";
        case Color::Cyan: return "\x1b[46m";
        case Color::White: return "\x1b[47m";
        case Color::BrightBlack: return "\x1b[100m";
        case Color::BrightRed: return "\x1b[101m";
        case Color::BrightGreen: return "\x1b[102m";
        case Color::BrightYellow: return "\x1b[103m";
        case Color::BrightBlue: return "\x1b[104m";
        case Color::BrightMagenta: return "\x1b[105m";
        case Color::BrightCyan: return "\x1b[106m";
        case Color::BrightWhite: return "\x1b[107m";
        case Color::Default: return "\x1b[49m";
    }
    return "\x1b[49m";
}

/// Double-Buffered Differential Terminal User Interface Renderer.
/// Compares frontBuffer and backBuffer cells, emitting ANSI escape codes
/// and repositioning only for modified cells.
class TuiBackend {
public:
    explicit TuiBackend(Size initialSize = Size{80, 24})
        : size_(initialSize),
          frontBuffer_(initialSize.width * initialSize.height),
          backBuffer_(initialSize.width * initialSize.height) {
        clearBackBuffer();
    }

    void resize(Size newSize) {
        size_ = newSize;
        frontBuffer_.assign(size_.width * size_.height, TerminalCell{});
        backBuffer_.assign(size_.width * size_.height, TerminalCell{});
    }

    [[nodiscard]] Size getSize() const noexcept { return size_; }

    void setCell(int x, int y, char ch, Color fg = Color::Default, Color bg = Color::Default, FontStyle style = FontStyle::Normal) {
        if (x < 0 || x >= size_.width || y < 0 || y >= size_.height) return;
        backBuffer_[y * size_.width + x] = TerminalCell{ch, fg, bg, style};
    }

    [[nodiscard]] TerminalCell getCell(int x, int y) const {
        if (x < 0 || x >= size_.width || y < 0 || y >= size_.height) return TerminalCell{};
        return backBuffer_[y * size_.width + x];
    }

    void clearBackBuffer() {
        std::fill(backBuffer_.begin(), backBuffer_.end(), TerminalCell{' ', Color::Default, Color::Default, FontStyle::Normal});
    }

    /// Draws a string at specified coordinates
    void drawString(int x, int y, const std::string& str, Color fg = Color::Default, Color bg = Color::Default, FontStyle style = FontStyle::Normal) {
        for (size_t i = 0; i < str.size(); ++i) {
            setCell(x + static_cast<int>(i), y, str[i], fg, bg, style);
        }
    }

    /// Draws a styled border around rect
    void drawBorder(Rect rect, BorderStyle style, Color color = Color::Default) {
        if (style == BorderStyle::None || rect.width < 2 || rect.height < 2) return;

        char h = '-';
        char v = '|';
        char tl = '+', tr = '+', bl = '+', br = '+';

        if (style == BorderStyle::Rounded) {
            h = '-'; v = '|';
            tl = '/'; tr = '\\'; bl = '\\'; br = '/';
        } else if (style == BorderStyle::Double) {
            h = '='; v = '#';
            tl = '#'; tr = '#'; bl = '#'; br = '#';
        }

        // Top & bottom horizontal bars
        for (int x = rect.x + 1; x < rect.x + rect.width - 1; ++x) {
            setCell(x, rect.y, h, color);
            setCell(x, rect.y + rect.height - 1, h, color);
        }

        // Left & right vertical bars
        for (int y = rect.y + 1; y < rect.y + rect.height - 1; ++y) {
            setCell(rect.x, y, v, color);
            setCell(rect.x + rect.width - 1, y, v, color);
        }

        // Corners
        setCell(rect.x, rect.y, tl, color);
        setCell(rect.x + rect.width - 1, rect.y, tr, color);
        setCell(rect.x, rect.y + rect.height - 1, bl, color);
        setCell(rect.x + rect.width - 1, rect.y + rect.height - 1, br, color);
    }

    /// Renders DOM tree into the back-buffer
    void renderTree(const std::shared_ptr<UINode>& root, std::optional<uint32_t> focusedNodeId = std::nullopt) {
        if (!root) return;
        renderNode(root, focusedNodeId);
    }

    /// Flushes back-buffer to output stream using differential ANSI codes,
    /// updating front-buffer
    std::string flushToString() {
        std::stringstream ss;
        Color currentFg = Color::Default;
        Color currentBg = Color::Default;
        int currentCursorX = -1;
        int currentCursorY = -1;

        for (int y = 0; y < size_.height; ++y) {
            for (int x = 0; x < size_.width; ++x) {
                size_t idx = y * size_.width + x;
                const auto& back = backBuffer_[idx];
                const auto& front = frontBuffer_[idx];

                if (back != front) {
                    // Position cursor if not already at cell
                    if (currentCursorX != x || currentCursorY != y) {
                        ss << "\x1b[" << (y + 1) << ";" << (x + 1) << "H";
                        currentCursorX = x;
                        currentCursorY = y;
                    }

                    // Apply colors
                    if (back.fgColor != currentFg) {
                        ss << colorToAnsiFg(back.fgColor);
                        currentFg = back.fgColor;
                    }
                    if (back.bgColor != currentBg) {
                        ss << colorToAnsiBg(back.bgColor);
                        currentBg = back.bgColor;
                    }

                    // Write character
                    ss << back.character;
                    currentCursorX++;
                    frontBuffer_[idx] = back;
                }
            }
        }

        // Reset attributes
        if (currentFg != Color::Default || currentBg != Color::Default) {
            ss << "\x1b[0m";
        }
        return ss.str();
    }

    void renderToTerminal() {
        std::string diff = flushToString();
        if (!diff.empty()) {
            std::cout << diff << std::flush;
        }
    }

private:
    void renderNode(const std::shared_ptr<UINode>& node, std::optional<uint32_t> focusedNodeId) {
        if (!node || !node->isVisible()) return;

        auto bounds = node->getBounds();
        const auto& style = node->getStyle();

        bool isFocused = (focusedNodeId.has_value() && *focusedNodeId == node->getId());

        // Draw border if requested or if focused
        if (isFocused) {
            drawBorder(bounds, BorderStyle::Single, Color::Cyan);
        } else if (style.borderStyle != BorderStyle::None) {
            drawBorder(bounds, style.borderStyle, style.borderColor);
        }

        // Content rendering based on node kind
        switch (node->getKind()) {
            case NodeKind::Text: {
                auto t = std::static_pointer_cast<TextNode>(node);
                drawString(bounds.x, bounds.y, t->getContent(), style.color, style.backgroundColor, style.fontStyle);
                break;
            }
            case NodeKind::Button: {
                auto b = std::static_pointer_cast<ButtonNode>(node);
                std::string btnText = "[" + b->getLabel() + "]";
                Color fg = isFocused ? Color::Yellow : Color::Default;
                drawString(bounds.x, bounds.y, btnText, fg, style.backgroundColor, isFocused ? FontStyle::Bold : FontStyle::Normal);
                break;
            }
            case NodeKind::Input: {
                auto inp = std::static_pointer_cast<InputNode>(node);
                std::string text = inp->getValue();
                // Draw input box background
                for (int x = bounds.x; x < bounds.x + bounds.width; ++x) {
                    setCell(x, bounds.y, ' ', Color::Default, isFocused ? Color::BrightBlack : Color::Default);
                }
                drawString(bounds.x, bounds.y, text, Color::White, isFocused ? Color::BrightBlack : Color::Default);
                break;
            }
            default:
                break;
        }

        for (const auto& child : node->getChildren()) {
            renderNode(child, focusedNodeId);
        }
    }

    Size size_;
    std::vector<TerminalCell> frontBuffer_;
    std::vector<TerminalCell> backBuffer_;
};

} // namespace keet::ui
