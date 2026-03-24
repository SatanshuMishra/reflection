// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace reflection {

/// Manages mirror window creation, tracking, and lifecycle.
/// Mirrors the macOS AppDelegate's windowControllers dictionary.
class WindowManager {
public:
    WindowManager() = default;
    ~WindowManager();

    // Non-copyable
    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;

    // TODO (Milestone 3): Create mirror window for a device
    // TODO (Milestone 3): Close mirror window for a device
    // TODO (Milestone 3): Track active windows

private:
    // TODO: Map of device_id -> HWND
};

} // namespace reflection
