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
#include <thread>
#include <vector>

namespace reflection {

class AirPlayService;
class MFVideoDecoder;
class MirrorWindow;
class SystemTray;
class VideoFrameQueue;
struct AirPlayClientInfo;

/// Main application class.
///
/// Architecture: decode thread produces raw BGRA pixels, main thread
/// creates the D3D11 texture and renders. This avoids GPU driver issues
/// with CreateTexture2D on background threads (especially on Hyper-V
/// virtual GPUs where pSysMem copies may be deferred).
class App {
public:
    explicit App(HINSTANCE instance);
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool init(int cmd_show);
    int run();

private:
    HINSTANCE instance_;
    HWND message_hwnd_ = nullptr;

    std::unique_ptr<SystemTray> system_tray_;
    std::unique_ptr<MirrorWindow> mirror_window_;
    std::unique_ptr<AirPlayService> airplay_service_;
    std::unique_ptr<MFVideoDecoder> decoder_;
    std::unique_ptr<VideoFrameQueue> frame_queue_;
    std::jthread decode_thread_;

    // Shared state: raw BGRA pixels from decode thread → main thread.
    // Using raw bytes instead of ID3D11Texture2D avoids all GPU threading
    // issues. The main thread creates the texture on the UI thread where
    // the D3D11 immediate context lives.
    std::mutex frame_mutex_;
    std::vector<uint8_t> latest_bgra_;
    int latest_width_ = 0;
    int latest_height_ = 0;
    bool has_new_frame_ = false;

    std::mutex connection_mutex_;
    std::optional<std::string> pending_device_name_;

    bool create_message_window();
    void on_tray_menu(int menu_item_id);
    static std::array<uint8_t, 6> get_machine_mac_address();
    bool start_airplay_service();

    void on_ipad_connected();
    void on_ipad_disconnected();
    void on_mirror_window_closed();
    void on_render_timer();
    void decode_loop(std::stop_token stop_token);
    void cleanup_mirror_session();

    static LRESULT CALLBACK message_wnd_proc(
        HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};

} // namespace reflection
