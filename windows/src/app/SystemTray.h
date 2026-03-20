#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <functional>
#include <string>

namespace reflection {

/// Manages the Windows system tray icon and right-click context menu.
/// Mirrors the macOS StatusBarController.
class SystemTray {
public:
    using MenuCallback = std::function<void(int menu_item_id)>;

    explicit SystemTray(HINSTANCE instance);
    ~SystemTray();

    // Non-copyable
    SystemTray(const SystemTray&) = delete;
    SystemTray& operator=(const SystemTray&) = delete;

    /// Install the tray icon. Returns false on failure.
    bool install(HWND parent_hwnd);

    /// Remove the tray icon.
    void remove();

    /// Update the tray tooltip text.
    void set_tooltip(const std::wstring& text);

    /// Show the context menu at the cursor position.
    void show_context_menu(HWND hwnd);

    /// Set callback for menu item selection.
    void set_menu_callback(MenuCallback callback);

    // Menu item IDs
    static constexpr int kMenuDisconnect = 1001;
    static constexpr int kMenuSettings = 1002;
    static constexpr int kMenuAbout = 1003;
    static constexpr int kMenuCheckUpdate = 1004;
    static constexpr int kMenuQuit = 1005;

private:
    HINSTANCE instance_;
    NOTIFYICONDATA nid_{};
    bool installed_ = false;
    MenuCallback menu_callback_;
};

} // namespace reflection
