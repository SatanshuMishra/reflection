#include "session/FrameStaleMonitor.h"

namespace reflection {

FrameStaleMonitor::FrameStaleMonitor(
    double threshold_sec,
    uint64_t check_interval_ms
)
    : threshold_(threshold_sec)
    , check_interval_(check_interval_ms)
{
}

FrameStaleMonitor::~FrameStaleMonitor() {
    stop_monitoring();
}

bool FrameStaleMonitor::is_receiving_frames() const {
    std::lock_guard lock(mutex_);
    return is_receiving_;
}

void FrameStaleMonitor::record_frame() {
    std::lock_guard lock(mutex_);
    last_frame_time_ = std::chrono::steady_clock::now();
}

void FrameStaleMonitor::start_monitoring(StatusCallback callback) {
    stop_monitoring();

    {
        std::lock_guard lock(mutex_);
        status_callback_ = std::move(callback);
        is_receiving_ = false;
        last_frame_time_ = {};
    }

    monitor_thread_ = std::jthread([this](std::stop_token token) {
        monitor_loop(std::move(token));
    });
}

void FrameStaleMonitor::stop_monitoring() {
    // Request stop and join
    if (monitor_thread_.joinable()) {
        monitor_thread_.request_stop();
        monitor_thread_.join();
    }

    StatusCallback callback_to_fire;
    bool was_receiving = false;

    {
        std::lock_guard lock(mutex_);
        was_receiving = is_receiving_;
        if (was_receiving) {
            is_receiving_ = false;
            callback_to_fire = status_callback_;
        }
    }

    // Fire callback outside lock (mirrors macOS behavior)
    if (was_receiving && callback_to_fire) {
        callback_to_fire(false);
    }
}

std::chrono::duration<double> FrameStaleMonitor::elapsed_since_last_frame() const {
    std::lock_guard lock(mutex_);
    if (last_frame_time_ == std::chrono::steady_clock::time_point{}) {
        // No frame recorded yet — treat as infinitely stale
        return std::chrono::duration<double>(1e9);
    }
    return std::chrono::steady_clock::now() - last_frame_time_;
}

void FrameStaleMonitor::monitor_loop(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        std::this_thread::sleep_for(check_interval_);
        if (stop_token.stop_requested()) break;

        const auto elapsed = elapsed_since_last_frame();
        const bool receiving = elapsed <= threshold_;

        StatusCallback callback_to_fire;

        {
            std::lock_guard lock(mutex_);
            if (stop_token.stop_requested()) break;
            if (receiving == is_receiving_) continue; // No change
            is_receiving_ = receiving;
            callback_to_fire = status_callback_;
        }

        // Fire callback outside lock
        if (callback_to_fire) {
            callback_to_fire(receiving);
        }
    }
}

} // namespace reflection
