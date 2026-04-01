#include "config.hpp"
#include "port.hpp"
#include "scanner.hpp"
#include "net_utils.hpp"
#include "fd_utils.hpp"
#include "ipc_paths.hpp"
#include "statistics.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#include <cerrno>
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// flags for main to react to signals
volatile sig_atomic_t g_child_signal_received = 0;
volatile sig_atomic_t g_stop_requested = 0;

// keeps the read end of a child report pipe together with the child pid
struct ChildReportPipe {
    int fd = -1;
    pid_t pid = -1;
};

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

// handle one client request and return the scan result for parent statistics
ScanResult handle_client(int client_socket, const ServerConfig& config) {
    const std::string input = recv_message(client_socket);
    const ScanResult result = scan_content(input, config);
    const std::string response = build_response(result);

    std::cout << "Received " << input.size() << " bytes from client\n";
    send_message(client_socket, response);

    return result;
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

// create the shared request fifo if it does not exist yet
void ensure_request_fifo_exists() {
    if (mkfifo(kStatsRequestFifo, 0666) < 0) {
        if (errno != EEXIST) {
            throw std::runtime_error("Failed to create request FIFO");
        }
    }
}

// parse one stats request and send current statistics back to the requested fifo
void process_stats_request_line(const std::string& line, const Statistics& stats) {
    if (line.empty()) {
        return;
    }

    const json request = json::parse(line);
    const std::string reply_fifo = request.at("reply_fifo").get<std::string>();

    const int reply_fd = open(reply_fifo.c_str(), O_WRONLY);
    if (reply_fd < 0) {
        std::cerr << "Failed to open reply FIFO: " << reply_fifo << "\n";
        return;
    }

    const std::string response = build_statistics_json(stats);
    write_all_fd(reply_fd, response);
    close(reply_fd);
}

// read one or more newline-delimited stats requests from the request fifo
void read_request_fifo(int request_fd, std::string& request_buffer, const Statistics& stats) {
    char buffer[1024];

    while (true) {
        const ssize_t bytes_read = read(request_fd, buffer, sizeof(buffer));

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            std::cerr << "Failed to read request FIFO\n";
            break;
        }

        if (bytes_read == 0) {
            break;
        }

        request_buffer.append(buffer, static_cast<std::size_t>(bytes_read));

        while (true) {
            const std::size_t pos = request_buffer.find('\n');
            if (pos == std::string::npos) {
                break;
            }

            const std::string line = request_buffer.substr(0, pos);
            request_buffer.erase(0, pos + 1);

            try {
                process_stats_request_line(line, stats);
            } catch (const std::exception& e) {
                std::cerr << "Failed to process stats request: " << e.what() << "\n";
            }
        }
    }
}

// read one finished child report and merge it into the shared statistics
void process_child_report(ChildReportPipe& entry, Statistics& stats) {
    const std::string report = read_all_fd(entry.fd);
    close(entry.fd);
    entry.fd = -1;

    if (!report.empty()) {
        apply_scan_report_json(stats, report);
    }
}

// start server, accept clients, fork children, handle stats requests, and stop gracefully
int main(int argc, char* argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }

    try {
        const std::string config_path = argv[1];
        const int port = parse_port(argv[2]);

        const ServerConfig config = load_config(config_path);
        Statistics stats{};

        const int server_socket = create_listening_socket(port);

        install_signal_handlers();

        ensure_request_fifo_exists();

        const int request_fifo_fd = open(kStatsRequestFifo, O_RDONLY | O_NONBLOCK);
        if (request_fifo_fd < 0) {
            close(server_socket);
            throw std::runtime_error("Failed to open request FIFO for reading");
        }

        const int request_fifo_dummy_wfd = open(kStatsRequestFifo, O_WRONLY | O_NONBLOCK);
        if (request_fifo_dummy_wfd < 0) {
            close(server_socket);
            close(request_fifo_fd);
            throw std::runtime_error("Failed to open request FIFO dummy writer");
        }

        std::vector<ChildReportPipe> child_reports;
        std::string request_buffer;

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

            std::vector<pollfd> poll_fds;
            poll_fds.push_back({server_socket, POLLIN, 0});
            poll_fds.push_back({request_fifo_fd, POLLIN, 0});

            for (const auto& entry : child_reports) {
                poll_fds.push_back({entry.fd, static_cast<short>(POLLIN | POLLHUP), 0});
            }

            const int ready = poll(poll_fds.data(), poll_fds.size(), -1);

            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }

                std::cerr << "poll() failed\n";
                break;
            }

            if (poll_fds[0].revents & POLLIN) {
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
                } else {
                    int report_pipe[2];
                    if (pipe(report_pipe) < 0) {
                        std::cerr << "Failed to create child report pipe\n";
                        close(client_socket);
                    } else {
                        const pid_t pid = fork();

                        if (pid < 0) {
                            std::cerr << "Failed to create child process\n";
                            close(client_socket);
                            close(report_pipe[0]);
                            close(report_pipe[1]);
                        } else if (pid == 0) {
                            // child handles one client and sends scan report to parent
                            close(server_socket);
                            close(request_fifo_fd);
                            close(request_fifo_dummy_wfd);
                            close(report_pipe[0]);

                            for (const auto& entry : child_reports) {
                                close(entry.fd);
                            }

                            try {
                                const ScanResult result = handle_client(client_socket, config);

                                const std::string report = build_scan_report_json(result);
                                write_all_fd(report_pipe[1], report);

                                close(report_pipe[1]);
                                close(client_socket);
                                _exit(0);
                            } catch (const std::exception& e) {
                                std::cerr << "Client handling error: " << e.what() << "\n";
                                close(report_pipe[1]);
                                close(client_socket);
                                _exit(1);
                            }
                        } else {
                            // parent closes client socket and keeps only child report pipe
                            std::cout << "Child process created with PID: " << pid << "\n";
                            close(client_socket);
                            close(report_pipe[1]);
                            child_reports.push_back({report_pipe[0], pid});
                        }
                    }
                }
            }

            if (poll_fds[1].revents & POLLIN) {
                read_request_fifo(request_fifo_fd, request_buffer, stats);
            }

            for (std::size_t i = 0; i < child_reports.size();) {
                const short revents = poll_fds[2 + i].revents;

                if (revents & (POLLIN | POLLHUP)) {
                    try {
                        process_child_report(child_reports[i], stats);
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to process child report: " << e.what() << "\n";
                        close(child_reports[i].fd);
                    }

                    child_reports.erase(child_reports.begin() + static_cast<std::ptrdiff_t>(i));
                    continue;
                }

                ++i;
            }
        }

        // On stop requested - stop accepting new clients and wait for children
        std::cout << "Shutdown requested. Stopping server...\n";

        close(server_socket);
        close(request_fifo_fd);
        close(request_fifo_dummy_wfd);
        unlink(kStatsRequestFifo);

        for (const auto& entry : child_reports) {
            kill(entry.pid, SIGTERM);
            close(entry.fd);
        }

        cleanup_child_processes();
        wait_for_all_children();

        std::cout << "Server stopped.\n";
        return 0;
    } catch (const std::exception& e) {
        unlink(kStatsRequestFifo);
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}