// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#include "decode/IAudioDecoder.h"

namespace reflection {

/// AAC-ELD decoder using Fraunhofer FDK-AAC library.
///
/// TODO (Milestone 4): Full implementation
class FdkAacDecoder : public IAudioDecoder {
public:
    FdkAacDecoder();
    ~FdkAacDecoder() override;

    void init(uint32_t sample_rate, uint32_t channels) override;
    bool decode(const uint8_t* data, size_t size, uint64_t timestamp) override;
    void shutdown() override;

private:
    bool initialized_ = false;
    // TODO (Milestone 4): HANDLE_AACDECODER from FDK-AAC
};

} // namespace reflection
