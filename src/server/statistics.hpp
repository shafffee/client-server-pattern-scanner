#ifndef STATISTICS_H
#define STATISTICS_H

#include "scanner.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

// this struct stores scan statistics inside the parent process
struct Statistics {
    std::uint64_t checked_files = 0;
    std::unordered_map<std::string, std::uint64_t> pattern_hits;
};

// builds a small json report that a child process sends back to the parent
std::string build_scan_report_json(const ScanResult& result);

// applies one child scan report to the parent statistics
void apply_scan_report_json(Statistics& stats, const std::string& report_json);

// converts the current statistics into json for stats_viewer
std::string build_statistics_json(const Statistics& stats);

#endif