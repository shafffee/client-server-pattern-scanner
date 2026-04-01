#include "fd_utils.hpp"
#include "ipc_paths.hpp"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <string>

using json = nlohmann::json;

// creates a unique fifo path for this stats_viewer process
std::string make_reply_fifo_path() {
    return "/tmp/malware_stats_reply_" + std::to_string(getpid()) + ".fifo";
}

// prints the received statistics json in a readable form
void print_statistics(const std::string& stats_json) {
    const json j = json::parse(stats_json);

    std::cout << "==================================\n";
    std::cout << "STATS VIEWER\n";
    std::cout << "==================================\n";
    std::cout << "Checked files: " << j.at("checked_files").get<std::uint64_t>() << "\n";
    std::cout << "Pattern hits:\n";

    const auto& pattern_hits = j.at("pattern_hits");
    if (pattern_hits.empty()) {
        std::cout << " - none\n";
    } else {
        for (auto it = pattern_hits.begin(); it != pattern_hits.end(); ++it) {
            std::cout << " - " << it.key() << ": " << it.value().get<std::uint64_t>() << "\n";
        }
    }
}

int main() {
    std::string reply_fifo = make_reply_fifo_path();

    // remove any old fifo with the same name just in case
    unlink(reply_fifo.c_str());

    // create a private fifo where the server will send the stats response
    if (mkfifo(reply_fifo.c_str(), 0600) < 0) {
        throw std::runtime_error("Failed to create reply FIFO");
    }

    try {
        // build a json request with the path to our reply fifo
        json request;
        request["reply_fifo"] = reply_fifo;
        const std::string request_line = request.dump() + "\n";

        // open the server request fifo and send the request
        const int request_fd = open(kStatsRequestFifo, O_WRONLY);
        if (request_fd < 0) {
            unlink(reply_fifo.c_str());
            throw std::runtime_error("Failed to open server request FIFO");
        }

        write_all_fd(request_fd, request_line);
        close(request_fd);

        // open our reply fifo and wait for the server response
        const int reply_fd = open(reply_fifo.c_str(), O_RDONLY);
        if (reply_fd < 0) {
            unlink(reply_fifo.c_str());
            throw std::runtime_error("Failed to open reply FIFO");
        }

        const std::string stats_json = read_all_fd(reply_fd);
        close(reply_fd);

        // clean up the temporary fifo after reading the response
        unlink(reply_fifo.c_str());

        // show the received statistics and exit normally
        print_statistics(stats_json);
        return 0;
    } catch (...) {
        // make sure the temporary fifo is removed even if something fails
        unlink(reply_fifo.c_str());
        throw;
    }
}