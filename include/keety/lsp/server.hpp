#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <keety/lsp/analyzer.hpp>
#include <keety/lsp/document.hpp>
#include <keety/lsp/protocol.hpp>
#include <keety/lsp/transport.hpp>

namespace keety::lsp {

/// Core LSP Server lifecycle and request dispatcher.
class Server {
public:
    explicit Server(Transport transport);

    /// Runs the message loop on transport until exit notification or EOF.
    /// Returns process exit code (0 on clean exit, non-zero on error).
    int run();

    /// Processes a single raw JSON-RPC string message (useful for testing).
    std::optional<std::string> handleRawMessage(const std::string& rawJson);

    DocumentManager& documentManager() noexcept { return docManager_; }
    const DocumentManager& documentManager() const noexcept { return docManager_; }

    bool isInitialized() const noexcept { return initialized_; }
    bool isShutdown() const noexcept { return shutdownReceived_; }

private:
    void dispatchMessage(const JsonValue& j);
    void handleRequest(const RequestId& id, const std::string& method, const JsonValue& params);
    void handleNotification(const std::string& method, const JsonValue& params);

    // Lifecycle handlers
    JsonValue handleInitialize(const JsonValue& params);
    void handleInitialized(const JsonValue& params);
    JsonValue handleShutdown();
    void handleExit();

    // Document sync handlers
    void handleDidOpen(const JsonValue& params);
    void handleDidChange(const JsonValue& params);
    void handleDidClose(const JsonValue& params);

    // Language feature handlers
    JsonValue handleDefinition(const JsonValue& params);
    JsonValue handleReferences(const JsonValue& params);
    JsonValue handlePrepareRename(const JsonValue& params);
    JsonValue handleRename(const JsonValue& params);
    JsonValue handleHover(const JsonValue& params);
    JsonValue handleSignatureHelp(const JsonValue& params);
    JsonValue handleCompletion(const JsonValue& params);
    JsonValue handleDocumentSymbol(const JsonValue& params);
    JsonValue handleSemanticTokensFull(const JsonValue& params);
    JsonValue handleFormatting(const JsonValue& params);
    JsonValue handleCodeAction(const JsonValue& params);

    void publishDiagnostics(const std::string& uri, int32_t version);

    Transport transport_;
    DocumentManager docManager_;
    Analyzer analyzer_;

    bool initialized_ = false;
    bool shutdownReceived_ = false;
    std::atomic<bool> shouldExit_ = false;
    int exitCode_ = 0;
};

} // namespace keety::lsp
