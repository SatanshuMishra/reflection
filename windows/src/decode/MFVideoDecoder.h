#pragma once

#include "decode/IVideoDecoder.h"

namespace reflection {

/// Media Foundation H.264 decoder with D3D11 hardware acceleration.
/// Uses DXVA2/D3D11VA for GPU-accelerated decode.
///
/// TODO (Milestone 3): Full implementation
class MFVideoDecoder : public IVideoDecoder {
public:
    MFVideoDecoder();
    ~MFVideoDecoder() override;

    void init() override;
    bool decode(const uint8_t* data, size_t size, uint64_t timestamp) override;
    void shutdown() override;

private:
    bool initialized_ = false;
    // TODO (Milestone 3): IMFTransform, IMFDXGIDeviceManager, etc.
};

} // namespace reflection
