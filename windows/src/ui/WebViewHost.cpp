// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "ui/WebViewHost.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"

#include <shlobj.h>

namespace reflection {

WebViewHost::WebViewHost(HWND parent) : parent_(parent) {}

WebViewHost::~WebViewHost() {
    if (controller_) {
        controller_->Close();
    }
}

bool WebViewHost::init() {
    // User data folder for WebView2 — in the user's local app data
    wchar_t* app_data = nullptr;
    SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &app_data);
    std::wstring user_data = std::wstring(app_data) + L"\\" +
        std::wstring(constants::kWebViewSubdir) + L"\\WebView2";
    CoTaskMemFree(app_data);

    // Create the WebView2 environment with options
    auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();

    // Disable unnecessary features for security and performance
    options->put_AdditionalBrowserArguments(
        L"--disable-features=msSmartScreenProtection "
        L"--disable-web-security=false");

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, // Use installed Edge
        user_data.c_str(),
        options.Get(),
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                on_environment_created(result, env);
                return S_OK;
            }).Get());

    if (FAILED(hr)) {
        Logger::error("Failed to create WebView2 environment: 0x{:08X}",
                      static_cast<unsigned>(hr));
        return false;
    }

    return true;
}

void WebViewHost::on_environment_created(HRESULT result,
                                          ICoreWebView2Environment* env) {
    if (FAILED(result) || !env) {
        Logger::error("WebView2 environment creation failed: 0x{:08X}",
                      static_cast<unsigned>(result));
        return;
    }

    environment_ = env;

    env->CreateCoreWebView2Controller(
        parent_,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this](HRESULT res, ICoreWebView2Controller* ctrl) -> HRESULT {
                on_controller_created(res, ctrl);
                return S_OK;
            }).Get());
}

void WebViewHost::on_controller_created(HRESULT result,
                                         ICoreWebView2Controller* controller) {
    if (FAILED(result) || !controller) {
        Logger::error("WebView2 controller creation failed: 0x{:08X}",
                      static_cast<unsigned>(result));
        return;
    }

    controller_ = controller;
    controller_->get_CoreWebView2(&webview_);

    // Configure settings for security
    Microsoft::WRL::ComPtr<ICoreWebView2Settings> settings;
    webview_->get_Settings(&settings);

    settings->put_IsScriptEnabled(TRUE);
    settings->put_AreDefaultScriptDialogsEnabled(FALSE);
    settings->put_IsWebMessageEnabled(TRUE);
    settings->put_AreDevToolsEnabled(FALSE);
    settings->put_AreDefaultContextMenusEnabled(FALSE);
    settings->put_IsStatusBarEnabled(FALSE);
    settings->put_IsZoomControlEnabled(FALSE);

    // Disable navigation to external URLs — local content only
    webview_->add_NavigationStarting(
        Microsoft::WRL::Callback<ICoreWebView2NavigationStartingEventHandler>(
            [](ICoreWebView2* /*sender*/,
               ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                wchar_t* uri = nullptr;
                args->get_Uri(&uri);
                if (uri) {
                    std::wstring url(uri);
                    CoTaskMemFree(uri);
                    // Allow file:// and about: URLs only.
                    // data: URIs are blocked to prevent JS injection attacks.
                    if (url.find(L"file://") != 0 &&
                        url.find(L"about:") != 0) {
                        args->put_Cancel(TRUE);
                    }
                }
                return S_OK;
            }).Get(),
        nullptr);

    // Fire navigation-completed callback when page finishes loading.
    // Messages sent before this are lost because JS listeners aren't ready.
    webview_->add_NavigationCompleted(
        Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [this](ICoreWebView2* /*sender*/,
                   ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                BOOL success = FALSE;
                args->get_IsSuccess(&success);
                if (success && nav_completed_handler_) {
                    nav_completed_handler_();
                }
                return S_OK;
            }).Get(),
        nullptr);

    // Handle messages from JS → C++
    webview_->add_WebMessageReceived(
        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2* /*sender*/,
                   ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                wchar_t* message = nullptr;
                args->TryGetWebMessageAsString(&message);
                if (message && message_handler_) {
                    message_handler_(message);
                    CoTaskMemFree(message);
                }
                return S_OK;
            }).Get(),
        nullptr);

    // Fit WebView2 to parent window
    resize();

    // Mark as ready
    ready_ = true;
    Logger::info("WebView2 initialized successfully");

    // Process any pending navigation
    if (!pending_url_.empty()) {
        navigate(pending_url_);
        pending_url_.clear();
    } else if (!pending_html_.empty()) {
        navigate_to_string(pending_html_);
        pending_html_.clear();
    }

    // Flush any queued messages
    flush_pending_messages();

    // Notify ready handler
    if (ready_handler_) {
        ready_handler_();
    }
}

void WebViewHost::navigate(const std::wstring& url) {
    if (!ready_ || !webview_) {
        pending_url_ = url;
        return;
    }
    webview_->Navigate(url.c_str());
}

void WebViewHost::navigate_to_string(const std::wstring& html) {
    if (!ready_ || !webview_) {
        pending_html_ = html;
        return;
    }
    webview_->NavigateToString(html.c_str());
}

void WebViewHost::post_message(const std::wstring& json) {
    if (!ready_ || !webview_) {
        pending_messages_.push_back(json);
        return;
    }
    webview_->PostWebMessageAsString(json.c_str());
}

void WebViewHost::resize() {
    if (!controller_) return;

    RECT bounds;
    GetClientRect(parent_, &bounds);
    controller_->put_Bounds(bounds);
}

void WebViewHost::resize(const RECT& bounds) {
    if (!controller_) return;
    controller_->put_Bounds(bounds);
}

void WebViewHost::flush_pending_messages() {
    for (const auto& msg : pending_messages_) {
        webview_->PostWebMessageAsString(msg.c_str());
    }
    pending_messages_.clear();
}

} // namespace reflection
