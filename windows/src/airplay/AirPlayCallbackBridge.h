#pragma once

#include "airplay/AirPlayCoreTypes.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace reflection {

/// Static bridge between RPiPlay's C function-pointer callbacks and our
/// C++ callback system.
///
/// Design: RPiPlay uses C function pointers. We can't pass captures/lambdas
/// to C function pointers. Instead, we use a global context pointer that
/// is set before start() and cleared after stop(). The static functions
/// check the context, then invoke the corresponding C++ std::function.
///
/// Thread safety: The context pointer is set once before start() and
/// cleared once after stop(). It is not modified during operation.
/// The callbacks themselves are set once before start() and not modified.
///
/// Single-instance by design: Only one AirPlay receiver per machine.
class AirPlayCallbackBridge {
public:
    using VideoFrameCallback = std::function<void(const AirPlayVideoFrame&)>;
    using AudioFrameCallback = std::function<void(const AirPlayAudioFrame&)>;
    using ConnectionCallback = std::function<void(const AirPlayConnectionEvent&)>;
    using DisconnectionCallback = std::function<void(const std::string&)>;

    // --- Context management ---

    /// Set the active context pointer (any non-null value enables callbacks).
    static void set_context(void* context);

    /// Clear the context pointer (disables all callbacks).
    static void clear_context();

    /// Whether a context is currently set.
    [[nodiscard]] static bool has_context();

    // --- Callback setters ---

    static void set_video_callback(VideoFrameCallback callback);
    static void set_audio_callback(AudioFrameCallback callback);
    static void set_connection_callback(ConnectionCallback callback);
    static void set_disconnection_callback(DisconnectionCallback callback);

    // --- Static C-compatible callback functions ---
    // These can be used as RPiPlay raop_callbacks_t function pointers.

    static void on_video_frame(const uint8_t* data, size_t size, uint64_t timestamp);
    static void on_audio_frame(const uint8_t* data, size_t size, uint64_t timestamp);
    static void on_client_connected(const char* device_id, const char* device_name);
    static void on_client_disconnected(const char* device_id);
};

} // namespace reflection
