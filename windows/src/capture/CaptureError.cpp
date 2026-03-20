#include "capture/CaptureError.h"

namespace reflection {

std::string CaptureError::user_message() const {
    switch (code_) {
        case CaptureErrorCode::DeviceNotFound:
            return "No iPad found. Make sure your iPad is on the same Wi-Fi network.";
        case CaptureErrorCode::PermissionDenied:
            return "Network access denied. Check Windows Firewall settings.";
        case CaptureErrorCode::SessionConfigurationFailed:
            return "Failed to start AirPlay session" +
                   (detail_.empty() ? std::string(".") : ": " + detail_);
        case CaptureErrorCode::CaptureInterrupted:
            return "Mirroring was interrupted" +
                   (detail_.empty() ? std::string(".") : ": " + detail_);
        case CaptureErrorCode::DeviceDisconnected:
            return "iPad disconnected.";
        case CaptureErrorCode::NetworkError:
            return "Network error" +
                   (detail_.empty() ? std::string(".") : ": " + detail_);
        case CaptureErrorCode::FirewallBlocked:
            return "Windows Firewall is blocking AirPlay. Allow Reflection through the firewall.";
        case CaptureErrorCode::UnknownError:
            return "An unexpected error occurred" +
                   (detail_.empty() ? std::string(".") : ": " + detail_);
    }
    return "Unknown error.";
}

std::string CaptureError::make_message(CaptureErrorCode code, const std::string& detail) {
    switch (code) {
        case CaptureErrorCode::DeviceNotFound:          return "Device not found";
        case CaptureErrorCode::PermissionDenied:        return "Permission denied";
        case CaptureErrorCode::SessionConfigurationFailed: return "Session config failed: " + detail;
        case CaptureErrorCode::CaptureInterrupted:      return "Capture interrupted: " + detail;
        case CaptureErrorCode::DeviceDisconnected:      return "Device disconnected";
        case CaptureErrorCode::NetworkError:            return "Network error: " + detail;
        case CaptureErrorCode::FirewallBlocked:         return "Firewall blocked";
        case CaptureErrorCode::UnknownError:            return "Unknown error: " + detail;
    }
    return "Unknown error";
}

} // namespace reflection
