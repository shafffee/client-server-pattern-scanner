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

// flags for main to react to signals
volatile sig_atomic_t g_child_signal_received = 0;
volatile sig_atomic_t g_stop_requested = 0;

// mark that a child has finished
extern "C" void handle_sigchld(int) {
    g_child_signal_received = 1;
}

// mark that server shutdown was requested
extern "C" void handle_termination_signal(int) {
    g_stop_requested = 1;
}

// install all signal handlers used by the server
void install_signal_handlers() {
    // SIGCHLD
    struct sigaction chld_action{};
    chld_action.sa_handler = handle_sigchld;
    sigemptyset(&chld_action.sa_mask);
    chld_action.sa_flags = SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &chld_action, nullptr) < 0) {
        throw std::runtime_error("Failed to install SIGCHLD handler");
    }

    // SIGINT / SIGTERM
    struct sigaction term_action{};
    term_action.sa_handler = handle_termination_signal;
    sigemptyset(&term_action.sa_mask);
    term_action.sa_flags = 0;

    if (sigaction(SIGINT, &term_action, nullptr) < 0) {
        throw std::runtime_error("Failed to install SIGINT handler");
    }

    if (sigaction(SIGTERM, &term_action, nullptr) < 0) {
        throw std::runtime_error("Failed to install SIGTERM handler");
    }
}

// print server usage
void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " <config_path> <port>\n";
}


// create a response for the client
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

// handle one client request
void handle_client(int client_socket, const ServerConfig& config) {
    const std::string input = recv_message(client_socket);
    const ScanResult result = scan_content(input, config);
    const std::string response = build_response(result);

    std::cout << "Received " << input.size() << " bytes from client\n";
    send_message(client_socket, response);
}

// collect finished child processes to avoid zombies
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

// wait for all children to finish
void wait_for_all_children() {
    while (true) {
        const pid_t pid = waitpid(-1, nullptr, 0);

        if (pid > 0) {
            std::cout << "Waited for child process " << pid << "\n";
            continue;
        }

        if (pid < 0) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == ECHILD) {
                break;
            }

            break;
        }
    }
}

// start server, accept clients, fork children, and stop gracefully
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

        install_signal_handlers();

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

        while (!g_stop_requested) {
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

                    if (g_stop_requested) {
                        break;
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
                // child handles one client and then exits
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

            // parent closes client socket and goes back to accept
            std::cout << "Child process created with PID: " << pid << "\n";
            close(client_socket);
        }

        // On stop requested - stop accepting new clients and wait for children
        std::cout << "Shutdown requested. Stopping server...\n";

        close(server_socket);

        cleanup_child_processes();
        wait_for_all_children();

        std::cout << "Server stopped.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}