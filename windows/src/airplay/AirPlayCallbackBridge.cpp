#include "airplay/AirPlayCallbackBridge.h"

#include <mutex>

namespace reflection {

namespace {
    // Global state — single-instance by design.
    // Protected by mutex because RPiPlay callbacks fire from RAOP threads
    // while set_context/clear_context are called from the owning thread.
    std::mutex g_mutex;
    void* g_context = nullptr;

    AirPlayCallbackBridge::VideoFrameCallback g_video_callback;
    AirPlayCallbackBridge::AudioFrameCallback g_audio_callback;
    AirPlayCallbackBridge::ConnectionCallback g_connection_callback;
    AirPlayCallbackBridge::DisconnectionCallback g_disconnection_callback;
} // anonymous namespace

// --- Context management ---

void AirPlayCallbackBridge::set_context(void* context) {
    std::lock_guard lock(g_mutex);
    g_context = context;
}

void AirPlayCallbackBridge::clear_context() {
    std::lock_guard lock(g_mutex);
    g_context = nullptr;
}

bool AirPlayCallbackBridge::has_context() {
    std::lock_guard lock(g_mutex);
    return g_context != nullptr;
}

// --- Callback setters ---

void AirPlayCallbackBridge::set_video_callback(VideoFrameCallback callback) {
    std::lock_guard lock(g_mutex);
    g_video_callback = std::move(callback);
}

void AirPlayCallbackBridge::set_audio_callback(AudioFrameCallback callback) {
    std::lock_guard lock(g_mutex);
    g_audio_callback = std::move(callback);
}

void AirPlayCallbackBridge::set_connection_callback(ConnectionCallback callback) {
    std::lock_guard lock(g_mutex);
    g_connection_callback = std::move(callback);
}

void AirPlayCallbackBridge::set_disconnection_callback(DisconnectionCallback callback) {
    std::lock_guard lock(g_mutex);
    g_disconnection_callback = std::move(callback);
}

// --- C-compatible callback functions ---
// These are called from RPiPlay's internal threads.
// Must acquire the mutex to safely read context and callbacks.

void AirPlayCallbackBridge::on_video_frame(
    const uint8_t* data, size_t size, uint64_t timestamp
) {
    VideoFrameCallback cb;
    {
        std::lock_guard lock(g_mutex);
        if (!g_context || !g_video_callback) return;
        cb = g_video_callback; // Copy under lock
    }
    cb(AirPlayVideoFrame{data, size, timestamp, 0}); // Invoke outside lock
}

void AirPlayCallbackBridge::on_audio_frame(
    const uint8_t* data, size_t size, uint64_t timestamp
) {
    AudioFrameCallback cb;
    {
        std::lock_guard lock(g_mutex);
        if (!g_context || !g_audio_callback) return;
        cb = g_audio_callback;
    }
    cb(AirPlayAudioFrame{data, size, timestamp, 0, 44100, 2});
}

void AirPlayCallbackBridge::on_client_connected(
    const char* device_id, const char* device_name
) {
    ConnectionCallback cb;
    {
        std::lock_guard lock(g_mutex);
        if (!g_context || !g_connection_callback) return;
        cb = g_connection_callback;
    }
    cb(AirPlayConnectionEvent{
        device_id ? device_id : "",
        device_name ? device_name : "",
        "", 0, 0
    });
}

void AirPlayCallbackBridge::on_client_disconnected(const char* device_id) {
    DisconnectionCallback cb;
    {
        std::lock_guard lock(g_mutex);
        if (!g_context || !g_disconnection_callback) return;
        cb = g_disconnection_callback;
    }
    cb(device_id ? device_id : "");
}

} // namespace reflection
