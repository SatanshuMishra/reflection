// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "views/MirrorOverlay.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"

// GDI+ requires Windows headers + objidl.h (for IStream) before gdiplus.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cwchar>

namespace reflection {

namespace {

constexpr std::wstring_view kOverlayWindowClass = L"ReflectionMirrorOverlayClass";

// Button count and order (right-to-left): Close, Maximize, Minimize, FullScreen, Lock
constexpr int kButtonCount = 5;

// Icon drawing colors (ARGB)
constexpr Gdiplus::ARGB kIconColor = 0xFFFFFFFF;       // White
constexpr Gdiplus::ARGB kIconColorDim = 0xFFAAAAAA;     // Dimmed white

/// Convert a BGR COLORREF to a GDI+ Color.
/// COLORREF stores bytes as 0x00BBGGRR; GDI+ Color expects (A, R, G, B).
Gdiplus::Color colorref_to_gdiplus(uint32_t colorref, BYTE alpha = 255) {
    return Gdiplus::Color(
        alpha,
        static_cast<BYTE>(colorref & 0xFF),          // R
        static_cast<BYTE>((colorref >> 8) & 0xFF),   // G
        static_cast<BYTE>((colorref >> 16) & 0xFF));  // B
}

} // anonymous namespace

MirrorOverlay::MirrorOverlay() = default;

MirrorOverlay::~MirrorOverlay() {
    destroy();
}

// ---------------------------------------------------------------------------
// DPI Scaling
// ---------------------------------------------------------------------------

float MirrorOverlay::dpi_scale() const {
    if (!parent_) return 1.0f;
    const UINT dpi = GetDpiForWindow(parent_);
    return (dpi > 0) ? static_cast<float>(dpi) / 96.0f : 1.0f;
}

int MirrorOverlay::scaled_overlay_height() const {
    return static_cast<int>(constants::kOverlayHeight * dpi_scale());
}

int MirrorOverlay::scaled_button_width() const {
    return static_cast<int>(constants::kOverlayButtonWidth * dpi_scale());
}

// ---------------------------------------------------------------------------
// Window Lifecycle
// ---------------------------------------------------------------------------

bool MirrorOverlay::create(HWND parent, HINSTANCE instance) {
    parent_ = parent;
    session_start_ = std::chrono::steady_clock::now();

    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;  // Layered window — no background brush
    wc.lpszClassName = kOverlayWindowClass.data();
    if (!RegisterClassEx(&wc) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Logger::error("Failed to register overlay window class");
        return false;
    }

    RECT parent_rect{};
    GetClientRect(parent, &parent_rect);
    width_ = parent_rect.right;

    // Calculate screen position from parent's client area origin
    POINT screen_pos{0, 0};
    ClientToScreen(parent, &screen_pos);

    const int overlay_h = scaled_overlay_height();

    // Owned WS_POPUP with WS_EX_LAYERED for per-pixel alpha transparency.
    // WS_EX_LAYERED on top-level popups works universally (Vista+),
    // including under RDP — unlike WS_EX_LAYERED on child windows.
    // WS_EX_TOOLWINDOW: excluded from Alt+Tab and taskbar.
    // WS_EX_NOACTIVATE: never steals focus from the mirror window.
    hwnd_ = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kOverlayWindowClass.data(),
        nullptr,
        WS_POPUP,
        screen_pos.x, screen_pos.y,
        width_, overlay_h,
        parent,      // owner — makes this an owned popup
        nullptr, instance, this
    );

    if (!hwnd_) {
        Logger::error("Failed to create mirror overlay popup (error={})",
                      GetLastError());
        return false;
    }

    // Start the 1-second stopwatch timer for session duration display
    SetTimer(hwnd_, constants::kStopwatchTimerId,
             constants::kStopwatchIntervalMs, nullptr);

    // Start hidden — shown on mouse hover via show()
    return true;
}

