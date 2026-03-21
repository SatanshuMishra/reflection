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
class MFVideoDecoder;
class MirrorWindow;
class SystemTray;
class VideoFrameQueue;
struct AirPlayClientInfo;

/// Main application class. Manages the message loop, system tray,
/// and coordinates between AirPlay service and mirror windows.
///
/// On startup, the app runs as a tray-only application. When an iPad
/// connects via AirPlay, a mirror window is created with D3D11 rendering.
/// The WM_TIMER render loop decodes H.264 frames and displays them.
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

    // UI components
    std::unique_ptr<SystemTray> system_tray_;
    std::unique_ptr<MirrorWindow> mirror_window_;

    // AirPlay service
    std::unique_ptr<AirPlayService> airplay_service_;

    // Video pipeline (created on iPad connect, destroyed on disconnect)
    std::unique_ptr<MFVideoDecoder> decoder_;
    std::unique_ptr<VideoFrameQueue> frame_queue_;

    // Thread-safe storage for connection info from RAOP thread
    std::mutex connection_mutex_;
    std::optional<std::string> pending_device_name_;

    /// Create a hidden message-only window for receiving tray notifications.
    bool create_message_window();

    /// Handle tray menu item selection.
    void on_tray_menu(int menu_item_id);

    /// Read the machine's actual MAC address for AirPlay identification.
    static std::array<uint8_t, 6> get_machine_mac_address();

    /// Start the AirPlay receiver service.
    bool start_airplay_service();

    /// Handle iPad connection (called on main thread via PostMessage).
    void on_ipad_connected();

    /// Handle iPad disconnection (called on main thread via PostMessage).
    void on_ipad_disconnected();

    /// Handle mirror window close (called on main thread via PostMessage).
    void on_mirror_window_closed();

    /// Process render timer tick — decode frames and render.
    void on_render_timer();

    /// Clean up the mirror session (decoder, window, queue).
    void cleanup_mirror_session();

    /// WndProc for the hidden message window.
    static LRESULT CALLBACK message_wnd_proc(
        HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};

} // namespace reflection
