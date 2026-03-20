#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>  // ComPtr

namespace reflection {

/// Direct3D 11 renderer for displaying decoded video frames.
/// Creates a swap chain associated with the mirror window HWND
/// and renders NV12 textures from the Media Foundation decoder.
///
/// Currently implements: device creation, swap chain, clear+present.
/// TODO (Milestone 3): NV12→RGB shader, texture upload, video rendering.
class D3D11Renderer {
public:
    D3D11Renderer();
    ~D3D11Renderer();

    /// Initialize D3D11 device, swap chain, and render target for the given window.
    bool init(HWND hwnd, int width, int height);

    /// Resize the swap chain (called on window resize).
    void resize(int width, int height);

    /// Render a frame. Clears to background color and presents.
    void render_frame();

    /// Shut down and release all D3D resources.
    void shutdown();

private:
    HWND hwnd_ = nullptr;
    bool initialized_ = false;

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_;

    /// Create the render target view from the swap chain back buffer.
    bool create_render_target();
};

} // namespace reflection