void MirrorOverlay::destroy() {
    if (hwnd_ && IsWindow(hwnd_)) {
        KillTimer(hwnd_, constants::kStopwatchTimerId);
        KillTimer(hwnd_, constants::kOverlayFadeTimerId);
        DestroyWindow(hwnd_);
    }
    hwnd_ = nullptr;
    animating_ = false;
}

// ---------------------------------------------------------------------------
// Show/Hide with Fade Animation
// ---------------------------------------------------------------------------

void MirrorOverlay::show() {
    if (!hwnd_) return;

    target_alpha_ = 255;

    if (!IsWindowVisible(hwnd_)) {
        current_alpha_ = 0;

        // Position at the parent's client area top-left (screen coords)
        POINT screen_pos{0, 0};
        ClientToScreen(parent_, &screen_pos);
        const int overlay_h = scaled_overlay_height();

        SetWindowPos(hwnd_, HWND_TOP,
                     screen_pos.x, screen_pos.y,
                     width_, overlay_h,
                     SWP_SHOWWINDOW | SWP_NOACTIVATE);
        update_layered_surface();
    }

    // Start fade-in timer if not already at target
    if (current_alpha_ != target_alpha_) {
        animating_ = true;
        SetTimer(hwnd_, constants::kOverlayFadeTimerId,
                 constants::kOverlayFadeStepMs, nullptr);
    }
}

void MirrorOverlay::hide() {
    if (!hwnd_) return;

    target_alpha_ = 0;
    hovered_button_ = OverlayButton::none;
    tracking_mouse_ = false;

    if (current_alpha_ == 0) {
        // Already invisible
        ShowWindow(hwnd_, SW_HIDE);
        animating_ = false;
        return;
    }

    // Start fade-out timer
    animating_ = true;
    SetTimer(hwnd_, constants::kOverlayFadeTimerId,
             constants::kOverlayFadeStepMs, nullptr);
}

bool MirrorOverlay::is_visible() const {
    return hwnd_ && IsWindowVisible(hwnd_);
}

// ---------------------------------------------------------------------------
// Positioning
// ---------------------------------------------------------------------------

void MirrorOverlay::reposition(int parent_width) {
    width_ = parent_width;
    if (!hwnd_ || !parent_) return;

    // Convert parent's client (0,0) to screen coordinates
    POINT screen_pos{0, 0};
    ClientToScreen(parent_, &screen_pos);
    const int overlay_h = scaled_overlay_height();

    SetWindowPos(hwnd_, HWND_TOP,
                 screen_pos.x, screen_pos.y,
                 parent_width, overlay_h,
                 SWP_NOACTIVATE);

    if (IsWindowVisible(hwnd_)) {
        update_layered_surface();
    }
}

// ---------------------------------------------------------------------------
// State Updates
// ---------------------------------------------------------------------------

void MirrorOverlay::set_maximized(bool maximized) {
    is_maximized_ = maximized;
    if (is_visible()) update_layered_surface();
}

void MirrorOverlay::set_fullscreen(bool fullscreen) {
    is_fullscreen_ = fullscreen;
    if (is_visible()) update_layered_surface();
}

void MirrorOverlay::set_aspect_locked(bool locked) {
    is_aspect_locked_ = locked;
    if (is_visible()) update_layered_surface();
}

void MirrorOverlay::set_close_callback(ButtonCallback cb) {
    on_close_ = std::move(cb);
}

void MirrorOverlay::set_minimize_callback(ButtonCallback cb) {
    on_minimize_ = std::move(cb);
}

void MirrorOverlay::set_maximize_callback(ButtonCallback cb) {
    on_maximize_ = std::move(cb);
}

void MirrorOverlay::set_fullscreen_callback(ButtonCallback cb) {
    on_fullscreen_ = std::move(cb);
}

void MirrorOverlay::set_aspect_lock_callback(ButtonCallback cb) {
    on_aspect_lock_ = std::move(cb);
}

// ---------------------------------------------------------------------------
// Layered Window Rendering
// ---------------------------------------------------------------------------

