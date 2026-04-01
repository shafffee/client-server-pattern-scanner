#ifndef FD_UTILS_H
#define FD_UTILS_H

#include <cstddef>
#include <string>
#include <string_view>

// writes exactly size bytes to a file descriptor
void write_all_fd(int fd, const void* data, std::size_t size);

// reads everything available from a file descriptor until eof
std::string read_all_fd(int fd);

// overload for writing full string to a file descriptor
inline void write_all_fd(int fd, std::string_view data) {
    write_all_fd(fd, data.data(), data.size());
}

#endif