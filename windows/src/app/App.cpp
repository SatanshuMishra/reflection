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
#include "settings/AppSettings.h"
#include "ui/ThemeManager.h"
#include "views/MirrorWindow.h"
#include "views/OnboardingWindow.h"
#include "views/StatusWindow.h"
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

    if (theme_manager_) {
        theme_manager_->stop();
    }

    status_window_.reset();

    if (message_hwnd_) {
        DestroyWindow(message_hwnd_);
        message_hwnd_ = nullptr;
    }
}

bool App::init(int /*cmd_show*/) {
    Logger::info("Initializing application...");

    // Create settings manager
    settings_ = std::make_unique<AppSettings>();

    if (!create_message_window()) {
        Logger::error("Failed to create message window");
        return false;
    }

    // --- Onboarding (first-run only) ---
    if (!OnboardingWindow::is_completed()) {
        Logger::info("First run detected -- showing onboarding wizard");
        OnboardingWindow onboarding;
        onboarding.show(instance_, *settings_);
        // After onboarding, the server name is set in AppSettings
    }

    // --- System tray ---
    system_tray_ = std::make_unique<SystemTray>(instance_);
    if (!system_tray_->install(message_hwnd_)) {
        Logger::error("Failed to install system tray icon");
        return false;
    }

    system_tray_->set_menu_callback(
        [this](int id) { on_tray_menu(id); });

    // Set the server name in the tray menu
    system_tray_->set_server_name(settings_->server_name());

    // --- Theme manager ---
    theme_manager_ = std::make_unique<ThemeManager>();
    theme_manager_->start(message_hwnd_);

    // --- Status window ---
    status_window_ = std::make_unique<StatusWindow>();
    status_window_->create(instance_, *settings_, message_hwnd_);

    // Apply initial theme to status window
    std::string theme = ThemeManager::get_effective_theme(settings_->theme());
    status_window_->set_theme(theme);

    // --- AirPlay service (lazy-start) ---
    // Only start if firewall permission has been granted. Broadcasting mDNS
    // without firewall access is wasteful and misleading -- iPad sees the
    // service but connections are silently blocked by Windows Firewall.
    if (settings_->firewall_configured()) {
        if (!start_airplay_service()) {
            Logger::error("Failed to start AirPlay service -- iPad won't see this PC");
            system_tray_->set_tooltip(L"Reflection -- AirPlay failed to start");
        }
    } else {
        Logger::info("Firewall permission not yet granted -- AirPlay service deferred");
        system_tray_->set_tooltip(L"Reflection -- Network permission required");
    }

    Logger::info("Application initialized -- waiting for AirPlay connections...");
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
                system_tray_->set_connection_state(false);
            }
            if (status_window_) {
                status_window_->set_disconnected();
            }
            break;

        case SystemTray::kMenuSettings:
            Logger::info("Settings requested from tray menu");
            if (status_window_) {
                status_window_->show();
            }
            break;

        case SystemTray::kMenuShowWindow:
            Logger::info("Show window requested from tray menu");
            if (status_window_) {
                status_window_->show();
            }
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

    if (msg == constants::kWmServerNameChanged) {
        auto* app = reinterpret_cast<App*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (app) {
            app->on_server_name_changed();
        }
        return 0;
    }

    if (msg == constants::kWmThemeChanged) {
        auto* app = reinterpret_cast<App*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (app && app->settings_ && app->status_window_) {
            std::string theme = ThemeManager::get_effective_theme(
                app->settings_->theme());
            app->status_window_->set_theme(theme);
            Logger::info("System theme changed -- updated to: {}", theme);
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

    if (msg == constants::kWmFirewallGranted) {
        auto* app = reinterpret_cast<App*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (app && !app->airplay_service_) {
            Logger::info("Firewall granted -- starting AirPlay service now");
            if (!app->start_airplay_service()) {
                Logger::error("Failed to start AirPlay service after firewall grant");
            } else if (app->system_tray_) {
                app->system_tray_->set_tooltip(L"Reflection -- Waiting for iPad...");
            }
        }
        return 0;
    }

    if (msg == constants::kWmForceReannounce) {
        auto* app = reinterpret_cast<App*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (app && app->airplay_service_) {
            app->airplay_service_->force_reannounce();
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
        Logger::info("Mirror window already exists -- bringing to front");
        SetForegroundWindow(mirror_window_->hwnd());
        return;
    }

    // Create the mirror window (lightweight — no D3D11 renderer, GStreamer renders into it)
    mirror_window_ = std::make_unique<MirrorWindow>();
    std::wstring title = L"Reflection -- " +
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
    Logger::info("GStreamer pipeline started -- rendering into mirror window");
#endif

    // Update tray and status window with connection state
    std::wstring wdevice(device_name.begin(), device_name.end());
    if (system_tray_) {
        system_tray_->set_connection_state(true, wdevice);
    }
    if (status_window_) {
        status_window_->set_connected(device_name);
    }

    Logger::info("Mirror session started for: {}", device_name);
}

void App::on_ipad_disconnected() {
    Logger::info("iPad disconnected -- cleaning up mirror session");
    cleanup_mirror_session();

    if (system_tray_) {
        system_tray_->set_connection_state(false);
    }
    if (status_window_) {
        status_window_->set_disconnected();
    }
}

void App::on_mirror_window_closed() {
    Logger::info("Mirror window closed by user -- forcing immediate disconnect");

    // 1. Stop rendering immediately
    if (pipeline_) {
        pipeline_->stop();
        pipeline_.reset();
    }
    mirror_window_.reset();

    // 2. Update UI to disconnected state IMMEDIATELY (don't wait for UxPlay timeout)
    if (system_tray_) {
        system_tray_->set_connection_state(false);
    }
    if (status_window_) {
        status_window_->set_disconnected();
    }

    // 3. Restart AirPlay service to force RTSP TEARDOWN on the iPad side.
    // Without this, UxPlay's internal keepalive waits ~60s before detecting
    // the dead session. Restarting closes the RAOP server socket, which
    // immediately triggers a TCP RST to the iPad, and re-advertises via mDNS.
    if (airplay_service_ && settings_) {
        const auto wname = settings_->server_name();
        const AirPlayServiceConfig config{
            .server_name = std::string(wname.begin(), wname.end()),
            .hardware_address = get_machine_mac_address(),
            .raop_port = constants::kRaopPort,
            .airplay_port = constants::kAirPlayPort,
        };
        if (airplay_service_->restart(config)) {
            Logger::info("AirPlay service restarted -- iPad will see immediate disconnect");
        } else {
            Logger::error("Failed to restart AirPlay service after mirror close");
        }
    }
}

void App::on_server_name_changed() {
    if (!settings_ || !airplay_service_) return;

    std::wstring wname = settings_->server_name();
    std::string name(wname.begin(), wname.end());
    Logger::info("Server name changed to '{}' -- restarting AirPlay service", name);

    // Update the system tray
    if (system_tray_) {
        system_tray_->set_server_name(wname);
    }

    // Restart the AirPlay service with the new name
    const AirPlayServiceConfig config{
        .server_name = name,
        .hardware_address = get_machine_mac_address(),
        .raop_port = constants::kRaopPort,
        .airplay_port = constants::kAirPlayPort,
    };

    if (!airplay_service_->restart(config)) {
        Logger::error("Failed to restart AirPlay service with new name");
        if (system_tray_) {
            system_tray_->set_tooltip(L"Reflection -- AirPlay restart failed");
        }
    } else {
        Logger::info("AirPlay service restarted as '{}'", name);
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

    // Convert wstring server name to narrow string for AirPlay protocol.
    // Must store the wstring in a local to avoid dangling iterators —
    // server_name() returns by value, so each call creates a new temporary.
    const auto wname = settings_ ? settings_->server_name() : L"Reflection";
    const AirPlayServiceConfig config{
        .server_name = std::string(wname.begin(), wname.end()),
        .hardware_address = get_machine_mac_address(),
        .raop_port = constants::kRaopPort,
        .airplay_port = constants::kAirPlayPort,
    };

    if (!airplay_service_->start(config)) {
        Logger::error("AirPlayService::start() failed");
        return false;
    }

    Logger::info("AirPlay service running -- iPad should see '{}' in Screen Mirroring",
                  config.server_name);
    return true;
}

} // namespace reflection
