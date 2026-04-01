#ifndef SCANNER_H
#define SCANNER_H

#include "config.hpp"

#include <string>
#include <vector>

// stores scan result for one file
struct ScanResult {
    bool infected = false;
    std::vector<std::string> matched_patterns;
};

// scan file content and get all matched patterns
ScanResult scan_content(const std::string& content, const ServerConfig& config);

#endif