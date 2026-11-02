#include <keety/lsp/transport.hpp>

#include <cctype>
#include <charconv>
#include <string_view>
#include <vector>

namespace keety::lsp {

std::string Transport::frameMessage(const std::string& jsonText) {
    std::string out = "Content-Length: ";
    out += std::to_string(jsonText.size());
    out += "\r\n\r\n";
    out += jsonText;
    return out;
}

void Transport::writeMessage(const std::string& jsonText) {
    out_ << "Content-Length: " << jsonText.size() << "\r\n\r\n" << jsonText;
    out_.flush();
}

std::optional<std::string> Transport::readMessage() {
    size_t contentLength = 0;
    bool foundContentLength = false;

    // Read headers until blank line (\r\n\r\n or \n\n)
    while (true) {
        std::string line;
        while (true) {
            int ch = in_.get();
            if (ch == EOF) {
                if (line.empty() && !foundContentLength) {
                    return std::nullopt;
                }
                break;
            }
            if (ch == '\n') {
                break;
            }
            line.push_back(static_cast<char>(ch));
        }

        // Strip trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Empty line indicates end of headers
        if (line.empty()) {
            if (foundContentLength) {
                break;
            }
            // If EOF hit before header finished
            if (in_.eof()) {
                return std::nullopt;
            }
            continue;
        }

        // Parse Header
        static const std::string kContentLength = "Content-Length:";
        if (line.size() >= kContentLength.size() &&
            line.compare(0, kContentLength.size(), kContentLength) == 0) {
            std::string_view val = std::string_view(line).substr(kContentLength.size());
            while (!val.empty() && std::isspace(static_cast<unsigned char>(val.front()))) {
                val.remove_prefix(1);
            }
            while (!val.empty() && std::isspace(static_cast<unsigned char>(val.back()))) {
                val.remove_suffix(1);
            }

            size_t parsed = 0;
            auto res = std::from_chars(val.data(), val.data() + val.size(), parsed);
            if (res.ec == std::errc()) {
                contentLength = parsed;
                foundContentLength = true;
            }
        }
    }

    if (!foundContentLength || contentLength == 0) {
        return std::nullopt;
    }

    // Read exactly contentLength bytes
    std::string body;
    body.resize(contentLength);
    in_.read(&body[0], static_cast<std::streamsize>(contentLength));
    if (static_cast<size_t>(in_.gcount()) < contentLength) {
        return std::nullopt; // Incomplete body
    }

    return body;
}

} // namespace keety::lsp
