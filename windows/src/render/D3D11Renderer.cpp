#include "render/D3D11Renderer.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace reflection {

D3D11Renderer::D3D11Renderer() = default;
D3D11Renderer::~D3D11Renderer() { shutdown(); }

bool D3D11Renderer::init(HWND hwnd, int width, int height) {
    if (initialized_) return true;
    hwnd_ = hwnd;

    Logger::info("Initializing D3D11 renderer: {}x{}", width, height);

    // Describe the swap chain
    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = static_cast<UINT>(width);
    scd.BufferDesc.Height = static_cast<UINT>(height);
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    // Create device, context, and swap chain in one call
    D3D_FEATURE_LEVEL feature_level{};
    const D3D_FEATURE_LEVEL requested_levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,                    // Default adapter
        D3D_DRIVER_TYPE_HARDWARE,   // Hardware GPU
        nullptr,                    // No software rasterizer
        flags,
        requested_levels,
        _countof(requested_levels),
        D3D11_SDK_VERSION,
        &scd,
        swap_chain_.GetAddressOf(),
        device_.GetAddressOf(),
        &feature_level,
        context_.GetAddressOf()
    );

    if (FAILED(hr)) {
        Logger::error("D3D11CreateDeviceAndSwapChain failed: 0x{:08X}", hr);

        // Fall back to WARP software renderer (works on all machines)
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            flags,
            requested_levels,
            _countof(requested_levels),
            D3D11_SDK_VERSION,
            &scd,
            swap_chain_.GetAddressOf(),
            device_.GetAddressOf(),
            &feature_level,
            context_.GetAddressOf()
        );

        if (FAILED(hr)) {
            Logger::error("D3D11 WARP fallback also failed: 0x{:08X}", hr);
            return false;
        }
        Logger::warn("Using WARP software renderer (no GPU acceleration)");
    }

    Logger::info("D3D11 device created — feature level: 0x{:X}", feature_level);

    // Create the render target from the back buffer
    if (!create_render_target()) {
        return false;
    }

    initialized_ = true;
    Logger::info("D3D11 renderer initialized");
    return true;
}

bool D3D11Renderer::create_render_target() {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    HRESULT hr = swap_chain_->GetBuffer(
        0, IID_PPV_ARGS(back_buffer.GetAddressOf()));

    if (FAILED(hr)) {
        Logger::error("Failed to get swap chain back buffer: 0x{:08X}", hr);
        return false;
    }

    hr = device_->CreateRenderTargetView(
        back_buffer.Get(), nullptr, render_target_.GetAddressOf());

    if (FAILED(hr)) {
        Logger::error("Failed to create render target view: 0x{:08X}", hr);
        return false;
    }

    return true;
}

void D3D11Renderer::resize(int width, int height) {
    if (!initialized_ || !swap_chain_) return;
    if (width <= 0 || height <= 0) return;

    Logger::debug("D3D11Renderer::resize {}x{}", width, height);

    // Release the render target before resizing
    render_target_.Reset();
    context_->OMSetRenderTargets(0, nullptr, nullptr);

    // Resize the swap chain buffers
    HRESULT hr = swap_chain_->ResizeBuffers(
        0,  // Keep existing buffer count
        static_cast<UINT>(width),
        static_cast<UINT>(height),
        DXGI_FORMAT_UNKNOWN,  // Keep existing format
        0
    );

    if (FAILED(hr)) {
        Logger::error("Failed to resize swap chain: 0x{:08X}", hr);
        return;
    }

    // Recreate the render target
    create_render_target();

    // Update the viewport
    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(width);
    vp.Height = static_cast<float>(height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);
}

void D3D11Renderer::render_frame() {
    if (!initialized_ || !render_target_) return;

    // Set render target and clear to background color
    context_->OMSetRenderTargets(1, render_target_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(render_target_.Get(), constants::kBgClearColor);

    // TODO (Milestone 3): Draw decoded video texture here

    // Present the frame (VSync off for now — 0 = immediate)
    swap_chain_->Present(1, 0);
}

void D3D11Renderer::shutdown() {
    if (!initialized_) return;
    Logger::info("Shutting down D3D11 renderer");

    // Release in reverse order
    render_target_.Reset();
    swap_chain_.Reset();
    context_.Reset();
    device_.Reset();

    initialized_ = false;
}

} // namespace reflection
