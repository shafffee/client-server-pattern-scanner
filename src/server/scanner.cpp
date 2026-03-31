#include "scanner.hpp"

ScanResult scan_content(const std::string& content, const ServerConfig& config) {
    ScanResult result;

    for (const auto& pattern : config.patterns) {
        if (content.find(pattern) != std::string::npos) {
            result.matched_patterns.push_back(pattern);
        }
    }

    result.infected = !result.matched_patterns.empty();
    return result;
}