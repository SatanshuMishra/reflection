#pragma once

#include <cstdint>

namespace reflection {

/// Abstract interface for H.264 video decoding.
/// Implementations: MFVideoDecoder (Windows Media Foundation, GPU-accelerated)
class IVideoDecoder {
public:
    virtual ~IVideoDecoder() = default;

    /// Initialize the decoder. Throws on failure.
    virtual void init() = 0;

    /// Decode an H.264 NAL unit. Returns true if a frame was produced.
    virtual bool decode(const uint8_t* data, size_t size, uint64_t timestamp) = 0;

    /// Shut down the decoder and release resources.
    virtual void shutdown() = 0;

protected:
    IVideoDecoder() = default;
};

} // namespace reflection
