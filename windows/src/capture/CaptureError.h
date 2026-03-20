#pragma once

#include <stdexcept>
#include <string>

namespace reflection {

/// Capture error types — mirrors the macOS CaptureError enum.
enum class CaptureErrorCode {
    DeviceNotFound,
    PermissionDenied,
    SessionConfigurationFailed,
    CaptureInterrupted,
    DeviceDisconnected,
    NetworkError,
    FirewallBlocked,
    UnknownError,
};

/// Capture error with code and human-readable message.
class CaptureError : public std::runtime_error {
public:
    CaptureError(CaptureErrorCode code, const std::string& detail)
        : std::runtime_error(make_message(code, detail))
        , code_(code)
        , detail_(detail) {}

    explicit CaptureError(CaptureErrorCode code)
        : CaptureError(code, "") {}

    [[nodiscard]] CaptureErrorCode code() const noexcept { return code_; }
    [[nodiscard]] const std::string& detail() const noexcept { return detail_; }

    /// User-friendly error description.
    [[nodiscard]] std::string user_message() const;

private:
    CaptureErrorCode code_;
    std::string detail_;

    static std::string make_message(CaptureErrorCode code, const std::string& detail);
};

} // namespace reflection
