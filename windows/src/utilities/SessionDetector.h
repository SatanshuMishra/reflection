// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#include <string_view>

namespace reflection {

/// Rendering mode determined by the Windows session type.
/// Console = user is at the physical PC (GPU rendering via DXGI).
/// Remote  = user is connected via RDP (software rendering via GDI).
enum class RenderMode {
    kConsole,
    kRemote
};

/// Detects whether the current Windows session is a console (local)
/// or remote (RDP/RemoteFX) session.
class SessionDetector {
public:
    /// Returns the appropriate rendering mode for the current session.
    /// Uses GetSystemMetrics(SM_REMOTESESSION) — returns kRemote for
    /// RDP, RemoteFX, and Hyper-V Enhanced Session Mode.
    [[nodiscard]] static RenderMode current_render_mode();

    /// Human-readable name for logging ("console" or "remote").
    [[nodiscard]] static std::string_view render_mode_name(RenderMode mode);
};

} // namespace reflection
