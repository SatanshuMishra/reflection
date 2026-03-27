// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "views/StatusWindow.h"
#include "ui/WebViewHost.h"
#include "ui/ThemeManager.h"
#include "settings/AppSettings.h"
#include "settings/AutoStartService.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"
#include "utilities/NameGenerator.h"
#include "utilities/WinUtils.h"

#include <dwmapi.h>
#include <shellapi.h>

#pragma comment(lib, "dwmapi.lib")

namespace reflection {

namespace {

/// Escape a string for safe embedding in a JSON string value.
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

std::wstring make_json_w(const std::string& json_str) {
    return win_utils::utf8_to_wide(json_str);
}

/// Parse a simple JSON string to find a string value for a key.
/// Handles escaped quotes within values.
std::string json_get_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.size();
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

bool json_get_bool(const std::string& json, const std::string& key) {
    std::string search_true = "\"" + key + "\":true";
    return json.find(search_true) != std::string::npos;
}

} // anonymous namespace

StatusWindow::StatusWindow() = default;

StatusWindow::~StatusWindow() {
    destroy();
}

bool StatusWindow::create(HINSTANCE instance, AppSettings& settings,
                           HWND message_hwnd) {
    instance_ = instance;
    settings_ = &settings;
    message_hwnd_ = message_hwnd;

    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(constants::kBackgroundColor);
    wc.lpszClassName = constants::kStatusWindowClass.data();
    RegisterClassEx(&wc);

    // Window style — borderless popup (custom chrome via WebView2).
    // WS_THICKFRAME enables resizing, WS_MINIMIZEBOX enables taskbar minimize.
    // No WS_CAPTION/WS_SYSMENU — title bar is drawn in HTML.
    const DWORD style = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN;
    const DWORD ex_style = WS_EX_APPWINDOW;

    // For a popup window the client area IS the window area (no chrome to subtract)
    RECT desired = {0, 0, constants::kStatusWindowWidth, constants::kStatusWindowHeight};
    AdjustWindowRectEx(&desired, style, FALSE, ex_style);

    const int total_w = desired.right - desired.left;
    const int total_h = desired.bottom - desired.top;

    // Center on screen
    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);
    const int x = (screen_w - total_w) / 2;
    const int y = (screen_h - total_h) / 2;

    hwnd_ = CreateWindowEx(
        ex_style,
        constants::kStatusWindowClass.data(),
        L"Reflection",
        style,
        x, y,
        total_w,
        total_h,
        nullptr, nullptr, instance,
        this
    );

    if (!hwnd_) {
        Logger::error("Failed to create status window");
        return false;
    }

    // Apply dark title bar based on current theme
    std::string theme = ThemeManager::get_effective_theme(settings.theme());
    ThemeManager::apply_dark_title_bar(hwnd_, theme == "dark");

    // Create WebView2 host
    webview_ = std::make_unique<WebViewHost>(hwnd_);

    webview_->set_message_handler([this](const std::wstring& json) {
        on_message_from_webview(json);
    });

    webview_->set_ready_handler([this]() {
        // Navigate to the status page
        std::wstring assets_path = get_assets_path();
        std::wstring url = L"file:///" + assets_path + L"/status.html";
        for (auto& c : url) {
            if (c == L'\\') c = L'/';
        }
        webview_->navigate(url);
    });

    // Send initial settings AFTER the page has fully loaded.
    // Messages sent before NavigationCompleted are lost because
    // the JS event listeners haven't been registered yet.
    webview_->set_navigation_completed_handler([this]() {
        // Send theme
        std::string theme = ThemeManager::get_effective_theme(
            settings_->theme());
        webview_->post_message(make_json_w(
            "{\"type\":\"themeChanged\",\"theme\":\"" + theme + "\"}"));

        // Send current settings (server name, toggles)
        std::string server_name = win_utils::wide_to_utf8(settings_->server_name());
        bool run_in_bg = settings_->minimize_to_tray();
        bool launch_login = settings_->start_on_login();

        std::string settings_json =
            "{\"type\":\"settingsUpdated\""
            ",\"serverName\":\"" + json_escape(server_name) + "\""
            ",\"theme\":\"" + json_escape(settings_->theme()) + "\""
            ",\"runInBackground\":" + (run_in_bg ? "true" : "false") +
            ",\"launchAtLogin\":" + (launch_login ? "true" : "false") +
            "}";
        webview_->post_message(make_json_w(settings_json));

        // Send firewall status so the UI can show a banner if needed
        bool fw_ok = settings_->firewall_configured();
        webview_->post_message(make_json_w(
            "{\"type\":\"firewallStatus\",\"configured\":" +
            std::string(fw_ok ? "true" : "false") + "}"));

        Logger::info("Status window loaded -- server name: {}, firewall: {}",
                     server_name, fw_ok ? "ok" : "missing");

        page_loaded_ = true;

        // Replay deferred connection state if set_connected() was called
        // before the page finished loading
        if (deferred_connected_) {
            set_connected(deferred_device_name_);
            deferred_connected_ = false;
            deferred_device_name_.clear();
        }
    });

