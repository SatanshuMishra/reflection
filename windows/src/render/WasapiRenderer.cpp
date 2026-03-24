// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "render/WasapiRenderer.h"
#include "utilities/Logger.h"

namespace reflection {

WasapiRenderer::WasapiRenderer() = default;
WasapiRenderer::~WasapiRenderer() { shutdown(); }

bool WasapiRenderer::init(
    uint32_t sample_rate, uint32_t channels, uint32_t bits_per_sample
) {
    Logger::info("Initializing WASAPI renderer: {}Hz, {}ch, {}bit",
                 sample_rate, channels, bits_per_sample);
    // TODO (Milestone 4): Initialize IAudioClient in shared mode
    initialized_ = true;
    return true;
}

bool WasapiRenderer::write(const uint8_t* /*data*/, size_t size) {
    if (!initialized_) return false;
    Logger::debug("WasapiRenderer::write {} bytes", size);
    // TODO (Milestone 4): Write PCM data to IAudioRenderClient
    return true;
}

void WasapiRenderer::shutdown() {
    if (!initialized_) return;
    Logger::info("Shutting down WASAPI renderer");
    // TODO (Milestone 4): Release IAudioClient, IAudioRenderClient
    initialized_ = false;
}

} // namespace reflection
