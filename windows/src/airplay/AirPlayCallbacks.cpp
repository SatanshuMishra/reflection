// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "airplay/AirPlayCallbacks.h"
#include "utilities/Logger.h"

namespace reflection {

void AirPlayCallbacks::on_video_frame(
    const uint8_t* /*data*/, size_t size, uint64_t timestamp
) {
    // TODO (Milestone 3): Route to MFVideoDecoder → D3D11Renderer
    Logger::debug("Video frame received: {} bytes, ts={}", size, timestamp);
}

void AirPlayCallbacks::on_audio_frame(
    const uint8_t* /*data*/, size_t size, uint64_t timestamp
) {
    // TODO (Milestone 4): Route to FdkAacDecoder → WasapiRenderer
    Logger::debug("Audio frame received: {} bytes, ts={}", size, timestamp);
}

} // namespace reflection
