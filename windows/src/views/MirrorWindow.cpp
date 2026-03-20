#include "views/MirrorWindow.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"

namespace reflection {

MirrorWindow::MirrorWindow() = default;

MirrorWindow::~MirrorWindow() {
    close();
}

bool MirrorWindow::create(HINSTANCE instance, const std::wstring& title) {
    Logger::info("Creating mirror window");

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

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    Logger::info("Mirror window created");
    return true;
}

void MirrorWindow::close() {
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

        case WM_SIZING: {
            // TODO (Milestone 3): Enforce aspect ratio
            return TRUE;
        }

        case WM_SIZE: {
            // TODO (Milestone 3): Notify D3D11Renderer of resize
            return 0;
        }

        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
            mmi->ptMinTrackSize.x = constants::kMinMirrorWidth;
            mmi->ptMinTrackSize.y = constants::kMinMirrorHeight;
            return 0;
        }

        case WM_DESTROY: {
            // TODO (Milestone 5): Notify MirrorSessionManager of window close
            // For now, closing the mirror window exits the app.
            PostQuitMessage(0);
            return 0;
        }

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}

} // namespace reflection
