#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <memory>
#include <string>

namespace reflection {

class D3D11Renderer;

/// Win32 window that hosts the D3D11 renderer for displaying mirrored content.
/// Owns a D3D11Renderer instance for GPU-accelerated NV12 video rendering.
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

    // Non-copyable
    MirrorWindow(const MirrorWindow&) = delete;
    MirrorWindow& operator=(const MirrorWindow&) = delete;

    /// Create and show the mirror window with an integrated D3D11 renderer.
    /// @param instance    Application HINSTANCE
    /// @param title       Window title
    /// @param app_hwnd    App's message window HWND (receives kWmMirrorWindowClosed)
    [[nodiscard]] bool create(HINSTANCE instance, const std::wstring& title,
                              HWND app_hwnd);

    /// Close and destroy the window.
    void close();

    /// Get the window handle.
    [[nodiscard]] HWND hwnd() const { return hwnd_; }

    /// Get the D3D11 renderer (for passing decoded textures to render).
    [[nodiscard]] D3D11Renderer* renderer() const { return renderer_.get(); }

private:
    HWND hwnd_ = nullptr;
    HWND app_hwnd_ = nullptr;  // App's message window for close notification
    std::unique_ptr<D3D11Renderer> renderer_;

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};

} // namespace reflection
