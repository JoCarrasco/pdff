#include <iostream>
#include "./core.h"

int main(const int argc, const char **argv) {
    if (argc < 2) return 1;

    const std::string file_path = argv[1];
    auto core = PDFCore();
    core.open(file_path);
    return core.run();
}