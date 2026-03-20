#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <array>
#include <memory>
#include <string>

namespace reflection {

class AirPlayService;
class MirrorWindow;
class SystemTray;

/// Main application class. Manages the message loop, system tray,
/// and coordinates between AirPlay service and mirror windows.
///
/// On startup, the app runs as a tray-only application. The mirror
/// window is created/shown when an iPad connects (future milestone).
class App {
public:
    explicit App(HINSTANCE instance);
    ~App();

    // Non-copyable
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    /// Initialize the application (AirPlay service, system tray, etc.).
    /// Returns false if initialization fails.
    bool init(int cmd_show);

    /// Run the Win32 message loop. Returns exit code.
    int run();

private:
    HINSTANCE instance_;
    HWND message_hwnd_ = nullptr;
    std::unique_ptr<SystemTray> system_tray_;
    std::unique_ptr<AirPlayService> airplay_service_;
    std::unique_ptr<MirrorWindow> mirror_window_;

    /// Create a hidden message-only window for receiving tray notifications.
    bool create_message_window();

    /// Handle tray menu item selection.
    void on_tray_menu(int menu_item_id);

    /// Read the machine's actual MAC address for AirPlay identification.
    static std::array<uint8_t, 6> get_machine_mac_address();

    /// Start the AirPlay receiver service.
    bool start_airplay_service();

    /// WndProc for the hidden message window.
    static LRESULT CALLBACK message_wnd_proc(
        HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};

} // namespace reflection
