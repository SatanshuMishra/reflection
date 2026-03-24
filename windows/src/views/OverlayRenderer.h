// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

namespace reflection {

/// Renders status overlays on the mirror window.
/// - "Connecting..." spinner during handshake
/// - "Device may be locked or sleeping" with lock icon
///
/// TODO (Milestone 5): Full implementation
class OverlayRenderer {
public:
    OverlayRenderer() = default;
    ~OverlayRenderer() = default;
};

} // namespace reflection
