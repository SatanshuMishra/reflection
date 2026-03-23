#include "ui/ThemeManager.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"

#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

namespace reflection {

namespace {

constexpr wchar_t kPersonalizeKey[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
constexpr wchar_t kAppsUseLightTheme[] = L"AppsUseLightTheme";

// DWMWA_USE_IMMERSIVE_DARK_MODE — attribute 20 (Windows 10 1809+, Windows 11)
constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_VALUE = 20;

} // anonymous namespace

ThemeManager::ThemeManager() = default;

ThemeManager::~ThemeManager() {
    stop();
}

void ThemeManager::start(HWND message_hwnd) {
    if (running_.load()) return;

    message_hwnd_ = message_hwnd;
    stop_event_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    running_.store(true);

    monitor_thread_ = std::thread(&ThemeManager::monitor_thread_func, this);
    Logger::info("ThemeManager: monitoring system theme changes");
}

void ThemeManager::stop() {
    if (!running_.load()) return;

    running_.store(false);
    if (stop_event_) {
        SetEvent(stop_event_);
    }

    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }

    if (stop_event_) {
        CloseHandle(stop_event_);
        stop_event_ = nullptr;
    }

    Logger::info("ThemeManager: stopped");
}

std::string ThemeManager::get_system_theme() {
    HKEY key;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, kPersonalizeKey,
                     0, KEY_READ, &key) != ERROR_SUCCESS) {
        return "dark"; // Default to dark if can't read
    }

    DWORD value = 1; // Default: light theme
    DWORD size = sizeof(value);
    RegQueryValueEx(key, kAppsUseLightTheme, nullptr, nullptr,
                    reinterpret_cast<LPBYTE>(&value), &size);
    RegCloseKey(key);

    // AppsUseLightTheme: 0 = dark, 1 = light
    return value == 0 ? "dark" : "light";
}

std::string ThemeManager::get_effective_theme(const std::string& app_setting) {
    if (app_setting == "system") {
        return get_system_theme();
    }
    return app_setting;
}

void ThemeManager::apply_dark_title_bar(HWND hwnd, bool dark) {
    BOOL use_dark = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_VALUE,
                          &use_dark, sizeof(use_dark));
}

void ThemeManager::monitor_thread_func() {
    std::string last_theme = get_system_theme();

    while (running_.load()) {
        HKEY key;
        if (RegOpenKeyEx(HKEY_CURRENT_USER, kPersonalizeKey,
                         0, KEY_READ | KEY_NOTIFY, &key) != ERROR_SUCCESS) {
            // Can't open key — wait and retry
            WaitForSingleObject(stop_event_, 2000);
            continue;
        }

        // Wait for a change to the Personalize key
        HANDLE events[2] = { stop_event_, nullptr };
        events[1] = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        LONG result = RegNotifyChangeKeyValue(
            key, FALSE, REG_NOTIFY_CHANGE_LAST_SET,
            events[1], TRUE);

        if (result == ERROR_SUCCESS) {
            // Wait for either the registry change or stop signal
            DWORD wait_result = WaitForMultipleObjects(2, events, FALSE, INFINITE);

            if (wait_result == WAIT_OBJECT_0) {
                // Stop event signaled
                CloseHandle(events[1]);
                RegCloseKey(key);
                break;
            }

            // Registry changed — check if theme actually changed
            std::string current_theme = get_system_theme();
            if (current_theme != last_theme) {
                last_theme = current_theme;
                Logger::info("ThemeManager: system theme changed to {}",
                             current_theme);
                if (message_hwnd_) {
                    PostMessage(message_hwnd_, constants::kWmThemeChanged,
                                0, 0);
                }
            }
        }

        CloseHandle(events[1]);
        RegCloseKey(key);
    }
}

} // namespace reflection
