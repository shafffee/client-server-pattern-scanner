#include "config.hpp"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// load and validate the server config file (from JSON)
ServerConfig load_config(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Failed to open config file: " + path);
    }

    json parsed;
    try {
        input >> parsed;
    } catch (const json::parse_error& e) {
        throw std::runtime_error("Invalid JSON in config file: " + std::string(e.what()));
    }

    if (!parsed.is_object()) {
        throw std::runtime_error("Config root must be a JSON object");
    }

    if (!parsed.contains("patterns")) {
        throw std::runtime_error("Config must contain 'patterns' field");
    }

    if (!parsed["patterns"].is_array()) {
        throw std::runtime_error("'patterns' must be an array");
    }

    ServerConfig config;

    for (const auto& item : parsed["patterns"]) {
        if (!item.is_string()) {
            throw std::runtime_error("Each pattern must be a string");
        }

        const std::string pattern = item.get<std::string>();
        if (pattern.empty()) {
            throw std::runtime_error("Patterns must not be empty");
        }

        config.patterns.push_back(pattern);
    }

    return config;
}