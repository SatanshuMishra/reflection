#include "utilities/UpdateChecker.h"
#include "utilities/Logger.h"

#include <sstream>
#include <vector>

namespace reflection {

namespace {

/// Parse a version string like "1.2.3" into components.
std::vector<int> parse_version(const std::string& version) {
    std::vector<int> parts;
    std::string cleaned = version;

    // Strip leading 'v' or 'vw' prefix
    if (!cleaned.empty() && cleaned[0] == 'v') {
        cleaned = cleaned.substr(1);
    }
    if (!cleaned.empty() && cleaned[0] == 'w') {
        cleaned = cleaned.substr(1);
    }

    std::istringstream stream(cleaned);
    std::string segment;
    while (std::getline(stream, segment, '.')) {
        try {
            parts.push_back(std::stoi(segment));
        } catch (...) {
            parts.push_back(0);
        }
    }
    return parts;
}

} // namespace

std::optional<UpdateInfo> UpdateChecker::check(const std::string& current_version) {
    Logger::info("Checking for updates (current: {})", current_version);

    // TODO (Milestone 6): Fetch from GitHub API via WinHTTP
    // Parse JSON response for tag_name, html_url, body
    // Compare versions
    // Return UpdateInfo if newer version available

    return std::nullopt;
}

bool UpdateChecker::is_newer(const std::string& remote, const std::string& local) {
    const auto remote_parts = parse_version(remote);
    const auto local_parts = parse_version(local);

    const size_t max_len = std::max(remote_parts.size(), local_parts.size());

    for (size_t i = 0; i < max_len; ++i) {
        const int r = (i < remote_parts.size()) ? remote_parts[i] : 0;
        const int l = (i < local_parts.size()) ? local_parts[i] : 0;

        if (r > l) return true;
        if (r < l) return false;
    }

    return false; // Equal
}

} // namespace reflection
