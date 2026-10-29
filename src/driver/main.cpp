#include <keety/version.hpp>

#include <iostream>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    std::cout << "keety - KeetScript compiler v" << keety::getVersionString() << "\n";
    return 0;
}
