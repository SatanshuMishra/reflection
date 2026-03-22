#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <objbase.h>

#ifdef USE_UXPLAY
#include <gst/gst.h>
#else
#include <mfapi.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#endif

#include "app/App.h"
#include "utilities/Logger.h"

#pragma comment(lib, "ws2_32.lib")

namespace {

/// RAII wrapper for Winsock initialization.
struct WinsockGuard {
    bool initialized = false;

    WinsockGuard() {
        WSADATA wsa_data{};
        const int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        initialized = (result == 0);
    }

    ~WinsockGuard() {
        if (initialized) {
            WSACleanup();
        }
    }

    WinsockGuard(const WinsockGuard&) = delete;
    WinsockGuard& operator=(const WinsockGuard&) = delete;
};

/// RAII wrapper for COM initialization.
struct ComGuard {
    bool initialized = false;

    ComGuard() {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        initialized = SUCCEEDED(hr);
    }

    ~ComGuard() {
        if (initialized) {
            CoUninitialize();
        }
    }

    ComGuard(const ComGuard&) = delete;
    ComGuard& operator=(const ComGuard&) = delete;
};

#ifdef USE_UXPLAY
/// RAII wrapper for GStreamer initialization.
struct GStreamerGuard {
    bool initialized = false;

    GStreamerGuard() {
        gst_init(nullptr, nullptr);
        initialized = true;  // gst_init always succeeds or aborts
    }

    ~GStreamerGuard() {
        gst_deinit();
    }

    GStreamerGuard(const GStreamerGuard&) = delete;
    GStreamerGuard& operator=(const GStreamerGuard&) = delete;
};
#else
/// RAII wrapper for Media Foundation initialization.
struct MFGuard {
    bool initialized = false;

    MFGuard() {
        const HRESULT hr = MFStartup(MF_VERSION);
        initialized = SUCCEEDED(hr);
    }

    ~MFGuard() {
        if (initialized) {
            MFShutdown();
        }
    }

    MFGuard(const MFGuard&) = delete;
    MFGuard& operator=(const MFGuard&) = delete;
};
#endif

} // namespace

int WINAPI wWinMain(
    _In_ HINSTANCE instance,
    _In_opt_ HINSTANCE /*prev_instance*/,
    _In_ LPWSTR /*cmd_line*/,
    _In_ int cmd_show
) {
    reflection::Logger::init();
    reflection::Logger::info("Reflection for Windows starting...");

    // Initialize subsystems (RAII — cleaned up in reverse order on exit)
    const WinsockGuard winsock;
    if (!winsock.initialized) {
        reflection::Logger::error("Failed to initialize Winsock");
        return 1;
    }

    const ComGuard com;
    if (!com.initialized) {
        reflection::Logger::error("Failed to initialize COM");
        return 1;
    }

#ifdef USE_UXPLAY
    const GStreamerGuard gst;
    if (!gst.initialized) {
        reflection::Logger::error("Failed to initialize GStreamer");
        return 1;
    }
    reflection::Logger::info("GStreamer {} initialized", gst_version_string());
#else
    const MFGuard mf;
    if (!mf.initialized) {
        reflection::Logger::error("Failed to initialize Media Foundation");
        return 1;
    }
#endif

    reflection::Logger::info("All subsystems initialized");

    // Create and run the application
    reflection::App app(instance);

    if (!app.init(cmd_show)) {
        reflection::Logger::error("Failed to initialize application");
        return 1;
    }

    const int exit_code = app.run();

    reflection::Logger::info("Reflection exiting with code {}", exit_code);
    reflection::Logger::shutdown();
    return exit_code;
}
