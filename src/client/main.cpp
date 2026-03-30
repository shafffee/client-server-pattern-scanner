#include "file_reader.hpp"
#include "port.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " <file_path> <port>\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }

    try {
        const std::string file_path = argv[1];
        const int port = parse_port(argv[2]);
        const std::string content = read_file(file_path);

        std::cout << "client_app\n";
        std::cout << "Port: " << port << "\n";
        std::cout << "File size: " << content.size() << " bytes\n";
        std::cout << "File content:\n";
        std::cout << content << "\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}