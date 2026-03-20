#pragma once

#include <cstdint>

namespace reflection {

/// Callback bridge between RPiPlay's C callbacks and our C++ pipeline.
///
/// RPiPlay's raop_callbacks_t uses C function pointers. This class
/// provides static methods that can be used as those callbacks, then
/// routes data to the appropriate C++ decode/render pipeline.
///
/// TODO (Milestone 2): Wire to RPiPlay's raop_callbacks_t
class AirPlayCallbacks {
public:
    /// Called when a video frame (H.264 NAL units) is received.
    static void on_video_frame(
        const uint8_t* data, size_t size, uint64_t timestamp
    );

    /// Called when an audio frame (AAC-ELD) is received.
    static void on_audio_frame(
        const uint8_t* data, size_t size, uint64_t timestamp
    );
};

} // namespace reflection
