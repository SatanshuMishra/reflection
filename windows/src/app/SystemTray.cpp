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

    // Use a system icon for now — TODO: custom icon in Milestone 8
    nid_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    wcscpy_s(nid_.szTip, L"Reflection — Waiting for iPad...");

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

void SystemTray::show_context_menu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    // Title (disabled)
    AppendMenu(menu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, L"Reflection");
    AppendMenu(menu, MF_SEPARATOR, 0, nullptr);

    // Status line
    // TODO: Update dynamically based on connection state
    AppendMenu(menu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, L"Status: Waiting...");
    AppendMenu(menu, MF_STRING | MF_GRAYED, kMenuDisconnect, L"Disconnect");
    AppendMenu(menu, MF_SEPARATOR, 0, nullptr);

    // Actions
    AppendMenu(menu, MF_STRING, kMenuSettings, L"Settings...");
    AppendMenu(menu, MF_STRING, kMenuAbout, L"About Reflection");
    AppendMenu(menu, MF_STRING, kMenuCheckUpdate, L"Check for Updates");
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

void SystemTray::set_menu_callback(MenuCallback callback) {
    menu_callback_ = std::move(callback);
}

} // namespace reflection
