// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#include <optional>
#include <string>

#include "capture/CaptureError.h"

namespace reflection {

/// Capture session state — mirrors the macOS CaptureState enum exactly.
enum class CaptureState {
    Idle,       /// No capture session active.
    Starting,   /// Capture is being set up.
    Running,    /// Actively receiving frames.
    Stopped,    /// Capture was stopped normally.
    Failed,     /// Capture failed (see associated error).
};

/// Returns a human-readable label for the capture state.
constexpr const char* capture_state_label(CaptureState state) {
    switch (state) {
        case CaptureState::Idle:     return "Idle";
        case CaptureState::Starting: return "Starting";
        case CaptureState::Running:  return "Running";
        case CaptureState::Stopped:  return "Stopped";
        case CaptureState::Failed:   return "Failed";
    }
    return "Unknown";
}

} // namespace reflection
