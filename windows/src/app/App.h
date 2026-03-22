#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace reflection {

class AirPlayService;
class MFVideoDecoder;
class MirrorWindow;
class SystemTray;
class VideoFrameQueue;
struct AirPlayClientInfo;

/// Main application class.
///
/// Architecture for responsive mirroring with software H.264 decode:
///
///   RAOP thread (RPiPlay)     Decode thread (App)      Main thread (App)
///   ─────────────────────     ───────────────────      ─────────────────
///   receive H.264 NALUs  →   pop from queue       ←   WM_TIMER fires
///   push to frame_queue_ →   MFT decode (~20ms)       grab latest_frame_
///                             NV12→BGRA (~8ms)         upload to D3D11
///                             store in latest_frame_   Draw + Present (~3ms)
///                                                      process WM_PAINT, etc.
///
/// The decode thread handles ALL heavy work. The main thread only does
/// the fast GPU render (~3ms), leaving ~30ms per timer tick for the
/// Win32 message pump to process window messages → window stays responsive.
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

    // UI
    std::unique_ptr<SystemTray> system_tray_;
    std::unique_ptr<MirrorWindow> mirror_window_;

    // AirPlay
    std::unique_ptr<AirPlayService> airplay_service_;

    // Video pipeline
    std::unique_ptr<MFVideoDecoder> decoder_;
    std::unique_ptr<VideoFrameQueue> frame_queue_;

    // Background decode thread
    std::jthread decode_thread_;

    // Latest decoded frame — written by decode thread, read by render timer.
    // The texture is created via ID3D11Device::CreateTexture2D which is
    // thread-safe (device has internal locking).
    std::mutex frame_mutex_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> latest_frame_;
    int latest_frame_width_ = 0;
    int latest_frame_height_ = 0;
    bool has_new_frame_ = false;

    // Connection state
    std::mutex connection_mutex_;
    std::optional<std::string> pending_device_name_;

    bool create_message_window();
    void on_tray_menu(int menu_item_id);
    static std::array<uint8_t, 6> get_machine_mac_address();
    bool start_airplay_service();

    void on_ipad_connected();
    void on_ipad_disconnected();
    void on_mirror_window_closed();

    /// Render timer — grabs latest decoded frame and renders (~3ms).
    void on_render_timer();

    /// Background decode loop — decodes H.264 + converts NV12→BGRA (~28ms/frame).
    void decode_loop(std::stop_token stop_token);

    void cleanup_mirror_session();

    static LRESULT CALLBACK message_wnd_proc(
        HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
};

} // namespace reflection