    if (!webview_->init()) {
        Logger::error("Failed to initialize WebView2 for status window");
        return false;
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    Logger::info("Status window created");
    return true;
}

void StatusWindow::destroy() {
    webview_.reset();
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (instance_) {
        UnregisterClass(constants::kStatusWindowClass.data(), instance_);
    }
}

bool StatusWindow::is_visible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

void StatusWindow::show() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_SHOW);
        SetForegroundWindow(hwnd_);
    }
}

void StatusWindow::hide() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

void StatusWindow::set_connected(const std::string& device_name) {
    if (!page_loaded_) {
        // Page hasn't loaded yet — defer until NavigationCompleted
        deferred_connected_ = true;
        deferred_device_name_ = device_name;
        return;
    }
    if (webview_) {
        webview_->post_message(make_json_w(
            "{\"type\":\"connected\",\"deviceName\":\"" + json_escape(device_name) + "\"}"));
    }
}

void StatusWindow::set_disconnected() {
    if (webview_) {
        webview_->post_message(make_json_w("{\"type\":\"disconnected\"}"));
    }
}

void StatusWindow::set_server_name(const std::string& name) {
    if (webview_) {
        webview_->post_message(make_json_w(
            "{\"type\":\"serverNameChanged\",\"name\":\"" + json_escape(name) + "\"}"));
    }
}

void StatusWindow::set_theme(const std::string& theme) {
    if (webview_) {
        webview_->post_message(make_json_w(
            "{\"type\":\"themeChanged\",\"theme\":\"" + theme + "\"}"));
    }
    if (hwnd_) {
        ThemeManager::apply_dark_title_bar(hwnd_, theme == "dark");
    }
}

LRESULT CALLBACK StatusWindow::wnd_proc(HWND hwnd, UINT msg,
                                          WPARAM wparam, LPARAM lparam) {
    StatusWindow* self = nullptr;

    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
        self = static_cast<StatusWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<StatusWindow*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    switch (msg) {
        case WM_SIZE:
            if (self && self->webview_) {
                self->webview_->resize();
            }
            return 0;

        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lparam);
            // Minimum client area: 360×300
            RECT min_rect = {0, 0, 360, 300};
            AdjustWindowRectEx(&min_rect,
                GetWindowLong(hwnd, GWL_STYLE), FALSE,
                GetWindowLong(hwnd, GWL_EXSTYLE));
            mmi->ptMinTrackSize.x = min_rect.right - min_rect.left;
            mmi->ptMinTrackSize.y = min_rect.bottom - min_rect.top;
            return 0;
        }

        case WM_CLOSE:
            if (self && self->settings_ && self->settings_->minimize_to_tray()) {
                // Minimize to tray instead of closing
                ShowWindow(hwnd, SW_HIDE);
                return 0;
            }
            // If not running in background, quit the app
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

