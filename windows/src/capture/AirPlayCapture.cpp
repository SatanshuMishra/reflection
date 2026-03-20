#include "capture/AirPlayCapture.h"
#include "utilities/Logger.h"

namespace reflection {

AirPlayCapture::AirPlayCapture(const std::string& device_id)
    : device_id_(device_id)
{
}

AirPlayCapture::~AirPlayCapture() {
    // Clear callbacks first to prevent firing into a destroyed owner.
    {
        std::lock_guard lock(mutex_);
        state_callback_ = nullptr;
        frame_status_callback_ = nullptr;
    }

    if (state_.load() == CaptureState::Running) {
        // stop_capture without callbacks — we already cleared them.
        state_.store(CaptureState::Stopped);
    }
}

CaptureState AirPlayCapture::state() const {
    return state_.load();
}

bool AirPlayCapture::is_receiving_frames() const {
    // TODO (Milestone 5): Delegate to FrameStaleMonitor
    return state_.load() == CaptureState::Running;
}

void AirPlayCapture::start_capture() {
    Logger::info("AirPlayCapture::start_capture for device: {}", device_id_);

    state_.store(CaptureState::Starting);
    fire_state_callback(CaptureState::Starting);

    // TODO (Milestone 2-5): Initialize AirPlay service, decoder, renderer
    // For now, transition to Running as a stub
    state_.store(CaptureState::Running);
    fire_state_callback(CaptureState::Running);
}

void AirPlayCapture::stop_capture() {
    Logger::info("AirPlayCapture::stop_capture for device: {}", device_id_);

    state_.store(CaptureState::Stopped);
    fire_state_callback(CaptureState::Stopped);
}

void AirPlayCapture::set_state_callback(StateCallback callback) {
    std::lock_guard lock(mutex_);
    state_callback_ = std::move(callback);
}

void AirPlayCapture::set_frame_status_callback(FrameStatusCallback callback) {
    std::lock_guard lock(mutex_);
    frame_status_callback_ = std::move(callback);
}

void AirPlayCapture::fire_state_callback(CaptureState new_state) {
    StateCallback cb;
    {
        std::lock_guard lock(mutex_);
        cb = state_callback_; // Copy under lock
    }
    if (cb) cb(new_state); // Invoke outside lock
}

} // namespace reflection
