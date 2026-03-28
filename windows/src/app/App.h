// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace reflection {

class AirPlayService;
class AppSettings;
class IGStreamerPipeline;
class MirrorWindow;
class StatusWindow;
class SystemTray;
class ThemeManager;
struct AirPlayClientInfo;

/// Main application class.
///
/// Architecture: GStreamer handles all heavy lifting (H.264 decode,
/// NV12→RGB conversion, rendering) on its own internal threads.
/// The main thread only runs the Win32 message loop for UI responsiveness.
/// Video/audio data flows directly from RAOP callbacks → GStreamer appsrc.
class App {
public:
    explicit App(HINSTANCE instance);
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool init(int cmd_show);
    int run();

    /// Access settings for external consumers (e.g., WinSparkle init).
    [[nodiscard]] const AppSettings& settings() const;

private:
    HINSTANCE instance_;
    HWND message_hwnd_ = nullptr;

    std::unique_ptr<AppSettings> settings_;
    std::unique_ptr<SystemTray> system_tray_;
    std::unique_ptr<StatusWindow> status_window_;
    std::unique_ptr<ThemeManager> theme_manager_;
    std::unique_ptr<MirrorWindow> mirror_window_;
    std::unique_ptr<AirPlayService> airplay_service_;
    std::shared_ptr<IGStreamerPipeline> pipeline_;

    std::mutex connection_mutex_;
    std::optional<std::string> pending_device_name_;

    /// True while a mirror window is open. Guards against stale WM_DESTROY
    /// messages from previous mirror windows triggering cascading restarts.
    /// Only accessed on the main (UI) thread — not atomic.
    bool has_active_session_ = false;

    /// Cached device name for window title.
    std::string device_name_cache_;

    bool create_message_window();
    void on_tray_menu(int menu_item_id);
    static std::array<uint8_t, 6> get_machine_mac_address();
    bool start_airplay_service();

    void on_ipad_connected();
    void on_ipad_disconnected();
    void on_mirror_window_closed();
    void on_server_name_changed();
    void cleanup_mirror_session();

    static LRESULT CALLBACK message_wnd_proc(
        HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};

} // namespace reflection
