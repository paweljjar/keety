#pragma once

#include <iostream>
#include <optional>
#include <string>

#include <keet/json.hpp>

namespace keety::lsp {

/// Handles reading and writing standard LSP framed messages:
/// "Content-Length: <len>\r\n\r\n<json-body>"
class Transport {
public:
    explicit Transport(std::istream& in, std::ostream& out) : in_(in), out_(out) {}

    /// Reads the next JSON-RPC payload from the input stream.
    /// Returns std::nullopt on EOF.
    std::optional<std::string> readMessage();

    /// Writes a JSON payload to the output stream with proper Content-Length headers.
    void writeMessage(const std::string& jsonText);

    /// Formats a raw json string with Content-Length header for testing or writing.
    static std::string frameMessage(const std::string& jsonText);

private:
    std::istream& in_;
    std::ostream& out_;
};

} // namespace keety::lsp
