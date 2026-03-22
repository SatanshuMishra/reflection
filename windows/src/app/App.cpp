#include "app/App.h"
#include "app/SystemTray.h"
#include "airplay/AirPlayService.h"
#include "airplay/AirPlayTypes.h"
#ifdef USE_UXPLAY
#include "airplay/UxPlayCore.h"
#include "pipeline/GStreamerPipeline.h"
#else
#include "airplay/RPiPlayCore.h"
#endif
#include "mdns/NativeMdnsAdvertiser.h"
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

namespace reflection {

App::App(HINSTANCE instance)
    : instance_(instance)
{
}

App::~App() {
    cleanup_mirror_session();

    if (airplay_service_) {
        airplay_service_->stop();
    }

    if (message_hwnd_) {
        DestroyWindow(message_hwnd_);
        message_hwnd_ = nullptr;
    }
}

bool App::init(int /*cmd_show*/) {
    Logger::info("Initializing application...");

    if (!create_message_window()) {
        Logger::error("Failed to create message window");
        return false;
    }

    system_tray_ = std::make_unique<SystemTray>(instance_);
    if (!system_tray_->install(message_hwnd_)) {
        Logger::error("Failed to install system tray icon");
        return false;
    }

    system_tray_->set_menu_callback(
        [this](int id) { on_tray_menu(id); });

    if (!start_airplay_service()) {
        Logger::error("Failed to start AirPlay service — iPad won't see this PC");
        system_tray_->set_tooltip(L"Reflection \u2014 AirPlay failed to start");
    }

    Logger::info("Application initialized \u2014 waiting for AirPlay connections...");
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
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = message_wnd_proc;
    wc.hInstance = instance_;
    wc.lpszClassName = constants::kAppWindowClass.data();

    RegisterClassEx(&wc);

    message_hwnd_ = CreateWindowEx(
        0,
        constants::kAppWindowClass.data(),
        constants::kAppName.data(),
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        nullptr,
        instance_,
        this
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
                system_tray_->set_tooltip(L"Reflection \u2014 Waiting for iPad...");
            }
            break;

        case SystemTray::kMenuSettings:
            Logger::info("Settings requested from tray menu");
            break;

        case SystemTray::kMenuAbout:
            Logger::info("About requested from tray menu");
            break;

        case SystemTray::kMenuCheckUpdate:
            Logger::info("Check for updates requested from tray menu");
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
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
            SetWindowLongPtr(hwnd, GWLP_USERDATA,
                             reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

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

    std::string device_name;
    {
        std::lock_guard lock(connection_mutex_);
        device_name = pending_device_name_.value_or("iPad");
        pending_device_name_.reset();
    }

    if (mirror_window_ && mirror_window_->hwnd()) {
        Logger::info("Mirror window already exists \u2014 bringing to front");
        SetForegroundWindow(mirror_window_->hwnd());
        return;
    }

    // Create the mirror window (lightweight — no D3D11 renderer, GStreamer renders into it)
    mirror_window_ = std::make_unique<MirrorWindow>();
    std::wstring title = L"Reflection \u2014 " +
        std::wstring(device_name.begin(), device_name.end());

    if (!mirror_window_->create(instance_, title, message_hwnd_)) {
        Logger::error("Failed to create mirror window");
        mirror_window_.reset();
        return;
    }

#ifdef USE_UXPLAY
    // Initialize GStreamer pipeline — handles ALL decode + render on GPU
    pipeline_ = std::make_unique<GStreamerPipeline>();
    if (!pipeline_->init(mirror_window_->hwnd())) {
        Logger::error("Failed to initialize GStreamer pipeline");
        pipeline_.reset();
        mirror_window_.reset();
        return;
    }

    // Start the pipeline (transitions to PLAYING state)
    pipeline_->start();
    Logger::info("GStreamer pipeline started \u2014 rendering into mirror window");
#endif

    if (system_tray_) {
        std::wstring tooltip = L"Reflection \u2014 Connected: " +
            std::wstring(device_name.begin(), device_name.end());
        system_tray_->set_tooltip(tooltip);
    }

    Logger::info("Mirror session started for: {}", device_name);
}

void App::on_ipad_disconnected() {
    Logger::info("iPad disconnected \u2014 cleaning up mirror session");
    cleanup_mirror_session();

    if (system_tray_) {
        system_tray_->set_tooltip(L"Reflection \u2014 Waiting for iPad...");
    }
}

void App::on_mirror_window_closed() {
    Logger::info("Mirror window closed by user");

    // Stop GStreamer pipeline first
    if (pipeline_) {
        pipeline_->stop();
        pipeline_.reset();
    }

    mirror_window_.reset();

    if (system_tray_) {
        system_tray_->set_tooltip(L"Reflection \u2014 Waiting for iPad...");
    }
}

void App::cleanup_mirror_session() {
    if (pipeline_) {
        pipeline_->stop();
        pipeline_.reset();
    }

    mirror_window_.reset();
}

// ---------------------------------------------------------------------------
// AirPlay Service
// ---------------------------------------------------------------------------

std::array<uint8_t, 6> App::get_machine_mac_address() {
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

#ifdef USE_UXPLAY
    auto core = std::make_unique<UxPlayCore>();
#else
    auto core = std::make_unique<RPiPlayCore>();
#endif
    auto mdns = std::make_unique<NativeMdnsAdvertiser>();

    airplay_service_ = std::make_unique<AirPlayService>(
        std::move(core), std::move(mdns));

    airplay_service_->set_client_connected_callback(
        [this](const AirPlayClientInfo& client) {
            Logger::info("iPad connected: {} ({})",
                         client.device_name, client.device_id);

            {
                std::lock_guard lock(connection_mutex_);
                pending_device_name_ = client.device_name;
            }

            PostMessage(message_hwnd_, constants::kWmIpadConnected, 0, 0);
        });

    airplay_service_->set_client_disconnected_callback(
        [this](const std::string& device_id) {
            Logger::info("iPad disconnected: {}", device_id);
            PostMessage(message_hwnd_, constants::kWmIpadDisconnected, 0, 0);
        });

    airplay_service_->set_video_frame_callback(
        [this](const uint8_t* data, size_t size, uint64_t timestamp, uint8_t /*frame_type*/) {
            // Push H.264 NAL units directly to GStreamer — thread-safe via GstAppSrc.
            // No queue, no decode thread, no CPU-side BGRA conversion needed.
            if (pipeline_) {
                pipeline_->push_video_data(data, size, timestamp);
            }
        });

    airplay_service_->set_audio_frame_callback(
        [this](const uint8_t* data, size_t size, uint64_t timestamp) {
            // Push audio directly to GStreamer audio pipeline
            if (pipeline_) {
                pipeline_->push_audio_data(data, size, timestamp);
            }
        });

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

    Logger::info("AirPlay service running \u2014 iPad should see 'Reflection' in Screen Mirroring");
    return true;
}

} // namespace reflection
