#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <string>

int connect_to_server(int port);
int create_listening_socket(int port);

void send_data(int socket_fd, const void* data, std::size_t size);
void recv_data(int socket_fd, void* data, std::size_t size);

void send_message(int socket_fd, std::string message);
std::string recv_message(int socket_fd);

#endif