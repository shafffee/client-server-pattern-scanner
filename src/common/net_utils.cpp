#include "net_utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <cstdint>
#include <cerrno>

#define MAX_MESSAGE_SIZE 16*1024*1024 //16MB

bool is_little_endian() {
    const std::uint16_t value = 0x0001;
    return *reinterpret_cast<const std::uint8_t*>(&value) == 0x01;
}

std::uint64_t byteswap_u64(std::uint64_t value) {
    return ((value & 0x00000000000000FFULL) << 56) |
           ((value & 0x000000000000FF00ULL) << 40) |
           ((value & 0x0000000000FF0000ULL) << 24) |
           ((value & 0x00000000FF000000ULL) << 8)  |
           ((value & 0x000000FF00000000ULL) >> 8)  |
           ((value & 0x0000FF0000000000ULL) >> 24) |
           ((value & 0x00FF000000000000ULL) >> 40) |
           ((value & 0xFF00000000000000ULL) >> 56);
}

std::uint64_t to_big_endian(std::uint64_t value) {
    return is_little_endian() ? byteswap_u64(value) : value;
}

std::uint64_t from_big_endian(std::uint64_t value) {
    return is_little_endian() ? byteswap_u64(value) : value;
}

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

int create_listening_socket(int port) {
    const int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        throw std::runtime_error("Failed to create socket");
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
        throw std::runtime_error("Failed to bind socket");
    }

    if (listen(server_socket, 10) < 0) {
        close(server_socket);
        throw std::runtime_error("Failed to listen on socket");
    }

    return server_socket;
}

void send_data(int socket_fd, const void* data, std::size_t size) {
    const char* buffer = static_cast<const char*>(data);
    size_t total_sent = 0;

    while (total_sent < size) {
        const ssize_t sent = send(
            socket_fd,
            buffer + total_sent,
            size - total_sent,
            MSG_NOSIGNAL
        );

        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("Failed to send data to socket");
        }
        if (sent == 0) {
            throw std::runtime_error("Socket was closed while sending data");
        }

        total_sent += static_cast<size_t>(sent);
    }
}

void recv_data(int socket_fd, void* data, std::size_t size) {
    char* buffer = static_cast<char*>(data);
    std::size_t total_received = 0;

    while (total_received < size) {
        const ssize_t received = recv(
            socket_fd,
            buffer + total_received,
            size - total_received,
            0
        );

        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("Failed to read data from socket");
        }

        if (received == 0) {
            throw std::runtime_error("Socket closed before full message was received");
        }

        total_received += static_cast<std::size_t>(received);
    }
}

void send_message(int socket_fd, const std::string& message) {
    const std::uint64_t size = static_cast<std::uint64_t>(message.size());
    const std::uint64_t size_be = to_big_endian(size);

    send_data(socket_fd, &size_be, sizeof(size_be));

    if (!message.empty()) {
        send_data(socket_fd, message.data(), message.size());
    }
}

std::string recv_message(int socket_fd) {
    std::uint64_t size_be = 0;
    recv_data(socket_fd, &size_be, sizeof(size_be));

    const std::uint64_t size = from_big_endian(size_be);

    if (size > MAX_MESSAGE_SIZE) {
        throw std::runtime_error("Received message is too large");
    }

    std::string message(static_cast<std::size_t>(size), '\0');

    if (size > 0) {
        recv_data(socket_fd, message.data(), static_cast<std::size_t>(size));
    }

    return message;
}