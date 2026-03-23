#pragma once

#include "airplay/IAirPlayCore.h"

#include <atomic>
#include <mutex>

// UxPlay C types — stream.h defines video_decode_struct / audio_decode_struct
// as anonymous struct typedefs, so forward declaration is not possible.
extern "C" {
    typedef struct raop_s raop_t;
    typedef struct raop_ntp_s raop_ntp_t;
#include "stream.h"
}

namespace reflection {

/// Production implementation of IAirPlayCore wrapping UxPlay's lib/.
///
/// Key improvements over RPiPlayCore:
///   - Quality negotiation via raop_set_plist() (width, height, maxFPS)
///   - H.265 codec support detection
///   - Better maintained upstream library
///
/// Lifecycle:
///   init()  → raop_init() with callbacks + raop_set_plist() for quality
///   start() → raop_start_httpd() listening on configured port
///   stop()  → raop_stop_httpd() + raop_destroy()
///
/// Callbacks from UxPlay fire on internal RAOP threads. This class copies
/// callbacks under a lock and invokes them outside the lock to prevent
/// deadlocks.
class UxPlayCore : public IAirPlayCore {
public:
    UxPlayCore() = default;
    ~UxPlayCore() override;

    // Non-copyable (owns C resources)
    UxPlayCore(const UxPlayCore&) = delete;
    UxPlayCore& operator=(const UxPlayCore&) = delete;

    [[nodiscard]] bool init(const AirPlayCoreConfig& config) override;
    [[nodiscard]] bool start() override;
    void stop() override;
    [[nodiscard]] bool is_running() const override;

    void set_video_callback(VideoFrameCallback callback) override;
    void set_audio_callback(AudioFrameCallback callback) override;
    void set_connection_callback(ConnectionCallback callback) override;
    void set_disconnection_callback(DisconnectionCallback callback) override;
    [[nodiscard]] std::string get_public_key() const override;

private:
    raop_t* raop_ = nullptr;
    std::atomic<bool> running_ = false;
    AirPlayCoreConfig config_;

    mutable std::mutex callback_mutex_;
    VideoFrameCallback video_callback_;
    AudioFrameCallback audio_callback_;
    ConnectionCallback connection_callback_;
    DisconnectionCallback disconnection_callback_;

    /// Configure quality negotiation via raop_set_plist().
    /// Requests high-resolution H.264/H.265 stream from iPad.
    void configure_quality_negotiation();

    /// Static C callbacks matching UxPlay's raop_callbacks_t signatures.
    /// Called from RAOP internal threads.
    static void on_video_process(void* cls, raop_ntp_t* ntp, video_decode_struct* data);
    static void on_audio_process(void* cls, raop_ntp_t* ntp, audio_decode_struct* data);
    static void on_conn_init(void* cls);
    static void on_conn_destroy(void* cls);
    static void on_video_report_size(void* cls, float* w_src, float* h_src,
                                     float* w, float* h);
    static int  on_video_set_codec(void* cls, int codec);
    static void on_report_client(void* cls, char* device_id, char* model,
                                 char* name, bool* admit);
};

} // namespace reflection
