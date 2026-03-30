#include "file_reader.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

std::string read_file(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    return buffer.str();
}