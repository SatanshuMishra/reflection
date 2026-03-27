// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "utilities/SessionDetector.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace reflection {

RenderMode SessionDetector::current_render_mode() {
    // SM_REMOTESESSION is nonzero for any Remote Desktop session
    // (RDP, RemoteFX, Hyper-V Enhanced Session Mode).
    const int remote = GetSystemMetrics(SM_REMOTESESSION);
    return (remote != 0) ? RenderMode::kRemote : RenderMode::kConsole;
}

std::string_view SessionDetector::render_mode_name(RenderMode mode) {
    switch (mode) {
        case RenderMode::kConsole: return "console";
        case RenderMode::kRemote:  return "remote";
    }
    return "unknown";
}

} // namespace reflection
