// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace reflection {

class AppSettings;

/// Manages dark/light/system theme for the application.
///
/// - Reads the Windows system theme from the registry
/// - Monitors for real-time theme changes via RegNotifyChangeKeyValue
/// - Resolves "system" mode to actual dark/light
/// - Notifies listeners when the effective theme changes
class ThemeManager {
public:
    using ThemeChangeCallback = std::function<void(const std::string& theme)>;

    ThemeManager();
    ~ThemeManager();

    // Non-copyable
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    /// Start monitoring for system theme changes.
    /// @param message_hwnd  Window to receive kWmThemeChanged messages
    void start(HWND message_hwnd);

    /// Stop monitoring.
    void stop();

    /// Get the current system theme preference.
    /// @return "dark" or "light"
    [[nodiscard]] static std::string get_system_theme();

    /// Resolve the effective theme based on the app setting.
    /// If setting is "system", returns the system theme. Otherwise returns the setting.
    /// @return "dark" or "light"
    [[nodiscard]] static std::string get_effective_theme(const std::string& app_setting);

    /// Apply immersive dark mode to a window's title bar.
    /// Works on Windows 10 1809+ and Windows 11.
    static void apply_dark_title_bar(HWND hwnd, bool dark);

private:
    void monitor_thread_func();

    HWND message_hwnd_ = nullptr;
    std::atomic<bool> running_{false};
    std::thread monitor_thread_;
    HANDLE stop_event_ = nullptr;
};

} // namespace reflection
