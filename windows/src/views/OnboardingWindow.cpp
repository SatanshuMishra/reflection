// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "views/OnboardingWindow.h"
#include "ui/WebViewHost.h"
#include "ui/ThemeManager.h"
#include "settings/AppSettings.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"
#include "utilities/NameGenerator.h"
#include "utilities/WinUtils.h"

#include <dwmapi.h>
#include <shellapi.h>

#pragma comment(lib, "dwmapi.lib")

// Conditional defines for older Windows SDKs (< 10.0.22000)
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

namespace reflection {

namespace {

/// Escape a string for safe embedding in a JSON string value.
/// Handles: backslash, double-quote, and control characters.
std::string json_escape(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // Skip non-printable control characters
                } else {
                    result += c;
                }
                break;
        }
    }
    return result;
}

/// Simple JSON builder with proper escaping for string values.
std::wstring make_json(const std::string& type,
                        const std::string& key = "",
                        const std::string& value = "") {
    std::string json = "{\"type\":\"" + json_escape(type) + "\"";
    if (!key.empty()) {
        json += ",\"" + json_escape(key) + "\":\"" + json_escape(value) + "\"";
    }
    json += "}";
    return win_utils::utf8_to_wide(json);
}

/// Parse a simple JSON string to find a string value for a key.
/// Handles escaped quotes within values.
std::string json_get_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
    // Find closing quote, skipping escaped quotes
    std::string result;
    for (size_t i = pos; i < json.size(); ++i) {
        if (json[i] == '\\' && i + 1 < json.size()) {
            result += json[i + 1];
            ++i;
        } else if (json[i] == '"') {
            break;
        } else {
            result += json[i];
        }
    }
    return result;
}

std::string json_get_type(const std::string& json) {
    return json_get_string(json, "type");
}

} // anonymous namespace

OnboardingWindow::OnboardingWindow() = default;

OnboardingWindow::~OnboardingWindow() {
    webview_.reset();
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

bool OnboardingWindow::show(HINSTANCE instance, AppSettings& settings) {
    instance_ = instance;
    settings_ = &settings;
    completed_ = false;

    Logger::info("Showing onboarding wizard");

    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(constants::kBackgroundColor);
    wc.lpszClassName = constants::kOnboardingWindowClass.data();
    RegisterClassEx(&wc);

    // Calculate centered position
    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);
    const int x = (screen_w - constants::kOnboardingWidth) / 2;
    const int y = (screen_h - constants::kOnboardingHeight) / 2;

    // Create chromeless window (WS_POPUP for no title bar)
    // WS_MINIMIZEBOX allows the window to be minimized via taskbar
    hwnd_ = CreateWindowEx(
        WS_EX_APPWINDOW,
        constants::kOnboardingWindowClass.data(),
        L"Reflection -- Setup",
        WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_MINIMIZEBOX,
        x, y,
        constants::kOnboardingWidth,
        constants::kOnboardingHeight,
        nullptr, nullptr, instance,
        this  // Pass this pointer for WM_CREATE
    );

    if (!hwnd_) {
        Logger::error("Failed to create onboarding window");
        return false;
    }

    // Apply dark title bar (for window edges/shadow)
    ThemeManager::apply_dark_title_bar(hwnd_, true);

    // Apply rounded corners on Windows 11
    DWORD corner_pref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE,
                          &corner_pref, sizeof(corner_pref));

    // Create WebView2 host
    webview_ = std::make_unique<WebViewHost>(hwnd_);

    webview_->set_message_handler([this](const std::wstring& json) {
        on_message_from_webview(json);
    });

    webview_->set_ready_handler([this]() {
        // Navigate to the onboarding page
        std::wstring assets_path = get_assets_path();
        std::wstring url = L"file:///" + assets_path + L"/onboarding.html";
        // Convert backslashes to forward slashes for file:// URL
        for (auto& c : url) {
            if (c == L'\\') c = L'/';
        }
        webview_->navigate(url);
    });

    // Send initial data AFTER the page has fully loaded.
    // Messages sent before NavigationCompleted are lost because
    // the JS event listeners haven't been registered yet.
    webview_->set_navigation_completed_handler([this]() {
        // Send initial theme
        std::string theme = ThemeManager::get_effective_theme(
            settings_->theme());
        webview_->post_message(make_json("themeChanged", "theme", theme));

        // Generate and send initial random name
        std::string name = NameGenerator::generate();
        webview_->post_message(make_json("setRandomName", "name", name));
    });

    if (!webview_->init()) {
        Logger::error("Failed to initialize WebView2 for onboarding");
        // Fallback: skip onboarding
        mark_completed();
        return true;
    }

    // Show and update the window
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    // Run modal message loop until onboarding completes
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (completed_) break;
    }

    // Clean up
    webview_.reset();
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    UnregisterClass(constants::kOnboardingWindowClass.data(), instance);

    return completed_;
}

