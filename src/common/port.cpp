#include "port.hpp"

#include <stdexcept>
#include <string>

int parse_port(const std::string& port_str) {
    try {
        size_t pos = 0;
        const int port = std::stoi(port_str, &pos);

        if (pos != port_str.size()) {
            throw std::runtime_error("Port contains invalid characters");
        }

        if (port < 1 || port > 65535) {
            throw std::runtime_error("Port must be in range 1..65535");
        }

        return port;
    } catch (const std::invalid_argument&) {
        throw std::runtime_error("Port must be a valid integer");
    } catch (const std::out_of_range&) {
        throw std::runtime_error("Port value is out of range");
    }
}