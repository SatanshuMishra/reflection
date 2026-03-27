// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "airplay/UxPlayCore.h"
#include "utilities/Logger.h"

// UxPlay C headers (stream.h already included via UxPlayCore.h)
extern "C" {
#include "raop.h"
#include "dnssd.h"
#include "global.h"
#include "logger.h"
}

#include <atomic>
#include <cstring>

namespace {

void uxplay_log_callback(void* /*cls*/, int level, const char* msg) {
    switch (level) {
        case LOGGER_ERR:
        case LOGGER_CRIT:
        case LOGGER_ALERT:
        case LOGGER_EMERG:
            reflection::Logger::error("[UxPlay] {}", msg);
            break;
        case LOGGER_WARNING:
            reflection::Logger::warn("[UxPlay] {}", msg);
            break;
        default:
            reflection::Logger::debug("[UxPlay] {}", msg);
            break;
    }
}

// Stub callbacks for raop_callbacks_t fields that UxPlay calls without
// NULL checks. Without these, the RTSP handler crashes at addr=0x0.
void stub_video_pause(void*) {}
void stub_video_resume(void*) {}
void stub_conn_feedback(void*) {}
void stub_conn_reset(void*, int) {}
void on_video_reset_handler(void* cls, reset_type_t type) {
    // Forward to UxPlayCore's public handler method
    auto* self = static_cast<reflection::UxPlayCore*>(cls);
    self->handle_video_reset(static_cast<int>(type));
}
double stub_audio_set_client_volume(void*) { return -30.0; }
void stub_audio_flush(void*) {}
void stub_video_flush(void*) {}

} // anonymous namespace

