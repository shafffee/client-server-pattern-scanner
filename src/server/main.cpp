#include "config.hpp"
#include "port.hpp"
#include "scanner.hpp"
#include "net_utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>
#include <string>

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " <config_path> <port>\n";
}

std::string build_response(const ScanResult& result) {
    std::string response;

    if (result.infected) {
        response += "Scan result: INFECTED\n";
        response += "Matched patterns:\n";

        for (const auto& pattern : result.matched_patterns) {
            response += " [!] " + pattern + "\n";
        }
    } else {
        response += "Scan result: CLEAN\n";
    }

    return response;
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
        const int server_socket = create_listening_socket(port);

        std::cout << "==================================" << std::endl;  
        std::cout << "SERVER                            " << std::endl; 
        std::cout << "==================================" << std::endl;

        std::cout << "Port: " << port << "\n";
        std::cout << "Loaded patterns:\n";

        for (const auto& pattern : config.patterns) {
            std::cout << " - " << pattern << "\n";
        }
        std::cout << "==================================" << std::endl;

        while (true) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);

            const int client_socket = accept(
                server_socket,
                reinterpret_cast<sockaddr*>(&client_addr),
                &client_len
            );

            if (client_socket < 0) {
                std::cerr << "Failed to accept client\n";
                continue;
            }

            try {
                const std::string input = recv_message(client_socket);
                const ScanResult result = scan_content(input, config);
                const std::string response = build_response(result);

                std::cout << "Received " << input.size() << " bytes from client\n";
                send_message(client_socket, response);
            } catch (const std::exception& e) {
                std::cerr << "Client handling error: " << e.what() << "\n";
            }

            close(client_socket);
        }

        close(server_socket);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}