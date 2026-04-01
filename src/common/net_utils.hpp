#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <string>

// create a client socket and connect to localhost server
int connect_to_server(int port);
// create a server socket, bind it, and start listening
int create_listening_socket(int port);

// send exactly size bytes to the socket
void send_data(int socket_fd, const void* data, std::size_t size);
// receive exactly size bytes from the socket
void recv_data(int socket_fd, void* data, std::size_t size);

// send one full message with size prefix
void send_message(int socket_fd, const std::string& message);
// receive one full message using the size prefix
std::string recv_message(int socket_fd);

#endif