#include <iostream>

#include <keety/lsp/server.hpp>
#include <keety/lsp/transport.hpp>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

int main() {
#if defined(_WIN32)
    // Set stdin and stdout to binary mode on Windows to preserve exact \r\n byte framing
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    keety::lsp::Transport transport(std::cin, std::cout);
    keety::lsp::Server server(std::move(transport));

    return server.run();
}