LRESULT CALLBACK OnboardingWindow::wnd_proc(HWND hwnd, UINT msg,
                                              WPARAM wparam, LPARAM lparam) {
    OnboardingWindow* self = nullptr;

    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
        self = static_cast<OnboardingWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<OnboardingWindow*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    switch (msg) {
        case WM_SIZE:
            if (self && self->webview_) {
                self->webview_->resize();
            }
            return 0;

        case WM_CLOSE:
            // Don't allow closing — user must complete onboarding
            return 0;

        case WM_DESTROY:
            return 0;

        // Allow dragging the chromeless window
        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProc(hwnd, msg, wparam, lparam);
            if (hit == HTCLIENT) {
                // Allow dragging from the top 40px (step indicator area)
                POINT pt = {LOWORD(lparam), HIWORD(lparam)};
                ScreenToClient(hwnd, &pt);
                if (pt.y < 40) {
                    return HTCAPTION;
                }
            }
            return hit;
        }

        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

void OnboardingWindow::on_message_from_webview(const std::wstring& json_w) {
    std::string json = win_utils::wide_to_utf8(json_w);
    std::string type = json_get_type(json);

    Logger::info("Onboarding message: type={}", type);

    if (type == "setServerName") {
        std::string name = json_get_string(json, "name");
        if (!name.empty()) {
            settings_->set_server_name(win_utils::utf8_to_wide(name));
            Logger::info("Server name set to: {}", name);
        }
    } else if (type == "generateName") {
        std::string name = NameGenerator::generate();
        webview_->post_message(make_json("setRandomName", "name", name));
    } else if (type == "configureFirewall") {
        configure_firewall();
    } else if (type == "onboardingComplete") {
        mark_completed();
        completed_ = true;
        // Post quit to break the modal message loop
        PostMessage(hwnd_, WM_CLOSE, 0, 0);
    } else if (type == "minimizeWindow") {
        ShowWindow(hwnd_, SW_MINIMIZE);
    } else if (type == "startDrag") {
        // Initiate native window move — release mouse capture first so
        // the system can take over the drag. PostMessage is used so the
        // WebView2 message handler returns before the blocking drag loop.
        ReleaseCapture();
        PostMessage(hwnd_, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
    }
}

void OnboardingWindow::configure_firewall() {
    // Get the path to the current executable (zero-initialized buffer)
    wchar_t exe_path[MAX_PATH]{};
    DWORD path_len = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    if (path_len == 0 || path_len == MAX_PATH) {
        Logger::error("GetModuleFileName failed or truncated: {}", GetLastError());
        webview_->post_message(
            L"{\"type\":\"firewallResult\",\"success\":false,"
            L"\"message\":\"Could not determine executable path\"}");
        return;
    }

    // Sanitize: reject path if it contains embedded quotes (injection vector)
    std::wstring path(exe_path);
    if (path.find(L'"') != std::wstring::npos) {
        Logger::error("Executable path contains invalid characters — aborting firewall config");
        webview_->post_message(
            L"{\"type\":\"firewallResult\",\"success\":false,"
            L"\"message\":\"Invalid executable path\"}");
        return;
    }

    // Build netsh arguments — run netsh.exe directly (not via cmd.exe)
    // to avoid shell quoting issues and multiple process spawns.
    std::wstring netsh_args =
        L"advfirewall firewall add rule "
        L"name=\"" + std::wstring(constants::kAppInstanceName) + L"\" dir=in action=allow "
        L"program=\"" + path + L"\" "
        L"enable=yes";

    Logger::info("Configuring firewall: netsh.exe {}",
                 win_utils::wide_to_utf8(netsh_args));

    // Run netsh.exe elevated directly (triggers single UAC prompt)
    SHELLEXECUTEINFO sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";       // Request elevation
    sei.lpFile = L"netsh.exe";   // Run netsh directly, not via cmd.exe
    sei.lpParameters = netsh_args.c_str();
    sei.nShow = SW_HIDE;         // Don't show console window

    bool success = false;
    DWORD exit_code = 1;

    if (ShellExecuteEx(&sei)) {
        if (sei.hProcess) {
            WaitForSingleObject(sei.hProcess, 15000); // 15s timeout
            GetExitCodeProcess(sei.hProcess, &exit_code);
            CloseHandle(sei.hProcess);
            success = (exit_code == 0);
            Logger::info("netsh.exe exited with code: {}", exit_code);
        } else {
            Logger::warn("ShellExecuteEx succeeded but no process handle");
        }
    } else {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED) {
            Logger::info("User cancelled UAC prompt");
        } else {
            Logger::error("ShellExecuteEx failed: error={}", err);
        }
    }

    // Send result back to WebView2
    if (success) {
        Logger::info("Firewall rule added successfully");
        if (settings_) {
            settings_->set_firewall_configured(true);
        }
        webview_->post_message(
            L"{\"type\":\"firewallResult\",\"success\":true}");
    } else {
        Logger::warn("Firewall configuration failed (exit_code={})", exit_code);
        std::wstring msg = L"{\"type\":\"firewallResult\",\"success\":false,"
                           L"\"message\":\"Firewall configuration failed -- "
                           L"you can configure it manually in Windows Security\"}";
        webview_->post_message(msg);
    }
}

bool OnboardingWindow::is_completed() {
    HKEY key;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, constants::kRegistryRoot.data(),
                     0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD value = 0;
    DWORD size = sizeof(value);
    const bool completed = (RegQueryValueEx(
        key, constants::kRegKeyOnboardingCompleted.data(),
        nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &size
    ) == ERROR_SUCCESS) && value != 0;

    RegCloseKey(key);
    return completed;
}

void OnboardingWindow::mark_completed() {
    HKEY key;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, constants::kRegistryRoot.data(),
                       0, nullptr, 0, KEY_WRITE, nullptr, &key,
                       nullptr) == ERROR_SUCCESS) {
        DWORD value = 1;
        RegSetValueEx(key, constants::kRegKeyOnboardingCompleted.data(),
                      0, REG_DWORD, reinterpret_cast<const BYTE*>(&value),
                      sizeof(value));
        RegCloseKey(key);
    }
}

std::wstring OnboardingWindow::get_assets_path() {
    // Get the directory of the current executable
    wchar_t exe_path[MAX_PATH]{};
    DWORD len = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return L".";

    std::wstring path(exe_path);
    auto last_slash = path.find_last_of(L'\\');
    if (last_slash != std::wstring::npos) {
        path = path.substr(0, last_slash);
    }

    // Assets are in src/ui/assets/ relative to the source tree,
    // but at runtime they should be next to the executable
    return path + L"\\assets";
}

} // namespace reflection
