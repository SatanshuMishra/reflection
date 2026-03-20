#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <string>

namespace reflection {

/// Win32 window that hosts the D3D11 renderer for displaying mirrored content.
/// Mirrors the macOS MirrorWindowController.
///
/// TODO (Milestone 3): Full implementation
class MirrorWindow {
public:
    MirrorWindow();
    ~MirrorWindow();

    /// Create and show the mirror window.
    bool create(HINSTANCE instance, const std::wstring& title);

    /// Close and destroy the window.
    void close();

    /// Get the window handle.
    [[nodiscard]] HWND hwnd() const { return hwnd_; }

private:
    HWND hwnd_ = nullptr;
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};

} // namespace reflection
