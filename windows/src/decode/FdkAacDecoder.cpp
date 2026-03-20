#include "decode/FdkAacDecoder.h"
#include "utilities/Logger.h"

namespace reflection {

FdkAacDecoder::FdkAacDecoder() = default;
FdkAacDecoder::~FdkAacDecoder() { shutdown(); }

void FdkAacDecoder::init(uint32_t sample_rate, uint32_t channels) {
    Logger::info("Initializing FDK-AAC decoder: {}Hz, {} channels",
                 sample_rate, channels);
    // TODO (Milestone 4): aacDecoder_Open, configure for AAC-ELD
    initialized_ = true;
}

bool FdkAacDecoder::decode(
    const uint8_t* /*data*/, size_t size, uint64_t timestamp
) {
    if (!initialized_) return false;
    Logger::debug("FdkAacDecoder::decode {} bytes, ts={}", size, timestamp);
    // TODO (Milestone 4): aacDecoder_Fill + aacDecoder_DecodeFrame
    return false;
}

void FdkAacDecoder::shutdown() {
    if (!initialized_) return;
    Logger::info("Shutting down FDK-AAC decoder");
    // TODO (Milestone 4): aacDecoder_Close
    initialized_ = false;
}

} // namespace reflection
