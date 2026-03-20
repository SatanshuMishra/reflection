#pragma once

#include "airplay/AirPlayCoreTypes.h"
#include "airplay/AirPlayTypes.h"
#include "airplay/IAirPlayCore.h"
#include "mdns/IMdnsAdvertiser.h"

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
    using VideoFrameCallback = std::function<void(const uint8_t* data, size_t size, uint64_t timestamp)>;
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

    /// Whether the service is currently running.
    [[nodiscard]] bool is_running() const;

    // --- Callback setters (call before start()) ---

    void set_client_connected_callback(ClientConnectedCallback callback);
    void set_client_disconnected_callback(ClientDisconnectedCallback callback);
    void set_video_frame_callback(VideoFrameCallback callback);
    void set_audio_frame_callback(AudioFrameCallback callback);

private:
    std::unique_ptr<IAirPlayCore> core_;
    std::unique_ptr<IMdnsAdvertiser> mdns_;
    bool running_ = false;

    // Stored callbacks
    ClientConnectedCallback client_connected_cb_;
    ClientDisconnectedCallback client_disconnected_cb_;
    VideoFrameCallback video_frame_cb_;
    AudioFrameCallback audio_frame_cb_;

    /// Wire stored callbacks to the core via adapter lambdas.
    void wire_callbacks_to_core();
};

} // namespace reflection
