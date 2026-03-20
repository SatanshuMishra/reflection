#pragma once

#include <cstdint>

namespace reflection {

/// WASAPI audio renderer for low-latency audio output.
///
/// TODO (Milestone 4): Full implementation
class WasapiRenderer {
public:
    WasapiRenderer();
    ~WasapiRenderer();

    bool init(uint32_t sample_rate, uint32_t channels, uint32_t bits_per_sample);
    bool write(const uint8_t* data, size_t size);
    void shutdown();

private:
    bool initialized_ = false;
    // TODO (Milestone 4): IAudioClient, IAudioRenderClient
};

} // namespace reflection
