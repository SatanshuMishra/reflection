// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "session/MirrorSessionManager.h"
#include "utilities/Logger.h"

namespace reflection {

MirrorSessionManager::MirrorSessionManager(CaptureFactory factory)
    : capture_factory_(std::move(factory))
{
}

MirrorSessionManager::~MirrorSessionManager() {
    // Stop all active sessions — clear callbacks first to avoid
    // firing into a partially-destroyed owner.
    std::lock_guard lock(mutex_);
    error_callback_ = nullptr;
    session_change_callback_ = nullptr;
    disconnect_callback_ = nullptr;

    for (auto& [id, session] : sessions_) {
        if (session) {
            try {
                session->set_state_callback(nullptr);
                session->stop_capture();
            } catch (...) {
                // Ignore errors during cleanup
            }
        }
    }
    sessions_.clear();
}

void MirrorSessionManager::start_mirroring(const std::string& device_id) {
    {
        std::lock_guard lock(mutex_);
        if (sessions_.contains(device_id)) {
            Logger::debug("Session already exists for device: {}", device_id);
            return;
        }
        // Reserve the slot atomically to prevent TOCTOU race.
        // If two threads call start_mirroring("same_id") concurrently,
        // the second one will see the reserved slot and return early.
        sessions_.emplace(device_id, nullptr);
    }

    Logger::info("Starting mirroring for device: {}", device_id);

    auto capture = capture_factory_(device_id);
    if (!capture) {
        Logger::error("Capture factory returned null for device: {}", device_id);
        std::lock_guard lock(mutex_);
        sessions_.erase(device_id); // Release the reserved slot
        return;
    }

    // Set up state observation
    capture->set_state_callback(
        [this, id = device_id](CaptureState state) {
            on_capture_state_changed(id, state);
        }
    );

    try {
        capture->start_capture();
    } catch (const CaptureError& e) {
        Logger::error("Failed to start capture for {}: {}", device_id, e.what());

        ErrorCallback cb;
        {
            std::lock_guard lock(mutex_);
            sessions_.erase(device_id); // Release the reserved slot
            current_error_ = e;
            cb = error_callback_; // Copy callback under lock
        }
        if (cb) cb(e); // Invoke outside lock
        return;
    }

    SessionChangeCallback cb;
    {
        std::lock_guard lock(mutex_);
        sessions_[device_id] = std::move(capture); // Fill the reserved slot
        cb = session_change_callback_; // Copy callback under lock
    }
    if (cb) cb(device_id, true); // Invoke outside lock
}

void MirrorSessionManager::stop_mirroring(const std::string& device_id) {
    std::unique_ptr<IScreenCapture> session;
    SessionChangeCallback cb;

    {
        std::lock_guard lock(mutex_);
        auto it = sessions_.find(device_id);
        if (it == sessions_.end()) return;
        session = std::move(it->second);
        sessions_.erase(it);
        cb = session_change_callback_; // Copy callback under lock
    }

    Logger::info("Stopping mirroring for device: {}", device_id);

    if (session) {
        try {
            session->set_state_callback(nullptr); // Prevent callbacks during stop
            session->stop_capture();
        } catch (const std::exception& e) {
            Logger::warn("Error stopping capture for {}: {}", device_id, e.what());
        }
    }

    if (cb) cb(device_id, false); // Invoke outside lock
}

bool MirrorSessionManager::has_session(const std::string& device_id) const {
    std::lock_guard lock(mutex_);
    return sessions_.contains(device_id);
}

std::optional<CaptureState> MirrorSessionManager::session_state(
    const std::string& device_id
) const {
    std::lock_guard lock(mutex_);
    auto it = sessions_.find(device_id);
    if (it == sessions_.end() || !it->second) return std::nullopt;
    return it->second->state();
}

std::optional<CaptureError> MirrorSessionManager::current_error() const {
    std::lock_guard lock(mutex_);
    return current_error_;
}

void MirrorSessionManager::clear_error() {
    std::lock_guard lock(mutex_);
    current_error_.reset();
}

void MirrorSessionManager::on_mirror_window_closed(const std::string& device_id) {
    Logger::info("Mirror window closed for device: {}", device_id);
    stop_mirroring(device_id);
}

void MirrorSessionManager::set_error_callback(ErrorCallback callback) {
    std::lock_guard lock(mutex_);
    error_callback_ = std::move(callback);
}

void MirrorSessionManager::set_session_change_callback(SessionChangeCallback callback) {
    std::lock_guard lock(mutex_);
    session_change_callback_ = std::move(callback);
}

void MirrorSessionManager::set_disconnect_callback(DisconnectCallback callback) {
    std::lock_guard lock(mutex_);
    disconnect_callback_ = std::move(callback);
}

void MirrorSessionManager::remove_session(const std::string& device_id) {
    std::lock_guard lock(mutex_);
    sessions_.erase(device_id);
}

void MirrorSessionManager::on_capture_state_changed(
    const std::string& device_id,
    CaptureState state
) {
    Logger::debug("Capture state changed for {}: {}", device_id, capture_state_label(state));

    if (state == CaptureState::Failed) {
        DisconnectCallback cb;
        {
            std::lock_guard lock(mutex_);
            auto it = sessions_.find(device_id);
            if (it == sessions_.end()) return;
            cb = disconnect_callback_; // Copy callback under lock
        }

        // For disconnections, notify via disconnect callback (not error).
        // This mirrors macOS behavior: .deviceDisconnected posts a notification
        // instead of showing an error alert.
        if (cb) cb(device_id); // Invoke outside lock

        stop_mirroring(device_id);
    }
}

} // namespace reflection
