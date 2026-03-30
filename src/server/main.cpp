#include "config.hpp"
#include "port.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " <config_path> <port>\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }

    try {
        const std::string config_path = argv[1];
        const int port = parse_port(argv[2]);

        const ServerConfig config = load_config(config_path);

        std::cout << "server_app\n";
        std::cout << "Port: " << port << "\n";
        std::cout << "Loaded patterns:\n";

        for (const auto& pattern : config.patterns) {
            std::cout << " - " << pattern << "\n";
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}