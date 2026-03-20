#pragma once

#include "capture/IScreenCapture.h"

#include <optional>

namespace reflection::testing {

/// Mock capture implementation for unit testing.
/// Mirrors the macOS MockScreenCapture exactly.
class MockScreenCapture : public IScreenCapture {
public:
    // Counters for verification
    int start_capture_call_count = 0;
    int stop_capture_call_count = 0;

    // Control behavior
    std::optional<CaptureError> should_throw_on_start;

    CaptureState state() const override { return state_; }
    bool is_receiving_frames() const override { return receiving_frames_; }

    void start_capture() override {
        ++start_capture_call_count;
        if (should_throw_on_start) {
            throw *should_throw_on_start;
        }
        set_state(CaptureState::Running);
    }

    void stop_capture() override {
        ++stop_capture_call_count;
        set_state(CaptureState::Stopped);
    }

    void set_state_callback(StateCallback callback) override {
        state_callback_ = std::move(callback);
    }

    void set_frame_status_callback(FrameStatusCallback callback) override {
        frame_status_callback_ = std::move(callback);
    }

    // --- Test helpers ---

    void simulate_disconnect() {
        set_state(CaptureState::Failed);
    }

    void simulate_frame_status(bool receiving) {
        receiving_frames_ = receiving;
        if (frame_status_callback_) {
            frame_status_callback_(receiving);
        }
    }

    void set_state(CaptureState new_state) {
        state_ = new_state;
        if (state_callback_) {
            state_callback_(new_state);
        }
    }

private:
    CaptureState state_ = CaptureState::Idle;
    bool receiving_frames_ = false;
    StateCallback state_callback_;
    FrameStatusCallback frame_status_callback_;
};

} // namespace reflection::testing
