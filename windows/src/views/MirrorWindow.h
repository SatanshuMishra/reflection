// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <memory>
#include <string>

namespace reflection {

class MirrorOverlay;

/// Saved window state for fullscreen toggle restore.
struct SavedWindowState {
    LONG style = 0;
    LONG ex_style = 0;
    RECT rect{};
    bool was_maximized = false;
};

/// Custom borderless mirror window with hover overlay toolbar.
///
/// Architecture (child HWND split):
///   MirrorWindow (WS_POPUP | WS_THICKFRAME) — frame container
///     +-- video_child_  (WS_CHILD) — GStreamer d3d11videosink renders here
///     +-- overlay_       (WS_CHILD) — hover toolbar (above video in z-order)
///
/// Features:
///   - Titlebar removed (WS_POPUP), resizable (WS_THICKFRAME)
///   - Hover overlay with device name + Close/Min/Max/FullScreen/Lock buttons
///   - Aspect ratio lock toggle (WM_SIZING enforcement)
///   - Full-screen mode (F11/Escape)
///   - Auto-hide overlay after 1s mouse leave
///
/// Lifecycle:
///   - Created when an iPad connects via AirPlay
///   - Destroyed when the iPad disconnects or user closes
///   - Posts kWmMirrorWindowClosed to app's message window on close
class MirrorWindow {
public:
    MirrorWindow();
    ~MirrorWindow();

    MirrorWindow(const MirrorWindow&) = delete;
    MirrorWindow& operator=(const MirrorWindow&) = delete;

    /// Create and show the mirror window.
    /// @param instance     Application HINSTANCE
    /// @param title        Window title (shown in taskbar)
    /// @param app_hwnd     App's message window (receives kWmMirrorWindowClosed)
    /// @param device_name  Device name for overlay display (e.g., "iPad Air")
    [[nodiscard]] bool create(HINSTANCE instance, const std::wstring& title,
                               HWND app_hwnd, const std::wstring& device_name);

    /// Close and destroy the window.
    void close();

    /// Get the top-level window handle.
    [[nodiscard]] HWND hwnd() const { return hwnd_; }

    /// Get the video child HWND (GStreamer renders into this).
    [[nodiscard]] HWND video_hwnd() const { return video_child_; }

    /// Toggle aspect ratio lock mode.
    void toggle_aspect_lock();

    /// Toggle fullscreen mode.
    void toggle_fullscreen();

    /// Query state.
    [[nodiscard]] bool is_fullscreen() const { return is_fullscreen_; }
    [[nodiscard]] bool is_aspect_locked() const { return aspect_locked_; }

private:
    HWND hwnd_ = nullptr;
    HWND app_hwnd_ = nullptr;
    HWND video_child_ = nullptr;
    HINSTANCE instance_ = nullptr;

    std::unique_ptr<MirrorOverlay> overlay_;
    std::wstring device_name_;

    // Fullscreen state
    bool is_fullscreen_ = false;
    SavedWindowState saved_state_{};

    // Aspect ratio lock
    bool aspect_locked_ = false;
    double aspect_ratio_ = 4.0 / 3.0;  // Updated from constants

    // Mouse tracking for overlay auto-hide
    bool tracking_mouse_ = false;
    bool overlay_visible_ = false;

    // Child HWND creation
    [[nodiscard]] bool create_video_child();
    [[nodiscard]] bool create_overlay();

    // Overlay show/hide
    void show_overlay();
    void hide_overlay();
    void start_hide_timer();
    void cancel_hide_timer();

    // Resize helpers
    void resize_children();
    void enforce_aspect_ratio(RECT& rect, WPARAM edge) const;
    void snap_to_aspect_ratio();

    // Fullscreen helpers
    void enter_fullscreen();
    void exit_fullscreen();

    // DWM styling
    void apply_dwm_attributes();

    // WndProc
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp);
    LRESULT handle_message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};

} // namespace reflection
