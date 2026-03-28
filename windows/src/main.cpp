// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <objbase.h>
#include <objidl.h>

#include <gdiplus.h>

#include <cstdlib>
#include <filesystem>
#include <shellapi.h>
#include <shlobj.h>
#include <string>

#ifdef USE_UXPLAY
#include <gst/gst.h>
#else
#include <mfapi.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#endif

#include <winsparkle/winsparkle.h>

#include "app/App.h"
#include "settings/AppSettings.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"
#include "utilities/WinUtils.h"

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

/// RAII wrapper for GDI+ initialization.
/// Required by MirrorOverlay for rendering the hover toolbar.
struct GdiplusGuard {
    ULONG_PTR token = 0;
    bool initialized = false;

    GdiplusGuard() {
        Gdiplus::GdiplusStartupInput input{};
        initialized =
            (Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok);
    }

    ~GdiplusGuard() {
        if (initialized) {
            Gdiplus::GdiplusShutdown(token);
        }
    }

    GdiplusGuard(const GdiplusGuard&) = delete;
    GdiplusGuard& operator=(const GdiplusGuard&) = delete;
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

    // Check MSYS2_PREFIX environment variable (dev builds only).
    // In production, this is not used — bundled DLLs are co-located.
    if (msys2_prefix.empty()) {
        const char* env_prefix = std::getenv("MSYS2_PREFIX");
        if (env_prefix && env_prefix[0]) {
            std::string candidate = env_prefix;
            // Validate: only accept if the directory actually contains GStreamer
            std::string test = candidate + "\\bin\\libgstreamer-1.0-0.dll";
            if (GetFileAttributesA(test.c_str()) != INVALID_FILE_ATTRIBUTES) {
                msys2_prefix = candidate;
            } else {
                reflection::Logger::warn("MSYS2_PREFIX '{}' does not contain GStreamer — ignoring", candidate);
            }
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

/// RAII wrapper for WinSparkle auto-update initialization.
/// Configures appcast URL, Ed25519 public key, and check interval
/// from constants. Reads auto-update preference from AppSettings.
struct WinSparkleGuard {
    bool initialized = false;

    explicit WinSparkleGuard(const reflection::AppSettings& settings) {
        // All configuration must happen BEFORE win_sparkle_init()

        // App identity (overrides VERSIONINFO resource)
        const auto version_w = reflection::win_utils::utf8_to_wide(REFLECTION_VERSION);
        win_sparkle_set_app_details(L"Satanshu Mishra", L"Reflection",
                                    version_w.c_str());

        // Appcast URL (HTTPS, hosted on GitHub Pages)
        win_sparkle_set_appcast_url(
            std::string(reflection::constants::kAppcastUrl).c_str());

        // Ed25519 public key for signature verification.
        // win_sparkle_set_eddsa_public_key returns 1 on success, 0 on failure.
        if (win_sparkle_set_eddsa_public_key(
                std::string(reflection::constants::kEdDsaPublicKey).c_str()) == 0) {
            reflection::Logger::error("WinSparkle: failed to set Ed25519 public key");
            return;
        }

        // Store WinSparkle preferences under our instance-specific registry path
        // (e.g., Software\Reflection\WinSparkle or Software\Reflection-Dev\WinSparkle)
        const std::string reg_path =
            reflection::win_utils::wide_to_utf8(
                std::wstring(reflection::constants::kRegistryRoot))
            + "\\WinSparkle";
        win_sparkle_set_registry_path(reg_path.c_str());

        // Auto-check preference from user settings (default: on)
        win_sparkle_set_automatic_check_for_updates(
            settings.auto_update_enabled() ? 1 : 0);
        win_sparkle_set_update_check_interval(
            reflection::constants::kUpdateCheckIntervalSec);

        // Shutdown callback — WinSparkle calls this when an update needs to install
        win_sparkle_set_shutdown_request_callback([]() {
            PostQuitMessage(0);
        });
        win_sparkle_set_can_shutdown_callback([]() -> int {
            return 1;  // Always allow shutdown for updates
        });

        win_sparkle_init();
        initialized = true;
        reflection::Logger::info("WinSparkle initialized (auto-check: {})",
                                  settings.auto_update_enabled() ? "on" : "off");
    }

    ~WinSparkleGuard() {
        if (initialized) {
            win_sparkle_cleanup();
        }
    }

    WinSparkleGuard(const WinSparkleGuard&) = delete;
    WinSparkleGuard& operator=(const WinSparkleGuard&) = delete;
};

/// SEH crash handler — writes crash address to debugger output.
/// Uses OutputDebugStringA directly to avoid deadlock risk if the crash
/// occurred while Logger's g_log_mutex was held.
LONG WINAPI crash_filter(EXCEPTION_POINTERS* ep) {
    if (ep && ep->ExceptionRecord) {
        char buf[256]{};
        snprintf(buf, sizeof(buf),
            "CRASH: code=0x%08lX addr=%p\n",
            ep->ExceptionRecord->ExceptionCode,
            ep->ExceptionRecord->ExceptionAddress);
        OutputDebugStringA(buf);
        fflush(nullptr);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

/// Delete ALL WebView2 user data under %LOCALAPPDATA%\Reflection(-Dev).
///
/// WebView2 creates multiple subdirectories (WebView2/, EBWebView/) in our
/// app data folder. Deleting only WebView2/ leaves ~29 MB of EBWebView cache
/// behind. The correct approach is to delete the entire parent directory.
///
/// SAFEGUARDS:
///   S1: Path must contain "\Reflection" (or "\Reflection-Dev")
///   S2: Path must start with the system's LocalAppData directory
///   S3: Only the instance-specific subdirectory is deleted, never LocalAppData itself
void delete_webview2_data() {
    wchar_t* app_data_raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &app_data_raw))) {
        reflection::Logger::warn("--reset-all: could not resolve LocalAppData");
        return;
    }
    std::wstring app_data(app_data_raw);
    CoTaskMemFree(app_data_raw);

    // Delete the entire app data directory (contains WebView2/, EBWebView/, etc.)
    std::wstring app_data_dir = app_data + L"\\" +
        std::wstring(reflection::constants::kWebViewSubdir);

    // SAFEGUARD S1: Path must contain our app name
    if (app_data_dir.find(L"\\Reflection") == std::wstring::npos) {
        reflection::Logger::error("SAFETY: app data path missing \\Reflection -- aborting delete");
        return;
    }

    // SAFEGUARD S2: Path must start with LocalAppData
    if (app_data_dir.find(app_data) != 0) {
        reflection::Logger::error("SAFETY: app data path not under LocalAppData -- aborting delete");
        return;
    }

    // SAFEGUARD S3: Path must be longer than LocalAppData itself
    if (app_data_dir.length() <= app_data.length() + 1) {
        reflection::Logger::error("SAFETY: app data path too short -- aborting delete");
        return;
    }

    std::error_code ec;
    auto removed = std::filesystem::remove_all(app_data_dir, ec);
    if (ec) {
        reflection::Logger::warn("--reset-all: app data delete failed: {}", ec.message());
    } else {
        reflection::Logger::info("--reset-all: deleted app data directory ({} items)", removed);
    }
}

/// Delete the log file next to the executable.
void delete_log_file() {
    wchar_t exe_path[MAX_PATH]{};
    DWORD len = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return;

    std::wstring log_path(exe_path);
    auto dot = log_path.rfind(L'.');
    if (dot != std::wstring::npos) {
        log_path = log_path.substr(0, dot);
    }
    log_path += L".log";

    if (DeleteFileW(log_path.c_str())) {
        reflection::Logger::info("--reset-all: deleted log file");
    }
}

/// Clear all Reflection user data (registry + files).
/// SAFEGUARD S1: Only deletes from hardcoded paths in Constants.h.
/// SAFEGUARD S3: For shared registry keys (Run), deletes VALUE only, never the KEY.
void reset_all_data() {
    reflection::Logger::info("--reset-all: clearing all Reflection data");

    // 1. Delete our instance-specific registry key + all values
    reflection::Logger::info("--reset-all: deleting registry key HKCU\\{}",
        reflection::win_utils::wide_to_utf8(
            std::wstring(reflection::constants::kRegistryRoot)));
    LSTATUS result = RegDeleteTreeW(HKEY_CURRENT_USER,
        reflection::constants::kRegistryRoot.data());
    if (result == ERROR_SUCCESS) {
        reflection::Logger::info("--reset-all: registry key deleted");
    } else if (result == ERROR_FILE_NOT_FOUND) {
        reflection::Logger::info("--reset-all: registry key not found (already clean)");
    } else {
        reflection::Logger::warn("--reset-all: RegDeleteTree failed: {}", result);
    }

    // 2. Delete auto-start VALUE from Run key (NEVER delete the Run key itself)
    // SAFEGUARD S3: RegDeleteValue on the specific value, not RegDeleteKey on the parent
    HKEY run_key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_WRITE, &run_key) == ERROR_SUCCESS) {
        reflection::Logger::info("--reset-all: removing auto-start entry from Run key");
        RegDeleteValueW(run_key, reflection::constants::kRunRegistryValueName.data());
        RegCloseKey(run_key);
    }

