// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <objbase.h>

#include <cstdlib>
#include <string>

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
/// Configure GStreamer plugin and DLL paths BEFORE gst_init().
///
/// Two-tier detection:
///   Tier 1 (Release): Bundled plugins/ directory next to the executable.
///     Core DLLs are co-located, plugins are in a plugins/ subdirectory.
///     GST_PLUGIN_SYSTEM_PATH is cleared to prevent loading incompatible
///     system plugins.
///   Tier 2 (Dev): MSYS2 ucrt64 installation. Prepends the MSYS2 bin/
///     to PATH and sets GST_PLUGIN_PATH to the MSYS2 plugin directory.
void configure_gstreamer_environment() {
    // Get the directory containing Reflection.exe
    char exe_path[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
    std::string exe_dir(exe_path);
    const auto last_sep = exe_dir.find_last_of("\\/");
    if (last_sep != std::string::npos) {
        exe_dir = exe_dir.substr(0, last_sep);
    }

    // Tier 1: Check for bundled plugins/ directory next to the executable
    std::string bundled_plugins = exe_dir + "\\plugins";
    if (GetFileAttributesA(bundled_plugins.c_str()) != INVALID_FILE_ATTRIBUTES) {
        _putenv_s("GST_PLUGIN_PATH", bundled_plugins.c_str());
        // Prevent GStreamer from scanning system-wide plugin directories
        // which may contain incompatible versions
        _putenv_s("GST_PLUGIN_SYSTEM_PATH", "");
        reflection::Logger::info("GStreamer plugins: bundled ({})", bundled_plugins);

        // In dev builds, plugin DLLs have transitive dependencies (e.g.,
        // libgstcodecparsers, libgstpbutils) that may not be co-located.
        // Prepend MSYS2 bin/ to PATH so Windows can resolve them.
        // In production (CI with GStreamer MSVC), all DLLs are co-located
        // and this is a harmless no-op if MSYS2 isn't installed.
        // Fall through to Tier 2 PATH setup (don't return here).
    } else {
        // No bundled plugins -- set plugin path in Tier 2 below.
    }

    // Tier 2: Find MSYS2 installation for DLL resolution + plugin path (if not bundled)
    std::string msys2_prefix;

    const char* candidates[] = {
        "C:\\msys64\\ucrt64",
        "C:\\msys2\\ucrt64",
        "D:\\msys64\\ucrt64",
    };

    for (const char* candidate : candidates) {
        std::string test = std::string(candidate) + "\\bin\\libgstreamer-1.0-0.dll";
        if (GetFileAttributesA(test.c_str()) != INVALID_FILE_ATTRIBUTES) {
            msys2_prefix = candidate;
            break;
        }
    }

    // Check MSYS2_PREFIX environment variable
    if (msys2_prefix.empty()) {
        const char* env_prefix = std::getenv("MSYS2_PREFIX");
        if (env_prefix && env_prefix[0]) {
            msys2_prefix = env_prefix;
        }
    }

    if (msys2_prefix.empty()) {
        msys2_prefix = "C:\\msys64\\ucrt64";  // Last resort fallback
    }

    // Set GST_PLUGIN_PATH only if Tier 1 didn't already set it
    const char* existing_plugin_path = std::getenv("GST_PLUGIN_PATH");
    if (!existing_plugin_path || !existing_plugin_path[0]) {
        std::string plugin_path = msys2_prefix + "\\lib\\gstreamer-1.0";
        _putenv_s("GST_PLUGIN_PATH", plugin_path.c_str());
        reflection::Logger::info("GStreamer plugins: MSYS2 ({})", plugin_path);
    }

    // ALWAYS prepend MSYS2 bin/ to PATH so transitive DLL dependencies
    // (libgstcodecparsers, libgstpbutils, etc.) are resolved from MSYS2
    // before Git for Windows or other MinGW installs
    std::string msys2_bin = msys2_prefix + "\\bin";
    const char* current_path = std::getenv("PATH");
    std::string new_path = msys2_bin;
    if (current_path && current_path[0]) {
        new_path += ";";
        new_path += current_path;
    }
    _putenv_s("PATH", new_path.c_str());

    reflection::Logger::info("GStreamer DLL path: MSYS2 ({})", msys2_bin);
}

/// RAII wrapper for GStreamer initialization.
struct GStreamerGuard {
    bool initialized = false;

    GStreamerGuard() {
        configure_gstreamer_environment();
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

/// SEH crash handler — writes crash address to log before terminating.
LONG WINAPI crash_filter(EXCEPTION_POINTERS* ep) {
    if (ep && ep->ExceptionRecord) {
        char buf[256];
        snprintf(buf, sizeof(buf),
            "CRASH: code=0x%08lX addr=%p",
            ep->ExceptionRecord->ExceptionCode,
            ep->ExceptionRecord->ExceptionAddress);
        reflection::Logger::error("{}", buf);
        fflush(nullptr);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

/// Parse command-line for development flags.
/// Supported: --reset-onboarding  (clears the OnboardingCompleted registry value)
void handle_dev_flags(LPWSTR cmd_line) {
    if (!cmd_line || !cmd_line[0]) return;

    std::wstring args(cmd_line);

    if (args.find(L"--reset-onboarding") != std::wstring::npos) {
        HKEY key;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Reflection",
                         0, KEY_WRITE, &key) == ERROR_SUCCESS) {
            RegDeleteValue(key, L"OnboardingCompleted");
            RegCloseKey(key);
            reflection::Logger::info("--reset-onboarding: cleared OnboardingCompleted");
        }
    }
}

int WINAPI wWinMain(
    _In_ HINSTANCE instance,
    _In_opt_ HINSTANCE /*prev_instance*/,
    _In_ LPWSTR cmd_line,
    _In_ int cmd_show
) {
    SetUnhandledExceptionFilter(crash_filter);
    reflection::Logger::init();
    reflection::Logger::info("Reflection for Windows starting...");

    // Process development flags before app init
    handle_dev_flags(cmd_line);

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
