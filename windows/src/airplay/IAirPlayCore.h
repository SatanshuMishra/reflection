#pragma once

#include "airplay/AirPlayCoreTypes.h"

#include <functional>
#include <memory>

namespace reflection {

/// Abstract interface to the AirPlay protocol core.
/// Wraps RPiPlay's lib/ (RAOP server, FairPlay, H.264/AAC receive).
/// Implementations: RPiPlayCore (production), MockAirPlayCore (tests).
class IAirPlayCore {
public:
    using VideoFrameCallback = std::function<void(const AirPlayVideoFrame&)>;
    using AudioFrameCallback = std::function<void(const AirPlayAudioFrame&)>;
    using ConnectionCallback = std::function<void(const AirPlayConnectionEvent&)>;
    using DisconnectionCallback = std::function<void(const std::string& device_id)>;

    virtual ~IAirPlayCore() = default;

    /// Initialize the core with the given configuration.
    [[nodiscard]] virtual bool init(const AirPlayCoreConfig& config) = 0;

    /// Start the RAOP server (begins accepting connections).
    [[nodiscard]] virtual bool start() = 0;

    /// Stop the RAOP server and disconnect any clients.
    virtual void stop() = 0;

    /// Whether the core is currently running and accepting connections.
    [[nodiscard]] virtual bool is_running() const = 0;

    /// Set callback for received video frames.
    virtual void set_video_callback(VideoFrameCallback callback) = 0;

    /// Set callback for received audio frames.
    virtual void set_audio_callback(AudioFrameCallback callback) = 0;

    /// Set callback for client connections.
    virtual void set_connection_callback(ConnectionCallback callback) = 0;

    /// Set callback for client disconnections.
    virtual void set_disconnection_callback(DisconnectionCallback callback) = 0;
};

} // namespace reflection
