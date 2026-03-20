#pragma once

#include <string>

namespace reflection {

/// Manages Windows auto-start via Registry Run key.
/// Mirrors the macOS LoginItemService (SMAppService).
class AutoStartService {
public:
    /// Check if auto-start is currently enabled.
    [[nodiscard]] static bool is_enabled();

    /// Enable auto-start (adds to HKCU\...\Run).
    static bool enable();

    /// Disable auto-start (removes from HKCU\...\Run).
    static bool disable();
};

} // namespace reflection
