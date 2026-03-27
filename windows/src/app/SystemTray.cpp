// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "app/SystemTray.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"

#include <shellapi.h>

namespace reflection {

SystemTray::SystemTray(HINSTANCE instance)
    : instance_(instance)
{
}

SystemTray::~SystemTray() {
    remove();
}

bool SystemTray::install(HWND parent_hwnd) {
    if (installed_) return true;

    nid_.cbSize = sizeof(NOTIFYICONDATA);
    nid_.hWnd = parent_hwnd;
    nid_.uID = constants::kTrayIconId;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = constants::kWmTrayIcon;

    // Use application icon if available, otherwise system default
    nid_.hIcon = LoadIcon(instance_, MAKEINTRESOURCE(1));
    if (!nid_.hIcon) {
        nid_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    }

    wcscpy_s(nid_.szTip, L"Reflection -- Waiting for iPad...");

    installed_ = Shell_NotifyIcon(NIM_ADD, &nid_) != FALSE;

    if (installed_) {
        Logger::info("System tray icon installed");
    } else {
        Logger::error("Failed to install system tray icon");
    }

    return installed_;
}

void SystemTray::remove() {
    if (!installed_) return;

    Shell_NotifyIcon(NIM_DELETE, &nid_);
    installed_ = false;
    Logger::info("System tray icon removed");
}

void SystemTray::set_tooltip(const std::wstring& text) {
    if (!installed_) return;

    wcsncpy_s(nid_.szTip, text.c_str(), _TRUNCATE);
    nid_.uFlags = NIF_TIP;
    Shell_NotifyIcon(NIM_MODIFY, &nid_);
}

void SystemTray::set_connection_state(bool connected,
                                       const std::wstring& device_name) {
    connected_ = connected;
    device_name_ = device_name;

    // Update tooltip
    if (connected) {
        set_tooltip(L"Reflection -- Connected: " + device_name);
    } else {
        set_tooltip(L"Reflection -- Waiting for iPad...");
    }
}

void SystemTray::set_server_name(const std::wstring& name) {
    server_name_ = name;
}

void SystemTray::show_context_menu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    // Title (disabled, bold via owner draw would be needed, using plain text)
    AppendMenu(menu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, L"Reflection");
    AppendMenu(menu, MF_SEPARATOR, 0, nullptr);

    // Dynamic status lines
    std::wstring discoverable_line = L"Discoverable as: " + server_name_;
    AppendMenu(menu, MF_STRING | MF_DISABLED | MF_GRAYED, 0,
               discoverable_line.c_str());

    if (connected_) {
        std::wstring connected_line = L"Connected to: " + device_name_;
        AppendMenu(menu, MF_STRING | MF_DISABLED | MF_GRAYED, 0,
                   connected_line.c_str());
    } else {
        AppendMenu(menu, MF_STRING | MF_DISABLED | MF_GRAYED, 0,
                   L"Waiting for connection...");
    }

    AppendMenu(menu, MF_SEPARATOR, 0, nullptr);

    // Actions
    UINT disconnect_flags = connected_ ? MF_STRING : (MF_STRING | MF_GRAYED);
    AppendMenu(menu, disconnect_flags, kMenuDisconnect, L"Disconnect");
    AppendMenu(menu, MF_STRING, kMenuSettings, L"Settings...");
    AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(menu, MF_STRING, kMenuShowWindow, L"Show Reflection");
    AppendMenu(menu, MF_STRING, kMenuCheckUpdates, L"Check for Updates...");
    AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(menu, MF_STRING, kMenuQuit, L"Quit");

    // Required: set foreground so menu dismisses on click-away
    SetForegroundWindow(hwnd);

    POINT pt;
    GetCursorPos(&pt);

    const int cmd = TrackPopupMenuEx(
        menu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        pt.x, pt.y, hwnd, nullptr
    );

    DestroyMenu(menu);

    if (cmd > 0 && menu_callback_) {
        menu_callback_(cmd);
    }
}

void SystemTray::show_balloon(const std::wstring& title,
                               const std::wstring& message) {
    if (!installed_) return;

    nid_.uFlags = NIF_INFO;
    wcsncpy_s(nid_.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid_.szInfo, message.c_str(), _TRUNCATE);
    nid_.dwInfoFlags = NIIF_INFO;

    Shell_NotifyIcon(NIM_MODIFY, &nid_);
}

void SystemTray::set_menu_callback(MenuCallback callback) {
    menu_callback_ = std::move(callback);
}

} // namespace reflection
