#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <string>
#include <vector>

struct ServerConfig {
    std::vector<std::string> patterns;
};

ServerConfig load_config(const std::string& path);

#endif