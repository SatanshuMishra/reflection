// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#include <cstddef>
#include <cstdint>

#ifndef _WINDEF_
// Forward declare HWND without including Windows.h
struct HWND__;
typedef HWND__* HWND;
#endif

namespace reflection {

/// Abstract interface for the GStreamer decode/render pipeline.
///
/// Replaces the entire MFVideoDecoder + D3D11Renderer chain with a
/// single GStreamer pipeline that handles H.264 decode (hardware or
/// software) and zero-copy rendering via d3d11videosink.
///
/// Implementations: GStreamerPipeline (production), MockGStreamerPipeline (tests)
class IGStreamerPipeline {
public:
    virtual ~IGStreamerPipeline() = default;

    /// Initialize the pipeline and attach to the given window handle.
    /// Creates the GStreamer pipeline elements and configures d3d11videosink
    /// to render into the provided HWND.
    [[nodiscard]] virtual bool init(HWND window_handle) = 0;

    /// Start the pipeline (transitions to PLAYING state).
    virtual void start() = 0;

    /// Stop the pipeline (transitions to NULL state).
    virtual void stop() = 0;

    /// Push H.264/H.265 NAL unit data into the video pipeline.
    /// Called from the RAOP thread — must be thread-safe.
    /// @param data Raw NAL unit bytes (non-owning, copied internally)
    /// @param size Number of bytes
    /// @param timestamp NTP timestamp from the sender
    virtual void push_video_data(const uint8_t* data, size_t size,
                                  uint64_t timestamp) = 0;

    /// Push audio frame data into the audio pipeline.
    /// Called from the RAOP thread — must be thread-safe.
    /// @param data Raw audio bytes (non-owning, copied internally)
    /// @param size Number of bytes
    /// @param timestamp NTP timestamp from the sender
    virtual void push_audio_data(const uint8_t* data, size_t size,
                                  uint64_t timestamp) = 0;

    /// Attach the video overlay to a window handle.
    /// Can be called after init() to set or change the render target.
    virtual void set_window_handle(HWND window_handle) = 0;

    /// Flush the pipeline and reset decoder state.
    /// Called when the video stream restarts (e.g., iPad lock/unlock) to clear
    /// stale reference frames and prevent decode artifacts.
    virtual void flush_and_reset() = 0;

    /// Whether the pipeline is currently in PLAYING state.
    [[nodiscard]] virtual bool is_playing() const = 0;
};

} // namespace reflection
