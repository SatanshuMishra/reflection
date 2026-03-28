// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cstdint>
#include <functional>
#include <string>

// Forward-declare GDI+ types to avoid including gdiplus.h in this header.
namespace Gdiplus { class Graphics; }

namespace reflection {

/// Button identifiers for the overlay toolbar.
enum class OverlayButton {
    none,
    close,
    maximize,
    minimize,
    fullscreen,
    aspect_lock,
};

/// Hover-activated overlay toolbar for the mirror window.
///
/// Architecture: owned WS_POPUP window with WS_EX_LAYERED for per-pixel
/// alpha transparency. Renders a gradient-faded dark bar at the top of
/// the mirror window with device name (left) and control buttons (right).
///
/// The popup is owned by the MirrorWindow HWND, which means:
///   - It stays above the owner in z-order
///   - It hides when the owner is minimized
///   - It is destroyed when the owner is destroyed
///   - It does NOT appear in Alt+Tab (WS_EX_TOOLWINDOW)
///   - It does NOT steal focus (WS_EX_NOACTIVATE)
///
/// WS_EX_LAYERED on a top-level popup works universally (Vista+),
/// including under RDP and virtual GPU drivers — unlike WS_EX_LAYERED
/// on child windows which fails in those environments.
///
/// Show/hide uses a timer-driven alpha ramp (200ms fade) via
/// SourceConstantAlpha in UpdateLayeredWindow's BLENDFUNCTION.
class MirrorOverlay {
public:
    using ButtonCallback = std::function<void()>;

    MirrorOverlay();
    ~MirrorOverlay();

    MirrorOverlay(const MirrorOverlay&) = delete;
    MirrorOverlay& operator=(const MirrorOverlay&) = delete;

    /// Create the overlay popup window owned by the parent mirror window.
    /// @param parent       Owner MirrorWindow HWND
    /// @param instance     Application HINSTANCE
    /// @param device_name  Name of the mirrored device (e.g., "iPad Air")
    [[nodiscard]] bool create(HWND parent, HINSTANCE instance,
                               const std::wstring& device_name);

    /// Destroy the overlay window.
    void destroy();

    /// Show the overlay with fade-in animation.
    void show();

    /// Hide the overlay with fade-out animation.
    void hide();

    [[nodiscard]] bool is_visible() const;

    /// Reposition the overlay to align with the parent's current position.
    /// Called when the parent moves, resizes, or changes DPI.
    void reposition(int parent_width);

    /// Update display state for button icon changes.
    void set_maximized(bool maximized);
    void set_fullscreen(bool fullscreen);
    void set_aspect_locked(bool locked);

    /// Set callbacks for each button.
    void set_close_callback(ButtonCallback cb);
    void set_minimize_callback(ButtonCallback cb);
    void set_maximize_callback(ButtonCallback cb);
    void set_fullscreen_callback(ButtonCallback cb);
    void set_aspect_lock_callback(ButtonCallback cb);

    /// Get the overlay HWND (for mouse tracking coordination).
    [[nodiscard]] HWND hwnd() const { return hwnd_; }

private:
    HWND hwnd_ = nullptr;
    HWND parent_ = nullptr;
    std::wstring device_name_;

    // Button state
    bool is_maximized_ = false;
    bool is_fullscreen_ = false;
    bool is_aspect_locked_ = false;
    OverlayButton hovered_button_ = OverlayButton::none;

    // Fade animation state
    uint8_t current_alpha_ = 0;    // Current opacity (0=invisible, 255=full)
    uint8_t target_alpha_ = 0;     // Target opacity for animation
    bool animating_ = false;        // Fade timer is running

    // Callbacks
    ButtonCallback on_close_;
    ButtonCallback on_minimize_;
    ButtonCallback on_maximize_;
    ButtonCallback on_fullscreen_;
    ButtonCallback on_aspect_lock_;

    // DPI scaling
    [[nodiscard]] float dpi_scale() const;

    // Rendering — layered window with per-pixel alpha (UpdateLayeredWindow)
    void update_layered_surface();
    void paint_to_graphics(Gdiplus::Graphics& gfx, int w, int h) const;
    void draw_device_name(Gdiplus::Graphics& gfx) const;
    void draw_buttons(Gdiplus::Graphics& gfx) const;
    void draw_button_icon(Gdiplus::Graphics& gfx, OverlayButton button,
                           const RECT& btn_rect) const;

    // Hit testing
    [[nodiscard]] OverlayButton hit_test(int x, int y) const;
    [[nodiscard]] RECT button_rect(OverlayButton button) const;
    [[nodiscard]] int button_index(OverlayButton button) const;

    // Scaled overlay height for current DPI
    [[nodiscard]] int scaled_overlay_height() const;
    [[nodiscard]] int scaled_button_width() const;

    // Message handling
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp);
    LRESULT handle_message(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    // Track mouse for hover effects within the overlay
    bool tracking_mouse_ = false;
    int width_ = 0;
};

} // namespace reflection
