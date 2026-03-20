#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <memory>
#include <string>

namespace reflection {

class AirPlayService;
class MirrorWindow;

/// Main application class. Manages the message loop, system tray,
/// and coordinates between AirPlay service and mirror windows.
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
    std::unique_ptr<AirPlayService> airplay_service_;
    std::unique_ptr<MirrorWindow> mirror_window_;

    /// Read the machine's actual MAC address for AirPlay identification.
    static std::array<uint8_t, 6> get_machine_mac_address();

    /// Start the AirPlay receiver service.
    bool start_airplay_service();
};

} // namespace reflection
