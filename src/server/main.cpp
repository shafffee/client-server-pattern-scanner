#include "config.hpp"
#include "port.hpp"
#include "scanner.hpp"
#include "net_utils.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>

volatile sig_atomic_t g_child_signal_received = 0;

extern "C" void handle_sigchld(int) {
    g_child_signal_received = 1;
}

void install_sigchld_handler() {
    struct sigaction sa{};
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, nullptr) < 0) {
        throw std::runtime_error("Failed to install SIGCHLD handler");
    }
}

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

void handle_client(int client_socket, const ServerConfig& config) {
    const std::string input = recv_message(client_socket);
    const ScanResult result = scan_content(input, config);
    const std::string response = build_response(result);

    std::cout << "Received " << input.size() << " bytes from client\n";
    send_message(client_socket, response);
}

void cleanup_child_processes() {
    while (true) {
        const pid_t pid = waitpid(-1, nullptr, WNOHANG);

        if (pid > 0) {
            std::cout << "Reaped child process " << pid << "\n";
            continue;
        }

        if (pid == 0) {
            break;
        }

        if (pid < 0) {
            if (errno == ECHILD) {
                break;
            }
            break;
        }
    }
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

        install_sigchld_handler();

        std::cout << "==================================\n";
        std::cout << "SERVER\n";
        std::cout << "==================================\n";

        std::cout << "Port: " << port << "\n";
        std::cout << "Loaded patterns:\n";

        for (const auto& pattern : config.patterns) {
            std::cout << " - " << pattern << "\n";
        }
        std::cout << "==================================\n";
        std::cout << "Waiting for clients...\n";

        while (true) {
            if (g_child_signal_received) {
                g_child_signal_received = 0;
                cleanup_child_processes();
            }

            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);

            const int client_socket = accept(
                server_socket,
                reinterpret_cast<sockaddr*>(&client_addr),
                &client_len
            );

            if (client_socket < 0) {
                if (errno == EINTR) {
                    if (g_child_signal_received) {
                        g_child_signal_received = 0;
                        cleanup_child_processes();
                    }
                    continue;
                }

                std::cerr << "Failed to accept client\n";
                continue;
            }

            const pid_t pid = fork();

            if (pid < 0) {
                std::cerr << "Failed to create child process\n";
                close(client_socket);
                continue;
            }

            if (pid == 0) {
                // child
                close(server_socket);

                try {
                    handle_client(client_socket, config);
                    close(client_socket);
                    _exit(0);
                } catch (const std::exception& e) {
                    std::cerr << "Client handling error: " << e.what() << "\n";
                    close(client_socket);
                    _exit(1);
                }
            }

            // parent
            std::cout << "Child process created with PID: " << pid << "\n";
            close(client_socket);
        }

        close(server_socket);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}