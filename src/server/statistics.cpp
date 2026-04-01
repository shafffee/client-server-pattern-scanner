#include "statistics.hpp"

#include <nlohmann/json.hpp>

#include <stdexcept>

using json = nlohmann::json;

// builds a small json report that a child process sends back to the parent
std::string build_scan_report_json(const ScanResult& result) {
    json j;
    j["checked_files"] = 1;
    j["matched_patterns"] = result.matched_patterns;
    return j.dump();
}

// applies one child scan report to the parent statistics
void apply_scan_report_json(Statistics& stats, const std::string& report_json) {
    if (report_json.empty()) {
        return;
    }

    const json j = json::parse(report_json);

    stats.checked_files += j.at("checked_files").get<std::uint64_t>();

    for (const auto& pattern : j.at("matched_patterns")) {
        const std::string name = pattern.get<std::string>();
        stats.pattern_hits[name] += 1;
    }
}

// converts the current statistics into json for stats_viewer
std::string build_statistics_json(const Statistics& stats) {
    json j;
    j["checked_files"] = stats.checked_files;
    j["pattern_hits"] = stats.pattern_hits;
    return j.dump();
}