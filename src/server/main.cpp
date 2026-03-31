#include "config.hpp"
#include "port.hpp"
#include "scanner.hpp"

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

        std::cout << "==================================" << std::endl;  
        std::cout << "SERVER                            " << std::endl; 
        std::cout << "==================================" << std::endl;

        std::cout << "Port: " << port << "\n";
        std::cout << "Loaded patterns:\n";

        for (const auto& pattern : config.patterns) {
            std::cout << " - " << pattern << "\n";
        }
        std::cout << "==================================" << std::endl;

        while(true){
            std::string input;
            std::getline(std::cin, input);

            ScanResult result = scan_content(input, config);

            if (result.infected) {
                std::cout << "Scan result: INFECTED\n";
                std::cout << "Matched patterns:\n";
            for (const auto& pattern : result.matched_patterns) {
                std::cout << " [!] " << pattern << "\n";
            }
            } else {
                std::cout << "Scan result: CLEAN\n";
            }
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}