#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <string>

namespace reflection {

/// Win32 window that hosts the GStreamer video overlay.
/// GStreamer's d3d11videosink renders directly into this HWND —
/// no custom D3D11 renderer needed.
///
/// Lifecycle:
/// - Created when an iPad connects via AirPlay
/// - Destroyed when the iPad disconnects or the user closes the window
/// - On close, posts kWmMirrorWindowClosed to the app's message window
///   so the app stays running as a tray icon
class MirrorWindow {
public:
    MirrorWindow();
    ~MirrorWindow();

    MirrorWindow(const MirrorWindow&) = delete;
    MirrorWindow& operator=(const MirrorWindow&) = delete;

    /// Create and show the mirror window.
    /// @param instance    Application HINSTANCE
    /// @param title       Window title
    /// @param app_hwnd    App's message window HWND (receives kWmMirrorWindowClosed)
    [[nodiscard]] bool create(HINSTANCE instance, const std::wstring& title,
                              HWND app_hwnd);

    /// Close and destroy the window.
    void close();

    /// Get the window handle (GStreamer renders into this via VideoOverlay).
    [[nodiscard]] HWND hwnd() const { return hwnd_; }

private:
    HWND hwnd_ = nullptr;
    HWND app_hwnd_ = nullptr;

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};

} // namespace reflection
