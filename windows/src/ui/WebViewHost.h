// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <wrl.h>
#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace reflection {

/// RAII wrapper around ICoreWebView2 for hosting HTML/CSS/JS UI panels.
///
/// Usage:
///   1. Create a WebViewHost with a parent HWND
///   2. Call init() — async, WebView2 environment created in background
///   3. Call navigate() or navigate_to_string() once ready
///   4. Use post_message() to send JSON from C++ to JS
///   5. Set set_message_handler() to receive JSON from JS to C++
///
/// The WebView2 control auto-resizes to fill the parent HWND.
class WebViewHost {
public:
    using MessageHandler = std::function<void(const std::wstring& json)>;

    explicit WebViewHost(HWND parent);
    ~WebViewHost();

    // Non-copyable, non-movable
    WebViewHost(const WebViewHost&) = delete;
    WebViewHost& operator=(const WebViewHost&) = delete;

    /// Initialize the WebView2 environment and controller.
    /// This is async — the WebView2 won't be ready immediately.
    /// Call set_ready_handler() to be notified when init completes.
    bool init();

    /// Whether the WebView2 is fully initialized and ready for use.
    [[nodiscard]] bool is_ready() const { return ready_; }

    /// Set a callback invoked when the WebView2 controller is ready.
    void set_ready_handler(std::function<void()> handler) {
        ready_handler_ = std::move(handler);
    }

    /// Set a callback invoked when navigation completes (page fully loaded).
    /// Use this to send initial data — messages sent before this may be lost.
    void set_navigation_completed_handler(std::function<void()> handler) {
        nav_completed_handler_ = std::move(handler);
    }

    /// Navigate to a file:// URL for a local HTML file.
    void navigate(const std::wstring& url);

    /// Load HTML content directly from a string.
    void navigate_to_string(const std::wstring& html);

    /// Send a JSON message from C++ to the WebView2 JS context.
    /// In JS, listen via: window.chrome.webview.addEventListener('message', ...)
    void post_message(const std::wstring& json);

    /// Set the handler for messages sent from JS to C++.
    /// In JS, send via: window.chrome.webview.postMessage({...})
    void set_message_handler(MessageHandler handler) {
        message_handler_ = std::move(handler);
    }

    /// Resize the WebView2 to match the parent window.
    /// Call this from the parent's WM_SIZE handler.
    void resize();

    /// Resize to specific bounds within the parent.
    void resize(const RECT& bounds);

private:
    void on_environment_created(HRESULT result,
                                ICoreWebView2Environment* env);
    void on_controller_created(HRESULT result,
                               ICoreWebView2Controller* controller);
    void flush_pending_messages();

    HWND parent_ = nullptr;
    bool ready_ = false;

    Microsoft::WRL::ComPtr<ICoreWebView2Environment> environment_;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview_;

    MessageHandler message_handler_;
    std::function<void()> ready_handler_;
    std::function<void()> nav_completed_handler_;

    // Messages queued before WebView2 is ready
    std::vector<std::wstring> pending_messages_;

    // Pending navigation (queued before ready)
    std::wstring pending_url_;
    std::wstring pending_html_;
};

} // namespace reflection
