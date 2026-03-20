#include "airplay/RPiPlayCore.h"
#include "utilities/Logger.h"

// RPiPlay C headers
extern "C" {
#include "raop.h"
#include "stream.h"
}

#include <cstring>

namespace reflection {

RPiPlayCore::~RPiPlayCore() {
    if (running_.load()) {
        stop();
    }
}

bool RPiPlayCore::init(const AirPlayCoreConfig& config) {
    if (raop_) {
        Logger::warn("RPiPlayCore::init called when already initialized");
        return true;
    }

    config_ = config;

    Logger::info("RPiPlayCore::init — server_name='{}', raop_port={}, airplay_port={}",
                 config.server_name, config.raop_port, config.airplay_port);

    // Build RPiPlay callback struct.
    // video_process and audio_process use void* params in our header (because
    // RPiPlay's h264_decode_struct/aac_decode_struct are typedef'd anonymous
    // structs that can't be forward-declared in C++). The function pointer
    // types are compatible at the ABI level — both are pointer-sized params.
    raop_callbacks_t cbs = {};
    cbs.cls = this;
    cbs.video_process = reinterpret_cast<decltype(cbs.video_process)>(&on_video_process);
    cbs.audio_process = reinterpret_cast<decltype(cbs.audio_process)>(&on_audio_process);
    cbs.conn_init = &on_conn_init;
    cbs.conn_destroy = &on_conn_destroy;

    raop_ = raop_init(config.max_connections, &cbs);
    if (!raop_) {
        Logger::error("raop_init failed");
        return false;
    }

    // Set log level to info
    raop_set_log_level(raop_, RAOP_LOG_INFO);

    Logger::info("RPiPlay RAOP server initialized");
    return true;
}

bool RPiPlayCore::start() {
    if (running_.load()) return true;

    if (!raop_) {
        Logger::error("RPiPlayCore::start called before init");
        return false;
    }

    Logger::info("RPiPlayCore::start — listening on port {}", config_.raop_port);

    // raop_start takes a mutable port (it may choose a different one if busy)
    unsigned short port = config_.raop_port;

    // Convert hardware address to char array for RPiPlay
    char hw_addr[6];
    for (int i = 0; i < 6; ++i) {
        hw_addr[i] = static_cast<char>(config_.hardware_address[i]);
    }

    int result = raop_start(raop_, &port, hw_addr, sizeof(hw_addr));
    if (result < 0) {
        Logger::error("raop_start failed with error: {}", result);
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

void RPiPlayCore::stop() {
    if (!running_.load() && !raop_) return;

    Logger::info("RPiPlayCore::stop");

    if (raop_) {
        raop_stop(raop_);
        raop_destroy(raop_);
        raop_ = nullptr;
    }

    running_.store(false);
    Logger::info("RAOP server stopped");
}

bool RPiPlayCore::is_running() const {
    return running_.load();
}

void RPiPlayCore::set_video_callback(VideoFrameCallback callback) {
    std::lock_guard lock(callback_mutex_);
    video_callback_ = std::move(callback);
}

void RPiPlayCore::set_audio_callback(AudioFrameCallback callback) {
    std::lock_guard lock(callback_mutex_);
    audio_callback_ = std::move(callback);
}

void RPiPlayCore::set_connection_callback(ConnectionCallback callback) {
    std::lock_guard lock(callback_mutex_);
    connection_callback_ = std::move(callback);
}

void RPiPlayCore::set_disconnection_callback(DisconnectionCallback callback) {
    std::lock_guard lock(callback_mutex_);
    disconnection_callback_ = std::move(callback);
}

// --------------------------------------------------------------------------
// Static C callbacks — called from RPiPlay's internal RAOP threads
// --------------------------------------------------------------------------

void RPiPlayCore::on_video_process(void* cls, raop_ntp_t* /*ntp*/, void* data) {
    auto* self = static_cast<RPiPlayCore*>(cls);
    auto* h264 = static_cast<h264_decode_struct*>(data);

    if (!h264 || !h264->data || h264->data_len <= 0) return;

    VideoFrameCallback cb;
    {
        std::lock_guard lock(self->callback_mutex_);
        if (!self->video_callback_) return;
        cb = self->video_callback_;
    }

    // Build our frame struct from RPiPlay's h264_decode_struct
    const AirPlayVideoFrame frame{
        .data = h264->data,
        .size = static_cast<size_t>(h264->data_len),
        .timestamp = h264->pts,
        .nal_type = static_cast<uint8_t>(h264->frame_type),
    };

    cb(frame);
}

void RPiPlayCore::on_audio_process(void* cls, raop_ntp_t* /*ntp*/, void* data) {
    auto* self = static_cast<RPiPlayCore*>(cls);
    auto* aac = static_cast<aac_decode_struct*>(data);

    if (!aac || !aac->data || aac->data_len <= 0) return;

    AudioFrameCallback cb;
    {
        std::lock_guard lock(self->callback_mutex_);
        if (!self->audio_callback_) return;
        cb = self->audio_callback_;
    }

    const AirPlayAudioFrame frame{
        .data = aac->data,
        .size = static_cast<size_t>(aac->data_len),
        .timestamp = aac->pts,
        .codec_type = 0,  // AAC-ELD
        .sample_rate = 44100,
        .channels = 2,
    };

    cb(frame);
}

void RPiPlayCore::on_conn_init(void* cls) {
    auto* self = static_cast<RPiPlayCore*>(cls);

    ConnectionCallback cb;
    {
        std::lock_guard lock(self->callback_mutex_);
        if (!self->connection_callback_) return;
        cb = self->connection_callback_;
    }

    // RPiPlay doesn't provide device info in conn_init — use defaults
    cb(AirPlayConnectionEvent{
        .device_id = "unknown",
        .device_name = "iPad",
        .device_model = "iPad",
        .width = 0,
        .height = 0,
    });
}

void RPiPlayCore::on_conn_destroy(void* cls) {
    auto* self = static_cast<RPiPlayCore*>(cls);

    DisconnectionCallback cb;
    {
        std::lock_guard lock(self->callback_mutex_);
        if (!self->disconnection_callback_) return;
        cb = self->disconnection_callback_;
    }

    cb("unknown");
}

} // namespace reflection
