// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <string>

namespace reflection {

/// Configuration for initializing the AirPlay core.
struct AirPlayCoreConfig {
    std::string server_name = "Reflection";
    std::array<uint8_t, 6> hardware_address = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint16_t raop_port = 5000;
    uint16_t airplay_port = 7000;
    int max_connections = 1;

    bool operator==(const AirPlayCoreConfig&) const = default;
};

/// Video frame data delivered by the AirPlay core.
///
/// LIFETIME CONTRACT: `data` is a non-owning pointer into RPiPlay's
/// internal buffer. It is only valid for the duration of the callback
/// invocation. Consumers must copy the data if they need to retain it
/// beyond the callback scope (e.g., into an IMFSample for decode).
struct AirPlayVideoFrame {
    const uint8_t* data = nullptr;
    size_t size = 0;
    uint64_t timestamp = 0;
    /// H.264 NAL unit type (0 = unknown)
    uint8_t nal_type = 0;
};

/// Audio frame data delivered by the AirPlay core.
///
/// LIFETIME CONTRACT: Same as AirPlayVideoFrame — `data` is only valid
/// for the duration of the callback invocation. Copy before returning.
struct AirPlayAudioFrame {
    const uint8_t* data = nullptr;
    size_t size = 0;
    uint64_t timestamp = 0;
    /// Audio codec type (0 = AAC-ELD, 1 = ALAC, 2 = PCM)
    uint8_t codec_type = 0;
    uint32_t sample_rate = 44100;
    uint8_t channels = 2;
};

/// Connection event info from the AirPlay core.
struct AirPlayConnectionEvent {
    std::string device_id;
    std::string device_name;
    std::string device_model;
    uint16_t width = 0;
    uint16_t height = 0;
};

} // namespace reflection
