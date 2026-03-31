#ifndef SCANNER_H
#define SCANNER_H

#include "config.hpp"

#include <string>
#include <vector>

struct ScanResult {
    bool infected = false;
    std::vector<std::string> matched_patterns;
};

ScanResult scan_content(const std::string& content, const ServerConfig& config);

#endif