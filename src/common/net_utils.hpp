#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <string>

int connect_to_server(int port);
std::string read_all_from_socket(int client_socket);
void send_all(int socket_fd, const std::string& data);
int create_listening_socket(int port);

#endif