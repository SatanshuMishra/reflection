#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "capture/CaptureError.h"
#include "capture/CaptureState.h"
#include "capture/IScreenCapture.h"
#include "session/DeviceModel.h"

namespace reflection {

/// Factory function type for creating capture instances.
using CaptureFactory = std::function<std::unique_ptr<IScreenCapture>(const std::string& device_id)>;

/// Orchestrates the capture lifecycle for connected devices.
/// Mirrors the macOS MirrorSessionManager.
///
/// Responsibilities:
///   - Creates and manages capture sessions per device
///   - Observes state changes and surfaces errors
///   - Cleans up sessions on disconnect or window close
class MirrorSessionManager {
public:
    using ErrorCallback = std::function<void(const CaptureError&)>;
    using SessionChangeCallback = std::function<void(const std::string& device_id, bool active)>;
    using DisconnectCallback = std::function<void(const std::string& device_id)>;

    /// Create with a capture factory for dependency injection (testability).
    explicit MirrorSessionManager(CaptureFactory factory);

    ~MirrorSessionManager();

    // Non-copyable
    MirrorSessionManager(const MirrorSessionManager&) = delete;
    MirrorSessionManager& operator=(const MirrorSessionManager&) = delete;

    /// Start mirroring for a device. No-op if session already exists.
    void start_mirroring(const std::string& device_id);

    /// Stop mirroring for a device. No-op if no session exists.
    void stop_mirroring(const std::string& device_id);

    /// Check if a device has an active session.
    [[nodiscard]] bool has_session(const std::string& device_id) const;

    /// Get the current capture state for a device.
    [[nodiscard]] std::optional<CaptureState> session_state(const std::string& device_id) const;

    /// Get the most recent error (if any).
    [[nodiscard]] std::optional<CaptureError> current_error() const;

    /// Clear the current error.
    void clear_error();

    /// Notify that a mirror window was closed for a device.
    void on_mirror_window_closed(const std::string& device_id);

    // Callbacks
    void set_error_callback(ErrorCallback callback);
    void set_session_change_callback(SessionChangeCallback callback);
    void set_disconnect_callback(DisconnectCallback callback);

private:
    CaptureFactory capture_factory_;
    mutable std::mutex mutex_;

    std::unordered_map<std::string, std::unique_ptr<IScreenCapture>> sessions_;
    std::optional<CaptureError> current_error_;

    ErrorCallback error_callback_;
    SessionChangeCallback session_change_callback_;
    DisconnectCallback disconnect_callback_;

    /// Internal: remove session and notify.
    void remove_session(const std::string& device_id);

    /// Handle state changes from a capture session.
    void on_capture_state_changed(const std::string& device_id, CaptureState state);
};

} // namespace reflection