void StatusWindow::on_message_from_webview(const std::wstring& json_w) {
    std::string json = win_utils::wide_to_utf8(json_w);
    std::string type = json_get_string(json, "type");

    Logger::info("Status window message: type={}", type);

    if (type == "setServerName") {
        std::string name = json_get_string(json, "name");
        if (!name.empty()) {
            settings_->set_server_name(win_utils::utf8_to_wide(name));
            Logger::info("Server name changed to: {}", name);
            // Signal the App to restart AirPlay with the new name
            if (message_hwnd_) {
                PostMessage(message_hwnd_, constants::kWmServerNameChanged, 0, 0);
            }
        }
    } else if (type == "setTheme") {
        std::string theme = json_get_string(json, "theme");
        settings_->set_theme(theme);
        std::string effective = ThemeManager::get_effective_theme(theme);
        set_theme(effective);
        Logger::info("Theme changed to: {} (effective: {})", theme, effective);
    } else if (type == "setRunInBackground") {
        bool enabled = json_get_bool(json, "enabled");
        settings_->set_minimize_to_tray(enabled);
        Logger::info("Run in background: {}", enabled ? "on" : "off");
    } else if (type == "setLaunchAtLogin") {
        bool enabled = json_get_bool(json, "enabled");
        settings_->set_start_on_login(enabled);
        // Update auto-start registry entry
        AutoStartService auto_start;
        if (enabled) {
            auto_start.enable();
        } else {
            auto_start.disable();
        }
        Logger::info("Launch at login: {}", enabled ? "on" : "off");
    } else if (type == "disconnect") {
        // Forward disconnect request to the main app
        if (message_hwnd_) {
            PostMessage(message_hwnd_, constants::kWmIpadDisconnected, 0, 0);
        }
    } else if (type == "configureFirewall") {
        configure_firewall();
    } else if (type == "minimizeWindow") {
        ShowWindow(hwnd_, SW_MINIMIZE);
    } else if (type == "closeWindow") {
        if (settings_ && settings_->minimize_to_tray()) {
            ShowWindow(hwnd_, SW_HIDE);
        } else {
            PostQuitMessage(0);
        }
    } else if (type == "startDrag") {
        ReleaseCapture();
        PostMessage(hwnd_, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
    } else if (type == "forceReannounce") {
        Logger::info("Manual broadcast triggered from UI");
        if (message_hwnd_) {
            PostMessage(message_hwnd_, constants::kWmForceReannounce, 0, 0);
        }
    }
}

void StatusWindow::configure_firewall() {
    wchar_t exe_path[MAX_PATH]{};
    DWORD path_len = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    if (path_len == 0 || path_len == MAX_PATH) {
        Logger::error("GetModuleFileName failed or truncated: {}", GetLastError());
        return;
    }

    // Sanitize: reject path with embedded quotes (injection vector)
    std::wstring path(exe_path);
    if (path.find(L'"') != std::wstring::npos) {
        Logger::error("Executable path contains invalid characters");
        return;
    }

    // Idempotent: delete any existing rule first, then add the new one.
    // netsh "add rule" never deduplicates — calling it twice creates two
    // identical entries. The delete-then-add pattern prevents duplicates
    // across installer + runtime rule creation. Both commands run in a
    // single cmd.exe /c invocation so only one UAC prompt is shown.
    const std::wstring rule_name = std::wstring(constants::kAppInstanceName);
    std::wstring cmd_args =
        L"/c netsh advfirewall firewall delete rule "
        L"name=\"" + rule_name + L"\" >nul 2>&1 & "
        L"netsh advfirewall firewall add rule "
        L"name=\"" + rule_name + L"\" dir=in action=allow "
        L"program=\"" + path + L"\" "
        L"enable=yes";

    Logger::info("Status window: configuring firewall (delete-then-add)");

    SHELLEXECUTEINFO sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = L"cmd.exe";
    sei.lpParameters = cmd_args.c_str();
    sei.nShow = SW_HIDE;

    bool success = false;
    if (ShellExecuteEx(&sei)) {
        if (sei.hProcess) {
            WaitForSingleObject(sei.hProcess, 15000);
            DWORD exit_code = 1;
            GetExitCodeProcess(sei.hProcess, &exit_code);
            CloseHandle(sei.hProcess);
            success = (exit_code == 0);
        }
    }

    if (success && settings_) {
        settings_->set_firewall_configured(true);

        // Notify main app to start AirPlay service now that firewall allows it
        if (message_hwnd_) {
            PostMessage(message_hwnd_, constants::kWmFirewallGranted, 0, 0);
        }
    }

    // Send result back to webview
    std::string result_json = "{\"type\":\"firewallResult\",\"success\":" +
        std::string(success ? "true" : "false") + "}";
    if (webview_) {
        webview_->post_message(make_json_w(result_json));
    }
}

std::wstring StatusWindow::get_assets_path() {
    wchar_t exe_path[MAX_PATH]{};
    DWORD len = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return L".";

    std::wstring path(exe_path);
    auto last_slash = path.find_last_of(L'\\');
    if (last_slash != std::wstring::npos) {
        path = path.substr(0, last_slash);
    }
    return path + L"\\assets";
}

} // namespace reflection
