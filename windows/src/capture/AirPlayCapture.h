// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#include "capture/IScreenCapture.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace reflection {

/// AirPlay-based capture implementation.
/// Wraps the AirPlay service, video decoder, and audio decoder into
/// the IScreenCapture interface.
///
/// TODO (Milestone 5): Full implementation
class AirPlayCapture : public IScreenCapture {
public:
    explicit AirPlayCapture(const std::string& device_id);
    ~AirPlayCapture() override;

    [[nodiscard]] CaptureState state() const override;
    [[nodiscard]] bool is_receiving_frames() const override;

    void start_capture() override;
    void stop_capture() override;

    void set_state_callback(StateCallback callback) override;
    void set_frame_status_callback(FrameStatusCallback callback) override;

private:
    std::string device_id_;
    std::atomic<CaptureState> state_{CaptureState::Idle};

    mutable std::mutex mutex_;
    StateCallback state_callback_;
    FrameStatusCallback frame_status_callback_;

    /// Thread-safe: copies callback under lock, invokes outside lock.
    void fire_state_callback(CaptureState new_state);
};

} // namespace reflection
