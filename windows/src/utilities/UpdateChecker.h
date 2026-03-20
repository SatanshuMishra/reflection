#pragma once

#include <optional>
#include <string>

namespace reflection {

/// Checks GitHub Releases API for newer versions.
/// Direct port of the macOS UpdateChecker.
struct UpdateInfo {
    std::string latest_version;
    std::string current_version;
    std::string release_url;
    std::string release_notes;
};

class UpdateChecker {
public:
    /// Check for updates. Returns nullopt on error or if already up to date.
    static std::optional<UpdateInfo> check(const std::string& current_version);

    /// Compare two semantic version strings. Returns true if remote > local.
    static bool is_newer(const std::string& remote, const std::string& local);
};

} // namespace reflection
