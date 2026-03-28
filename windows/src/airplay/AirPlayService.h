// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#include "airplay/AirPlayCoreTypes.h"
#include "airplay/AirPlayTypes.h"
#include "airplay/IAirPlayCore.h"
#include "mdns/IMdnsAdvertiser.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace reflection {

/// Orchestrates the AirPlay receiver lifecycle.
///
/// Manages three concerns:
/// 1. IAirPlayCore — the RAOP protocol server (RPiPlay)
/// 2. IMdnsAdvertiser — mDNS service advertisement
/// 3. Callback bridge — routes C callbacks to C++ std::function
///
/// Start sequence: init core → wire callbacks → start core → advertise mDNS
/// Stop sequence:  withdraw mDNS → stop core → clear bridge
class AirPlayService {
public:
    using ClientConnectedCallback = std::function<void(const AirPlayClientInfo&)>;
    using ClientDisconnectedCallback = std::function<void(const std::string& device_id)>;
    using VideoFrameCallback = std::function<void(const uint8_t* data, size_t size, uint64_t timestamp, uint8_t frame_type)>;
    using VideoResetCallback = std::function<void()>;
    using AudioFrameCallback = std::function<void(const uint8_t* data, size_t size, uint64_t timestamp)>;

    /// Dependency-injected constructor for testability.
    AirPlayService(
        std::unique_ptr<IAirPlayCore> core,
        std::unique_ptr<IMdnsAdvertiser> mdns);

    ~AirPlayService();

    // Non-copyable, non-movable (owns resources)
    AirPlayService(const AirPlayService&) = delete;
    AirPlayService& operator=(const AirPlayService&) = delete;

    /// Start the AirPlay receiver with the given configuration.
    /// Returns true if all subsystems started successfully.
    [[nodiscard]] bool start(const AirPlayServiceConfig& config);

    /// Stop the AirPlay receiver and clean up all subsystems.
    void stop();

    /// Restart the service with a new configuration.
    /// Stops the current service, then starts with the new config.
    /// Preserves existing callbacks.
    [[nodiscard]] bool restart(const AirPlayServiceConfig& config);

    /// Whether the service is currently running.
    [[nodiscard]] bool is_running() const;

    /// Trigger an immediate mDNS re-announcement burst.
    /// Use after disconnect/reconnect to help iPads rediscover quickly.
    void force_reannounce();

    // --- Callback setters (call before start()) ---

    void set_client_connected_callback(ClientConnectedCallback callback);
    void set_client_disconnected_callback(ClientDisconnectedCallback callback);
    void set_video_frame_callback(VideoFrameCallback callback);
    void set_video_reset_callback(VideoResetCallback callback);
    void set_audio_frame_callback(AudioFrameCallback callback);

private:
    std::unique_ptr<IAirPlayCore> core_;
    std::unique_ptr<IMdnsAdvertiser> mdns_;
    std::atomic<bool> running_ = false;

    // Stored callbacks
    ClientConnectedCallback client_connected_cb_;
    ClientDisconnectedCallback client_disconnected_cb_;
    VideoFrameCallback video_frame_cb_;
    VideoResetCallback video_reset_cb_;
    AudioFrameCallback audio_frame_cb_;

    /// Wire stored callbacks to the core via adapter lambdas.
    void wire_callbacks_to_core();
};

} // namespace reflection