namespace reflection {

UxPlayCore::~UxPlayCore() {
    if (running_.load()) {
        stop();
    }
}

bool UxPlayCore::init(const AirPlayCoreConfig& config) {
    if (raop_) {
        Logger::warn("UxPlayCore::init called when already initialized");
        return true;
    }

    config_ = config;

    Logger::info("UxPlayCore::init -- server_name='{}', raop_port={}, airplay_port={}",
                 config.server_name, config.raop_port, config.airplay_port);

    // Build UxPlay callback struct.
    // UxPlay calls many callbacks WITHOUT null checks, so every field
    // that gets invoked during the RTSP handshake must be non-NULL.
    raop_callbacks_t cbs = {};
    cbs.cls = this;

    // Core data callbacks
    cbs.video_process = &on_video_process;
    cbs.audio_process = &on_audio_process;

    // Connection lifecycle
    cbs.conn_init = &on_conn_init;
    cbs.conn_destroy = &on_conn_destroy;
    cbs.conn_feedback = &stub_conn_feedback;
    cbs.conn_reset = &stub_conn_reset;

    // Video control (called without NULL checks during stream setup)
    cbs.video_pause = &stub_video_pause;
    cbs.video_resume = &stub_video_resume;
    cbs.video_reset = &on_video_reset_handler;
    cbs.video_flush = &stub_video_flush;
    cbs.video_report_size = reinterpret_cast<decltype(cbs.video_report_size)>(&on_video_report_size);
    cbs.video_set_codec = reinterpret_cast<decltype(cbs.video_set_codec)>(&on_video_set_codec);

    // Audio control (audio_set_client_volume called without NULL check in RTSP INFO)
    cbs.audio_set_client_volume = &stub_audio_set_client_volume;
    cbs.audio_flush = &stub_audio_flush;

    // Client identification
    cbs.report_client_request = reinterpret_cast<decltype(cbs.report_client_request)>(&on_report_client);

    // UxPlay's raop_init takes only callbacks (not max_connections)
    raop_ = raop_init(&cbs);
    if (!raop_) {
        Logger::error("raop_init failed");
        return false;
    }

    // Build hardware address string for raop_init2
    char hw_str[18] = {};
    snprintf(hw_str, sizeof(hw_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             config.hardware_address[0], config.hardware_address[1],
             config.hardware_address[2], config.hardware_address[3],
             config.hardware_address[4], config.hardware_address[5]);

    // NOTE: keyfile must be "" not nullptr — UxPlay's crypto.c calls strlen(keyfile)
    int init2_result = raop_init2(raop_, 0, hw_str, "");
    if (init2_result < 0) {
        Logger::error("raop_init2 failed with error: {}", init2_result);
        raop_destroy(raop_);
        raop_ = nullptr;
        return false;
    }

    // Register a dnssd module so UxPlay's /info handler returns proper
    // deviceID, name, macAddress, and features. Without this, the iPad
    // sees features=0 and immediately sends TEARDOWN.
    int dnssd_error = 0;
    dnssd_t* dnssd = dnssd_init(
        config.server_name.c_str(),
        static_cast<int>(config.server_name.size()),
        reinterpret_cast<const char*>(config.hardware_address.data()),
        static_cast<int>(config.hardware_address.size()),
        &dnssd_error, 0);
    if (dnssd) {
        raop_set_dnssd(raop_, dnssd);  // Also copies pk_str into dnssd
        Logger::info("DNS-SD module registered (features=0x{:X})",
                     dnssd_get_airplay_features(dnssd));
    }

    configure_quality_negotiation();

    // Set UxPlay's internal logger callback before changing log level.
    // Without a callback, logger_log() asserts and crashes on NULL call.
    raop_set_log_callback(raop_, &uxplay_log_callback, nullptr);
    raop_set_log_level(raop_, LOGGER_INFO);

    Logger::info("UxPlay RAOP server initialized (with quality negotiation)");
    return true;
}

void UxPlayCore::configure_quality_negotiation() {
    if (!raop_) return;

    // Request 1920x1080 at 30fps from the iPad.
    // These values are communicated via AirPlay's SETUP plist exchange.
    // UxPlay's raop_set_plist() sends these as the receiver's preferred
    // resolution, causing the iPad to encode at higher quality.
    raop_set_plist(raop_, "width", 1920);
    raop_set_plist(raop_, "height", 1080);
    raop_set_plist(raop_, "maxFPS", 30);
    raop_set_plist(raop_, "overscanned", 0);
    raop_set_plist(raop_, "refreshRate", 30);

    Logger::info("Quality negotiation configured: 1920x1080 @ 30fps");
}

bool UxPlayCore::start() {
    if (running_.load()) return true;

    if (!raop_) {
        Logger::error("UxPlayCore::start called before init");
        return false;
    }

    Logger::info("UxPlayCore::start -- listening on port {}", config_.raop_port);

    unsigned short port = config_.raop_port;
    int result = raop_start_httpd(raop_, &port);
    if (result < 0) {
        Logger::error("raop_start_httpd failed with error: {}", result);
        return false;
    }

    if (port != config_.raop_port) {
        Logger::warn("RAOP port changed from {} to {} (requested port was busy)",
                     config_.raop_port, port);
        config_.raop_port = port;
    }

    running_.store(true);
    Logger::info("RAOP server started on port {}", port);
    return true;
}

std::string UxPlayCore::get_public_key() const {
    if (!raop_) return "";
    const char* pk = raop_get_pk_str(raop_);
    return pk ? std::string(pk) : "";
}

void UxPlayCore::stop() {
    if (!running_.load() || !raop_) return;

    Logger::info("UxPlayCore::stop");

    if (raop_) {
        raop_stop_httpd(raop_);
        raop_destroy(raop_);
        raop_ = nullptr;
    }

    running_.store(false);
    Logger::info("RAOP server stopped");
}

bool UxPlayCore::is_running() const {
    return running_.load();
}

void UxPlayCore::set_video_callback(VideoFrameCallback callback) {
    std::lock_guard lock(callback_mutex_);
    video_callback_ = std::move(callback);
}

void UxPlayCore::set_video_reset_callback(VideoResetCallback callback) {
    std::lock_guard lock(callback_mutex_);
    video_reset_callback_ = std::move(callback);
}

void UxPlayCore::handle_video_reset(int reset_type) {
    Logger::info("UxPlay video_reset (type={})", reset_type);

    VideoResetCallback cb;
    {
        std::lock_guard lock(callback_mutex_);
        cb = video_reset_callback_;
    }
    if (cb) cb();
}

void UxPlayCore::set_audio_callback(AudioFrameCallback callback) {
    std::lock_guard lock(callback_mutex_);
    audio_callback_ = std::move(callback);
}

void UxPlayCore::set_connection_callback(ConnectionCallback callback) {
    std::lock_guard lock(callback_mutex_);
    connection_callback_ = std::move(callback);
}

void UxPlayCore::set_disconnection_callback(DisconnectionCallback callback) {
    std::lock_guard lock(callback_mutex_);
    disconnection_callback_ = std::move(callback);
}

// --------------------------------------------------------------------------
// Static C callbacks — called from UxPlay's internal RAOP threads
// --------------------------------------------------------------------------

void UxPlayCore::on_video_process(void* cls, raop_ntp_t* /*ntp*/, video_decode_struct* data) {
    auto* self = static_cast<UxPlayCore*>(cls);

    if (!data || !data->data || data->data_len <= 0) {
        Logger::warn("on_video_process: null or empty video data");
        return;
    }

    // Log first frame (atomic to prevent data race from RAOP threads)
    static std::atomic<bool> first_frame{true};
    if (first_frame.exchange(false)) {
        const auto* d = data->data;
        if (data->data_len >= 5) {
            Logger::info("UxPlay first video frame: h265={}, nal_count={}, size={}, "
                         "bytes=[{:02X} {:02X} {:02X} {:02X} {:02X}]",
                         data->is_h265, data->nal_count, data->data_len,
                         d[0], d[1], d[2], d[3], d[4]);
        } else {
            Logger::info("UxPlay first video frame: h265={}, size={}",
                         data->is_h265, data->data_len);
        }
    }

    // Periodic frame count logging (atomic to prevent data race from RAOP threads)
    static std::atomic<uint64_t> frame_count{0};
    uint64_t count = ++frame_count;
    if (count % 300 == 0) {
        Logger::info("UxPlay video frames received: {}", count);
    }

    VideoFrameCallback cb;
    {
        std::lock_guard lock(self->callback_mutex_);
        if (!self->video_callback_) return;
        cb = self->video_callback_;
    }

    // Build frame struct from UxPlay's video_decode_struct
    const AirPlayVideoFrame frame{
        .data = data->data,
        .size = static_cast<size_t>(data->data_len),
        .timestamp = data->ntp_time_local,
        .nal_type = 0,  // UxPlay doesn't expose individual NAL type
    };

    cb(frame);
}

void UxPlayCore::on_audio_process(void* cls, raop_ntp_t* /*ntp*/, audio_decode_struct* data) {
    auto* self = static_cast<UxPlayCore*>(cls);

    if (!data || !data->data || data->data_len <= 0) return;

    AudioFrameCallback cb;
    {
        std::lock_guard lock(self->callback_mutex_);
        if (!self->audio_callback_) return;
        cb = self->audio_callback_;
    }

    const AirPlayAudioFrame frame{
        .data = data->data,
        .size = static_cast<size_t>(data->data_len),
        .timestamp = data->ntp_time_local,
        .codec_type = data->ct,
        .sample_rate = 44100,
        .channels = 2,
    };

    cb(frame);
}

void UxPlayCore::on_conn_init(void* cls) {
    auto* self = static_cast<UxPlayCore*>(cls);

    Logger::info("UxPlay: client connected");

    ConnectionCallback cb;
    {
        std::lock_guard lock(self->callback_mutex_);
        if (!self->connection_callback_) return;
        cb = self->connection_callback_;
    }

    cb(AirPlayConnectionEvent{
        .device_id = "unknown",
        .device_name = "iPad",
        .device_model = "iPad",
        .width = 0,
        .height = 0,
    });
}

void UxPlayCore::on_conn_destroy(void* cls) {
    auto* self = static_cast<UxPlayCore*>(cls);

    Logger::info("UxPlay: client disconnected");

    DisconnectionCallback cb;
    {
        std::lock_guard lock(self->callback_mutex_);
        if (!self->disconnection_callback_) return;
        cb = self->disconnection_callback_;
    }

    cb("unknown");
}

void UxPlayCore::on_video_report_size(void* cls, float* w_src, float* h_src,
                                       float* w, float* h) {
    if (w_src && h_src && w && h) {
        Logger::info("UxPlay video size report: source={}x{}, display={}x{}",
                     *w_src, *h_src, *w, *h);
    }
}

int UxPlayCore::on_video_set_codec(void* cls, int codec) {
    // codec: 0 = unknown, 1 = H.264, 2 = H.265
    const char* codec_name = (codec == 2) ? "H.265" : (codec == 1) ? "H.264" : "unknown";
    Logger::info("UxPlay video codec: {} ({})", codec_name, codec);
    return 0;  // Accept the codec
}

void UxPlayCore::on_report_client(void* cls, char* device_id, char* model,
                                   char* name, bool* admit) {
    Logger::info("UxPlay client request: id='{}', model='{}', name='{}'",
                 device_id ? device_id : "null",
                 model ? model : "null",
                 name ? name : "null");

    // Update connection info if we have a connection callback
    auto* self = static_cast<UxPlayCore*>(cls);

    // Always admit the client
    if (admit) *admit = true;
}

} // namespace reflection
