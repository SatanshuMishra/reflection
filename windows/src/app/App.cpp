#include "app/App.h"
#include "airplay/AirPlayService.h"
#include "airplay/AirPlayTypes.h"
#include "airplay/RPiPlayCore.h"
#include "mdns/NativeMdnsAdvertiser.h"
#include "views/MirrorWindow.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
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
    // Stop AirPlay service before window destruction
    if (airplay_service_) {
        airplay_service_->stop();
    }
}

bool App::init(int cmd_show) {
    Logger::info("Initializing application...");

    // Start AirPlay service (mDNS + RAOP)
    if (!start_airplay_service()) {
        Logger::error("Failed to start AirPlay service — iPad won't see this PC");
        // Continue anyway — the user can see the window and debug
    }

    // Create the mirror window (hidden until an iPad connects)
    mirror_window_ = std::make_unique<MirrorWindow>();
    if (!mirror_window_->create(instance_, L"Reflection")) {
        Logger::warn("Failed to create mirror window");
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
    airplay_service_->set_client_connected_callback(
        [this](const AirPlayClientInfo& client) {
            Logger::info("iPad connected: {} ({})",
                         client.device_name, client.device_id);
            // TODO (Milestone 3): Show mirror window, start decoder
        });

    airplay_service_->set_client_disconnected_callback(
        [this](const std::string& device_id) {
            Logger::info("iPad disconnected: {}", device_id);
            // TODO (Milestone 5): Hide mirror window, stop decoder
        });

    airplay_service_->set_video_frame_callback(
        [](const uint8_t* /*data*/, size_t size, uint64_t timestamp) {
            Logger::debug("Video frame: {} bytes, ts={}", size, timestamp);
            // TODO (Milestone 3): Decode H.264 → D3D11 texture → render
        });

    airplay_service_->set_audio_frame_callback(
        [](const uint8_t* /*data*/, size_t size, uint64_t timestamp) {
            Logger::debug("Audio frame: {} bytes, ts={}", size, timestamp);
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
