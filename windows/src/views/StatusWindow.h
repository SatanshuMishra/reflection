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

class WebViewHost;
class AppSettings;

/// Main application window showing discoverable/connected status and settings.
/// Replaces the macOS DeviceListView — since AirPlay is wireless on Windows,
/// there's no device list. Instead shows:
///   - "Discoverable as: {name}" (editable)
///   - "Connected to: {iPad name}" when mirroring
///   - Settings page (theme, run in background, launch at login)
class StatusWindow {
public:
    StatusWindow();
    ~StatusWindow();

    // Non-copyable
    StatusWindow(const StatusWindow&) = delete;
    StatusWindow& operator=(const StatusWindow&) = delete;

    /// Create and show the status window.
    /// @param instance  Application instance handle
    /// @param settings  App settings reference
    /// @param message_hwnd  The app's message-only window for forwarding events
    bool create(HINSTANCE instance, AppSettings& settings, HWND message_hwnd);

    /// Destroy the window and clean up.
    void destroy();

    /// Whether the window is currently visible.
    [[nodiscard]] bool is_visible() const;

    /// Show/focus the window.
    void show();

    /// Hide the window.
    void hide();

    /// Notify the status window of a connection state change.
    void set_connected(const std::string& device_name);
    void set_disconnected();

    /// Update the server name display.
    void set_server_name(const std::string& name);

    /// Update the theme.
    void set_theme(const std::string& theme);

    /// Get the window handle.
    [[nodiscard]] HWND hwnd() const { return hwnd_; }

private:
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg,
                                      WPARAM wparam, LPARAM lparam);
    void on_message_from_webview(const std::wstring& json);

    /// Run elevated netsh.exe to add a firewall inbound rule.
    void configure_firewall();

    /// Get the absolute path to the UI assets directory.
    static std::wstring get_assets_path();

    HWND hwnd_ = nullptr;
    HWND message_hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    AppSettings* settings_ = nullptr;
    std::unique_ptr<WebViewHost> webview_;

    // Deferred connection state — if set_connected() is called before
    // the WebView2 page has loaded, we replay it after navigation completes.
    bool page_loaded_ = false;
    bool deferred_connected_ = false;
    std::string deferred_device_name_;
};

} // namespace reflection
