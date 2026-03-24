// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <functional>
#include <memory>
#include <string>

namespace reflection {

class WebViewHost;
class AppSettings;

/// First-run onboarding wizard with WebView2-hosted UI.
/// 3-page flow: Welcome → Name Your Device → Firewall Setup
///
/// The onboarding window is chromeless (no title bar), centered on screen,
/// and hosts a WebView2 panel that renders the animated HTML/CSS/JS wizard.
class OnboardingWindow {
public:
    using CompletionCallback = std::function<void()>;

    OnboardingWindow();
    ~OnboardingWindow();

    // Non-copyable
    OnboardingWindow(const OnboardingWindow&) = delete;
    OnboardingWindow& operator=(const OnboardingWindow&) = delete;

    /// Show the onboarding wizard. Blocks until completed or cancelled.
    /// @param instance  Application instance handle
    /// @param settings  App settings to write server name to
    /// @return true if onboarding was completed successfully
    bool show(HINSTANCE instance, AppSettings& settings);

    /// Check if onboarding has been completed (registry flag).
    static bool is_completed();

    /// Mark onboarding as completed.
    static void mark_completed();

private:
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg,
                                      WPARAM wparam, LPARAM lparam);
    void on_message_from_webview(const std::wstring& json);
    void configure_firewall();

    /// Get the absolute path to the UI assets directory.
    static std::wstring get_assets_path();

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    AppSettings* settings_ = nullptr;
    std::unique_ptr<WebViewHost> webview_;
    bool completed_ = false;
};

} // namespace reflection