void MirrorOverlay::update_layered_surface() {
    if (!hwnd_) return;

    const int w = width_;
    const int h = scaled_overlay_height();
    if (w <= 0 || h <= 0) return;

    // 32-bit top-down ARGB DIB section for per-pixel alpha.
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;  // negative = top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC screen_dc = GetDC(nullptr);
    HDC mem_dc = CreateCompatibleDC(screen_dc);
    HBITMAP dib = CreateDIBSection(screen_dc, &bmi, DIB_RGB_COLORS,
                                   &bits, nullptr, 0);
    ReleaseDC(nullptr, screen_dc);

    if (!dib || !bits) {
        DeleteDC(mem_dc);
        return;
    }

    std::memset(bits, 0, static_cast<size_t>(w * h * 4));

    HBITMAP old_bmp = static_cast<HBITMAP>(SelectObject(mem_dc, dib));

    // GDI+ Bitmap wrapping the DIB for premultiplied alpha output.
    // PixelFormat32bppPARGB = premultiplied ARGB, which is what
    // UpdateLayeredWindow with AC_SRC_ALPHA expects.
    {
        Gdiplus::Bitmap bmp(w, h, w * 4,
                            PixelFormat32bppPARGB,
                            static_cast<BYTE*>(bits));

        Gdiplus::Graphics gfx(&bmp);
        gfx.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        gfx.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

        paint_to_graphics(gfx, w, h);
    }

    // Screen position from the overlay's own window rect
    RECT wnd_rect{};
    GetWindowRect(hwnd_, &wnd_rect);
    POINT dst_pos{wnd_rect.left, wnd_rect.top};

    SIZE wnd_size{w, h};
    POINT src_pos{0, 0};

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = current_alpha_;  // Animated: 0-255
    blend.AlphaFormat = AC_SRC_ALPHA;

    const BOOL result = UpdateLayeredWindow(hwnd_, nullptr, &dst_pos,
                                             &wnd_size, mem_dc, &src_pos,
                                             0, &blend, ULW_ALPHA);
    if (!result) {
        Logger::warn("UpdateLayeredWindow failed (error={})", GetLastError());
    }

    SelectObject(mem_dc, old_bmp);
    DeleteObject(dib);
    DeleteDC(mem_dc);
}

void MirrorOverlay::paint_to_graphics(Gdiplus::Graphics& gfx,
                                       int w, int h) const {
    // Glass-like gradient: semi-opaque at top → fully transparent at bottom
    const Gdiplus::LinearGradientBrush bg_brush(
        Gdiplus::PointF(0.0f, 0.0f),
        Gdiplus::PointF(0.0f, static_cast<Gdiplus::REAL>(h)),
        Gdiplus::Color(constants::kOverlayBgAlpha,
                        constants::kOverlayBgR,
                        constants::kOverlayBgG,
                        constants::kOverlayBgB),
        Gdiplus::Color(0x00,
                        constants::kOverlayBgR,
                        constants::kOverlayBgG,
                        constants::kOverlayBgB));
    gfx.FillRectangle(&bg_brush, 0, 0, w, h);

    draw_session_timer(gfx);
    draw_buttons(gfx);
}

