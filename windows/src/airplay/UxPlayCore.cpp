#include "airplay/UxPlayCore.h"
#include "utilities/Logger.h"

// UxPlay C headers
extern "C" {
#include "raop.h"
#include "stream.h"
#include "global.h"
}

#include <cstring>

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

    Logger::info("UxPlayCore::init — server_name='{}', raop_port={}, airplay_port={}",
                 config.server_name, config.raop_port, config.airplay_port);

    // Build UxPlay callback struct
    raop_callbacks_t cbs = {};
    cbs.cls = this;
    cbs.video_process = &on_video_process;
    cbs.audio_process = &on_audio_process;
    cbs.conn_init = &on_conn_init;
    cbs.conn_destroy = &on_conn_destroy;
    cbs.video_report_size = reinterpret_cast<decltype(cbs.video_report_size)>(&on_video_report_size);
    cbs.video_set_codec = reinterpret_cast<decltype(cbs.video_set_codec)>(&on_video_set_codec);
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

    // Second init phase: nohold=0, device_id from MAC, no keyfile
    int init2_result = raop_init2(raop_, 0, hw_str, nullptr);
    if (init2_result < 0) {
        Logger::error("raop_init2 failed with error: {}", init2_result);
        raop_destroy(raop_);
        raop_ = nullptr;
        return false;
    }

    // Configure quality negotiation — request high resolution from iPad
    configure_quality_negotiation();

    // Set log level
    raop_set_log_level(raop_, RAOP_LOG_INFO);

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

    Logger::info("UxPlayCore::start — listening on port {}", config_.raop_port);

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

void UxPlayCore::stop() {
    if (!running_.load() && !raop_) return;

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

    // Log first frame
    static bool first_frame = true;
    if (first_frame) {
        first_frame = false;
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

    // Periodic frame count logging
    static uint64_t frame_count = 0;
    ++frame_count;
    if (frame_count % 300 == 0) {
        Logger::info("UxPlay video frames received: {}", frame_count);
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
