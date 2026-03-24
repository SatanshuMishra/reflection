// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace reflection {

/// AirPlay connection state.
enum class AirPlayConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
};

/// Information about a connected AirPlay client.
struct AirPlayClientInfo {
    std::string device_id;
    std::string device_name;
    std::string device_model;
    uint16_t width = 0;
    uint16_t height = 0;
};

/// High-level configuration for starting the AirPlay service.
/// Used by AirPlayService to configure both the core and mDNS.
struct AirPlayServiceConfig {
    std::string server_name = "Reflection";
    std::array<uint8_t, 6> hardware_address = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint16_t raop_port = 5000;
    uint16_t airplay_port = 7000;

    bool operator==(const AirPlayServiceConfig&) const = default;

    /// Format hardware address as colon-separated hex string.
    [[nodiscard]] std::string hw_address_hex() const;
};

} // namespace reflection
