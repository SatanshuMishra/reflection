#include "views/MirrorWindow.h"
#include "render/D3D11Renderer.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"

namespace reflection {

MirrorWindow::MirrorWindow() = default;

MirrorWindow::~MirrorWindow() {
    close();
}

bool MirrorWindow::create(
    HINSTANCE instance, const std::wstring& title, HWND app_hwnd
) {
    Logger::info("Creating mirror window");
    app_hwnd_ = app_hwnd;

    // Use a null brush — we handle WM_ERASEBKGND ourselves to avoid GDI leaks.
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    wc.lpszClassName = constants::kMirrorWindowClass.data();

    RegisterClassEx(&wc);

    // Create window
    hwnd_ = CreateWindowEx(
        0,
        constants::kMirrorWindowClass.data(),
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        constants::kDefaultMirrorWidth, constants::kDefaultMirrorHeight,
        nullptr, nullptr, instance, this
    );

    if (!hwnd_) {
        Logger::error("Failed to create mirror window");
        return false;
    }

    // Initialize D3D11 renderer for this window
    renderer_ = std::make_unique<D3D11Renderer>();
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const int client_width = rc.right - rc.left;
    const int client_height = rc.bottom - rc.top;

    if (!renderer_->init(hwnd_, client_width, client_height)) {
        Logger::error("Failed to initialize D3D11 renderer for mirror window");
        renderer_.reset();
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    Logger::info("Mirror window created with D3D11 renderer");
    return true;
}

void MirrorWindow::close() {
    // Shut down renderer before destroying the window
    if (renderer_) {
        renderer_->shutdown();
        renderer_.reset();
    }

    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

LRESULT CALLBACK MirrorWindow::wnd_proc(
    HWND hwnd, UINT msg, WPARAM wp, LPARAM lp
) {
    switch (msg) {
        case WM_CREATE: {
            // Store 'this' pointer from CreateWindowEx lpParam
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
            SetWindowLongPtr(hwnd, GWLP_USERDATA,
                             reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return 0;
        }

        case WM_ERASEBKGND: {
            // Paint the background with our near-black color.
            // Using GetStockObject(NULL_BRUSH) in WNDCLASSEX + custom erase
            // avoids the GDI brush leak from CreateSolidBrush.
            HDC hdc = reinterpret_cast<HDC>(wp);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH brush = CreateSolidBrush(constants::kBackgroundColor);
            FillRect(hdc, &rc, brush);
            DeleteObject(brush); // No leak — created and destroyed in same scope
            return 1; // We handled it
        }

        case WM_SIZE: {
            auto* self = reinterpret_cast<MirrorWindow*>(
                GetWindowLongPtr(hwnd, GWLP_USERDATA));
            if (self && self->renderer_) {
                const int width = LOWORD(lp);
                const int height = HIWORD(lp);
                if (width > 0 && height > 0) {
                    self->renderer_->resize(width, height);
                }
            }
            return 0;
        }

        case WM_SIZING: {
            // TODO (Milestone 3 follow-up): Enforce aspect ratio
            return TRUE;
        }

        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
            mmi->ptMinTrackSize.x = constants::kMinMirrorWidth;
            mmi->ptMinTrackSize.y = constants::kMinMirrorHeight;
            return 0;
        }

        case WM_DESTROY: {
            auto* self = reinterpret_cast<MirrorWindow*>(
                GetWindowLongPtr(hwnd, GWLP_USERDATA));

            if (self && self->app_hwnd_) {
                // Notify app that the mirror window was closed.
                // App stays running as a tray icon and can accept reconnections.
                PostMessage(self->app_hwnd_,
                            constants::kWmMirrorWindowClosed, 0, 0);
            } else {
                // Fallback: if no app HWND, quit the app
                PostQuitMessage(0);
            }
            return 0;
        }

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}

} // namespace reflection
