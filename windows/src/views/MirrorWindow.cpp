#include "views/MirrorWindow.h"
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

    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = constants::kMirrorWindowClass.data();

    RegisterClassEx(&wc);

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

    Logger::info("Mirror window created (HWND for GStreamer overlay)");
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
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
            SetWindowLongPtr(hwnd, GWLP_USERDATA,
                             reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return 0;
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
                PostMessage(self->app_hwnd_,
                            constants::kWmMirrorWindowClosed, 0, 0);
            } else {
                PostQuitMessage(0);
            }
            return 0;
        }

        default:
            return DefWindowProc(hwnd, msg, wp, lp);
    }
}

} // namespace reflection
