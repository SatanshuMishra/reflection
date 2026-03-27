// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "views/MirrorWindow.h"
#include "views/MirrorOverlay.h"
#include "ui/ThemeManager.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"

#include <commctrl.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

namespace reflection {

namespace {

// DWM attribute constants (for older Windows SDK compatibility)
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
constexpr DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;
#endif
#ifndef DWMWCP_ROUND
constexpr DWORD DWMWCP_ROUND = 2;
#endif

/// Static child window class for the video rendering surface.
constexpr std::wstring_view kVideoChildClass = L"ReflectionVideoChild";

/// Resize border thickness for WM_NCHITTEST (pixels).
constexpr int kResizeBorderWidth = 6;

/// Caption drag region height when overlay is hidden (pixels).
constexpr int kDragRegionHeight = 32;

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction / Destruction
// ---------------------------------------------------------------------------

MirrorWindow::MirrorWindow() = default;

MirrorWindow::~MirrorWindow() {
    close();
}

// ---------------------------------------------------------------------------
// Window Creation
// ---------------------------------------------------------------------------

bool MirrorWindow::create(HINSTANCE instance, const std::wstring& title,
                           HWND app_hwnd, const std::wstring& device_name) {
    Logger::info("Creating custom mirror window");
    instance_ = instance;
    app_hwnd_ = app_hwnd;
    device_name_ = device_name;
    aspect_ratio_ = constants::kDefaultAspectRatio;

    // Register the main window class
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(constants::kBackgroundColor);
    wc.lpszClassName = constants::kMirrorWindowClass.data();
    if (!RegisterClassEx(&wc) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Logger::error("Failed to register mirror window class");
        return false;
    }

    // Borderless popup with thick frame for resize (matches StatusWindow)
    const DWORD style = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX
                      | WS_MAXIMIZEBOX | WS_CLIPCHILDREN;
    const DWORD ex_style = WS_EX_APPWINDOW;

    // Account for frame size
    RECT desired{0, 0,
                 constants::kDefaultMirrorWidth,
                 constants::kDefaultMirrorHeight};
    AdjustWindowRectEx(&desired, style, FALSE, ex_style);

    const int total_w = desired.right - desired.left;
    const int total_h = desired.bottom - desired.top;
    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);
    const int x = (screen_w - total_w) / 2;
    const int y = (screen_h - total_h) / 2;

    hwnd_ = CreateWindowEx(
        ex_style,
        constants::kMirrorWindowClass.data(),
        title.c_str(),
        style,
        x, y, total_w, total_h,
        nullptr, nullptr, instance, this
    );

    if (!hwnd_) {
        Logger::error("Failed to create mirror window");
        return false;
    }

    apply_dwm_attributes();

