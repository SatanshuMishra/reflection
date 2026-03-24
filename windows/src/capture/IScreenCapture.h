// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#include <functional>
#include <string>

#include "capture/CaptureState.h"
#include "capture/CaptureError.h"

namespace reflection {

/// Abstract capture interface — mirrors the macOS ScreenCapture protocol.
///
/// Implementations:
///   - AirPlayCapture: AirPlay-based wireless capture (Windows)
///   - (macOS uses USBCapture via CoreMediaIO/AVFoundation instead)
///
/// State changes are delivered via the on_state_changed callback.
/// Frame status changes are delivered via the on_frame_status_changed callback.
class IScreenCapture {
public:
    using StateCallback = std::function<void(CaptureState)>;
    using FrameStatusCallback = std::function<void(bool /*is_receiving*/)>;

    virtual ~IScreenCapture() = default;

    /// Current capture state.
    [[nodiscard]] virtual CaptureState state() const = 0;

    /// Whether frames are currently being received within the stale threshold.
    [[nodiscard]] virtual bool is_receiving_frames() const = 0;

    /// Start the capture session. Throws CaptureError on failure.
    virtual void start_capture() = 0;

    /// Stop the capture session gracefully.
    virtual void stop_capture() = 0;

    /// Register callback for state changes.
    virtual void set_state_callback(StateCallback callback) = 0;

    /// Register callback for frame receiving status changes.
    virtual void set_frame_status_callback(FrameStatusCallback callback) = 0;

    // Non-copyable, non-movable
    IScreenCapture(const IScreenCapture&) = delete;
    IScreenCapture& operator=(const IScreenCapture&) = delete;
    IScreenCapture(IScreenCapture&&) = delete;
    IScreenCapture& operator=(IScreenCapture&&) = delete;

protected:
    IScreenCapture() = default;
};

} // namespace reflection
