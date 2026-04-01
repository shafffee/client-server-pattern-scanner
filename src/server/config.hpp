#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <string>
#include <vector>

// stores all scan patterns loaded from config
struct ServerConfig {
    std::vector<std::string> patterns;
};

// load and validate the server config file (from JSON)
ServerConfig load_config(const std::string& path);

#endif