#include "net_utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>
#include <string>

int connect_to_server(int port) {
    const int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        throw std::runtime_error("Failed to create client socket");
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(static_cast<uint16_t>(port));

    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        close(client_socket);
        throw std::runtime_error("Invalid server address");
    }

    if (connect(client_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        close(client_socket);
        throw std::runtime_error("Failed to connect to server");
    }

    return client_socket;
}

std::string read_all_from_socket(int client_socket) {
    std::string result;
    char buffer[4096];

    while (true) {
        const ssize_t bytes_read = recv(client_socket, buffer, sizeof(buffer), 0);

        if (bytes_read < 0) {
            throw std::runtime_error("Failed to read from client socket");
        }

        if (bytes_read == 0) {
            break;
        }

        result.append(buffer, bytes_read);
    }

    return result;
}

void send_all(int socket_fd, const std::string& data) {
    size_t total_sent = 0;

    while (total_sent < data.size()) {
        const ssize_t sent = send(
            socket_fd,
            data.data() + total_sent,
            data.size() - total_sent,
            0
        );

        if (sent < 0) {
            throw std::runtime_error("Failed to send response to client");
        }

        total_sent += static_cast<size_t>(sent);
    }
}

int create_listening_socket(int port) {
    const int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        throw std::runtime_error("Failed to create server socket");
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server_socket);
        throw std::runtime_error("Failed to set SO_REUSEADDR");
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(server_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        close(server_socket);
        throw std::runtime_error("Failed to bind server socket");
    }

    if (listen(server_socket, 10) < 0) {
        close(server_socket);
        throw std::runtime_error("Failed to listen on server socket");
    }

    return server_socket;
}