#include "app/App.h"
#include "app/SystemTray.h"
#include "airplay/AirPlayService.h"
#include "airplay/AirPlayTypes.h"
#include "airplay/RPiPlayCore.h"
#include "decode/MFVideoDecoder.h"
#include "decode/VideoFrameQueue.h"
#include "mdns/NativeMdnsAdvertiser.h"
#include "render/D3D11Renderer.h"
#include "views/MirrorWindow.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <iphlpapi.h>

#include <vector>

// Link dependencies are managed by CMakeLists.txt: iphlpapi

namespace reflection {

App::App(HINSTANCE instance)
    : instance_(instance)
{
}

App::~App() {
    // Clean up mirror session first
    cleanup_mirror_session();

    // Stop AirPlay service before window destruction
    if (airplay_service_) {
        airplay_service_->stop();
    }

    // System tray icon removed automatically by SystemTray destructor

    // Destroy message window
    if (message_hwnd_) {
        DestroyWindow(message_hwnd_);
        message_hwnd_ = nullptr;
    }
}

bool App::init(int /*cmd_show*/) {
    Logger::info("Initializing application...");

    // Create hidden message window for tray icon messages
    if (!create_message_window()) {
        Logger::error("Failed to create message window");
        return false;
    }

    // Install system tray icon
    system_tray_ = std::make_unique<SystemTray>(instance_);
    if (!system_tray_->install(message_hwnd_)) {
        Logger::error("Failed to install system tray icon");
        return false;
    }

    // Wire tray menu callbacks
    system_tray_->set_menu_callback(
        [this](int id) { on_tray_menu(id); });

    // Create frame queue early — it needs to exist before RAOP callbacks fire
    frame_queue_ = std::make_unique<VideoFrameQueue>();

    // Start AirPlay service (mDNS + RAOP)
    if (!start_airplay_service()) {
        Logger::error("Failed to start AirPlay service — iPad won't see this PC");
        system_tray_->set_tooltip(L"Reflection — AirPlay failed to start");
        // Continue anyway — user can see the tray icon and quit
    }

    Logger::info("Application initialized — waiting for AirPlay connections...");
    return true;
}

int App::run() {
    Logger::info("Entering message loop");

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}

bool App::create_message_window() {
    // Register a simple window class for the hidden message window
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = message_wnd_proc;
    wc.hInstance = instance_;
    wc.lpszClassName = constants::kAppWindowClass.data();

    RegisterClassEx(&wc);

    // Create a message-only window (HWND_MESSAGE parent = invisible, no taskbar)
    message_hwnd_ = CreateWindowEx(
        0,
        constants::kAppWindowClass.data(),
        constants::kAppName.data(),
        0,  // No styles needed for a message-only window
        0, 0, 0, 0,
        HWND_MESSAGE,  // Message-only window — invisible
        nullptr,
        instance_,
        this  // Pass 'this' as lpParam for WM_CREATE
    );

    if (!message_hwnd_) {
        Logger::error("CreateWindowEx failed for message window: {}",
                      GetLastError());
        return false;
    }

    Logger::debug("Message window created (HWND_MESSAGE)");
    return true;
}

void App::on_tray_menu(int menu_item_id) {
    switch (menu_item_id) {
        case SystemTray::kMenuQuit:
            Logger::info("Quit requested from tray menu");
            cleanup_mirror_session();
            PostQuitMessage(0);
            break;

        case SystemTray::kMenuDisconnect:
            Logger::info("Disconnect requested from tray menu");
            cleanup_mirror_session();
            if (system_tray_) {
                system_tray_->set_tooltip(L"Reflection — Waiting for iPad...");
            }
            break;

        case SystemTray::kMenuSettings:
            Logger::info("Settings requested from tray menu");
            // TODO (Milestone 6): Open settings dialog
            break;

        case SystemTray::kMenuAbout:
            Logger::info("About requested from tray menu");
            // TODO (Milestone 6): Show about dialog
            break;

        case SystemTray::kMenuCheckUpdate:
            Logger::info("Check for updates requested from tray menu");
            // TODO (Milestone 6): Trigger UpdateChecker
            break;

        default:
            Logger::debug("Unknown tray menu item: {}", menu_item_id);
            break;
    }
}

LRESULT CALLBACK App::message_wnd_proc(
    HWND hwnd, UINT msg, WPARAM wp, LPARAM lp
) {
    switch (msg) {
        case WM_CREATE: {
            // Store 'this' pointer from CreateWindowEx lpParam
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
            SetWindowLongPtr(hwnd, GWLP_USERDATA,
                             reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_TIMER: {
            if (wp == constants::kRenderTimerId) {
                auto* app = reinterpret_cast<App*>(
                    GetWindowLongPtr(hwnd, GWLP_USERDATA));
                if (app) {
                    app->on_render_timer();
                }
            }
            return 0;
        }

        default:
            break;
    }

    // Handle custom app messages
    if (msg == constants::kWmTrayIcon) {
        auto* app = reinterpret_cast<App*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));

        if (app && app->system_tray_) {
            switch (LOWORD(lp)) {
                case WM_RBUTTONUP:
                case WM_CONTEXTMENU:
                    app->system_tray_->show_context_menu(hwnd);
                    break;
                case WM_LBUTTONDBLCLK:
                    // TODO (Milestone 6): Open settings or show status
                    break;
                default:
                    break;
            }
        }
        return 0;
    }

    if (msg == constants::kWmIpadConnected) {
        auto* app = reinterpret_cast<App*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (app) {
            app->on_ipad_connected();
        }
        return 0;
    }

    if (msg == constants::kWmIpadDisconnected) {
        auto* app = reinterpret_cast<App*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (app) {
            app->on_ipad_disconnected();
        }
        return 0;
    }

    if (msg == constants::kWmMirrorWindowClosed) {
        auto* app = reinterpret_cast<App*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (app) {
            app->on_mirror_window_closed();
        }
        return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// iPad Connection Lifecycle
// ---------------------------------------------------------------------------

void App::on_ipad_connected() {
    Logger::info("Handling iPad connection on main thread");

    // Get the device name stored by the RAOP callback thread
    std::string device_name;
    {
        std::lock_guard lock(connection_mutex_);
        device_name = pending_device_name_.value_or("iPad");
        pending_device_name_.reset();
    }

    // If we already have a mirror window, just bring it to front
    if (mirror_window_ && mirror_window_->hwnd()) {
        Logger::info("Mirror window already exists — bringing to front");
        SetForegroundWindow(mirror_window_->hwnd());
        return;
    }

    // Create the mirror window with D3D11 renderer
    mirror_window_ = std::make_unique<MirrorWindow>();
    std::wstring title = L"Reflection \u2014 " +
        std::wstring(device_name.begin(), device_name.end());

    if (!mirror_window_->create(instance_, title, message_hwnd_)) {
        Logger::error("Failed to create mirror window");
        mirror_window_.reset();
        return;
    }

    // Initialize the H.264 decoder (CPU-only, no D3D11 dependency).
    // The decoder produces raw BGRA bytes; the main thread creates textures.
    decoder_ = std::make_unique<MFVideoDecoder>();
    if (!decoder_->init()) {
        Logger::error("Failed to initialize MFVideoDecoder");
        decoder_.reset();
    }

    // Start background decode thread — handles ALL heavy work
    // (H.264 decode ~20ms + NV12→BGRA ~8ms per frame)
    decode_thread_ = std::jthread([this](std::stop_token token) {
        decode_loop(token);
    });

    // Start render timer — only does fast GPU render (~3ms per frame)
    SetTimer(message_hwnd_, constants::kRenderTimerId,
             constants::kRenderTimerIntervalMs, nullptr);

    // Update tray tooltip
    if (system_tray_) {
        std::wstring tooltip = L"Reflection \u2014 Connected: " +
            std::wstring(device_name.begin(), device_name.end());
        system_tray_->set_tooltip(tooltip);
    }

    Logger::info("Mirror session started for: {}", device_name);
}

void App::on_ipad_disconnected() {
    Logger::info("iPad disconnected — cleaning up mirror session");
    cleanup_mirror_session();

    if (system_tray_) {
        system_tray_->set_tooltip(L"Reflection \u2014 Waiting for iPad...");
    }
}

void App::on_mirror_window_closed() {
    Logger::info("Mirror window closed by user");
    KillTimer(message_hwnd_, constants::kRenderTimerId);

    // Stop decode thread FIRST (it references decoder_)
    if (decode_thread_.joinable()) {
        decode_thread_.request_stop();
        decode_thread_.join();
    }

    decoder_.reset();
    mirror_window_.reset();

    {
        std::lock_guard lock(frame_mutex_);
        latest_bgra_.clear();
        latest_bgra_.shrink_to_fit();
        has_new_frame_ = false;
    }

    if (frame_queue_) frame_queue_->clear();

    if (system_tray_) {
        system_tray_->set_tooltip(L"Reflection \u2014 Waiting for iPad...");
    }
}

void App::cleanup_mirror_session() {
    if (message_hwnd_) {
        KillTimer(message_hwnd_, constants::kRenderTimerId);
    }

    if (decode_thread_.joinable()) {
        decode_thread_.request_stop();
        decode_thread_.join();
    }

    decoder_.reset();
    mirror_window_.reset();

    {
        std::lock_guard lock(frame_mutex_);
        latest_bgra_.clear();
        latest_bgra_.shrink_to_fit();
        has_new_frame_ = false;
    }

    if (frame_queue_) frame_queue_->clear();
}

void App::on_render_timer() {
    // LIGHTWEIGHT: runs on main thread, must complete in <5ms.
    if (!mirror_window_ || !mirror_window_->renderer()) return;

    static uint64_t timer_ticks = 0;
    static uint64_t frames_rendered = 0;
    ++timer_ticks;

    // Grab the latest decoded BGRA pixels from the decode thread.
    // We std::move the vector to avoid holding the mutex during rendering.
    std::vector<uint8_t> bgra;
    int width = 0, height = 0;
    bool has_frame = false;

    {
        std::lock_guard lock(frame_mutex_);
        if (has_new_frame_) {
            bgra = std::move(latest_bgra_);  // O(1) move
            width = latest_width_;
            height = latest_height_;
            has_frame = true;
            has_new_frame_ = false;
        }
    }

    if (has_frame && !bgra.empty()) {
        ++frames_rendered;
        const int bgra_stride = width * 4;
        mirror_window_->renderer()->render_video_frame(
            bgra.data(), bgra_stride, width, height);

        // Log periodically
        if (frames_rendered % 30 == 0) {
            Logger::debug("Render: displayed frame #{} (timer ticks={})",
                          frames_rendered, timer_ticks);
        }
        return;
    }
}

void App::decode_loop(std::stop_token stop_token) {
    Logger::info("Decode thread started");

    uint64_t frames_fed = 0;
    uint64_t frames_decoded = 0;
    uint64_t empty_polls = 0;
    auto last_activity = std::chrono::steady_clock::now();

    while (!stop_token.stop_requested()) {
        if (!frame_queue_ || !decoder_ || !decoder_->is_initialized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        OwnedVideoFrame frame;
        if (!frame_queue_->try_pop(frame)) {
            ++empty_polls;
            // Log if we haven't received frames for a while
            if (empty_polls == 500) {  // ~1 second at 2ms sleep
                const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - last_activity).count();
                Logger::warn("Decode thread: no frames for ~1s (fed={}, decoded={}, "
                             "elapsed={}s, queue_size={})",
                             frames_fed, frames_decoded, elapsed, frame_queue_->size());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        empty_polls = 0;
        last_activity = std::chrono::steady_clock::now();
        ++frames_fed;

        // Log every 30th frame fed (roughly 1/sec at 30fps)
        if (frames_fed <= 5 || frames_fed % 30 == 0) {
            Logger::debug("Decode: feeding frame #{}, size={}, ts={}",
                          frames_fed, frame.data.size(), frame.timestamp);
        }

        DecodedFrame decoded_frame;
        const bool decoded = decoder_->decode(
            frame.data.data(),
            frame.data.size(),
            frame.timestamp,
            decoded_frame);

        if (decoded && !decoded_frame.bgra.empty()) {
            ++frames_decoded;

            if (frames_decoded == 1) {
                Logger::info("First decoded frame: {}x{} (after {} input frames)",
                             decoded_frame.width, decoded_frame.height, frames_fed);
            }

            if (frames_decoded % 30 == 0) {
                Logger::debug("Decode: produced frame #{} ({}x{})",
                              frames_decoded, decoded_frame.width, decoded_frame.height);
            }

            // Move raw BGRA bytes to shared state for main thread
            {
                std::lock_guard lock(frame_mutex_);
                latest_bgra_ = std::move(decoded_frame.bgra);  // O(1) move
                latest_width_ = decoded_frame.width;
                latest_height_ = decoded_frame.height;
                has_new_frame_ = true;
            }
        }

        // Periodic stats
        if (frames_fed % 300 == 0) {
            Logger::info("Decode stats: fed={}, decoded={}, queue={}",
                         frames_fed, frames_decoded, frame_queue_->size());
        }
    }

    Logger::info("Decode thread stopped (fed={}, decoded={})", frames_fed, frames_decoded);
}

// ---------------------------------------------------------------------------
// AirPlay Service
// ---------------------------------------------------------------------------

std::array<uint8_t, 6> App::get_machine_mac_address() {
    // Use GetAdaptersAddresses to find the first active Ethernet/Wi-Fi adapter's MAC
    ULONG buf_size = 15000;
    std::vector<uint8_t> buffer(buf_size);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST
                | GAA_FLAG_SKIP_DNS_SERVER;

    ULONG result = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &buf_size);
    if (result == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(buf_size);
        adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        result = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &buf_size);
    }

    if (result == NO_ERROR) {
        for (auto* adapter = adapters; adapter; adapter = adapter->Next) {
            if (adapter->OperStatus != IfOperStatusUp) continue;
            if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            if (adapter->PhysicalAddressLength != 6) continue;

            std::array<uint8_t, 6> mac{};
            for (int i = 0; i < 6; ++i) {
                mac[i] = adapter->PhysicalAddress[i];
            }

            Logger::debug("Using MAC address: {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
                          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            return mac;
        }
    }

    Logger::warn("Could not determine MAC address, using default");
    return {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
}

bool App::start_airplay_service() {
    Logger::info("Starting AirPlay service...");

    // Create production dependencies
    auto core = std::make_unique<RPiPlayCore>();
    auto mdns = std::make_unique<NativeMdnsAdvertiser>();

    airplay_service_ = std::make_unique<AirPlayService>(
        std::move(core), std::move(mdns));

    // Set up callbacks for connection events
    // NOTE: These fire on internal RAOP threads — use PostMessage to marshal
    // to the main thread for UI operations.

    airplay_service_->set_client_connected_callback(
        [this](const AirPlayClientInfo& client) {
            Logger::info("iPad connected: {} ({})",
                         client.device_name, client.device_id);

            // Store connection info (thread-safe)
            {
                std::lock_guard lock(connection_mutex_);
                pending_device_name_ = client.device_name;
            }

            // Marshal to main thread
            PostMessage(message_hwnd_, constants::kWmIpadConnected, 0, 0);
        });

    airplay_service_->set_client_disconnected_callback(
        [this](const std::string& device_id) {
            Logger::info("iPad disconnected: {}", device_id);

            // Marshal to main thread
            PostMessage(message_hwnd_, constants::kWmIpadDisconnected, 0, 0);
        });

    airplay_service_->set_video_frame_callback(
        [this](const uint8_t* data, size_t size, uint64_t timestamp) {
            // RAOP thread — copy data into queue for decode thread.
            static uint64_t cb_count = 0;
            ++cb_count;

            if (cb_count == 1) {
                Logger::info("App: first video frame callback — size={}, ts={}", size, timestamp);
            }
            if (cb_count % 300 == 0) {
                const auto queue_sz = frame_queue_ ? frame_queue_->size() : 0;
                Logger::info("App: video callback #{}, queue_size={}", cb_count, queue_sz);
            }

            if (frame_queue_) {
                frame_queue_->push(data, size, timestamp);
            }
        });

    airplay_service_->set_audio_frame_callback(
        [](const uint8_t* /*data*/, size_t /*size*/, uint64_t /*timestamp*/) {
            // TODO (Milestone 4): Decode AAC-ELD → WASAPI playback
        });

    // Configure the AirPlay service
    const AirPlayServiceConfig config{
        .server_name = "Reflection",
        .hardware_address = get_machine_mac_address(),
        .raop_port = constants::kRaopPort,
        .airplay_port = constants::kAirPlayPort,
    };

    if (!airplay_service_->start(config)) {
        Logger::error("AirPlayService::start() failed");
        return false;
    }

    Logger::info("AirPlay service running — iPad should see 'Reflection' in Screen Mirroring");
    return true;
}

} // namespace reflection
