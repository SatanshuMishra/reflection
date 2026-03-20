#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>

namespace reflection {

/// Monitors frame delivery timestamps to detect when a capture feed has gone
/// stale (e.g., when an iPad is locked or sleeping).
///
/// Direct port of the macOS FrameStaleMonitor.
/// - `std::mutex` replaces `NSLock`
/// - `std::jthread` replaces `Task`
/// - `std::chrono::steady_clock` replaces `Date`
///
/// Independently testable without any capture hardware.
class FrameStaleMonitor {
public:
    using StatusCallback = std::function<void(bool /*is_receiving*/)>;

    /// Create a monitor with configurable threshold and check interval.
    /// Defaults match the macOS app: threshold=1.0s, check every 500ms.
    explicit FrameStaleMonitor(
        double threshold_sec = 1.0,
        uint64_t check_interval_ms = 500
    );

    ~FrameStaleMonitor();

    // Non-copyable, non-movable
    FrameStaleMonitor(const FrameStaleMonitor&) = delete;
    FrameStaleMonitor& operator=(const FrameStaleMonitor&) = delete;

    /// Whether frames are currently being received within the threshold.
    [[nodiscard]] bool is_receiving_frames() const;

    /// Called by the video callback each time a frame arrives.
    /// Thread-safe — may be called from any thread.
    void record_frame();

    /// Start periodic monitoring. Calls status_callback when status changes.
    void start_monitoring(StatusCallback callback);

    /// Stop the monitoring thread and reset state.
    void stop_monitoring();

private:
    const std::chrono::duration<double> threshold_;
    const std::chrono::milliseconds check_interval_;

    mutable std::mutex mutex_;
    std::chrono::steady_clock::time_point last_frame_time_{};
    bool is_receiving_ = false;
    StatusCallback status_callback_;

    std::jthread monitor_thread_;

    /// Returns time elapsed since last recorded frame.
    std::chrono::duration<double> elapsed_since_last_frame() const;

    /// Monitoring loop executed on the background thread.
    void monitor_loop(std::stop_token stop_token);
};

} // namespace reflection
