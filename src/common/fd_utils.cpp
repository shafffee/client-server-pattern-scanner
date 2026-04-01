#include "fd_utils.hpp"

#include <unistd.h>

#include <cerrno>
#include <stdexcept>
#include <string>

// writes exactly size bytes to a file descriptor
void write_all_fd(int fd, const void* data, std::size_t size) {
    const char* buffer = static_cast<const char*>(data);
    std::size_t total_written = 0;

    while (total_written < size) {
        const ssize_t written = write(fd, buffer + total_written, size - total_written);

        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("Failed to write to file descriptor");
        }

        if (written == 0) {
            throw std::runtime_error("File descriptor was closed while writing");
        }

        total_written += static_cast<std::size_t>(written);
    }
}

// reads everything available from a file descriptor until eof
std::string read_all_fd(int fd) {
    std::string result;
    char buffer[4096];

    while (true) {
        const ssize_t bytes_read = read(fd, buffer, sizeof(buffer));

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("Failed to read from file descriptor");
        }

        if (bytes_read == 0) {
            break;
        }

        result.append(buffer, static_cast<std::size_t>(bytes_read));
    }

    return result;
}