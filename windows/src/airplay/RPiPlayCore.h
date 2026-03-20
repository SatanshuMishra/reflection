#pragma once

#include "airplay/IAirPlayCore.h"

#include <atomic>
#include <mutex>

// Forward declare RPiPlay's opaque types (avoid including C headers in our header)
extern "C" {
    typedef struct raop_s raop_t;
    typedef struct raop_ntp_s raop_ntp_t;
    struct h264_decode_struct;
    struct aac_decode_struct;
}

namespace reflection {

/// Production implementation of IAirPlayCore wrapping RPiPlay's lib/.
///
/// Lifecycle:
///   init()  → raop_init() with callbacks, pairing
///   start() → raop_start() listening on configured port
///   stop()  → raop_stop() + raop_destroy()
///
/// Callbacks from RPiPlay fire on internal RAOP threads. This class copies
/// callbacks under a lock (via AirPlayCallbackBridge) and invokes them
/// outside the lock to prevent deadlocks.
class RPiPlayCore : public IAirPlayCore {
public:
    RPiPlayCore() = default;
    ~RPiPlayCore() override;

    // Non-copyable (owns C resources)
    RPiPlayCore(const RPiPlayCore&) = delete;
    RPiPlayCore& operator=(const RPiPlayCore&) = delete;

    [[nodiscard]] bool init(const AirPlayCoreConfig& config) override;
    [[nodiscard]] bool start() override;
    void stop() override;
    [[nodiscard]] bool is_running() const override;

    void set_video_callback(VideoFrameCallback callback) override;
    void set_audio_callback(AudioFrameCallback callback) override;
    void set_connection_callback(ConnectionCallback callback) override;
    void set_disconnection_callback(DisconnectionCallback callback) override;

private:
    raop_t* raop_ = nullptr;
    std::atomic<bool> running_ = false;
    AirPlayCoreConfig config_;

    mutable std::mutex callback_mutex_;
    VideoFrameCallback video_callback_;
    AudioFrameCallback audio_callback_;
    ConnectionCallback connection_callback_;
    DisconnectionCallback disconnection_callback_;

    /// Static C callbacks matching RPiPlay's exact raop_callbacks_t signatures.
    /// These are called from RAOP internal threads.
    static void on_video_process(void* cls, raop_ntp_t* ntp, h264_decode_struct* data);
    static void on_audio_process(void* cls, raop_ntp_t* ntp, aac_decode_struct* data);
    static void on_conn_init(void* cls);
    static void on_conn_destroy(void* cls);
};

} // namespace reflection
