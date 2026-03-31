#include "file_reader.hpp"
#include "port.hpp"
#include "net_utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

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

        const int client_socket = connect_to_server(port);

        send_message(client_socket, content);
        shutdown(client_socket, SHUT_WR);

        const std::string response = recv_message(client_socket);

        close(client_socket);

        std::cout << "==================================\n";
        std::cout << "CLIENT\n";
        std::cout << "==================================\n";
        std::cout << "Sent file: " << file_path << "\n";
        std::cout << "Sent bytes: " << content.size() << "\n";
        std::cout << "==================================\n";
        std::cout << response;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}