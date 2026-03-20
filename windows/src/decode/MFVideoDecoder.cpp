#include "decode/MFVideoDecoder.h"
#include "utilities/Logger.h"

namespace reflection {

MFVideoDecoder::MFVideoDecoder() = default;

MFVideoDecoder::~MFVideoDecoder() {
    shutdown();
}

void MFVideoDecoder::init() {
    if (initialized_) return;

    Logger::info("Initializing Media Foundation H.264 decoder");

    // TODO (Milestone 3):
    // 1. Create ID3D11Device for hardware decode
    // 2. Create IMFDXGIDeviceManager and associate with device
    // 3. Enumerate hardware H.264 MFTs via MFTEnumEx
    // 4. Configure input type: MFVideoFormat_H264
    // 5. Configure output type: MFVideoFormat_NV12
    // 6. Set D3D manager on MFT

    initialized_ = true;
    Logger::info("Media Foundation decoder initialized");
}

bool MFVideoDecoder::decode(
    const uint8_t* /*data*/, size_t size, uint64_t timestamp
) {
    if (!initialized_) return false;

    // TODO (Milestone 3):
    // 1. Wrap NAL unit data in IMFSample + IMFMediaBuffer
    // 2. Call ProcessInput on the MFT
    // 3. Call ProcessOutput to get decoded frame
    // 4. Extract ID3D11Texture2D from output sample
    // 5. Pass texture to D3D11Renderer

    Logger::debug("MFVideoDecoder::decode {} bytes, ts={}", size, timestamp);
    return false;
}

void MFVideoDecoder::shutdown() {
    if (!initialized_) return;

    Logger::info("Shutting down Media Foundation decoder");

    // TODO (Milestone 3): Release MFT, device manager, device

    initialized_ = false;
}

} // namespace reflection
