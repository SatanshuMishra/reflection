#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace reflection {

/// Direct3D 11 renderer for displaying decoded video frames.
/// Creates a swap chain associated with the mirror window HWND
/// and renders NV12 textures from the Media Foundation decoder.
///
/// TODO (Milestone 3): Full implementation
class D3D11Renderer {
public:
    D3D11Renderer();
    ~D3D11Renderer();

    /// Initialize D3D11 device, swap chain, and shaders for the given window.
    bool init(HWND hwnd, int width, int height);

    /// Resize the swap chain (called on window resize).
    void resize(int width, int height);

    /// Render a frame. Currently a stub that clears to background color.
    void render_frame();

    /// Shut down and release all D3D resources.
    void shutdown();

private:
    HWND hwnd_ = nullptr;
    bool initialized_ = false;
    // TODO (Milestone 3): ID3D11Device, IDXGISwapChain, shaders, etc.
};

} // namespace reflection
