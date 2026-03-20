#pragma once

#include <cstdint>

namespace reflection {

/// Abstract interface for audio decoding.
/// Implementations: FdkAacDecoder (AAC-ELD via Fraunhofer FDK-AAC)
class IAudioDecoder {
public:
    virtual ~IAudioDecoder() = default;

    virtual void init(uint32_t sample_rate, uint32_t channels) = 0;
    virtual bool decode(const uint8_t* data, size_t size, uint64_t timestamp) = 0;
    virtual void shutdown() = 0;

protected:
    IAudioDecoder() = default;
};

} // namespace reflection