    if (!create_video_child()) {
        Logger::error("Failed to create video child HWND");
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    if (!create_overlay()) {
        Logger::error("Failed to create mirror overlay");
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    Logger::info("Custom mirror window created (video child HWND for GStreamer)");
    return true;
}

void MirrorWindow::close() {
    overlay_.reset();
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        video_child_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Child HWND Creation
// ---------------------------------------------------------------------------

bool MirrorWindow::create_video_child() {
    // Register a simple static child class for the video surface
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = instance_;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kVideoChildClass.data();
    if (!RegisterClassEx(&wc) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Logger::error("Failed to register video child window class");
        return false;
    }

    RECT client{};
    GetClientRect(hwnd_, &client);

    video_child_ = CreateWindowEx(
        0,
        kVideoChildClass.data(),
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, client.right, client.bottom,
        hwnd_, nullptr, instance_, nullptr
    );

    if (!video_child_) return false;

    // Subclass the video child to forward mouse events to the parent.
    // The video child fills the entire client area, so it receives all pointer
    // input — without forwarding, the parent's WM_MOUSEMOVE never fires and
    // the overlay never appears.
    //
    // SetWindowSubclass (comctl32) chains correctly with GStreamer's own HWND
    // subclassing, unlike SetWindowLongPtr(GWLP_WNDPROC) which would break it.
    SetWindowSubclass(
        video_child_,
        [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
           UINT_PTR /*id*/, DWORD_PTR ref_data) -> LRESULT {
            if (msg == WM_MOUSEMOVE || msg == WM_MOUSELEAVE) {
                auto* parent = reinterpret_cast<HWND__*>(
                    static_cast<uintptr_t>(ref_data));
                PostMessage(parent, msg, wp, lp);
            }
            return DefSubclassProc(hwnd, msg, wp, lp);
        },
        1,  // subclass ID
        static_cast<DWORD_PTR>(reinterpret_cast<uintptr_t>(hwnd_))
    );

    return true;
}

bool MirrorWindow::create_overlay() {
    overlay_ = std::make_unique<MirrorOverlay>();

    RECT client{};
    GetClientRect(hwnd_, &client);

    if (!overlay_->create(hwnd_, instance_, device_name_)) {
        return false;
    }

    // Wire up button callbacks
    overlay_->set_close_callback([this]() {
        PostMessage(hwnd_, WM_CLOSE, 0, 0);
    });

    overlay_->set_minimize_callback([this]() {
        ShowWindow(hwnd_, SW_MINIMIZE);
    });

    overlay_->set_maximize_callback([this]() {
        if (is_fullscreen_) return;
        if (IsZoomed(hwnd_)) {
            ShowWindow(hwnd_, SW_RESTORE);
        } else {
            ShowWindow(hwnd_, SW_MAXIMIZE);
        }
    });

    overlay_->set_fullscreen_callback([this]() {
        toggle_fullscreen();
    });

    overlay_->set_aspect_lock_callback([this]() {
        toggle_aspect_lock();
    });

    return true;
}

// ---------------------------------------------------------------------------
// DWM Styling
// ---------------------------------------------------------------------------

void MirrorWindow::apply_dwm_attributes() {
    // Dark title bar hint (affects window border color on Win10+)
    ThemeManager::apply_dark_title_bar(hwnd_, true);

    // Rounded corners on Windows 11
    DWORD corner_pref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE,
                           &corner_pref, sizeof(corner_pref));
}

// ---------------------------------------------------------------------------
// Overlay Show/Hide
// ---------------------------------------------------------------------------

void MirrorWindow::show_overlay() {
    if (overlay_visible_) return;
    overlay_visible_ = true;
    cancel_hide_timer();
    if (overlay_) {
        overlay_->set_maximized(IsZoomed(hwnd_) != 0);
        overlay_->set_fullscreen(is_fullscreen_);
        overlay_->set_aspect_locked(aspect_locked_);
        overlay_->show();
    }
}

void MirrorWindow::hide_overlay() {
    if (!overlay_visible_) return;
    overlay_visible_ = false;
    cancel_hide_timer();
    if (overlay_) {
        overlay_->hide();
    }
}

void MirrorWindow::start_hide_timer() {
    SetTimer(hwnd_, constants::kOverlayHideTimerId,
             constants::kOverlayHideDelayMs, nullptr);
}

void MirrorWindow::cancel_hide_timer() {
    KillTimer(hwnd_, constants::kOverlayHideTimerId);
}

// ---------------------------------------------------------------------------
// Resize Helpers
// ---------------------------------------------------------------------------

void MirrorWindow::resize_children() {
    RECT client{};
    GetClientRect(hwnd_, &client);

    if (video_child_) {
        SetWindowPos(video_child_, nullptr,
                     0, 0, client.right, client.bottom,
                     SWP_NOZORDER);
    }
    if (overlay_) {
        overlay_->reposition(client.right);
    }
}

void MirrorWindow::enforce_aspect_ratio(RECT& rect, WPARAM edge) const {
    // Compute frame overhead from a zero-origin rect
    const DWORD style = static_cast<DWORD>(GetWindowLong(hwnd_, GWL_STYLE));
    const DWORD ex_style = static_cast<DWORD>(GetWindowLong(hwnd_, GWL_EXSTYLE));
    RECT frame{0, 0, 0, 0};
    AdjustWindowRectEx(&frame, style, FALSE, ex_style);
    const int frame_w = (frame.right - frame.left);
    const int frame_h = (frame.bottom - frame.top);

    int client_w = (rect.right - rect.left) - frame_w;
    int client_h = (rect.bottom - rect.top) - frame_h;

    // Enforce minimum
    client_w = (std::max)(client_w, constants::kMinMirrorWidth);
    client_h = (std::max)(client_h, constants::kMinMirrorHeight);

    // Adjust based on which edge/corner is being dragged
    switch (edge) {
        case WMSZ_LEFT:
        case WMSZ_RIGHT:
            // Width changed — adjust height to match
            client_h = static_cast<int>(client_w / aspect_ratio_);
            break;

        case WMSZ_TOP:
        case WMSZ_BOTTOM:
            // Height changed — adjust width to match
            client_w = static_cast<int>(client_h * aspect_ratio_);
            break;

        case WMSZ_TOPLEFT:
        case WMSZ_TOPRIGHT:
        case WMSZ_BOTTOMLEFT:
        case WMSZ_BOTTOMRIGHT:
        default:
            // Corner drag — use the larger dimension as reference
            if (client_w / aspect_ratio_ > client_h) {
                client_h = static_cast<int>(client_w / aspect_ratio_);
            } else {
                client_w = static_cast<int>(client_h * aspect_ratio_);
            }
            break;
    }

    const int new_w = client_w + frame_w;
    const int new_h = client_h + frame_h;

    // Apply based on anchor edge
    switch (edge) {
        case WMSZ_LEFT:
        case WMSZ_TOPLEFT:
            rect.left = rect.right - new_w;
            rect.bottom = rect.top + new_h;
            break;
        case WMSZ_RIGHT:
        case WMSZ_BOTTOMRIGHT:
            rect.right = rect.left + new_w;
            rect.bottom = rect.top + new_h;
            break;
        case WMSZ_TOP:
        case WMSZ_TOPRIGHT:
            rect.top = rect.bottom - new_h;
            rect.right = rect.left + new_w;
            break;
        case WMSZ_BOTTOM:
        case WMSZ_BOTTOMLEFT:
            rect.bottom = rect.top + new_h;
            rect.left = rect.right - new_w;
            break;
        default:
            rect.right = rect.left + new_w;
            rect.bottom = rect.top + new_h;
            break;
    }
}

void MirrorWindow::snap_to_aspect_ratio() {
    if (!hwnd_) return;

    RECT rect{};
    GetWindowRect(hwnd_, &rect);

    const DWORD style = static_cast<DWORD>(GetWindowLong(hwnd_, GWL_STYLE));
    const DWORD ex_style = static_cast<DWORD>(GetWindowLong(hwnd_, GWL_EXSTYLE));

    // Calculate frame overhead
    RECT frame{0, 0, 0, 0};
    AdjustWindowRectEx(&frame, style, FALSE, ex_style);
    const int frame_w = (frame.right - frame.left);
    const int frame_h = (frame.bottom - frame.top);

    int client_w = (rect.right - rect.left) - frame_w;
    int client_h = (rect.bottom - rect.top) - frame_h;

    // Keep width, adjust height
    const int target_h = static_cast<int>(client_w / aspect_ratio_);
    client_h = target_h;

    // Enforce minimums
    if (client_w < constants::kMinMirrorWidth) {
        client_w = constants::kMinMirrorWidth;
        client_h = static_cast<int>(client_w / aspect_ratio_);
    }
    if (client_h < constants::kMinMirrorHeight) {
        client_h = constants::kMinMirrorHeight;
        client_w = static_cast<int>(client_h * aspect_ratio_);
    }

    SetWindowPos(hwnd_, nullptr,
                 rect.left, rect.top,
                 client_w + frame_w, client_h + frame_h,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

// ---------------------------------------------------------------------------
// Aspect Ratio Lock
// ---------------------------------------------------------------------------

void MirrorWindow::toggle_aspect_lock() {
    aspect_locked_ = !aspect_locked_;
    Logger::info("Aspect ratio lock: {}", aspect_locked_ ? "ON" : "OFF");

    if (aspect_locked_ && !is_fullscreen_) {
        snap_to_aspect_ratio();
    }

    if (overlay_) {
        overlay_->set_aspect_locked(aspect_locked_);
    }
}

// ---------------------------------------------------------------------------
// Fullscreen
// ---------------------------------------------------------------------------

void MirrorWindow::toggle_fullscreen() {
    if (is_fullscreen_) {
        exit_fullscreen();
    } else {
        enter_fullscreen();
    }
}

void MirrorWindow::enter_fullscreen() {
    if (is_fullscreen_) return;

    // Save current state for restoration
    saved_state_.style = GetWindowLong(hwnd_, GWL_STYLE);
    saved_state_.ex_style = GetWindowLong(hwnd_, GWL_EXSTYLE);
    GetWindowRect(hwnd_, &saved_state_.rect);
    saved_state_.was_maximized = (IsZoomed(hwnd_) != 0);

    // Remove resize frame for clean fullscreen
    const LONG new_style = saved_state_.style & ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    SetWindowLong(hwnd_, GWL_STYLE, new_style);

    // Get monitor rect for the monitor containing this window
    HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(monitor, &mi);

    SetWindowPos(hwnd_, HWND_TOP,
                 mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top,
                 SWP_FRAMECHANGED);

    is_fullscreen_ = true;
    if (overlay_) overlay_->set_fullscreen(true);
    Logger::info("Entered fullscreen");
}

void MirrorWindow::exit_fullscreen() {
    if (!is_fullscreen_) return;

    // Restore saved style
    SetWindowLong(hwnd_, GWL_STYLE, saved_state_.style);
    SetWindowLong(hwnd_, GWL_EXSTYLE, saved_state_.ex_style);

    SetWindowPos(hwnd_, nullptr,
                 saved_state_.rect.left, saved_state_.rect.top,
                 saved_state_.rect.right - saved_state_.rect.left,
                 saved_state_.rect.bottom - saved_state_.rect.top,
                 SWP_NOZORDER | SWP_FRAMECHANGED);

    if (saved_state_.was_maximized) {
        ShowWindow(hwnd_, SW_MAXIMIZE);
    }

    is_fullscreen_ = false;
    if (overlay_) overlay_->set_fullscreen(false);
    Logger::info("Exited fullscreen");
}

// ---------------------------------------------------------------------------
// WndProc
// ---------------------------------------------------------------------------

LRESULT CALLBACK MirrorWindow::wnd_proc(HWND hwnd, UINT msg,
                                         WPARAM wp, LPARAM lp) {
    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        SetWindowLongPtr(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }

    auto* self = reinterpret_cast<MirrorWindow*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (self) {
        return self->handle_message(hwnd, msg, wp, lp);
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT MirrorWindow::handle_message(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp) {
    switch (msg) {
        // ------------------------------------------------------------------
        // Size constraints
        // ------------------------------------------------------------------
        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);

            RECT min_rect{0, 0,
                          constants::kMinMirrorWidth,
                          constants::kMinMirrorHeight};
            const DWORD style = static_cast<DWORD>(
                GetWindowLong(hwnd, GWL_STYLE));
            const DWORD ex_style = static_cast<DWORD>(
                GetWindowLong(hwnd, GWL_EXSTYLE));
            AdjustWindowRectEx(&min_rect, style, FALSE, ex_style);

            mmi->ptMinTrackSize.x = min_rect.right - min_rect.left;
            mmi->ptMinTrackSize.y = min_rect.bottom - min_rect.top;
            return 0;
        }

        // ------------------------------------------------------------------
        // Aspect ratio enforcement during resize
        // ------------------------------------------------------------------
        case WM_SIZING: {
            if (aspect_locked_ && !is_fullscreen_) {
                auto* rect = reinterpret_cast<RECT*>(lp);
                enforce_aspect_ratio(*rect, wp);
                return TRUE;
            }
            break;
        }

        // ------------------------------------------------------------------
        // Resize children when window size changes
        // ------------------------------------------------------------------
        case WM_SIZE: {
            resize_children();

            // Update overlay maximize state
            if (overlay_) {
                overlay_->set_maximized(IsZoomed(hwnd) != 0);
            }
            return 0;
        }

        // ------------------------------------------------------------------
        // Reposition popup overlay when parent moves
        // ------------------------------------------------------------------
        case WM_MOVE: {
            // Owned popup doesn't auto-follow its owner on move
            if (overlay_ && overlay_visible_) {
                RECT client{};
                GetClientRect(hwnd, &client);
                overlay_->reposition(client.right);
            }
            return 0;
        }

        // ------------------------------------------------------------------
        // Custom hit testing for borderless window
        // ------------------------------------------------------------------
        case WM_NCHITTEST: {
            const POINT screen_pt{
                static_cast<int>(static_cast<short>(LOWORD(lp))),
                static_cast<int>(static_cast<short>(HIWORD(lp)))
            };
            POINT client_pt = screen_pt;
            ScreenToClient(hwnd, &client_pt);

            RECT client{};
            GetClientRect(hwnd, &client);

            // In fullscreen mode, no resize borders
            if (is_fullscreen_) {
                return HTCLIENT;
            }

            // Resize borders (edges and corners)
            const int bw = kResizeBorderWidth;

            if (client_pt.y < bw) {
                if (client_pt.x < bw) return HTTOPLEFT;
                if (client_pt.x >= client.right - bw) return HTTOPRIGHT;
                return HTTOP;
            }
            if (client_pt.y >= client.bottom - bw) {
                if (client_pt.x < bw) return HTBOTTOMLEFT;
                if (client_pt.x >= client.right - bw) return HTBOTTOMRIGHT;
                return HTBOTTOM;
            }
            if (client_pt.x < bw) return HTLEFT;
            if (client_pt.x >= client.right - bw) return HTRIGHT;

            // If overlay is visible, let it handle clicks
            if (overlay_visible_ && client_pt.y < constants::kOverlayHeight) {
                return HTCLIENT;
            }

            // Top drag region when overlay is hidden
            if (!overlay_visible_ && client_pt.y < kDragRegionHeight) {
                return HTCAPTION;
            }

            return HTCLIENT;
        }

        // ------------------------------------------------------------------
        // Mouse tracking for overlay auto-show/hide
        // ------------------------------------------------------------------
        case WM_MOUSEMOVE: {
            if (!tracking_mouse_) {
                TRACKMOUSEEVENT tme{};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                tracking_mouse_ = true;
            }
            show_overlay();
            cancel_hide_timer();
            return 0;
        }

        case WM_MOUSELEAVE: {
            tracking_mouse_ = false;
            // Check if mouse moved to overlay popup or video child
            POINT cursor{};
            GetCursorPos(&cursor);
            RECT window_rect{};
            GetWindowRect(hwnd, &window_rect);
            bool in_window = PtInRect(&window_rect, cursor) != 0;

            // Also check the overlay popup (it's a separate top-level window)
            if (!in_window && overlay_ && overlay_->hwnd()) {
                RECT overlay_rect{};
                GetWindowRect(overlay_->hwnd(), &overlay_rect);
                in_window = PtInRect(&overlay_rect, cursor) != 0;
            }

            if (in_window) {
                start_hide_timer();
            } else {
                hide_overlay();
            }
            return 0;
        }

        // Custom message from overlay child: mouse is active in overlay
        case constants::kWmOverlayMouseActivity: {
            cancel_hide_timer();
            return 0;
        }

        case WM_TIMER: {
            if (wp == constants::kOverlayHideTimerId) {
                // Verify mouse is truly outside the window
                POINT cursor{};
                GetCursorPos(&cursor);
                RECT window_rect{};
                GetWindowRect(hwnd, &window_rect);

                bool in_window = PtInRect(&window_rect, cursor) != 0;
                if (!in_window && overlay_ && overlay_->hwnd()) {
                    RECT overlay_rect{};
                    GetWindowRect(overlay_->hwnd(), &overlay_rect);
                    in_window = PtInRect(&overlay_rect, cursor) != 0;
                }

                if (!in_window) {
                    hide_overlay();
                    cancel_hide_timer();
                } else {
                    start_hide_timer();
                }
                return 0;
            }
            break;
        }

        // ------------------------------------------------------------------
        // Keyboard shortcuts
        // ------------------------------------------------------------------
        case WM_KEYDOWN: {
            switch (wp) {
                case VK_F11:
                    toggle_fullscreen();
                    return 0;
                case VK_ESCAPE:
                    if (is_fullscreen_) {
                        exit_fullscreen();
                        return 0;
                    }
                    break;
            }
            break;
        }

        // ------------------------------------------------------------------
        // Window close / destroy
        // ------------------------------------------------------------------
        case WM_CLOSE: {
            if (is_fullscreen_) {
                exit_fullscreen();
            }
            DestroyWindow(hwnd);
            return 0;
        }

        case WM_DESTROY: {
            overlay_.reset();
            video_child_ = nullptr;
            hwnd_ = nullptr;

            if (app_hwnd_) {
                PostMessage(app_hwnd_,
                            constants::kWmMirrorWindowClosed, 0, 0);
            } else {
                PostQuitMessage(0);
            }
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

} // namespace reflection