void MirrorOverlay::draw_session_timer(Gdiplus::Graphics& gfx) const {
    const float scale = dpi_scale();

    const Gdiplus::FontFamily family(L"Segoe UI");
    const Gdiplus::Font font(&family,
        constants::kOverlayTextSize * scale,
        Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    // Slightly dimmed white for the timer (less prominent than device name was)
    const Gdiplus::SolidBrush text_brush(Gdiplus::Color(200, 255, 255, 255));

    Gdiplus::StringFormat fmt;
    fmt.SetAlignment(Gdiplus::StringAlignmentNear);
    fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    const float padding = constants::kOverlayPaddingX * scale;
    const float buttons_w = kButtonCount * constants::kOverlayButtonWidth * scale;
    const Gdiplus::RectF text_rect(
        padding, 0.0f,
        static_cast<Gdiplus::REAL>(width_) - buttons_w - padding * 2.0f,
        static_cast<Gdiplus::REAL>(scaled_overlay_height()));

    // Calculate elapsed session duration
    const auto elapsed = std::chrono::steady_clock::now() - session_start_;
    const auto total_sec = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    const int hours = static_cast<int>(total_sec / 3600);
    const int minutes = static_cast<int>((total_sec % 3600) / 60);
    const int seconds = static_cast<int>(total_sec % 60);

    wchar_t time_str[16]{};
    if (hours > 0) {
        std::swprintf(time_str, std::size(time_str), L"%d:%02d:%02d", hours, minutes, seconds);
    } else {
        std::swprintf(time_str, std::size(time_str), L"%02d:%02d", minutes, seconds);
    }

    gfx.DrawString(time_str, -1, &font, text_rect, &fmt, &text_brush);
}

void MirrorOverlay::draw_buttons(Gdiplus::Graphics& gfx) const {
    constexpr OverlayButton buttons[] = {
        OverlayButton::close,
        OverlayButton::maximize,
        OverlayButton::minimize,
        OverlayButton::fullscreen,
        OverlayButton::aspect_lock,
    };

    for (const auto btn : buttons) {
        const RECT r = button_rect(btn);

        if (hovered_button_ == btn) {
            const uint32_t hover_colorref = (btn == OverlayButton::close)
                ? constants::kOverlayCloseHoverColor
                : constants::kOverlayHoverColor;
            const Gdiplus::SolidBrush hover_brush(
                colorref_to_gdiplus(hover_colorref, 0xCC));
            gfx.FillRectangle(&hover_brush,
                               static_cast<INT>(r.left),
                               static_cast<INT>(r.top),
                               static_cast<INT>(r.right - r.left),
                               static_cast<INT>(r.bottom - r.top));
        }

        draw_button_icon(gfx, btn, r);
    }
}

void MirrorOverlay::draw_button_icon(Gdiplus::Graphics& gfx,
                                      OverlayButton button,
                                      const RECT& btn_rect) const {
    const float scale = dpi_scale();
    const float pen_w = std::round(scale);  // 1px at 96dpi, 2px at 144+dpi
    const Gdiplus::Pen pen(Gdiplus::Color(kIconColor), pen_w);

    const float cx = (btn_rect.left + btn_rect.right) / 2.0f;
    const float cy = (btn_rect.top + btn_rect.bottom) / 2.0f;

    switch (button) {
        case OverlayButton::close: {
            const float s = 5.0f * scale;
            gfx.DrawLine(&pen, cx - s, cy - s, cx + s, cy + s);
            gfx.DrawLine(&pen, cx + s, cy - s, cx - s, cy + s);
            break;
        }

        case OverlayButton::maximize: {
            if (is_maximized_) {
                const float s = 4.5f * scale;
                const float offset = 2.0f * scale;
                gfx.DrawRectangle(&pen, cx - s + offset, cy - s - offset,
                                   s * 2 - offset, s * 2 - offset);
                const Gdiplus::SolidBrush fill(Gdiplus::Color(
                    0xB0, constants::kOverlayBgR,
                    constants::kOverlayBgG, constants::kOverlayBgB));
                gfx.FillRectangle(&fill, cx - s, cy - s + offset,
                                   s * 2 - offset, s * 2 - offset);
                gfx.DrawRectangle(&pen, cx - s, cy - s + offset,
                                   s * 2 - offset, s * 2 - offset);
            } else {
                const float s = 5.0f * scale;
                gfx.DrawRectangle(&pen, cx - s, cy - s, s * 2, s * 2);
            }
            break;
        }

        case OverlayButton::minimize: {
            const float s = 5.0f * scale;
            gfx.DrawLine(&pen, cx - s, cy, cx + s, cy);
            break;
        }

        case OverlayButton::fullscreen: {
            const float s = (is_fullscreen_ ? 5.0f : 6.0f) * scale;
            const float a = 3.0f * scale;
            if (is_fullscreen_) {
                gfx.DrawLine(&pen, cx - s, cy - s, cx - s + a, cy - s);
                gfx.DrawLine(&pen, cx - s, cy - s, cx - s, cy - s + a);
                gfx.DrawLine(&pen, cx + s, cy - s, cx + s - a, cy - s);
                gfx.DrawLine(&pen, cx + s, cy - s, cx + s, cy - s + a);
                gfx.DrawLine(&pen, cx - s, cy + s, cx - s + a, cy + s);
                gfx.DrawLine(&pen, cx - s, cy + s, cx - s, cy + s - a);
                gfx.DrawLine(&pen, cx + s, cy + s, cx + s - a, cy + s);
                gfx.DrawLine(&pen, cx + s, cy + s, cx + s, cy + s - a);
            } else {
                gfx.DrawLine(&pen, cx - s, cy - s + a, cx - s, cy - s);
                gfx.DrawLine(&pen, cx - s, cy - s, cx - s + a, cy - s);
                gfx.DrawLine(&pen, cx + s - a, cy - s, cx + s, cy - s);
                gfx.DrawLine(&pen, cx + s, cy - s, cx + s, cy - s + a);
                gfx.DrawLine(&pen, cx - s, cy + s - a, cx - s, cy + s);
                gfx.DrawLine(&pen, cx - s, cy + s, cx - s + a, cy + s);
                gfx.DrawLine(&pen, cx + s - a, cy + s, cx + s, cy + s);
                gfx.DrawLine(&pen, cx + s, cy + s, cx + s, cy + s - a);
            }
            break;
        }

        case OverlayButton::aspect_lock: {
            const Gdiplus::ARGB icon_color = is_aspect_locked_
                ? kIconColor : kIconColorDim;
            const Gdiplus::Pen lock_pen(Gdiplus::Color(icon_color), pen_w);

            const float bw = 5.0f * scale;
            const float bh = 4.0f * scale;
            const float sw = 3.5f * scale;
            const float sh = 4.0f * scale;

            if (is_aspect_locked_) {
                gfx.DrawArc(&lock_pen, cx - sw, cy - bh - sh,
                             sw * 2, sh * 2, 180.0f, 180.0f);
                gfx.DrawLine(&lock_pen, cx - sw, cy - bh,
                              cx - sw, cy - bh + scale);
                gfx.DrawLine(&lock_pen, cx + sw, cy - bh,
                              cx + sw, cy - bh + scale);
                const Gdiplus::SolidBrush body_brush{
                    Gdiplus::Color(icon_color)};
                gfx.FillRectangle(&body_brush,
                                   cx - bw, cy - bh + scale,
                                   bw * 2, bh * 2);
            } else {
                gfx.DrawArc(&lock_pen, cx - sw, cy - bh - sh - 2 * scale,
                             sw * 2, sh * 2, 180.0f, 180.0f);
                gfx.DrawLine(&lock_pen, cx - sw, cy - bh - 2 * scale,
                              cx - sw, cy - bh + scale);
                gfx.DrawRectangle(&lock_pen,
                                   cx - bw, cy - bh + scale,
                                   bw * 2, bh * 2);
            }
            break;
        }

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Hit Testing
// ---------------------------------------------------------------------------

int MirrorOverlay::button_index(OverlayButton button) const {
    switch (button) {
        case OverlayButton::close:       return 0;
        case OverlayButton::maximize:    return 1;
        case OverlayButton::minimize:    return 2;
        case OverlayButton::fullscreen:  return 3;
        case OverlayButton::aspect_lock: return 4;
        default:                         return -1;
    }
}

RECT MirrorOverlay::button_rect(OverlayButton button) const {
    const int idx = button_index(button);
    if (idx < 0) return {};

    const int btn_w = scaled_button_width();
    const int btn_left = width_ - (idx + 1) * btn_w;
    return RECT{
        btn_left, 0,
        btn_left + btn_w,
        scaled_overlay_height()
    };
}

OverlayButton MirrorOverlay::hit_test(int x, int y) const {
    if (y < 0 || y >= scaled_overlay_height()) return OverlayButton::none;

    constexpr OverlayButton buttons[] = {
        OverlayButton::close,
        OverlayButton::maximize,
        OverlayButton::minimize,
        OverlayButton::fullscreen,
        OverlayButton::aspect_lock,
    };

    for (const auto btn : buttons) {
        const RECT r = button_rect(btn);
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) {
            return btn;
        }
    }
    return OverlayButton::none;
}

// ---------------------------------------------------------------------------
// Message Handling
// ---------------------------------------------------------------------------

LRESULT CALLBACK MirrorOverlay::wnd_proc(HWND hwnd, UINT msg,
                                          WPARAM wp, LPARAM lp) {
    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        SetWindowLongPtr(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }

    auto* self = reinterpret_cast<MirrorOverlay*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (self) {
        return self->handle_message(hwnd, msg, wp, lp);
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT MirrorOverlay::handle_message(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;  // Layered window — no GDI background erase

        case WM_TIMER: {
            if (wp == constants::kOverlayFadeTimerId) {
                // Step alpha toward target (~60fps, 200ms total)
                constexpr int step = static_cast<int>(
                    255.0 * constants::kOverlayFadeStepMs
                          / constants::kOverlayFadeDurationMs);

                if (target_alpha_ > current_alpha_) {
                    current_alpha_ = static_cast<uint8_t>(
                        (std::min)(static_cast<int>(current_alpha_) + step, 255));
                } else {
                    current_alpha_ = static_cast<uint8_t>(
                        (std::max)(static_cast<int>(current_alpha_) - step, 0));
                }

                update_layered_surface();

                if (current_alpha_ == target_alpha_) {
                    KillTimer(hwnd_, constants::kOverlayFadeTimerId);
                    animating_ = false;
                    if (current_alpha_ == 0) {
                        ShowWindow(hwnd_, SW_HIDE);
                    }
                }
                return 0;
            }
            if (wp == constants::kStopwatchTimerId) {
                // Repaint the overlay to update the session timer display
                if (current_alpha_ > 0) {
                    update_layered_surface();
                }
                return 0;
            }
            break;
        }

        case WM_MOUSEMOVE: {
            if (!tracking_mouse_) {
                TRACKMOUSEEVENT tme{};
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd_;
                TrackMouseEvent(&tme);
                tracking_mouse_ = true;
            }

            const int x = GET_X_LPARAM(lp);
            const int y = GET_Y_LPARAM(lp);
            const auto new_hover = hit_test(x, y);

            if (new_hover != hovered_button_) {
                hovered_button_ = new_hover;
                update_layered_surface();
            }

            // Notify parent that mouse is active in overlay area
            if (parent_) {
                PostMessage(parent_, constants::kWmOverlayMouseActivity, 0, 0);
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            tracking_mouse_ = false;
            if (hovered_button_ != OverlayButton::none) {
                hovered_button_ = OverlayButton::none;
                update_layered_surface();
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            const int x = GET_X_LPARAM(lp);
            const int y = GET_Y_LPARAM(lp);
            const auto btn = hit_test(x, y);

            switch (btn) {
                case OverlayButton::close:
                    if (on_close_) on_close_();
                    break;
                case OverlayButton::minimize:
                    if (on_minimize_) on_minimize_();
                    break;
                case OverlayButton::maximize:
                    if (on_maximize_) on_maximize_();
                    break;
                case OverlayButton::fullscreen:
                    if (on_fullscreen_) on_fullscreen_();
                    break;
                case OverlayButton::aspect_lock:
                    if (on_aspect_lock_) on_aspect_lock_();
                    break;
                case OverlayButton::none: {
                    // Click on device name area — initiate window drag
                    if (parent_) {
                        POINT screen_pt{x, y};
                        ClientToScreen(hwnd_, &screen_pt);
                        ReleaseCapture();
                        PostMessage(parent_, WM_NCLBUTTONDOWN, HTCAPTION,
                                    MAKELPARAM(screen_pt.x, screen_pt.y));
                    }
                    break;
                }
            }
            return 0;
        }

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

} // namespace reflection