    // 3. Delete WebView2 user data (with path validation)
    delete_webview2_data();

    // 4. Delete log file — shutdown the logger FIRST so the file is not locked.
    // Logger::init() opens the log with _SH_DENYNO (shared read/write) but
    // DeleteFileW requires no open handles without FILE_SHARE_DELETE.
    reflection::Logger::info("--reset-all: cleanup complete -- shutting down logger");
    reflection::Logger::shutdown();
    delete_log_file();
}

/// Delete the Windows Firewall rule for Reflection (requires UAC elevation).
void reset_firewall() {
    reflection::Logger::info("--reset-firewall: removing firewall rule '{}'",
        reflection::win_utils::wide_to_utf8(
            std::wstring(reflection::constants::kAppInstanceName)));

    // Build netsh args with instance-qualified rule name
    std::wstring netsh_args =
        L"advfirewall firewall delete rule name=\"" +
        std::wstring(reflection::constants::kAppInstanceName) + L"\"";

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = L"netsh.exe";
    sei.lpParameters = netsh_args.c_str();
    sei.nShow = SW_HIDE;

    if (ShellExecuteExW(&sei)) {
        if (sei.hProcess) {
            WaitForSingleObject(sei.hProcess, 15000);
            DWORD exit_code = 1;
            GetExitCodeProcess(sei.hProcess, &exit_code);
            CloseHandle(sei.hProcess);
            reflection::Logger::info("--reset-firewall: netsh exited with code {}", exit_code);
        }
    } else {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED) {
            reflection::Logger::info("--reset-firewall: user cancelled UAC prompt");
        } else {
            reflection::Logger::error("--reset-firewall: ShellExecuteEx failed: {}", err);
        }
    }
}

