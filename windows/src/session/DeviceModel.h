#pragma once

#include <string>

namespace reflection {

/// Represents a connected AirPlay device (iPad).
/// Value type — mirrors the macOS DeviceModel struct.
struct DeviceModel {
    std::string id;          /// Unique connection identifier.
    std::string name;        /// Device display name (e.g., "iPad Pro").
    std::string model_id;    /// Device model (e.g., "iPad13,4").
    bool is_connected = false;  /// Whether the device is currently mirroring.

    bool operator==(const DeviceModel&) const = default;
};

} // namespace reflection