/// Parse command-line for development and installer flags.
///
/// Supported flags:
///   --reset-all           Clear all Reflection data (registry, files, WebView2)
///   --reset-onboarding    Clear onboarding flag only (legacy, subset of --reset-all)
///   --reset-firewall      Delete the Windows Firewall rule (requires UAC)
///   --no-reset            Skip the automatic debug-build reset
///   --uninstall-cleanup   Full cleanup + exit (called by installer during uninstall)
///
/// Debug build default: --reset-all is applied automatically unless --no-reset is passed.
/// Release builds never auto-reset.
///
/// Returns true if the app should exit immediately (--uninstall-cleanup).
bool handle_dev_flags(LPWSTR cmd_line) {
    std::wstring args = cmd_line ? cmd_line : L"";

    // --uninstall-cleanup: clear user data, then exit without launching UI.
    // Called by the installer's [UninstallRun] section. Handles registry,
    // WebView2 cache, and log files. Does NOT reset the firewall rule —
    // the installer handles that directly via netsh.exe with admin privileges,
    // avoiding a redundant UAC prompt from ShellExecuteEx("runas").
    if (args.find(L"--uninstall-cleanup") != std::wstring::npos) {
        reflection::Logger::info("--uninstall-cleanup: running cleanup");
        reset_all_data();  // Shuts down logger internally before deleting log
        return true;  // Signal caller to exit
    }

    const bool has_reset_all = args.find(L"--reset-all") != std::wstring::npos;
    const bool has_no_reset = args.find(L"--no-reset") != std::wstring::npos;
    const bool has_reset_firewall = args.find(L"--reset-firewall") != std::wstring::npos;
    const bool has_reset_onboarding = args.find(L"--reset-onboarding") != std::wstring::npos;

    bool do_reset_all = has_reset_all;

#ifdef _DEBUG
    // Debug builds: auto-reset unless --no-reset is passed
    if (!has_no_reset && !do_reset_all) {
        do_reset_all = true;
        reflection::Logger::info("Debug build: auto-resetting (use --no-reset to skip)");
    }
    if (has_no_reset) {
        reflection::Logger::info("Debug build: --no-reset specified, skipping auto-reset");
    }
#endif

    if (do_reset_all) {
        reset_all_data();  // Shuts down logger + deletes log file
        reflection::Logger::init();  // Re-init logger for continued app startup
    } else if (has_reset_onboarding) {
        // Legacy: just clear the onboarding flag
        HKEY key;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, reflection::constants::kRegistryRoot.data(),
                          0, KEY_WRITE, &key) == ERROR_SUCCESS) {
            RegDeleteValueW(key, L"OnboardingCompleted");
            RegCloseKey(key);
            reflection::Logger::info("--reset-onboarding: cleared OnboardingCompleted");
        }
    }

    if (has_reset_firewall) {
        reset_firewall();
    }

    return false;  // Continue normal startup
}

int WINAPI wWinMain(
    _In_ HINSTANCE instance,
    _In_opt_ HINSTANCE /*prev_instance*/,
    _In_ LPWSTR cmd_line,
    _In_ int cmd_show
) {
    // Per-monitor DPI awareness V2 — must be set before any window creation.
    // Prevents Windows from bitmap-scaling the app on high-DPI displays,
    // ensuring crisp text, icons, and video rendering.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    SetUnhandledExceptionFilter(crash_filter);
    reflection::Logger::init();
    reflection::Logger::info("Reflection for Windows starting...");

    // Process development/installer flags before app init.
    // --uninstall-cleanup returns true to signal immediate exit.
    if (handle_dev_flags(cmd_line)) {
        return 0;
    }

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

    const GdiplusGuard gdiplus;
    if (!gdiplus.initialized) {
        reflection::Logger::error("Failed to initialize GDI+");
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

    // Auto-update: initialize after app (settings must be available).
    // RAII cleanup happens before app destructor (reverse init order).
    const WinSparkleGuard winsparkle(app.settings());

    const int exit_code = app.run();

    reflection::Logger::info("Reflection exiting with code {}", exit_code);
    reflection::Logger::shutdown();
    return exit_code;
}
