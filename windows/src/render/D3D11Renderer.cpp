#include "render/D3D11Renderer.h"
#include "render/VideoShaders.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"

#include <d3dcompiler.h>
#include <algorithm>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace reflection {

D3D11Renderer::D3D11Renderer() = default;
D3D11Renderer::~D3D11Renderer() { shutdown(); }

bool D3D11Renderer::init(HWND hwnd, int width, int height) {
    if (initialized_) return true;
    hwnd_ = hwnd;
    window_width_ = width;
    window_height_ = height;

    Logger::info("Initializing D3D11 renderer: {}x{}", width, height);

    // Describe the swap chain (double-buffered, flip model for Win10+)
    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 2;
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
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    // Create device, context, and swap chain
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
        Logger::warn("D3D11 flip model failed (0x{:08X}), trying legacy swap effect", hr);

        // Fall back to legacy swap effect (works on older Windows)
        scd.BufferCount = 1;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
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
            // Last resort: WARP software renderer
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
    }

    Logger::info("D3D11 device created — feature level: 0x{:X}",
                 static_cast<unsigned int>(feature_level));

    // Create the render target from the back buffer
    if (!create_render_target()) {
        return false;
    }

    // Initialize video shader pipeline
    if (!init_video_pipeline()) {
        Logger::error("Failed to initialize video pipeline");
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

bool D3D11Renderer::init_video_pipeline() {
    // Compile vertex shader
    Microsoft::WRL::ComPtr<ID3DBlob> vs_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;

    HRESULT hr = D3DCompile(
        shaders::kVideoVertexShader,
        strlen(shaders::kVideoVertexShader),
        "VideoVS",
        nullptr, nullptr,
        "main", "vs_4_0",
        D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
        vs_blob.GetAddressOf(),
        error_blob.GetAddressOf());

    if (FAILED(hr)) {
        if (error_blob) {
            Logger::error("VS compile error: {}",
                          static_cast<const char*>(error_blob->GetBufferPointer()));
        }
        return false;
    }

    hr = device_->CreateVertexShader(
        vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
        nullptr, video_vs_.GetAddressOf());
    if (FAILED(hr)) {
        Logger::error("CreateVertexShader failed: 0x{:08X}", hr);
        return false;
    }

    // Compile pixel shader
    Microsoft::WRL::ComPtr<ID3DBlob> ps_blob;
    error_blob.Reset();

    hr = D3DCompile(
        shaders::kVideoPixelShader,
        strlen(shaders::kVideoPixelShader),
        "VideoPS",
        nullptr, nullptr,
        "main", "ps_4_0",
        D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
        ps_blob.GetAddressOf(),
        error_blob.GetAddressOf());

    if (FAILED(hr)) {
        if (error_blob) {
            Logger::error("PS compile error: {}",
                          static_cast<const char*>(error_blob->GetBufferPointer()));
        }
        return false;
    }

    hr = device_->CreatePixelShader(
        ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(),
        nullptr, video_ps_.GetAddressOf());
    if (FAILED(hr)) {
        Logger::error("CreatePixelShader failed: 0x{:08X}", hr);
        return false;
    }

    // Create sampler state (bilinear filtering)
    D3D11_SAMPLER_DESC sampler_desc{};
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

    hr = device_->CreateSamplerState(&sampler_desc, sampler_.GetAddressOf());
    if (FAILED(hr)) {
        Logger::error("CreateSamplerState failed: 0x{:08X}", hr);
        return false;
    }

    Logger::info("Video shader pipeline initialized");
    return true;
}

bool D3D11Renderer::ensure_staging_texture(int width, int height) {
    if (staging_nv12_ && staging_width_ == width && staging_height_ == height) {
        return true;  // Already the right size
    }

    staging_nv12_.Reset();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_NV12;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = device_->CreateTexture2D(
        &desc, nullptr, staging_nv12_.GetAddressOf());
    if (FAILED(hr)) {
        Logger::error("Failed to create staging NV12 texture: 0x{:08X}", hr);
        return false;
    }

    staging_width_ = width;
    staging_height_ = height;
    Logger::debug("Staging NV12 texture created: {}x{}", width, height);
    return true;
}

void D3D11Renderer::set_letterbox_viewport(int video_width, int video_height) {
    if (window_width_ <= 0 || window_height_ <= 0) return;
    if (video_width <= 0 || video_height <= 0) return;

    const float window_aspect = static_cast<float>(window_width_) / window_height_;
    const float video_aspect = static_cast<float>(video_width) / video_height;

    D3D11_VIEWPORT vp{};
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    if (video_aspect > window_aspect) {
        // Video is wider — pillarbox (bars on top/bottom)
        vp.Width = static_cast<float>(window_width_);
        vp.Height = vp.Width / video_aspect;
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = (window_height_ - vp.Height) / 2.0f;
    } else {
        // Video is taller — letterbox (bars on left/right)
        vp.Height = static_cast<float>(window_height_);
        vp.Width = vp.Height * video_aspect;
        vp.TopLeftX = (window_width_ - vp.Width) / 2.0f;
        vp.TopLeftY = 0.0f;
    }

    context_->RSSetViewports(1, &vp);
}

void D3D11Renderer::resize(int width, int height) {
    if (!initialized_ || !swap_chain_) return;
    if (width <= 0 || height <= 0) return;

    Logger::debug("D3D11Renderer::resize {}x{}", width, height);
    window_width_ = width;
    window_height_ = height;

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
}

void D3D11Renderer::render_frame() {
    if (!initialized_ || !render_target_) return;

    // Set render target and clear to background color
    context_->OMSetRenderTargets(1, render_target_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(render_target_.Get(), constants::kBgClearColor);

    // Set full viewport
    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(window_width_);
    vp.Height = static_cast<float>(window_height_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);

    // Present immediately (no VSync wait). DWM handles VSync for windowed
    // apps, so Present(0, 0) submits the frame to the compositor and returns
    // without blocking. Present(1, 0) would block up to 16ms per call,
    // starving the Win32 message pump and causing "Not Responding".
    swap_chain_->Present(0, 0);
}

void D3D11Renderer::render_video_frame(
    ID3D11Texture2D* nv12_texture, int video_width, int video_height
) {
    if (!initialized_ || !render_target_ || !nv12_texture) return;
    if (!video_vs_ || !video_ps_ || !sampler_) return;

    // Determine which texture to create SRVs from
    ID3D11Texture2D* srv_source = nv12_texture;

    // Check if the source texture has BIND_SHADER_RESOURCE
    D3D11_TEXTURE2D_DESC src_desc{};
    nv12_texture->GetDesc(&src_desc);

    if (!(src_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)) {
        // MFT decoder textures often lack BIND_SHADER_RESOURCE.
        // Copy to our staging texture that has it.
        if (!ensure_staging_texture(video_width, video_height)) {
            return;
        }
        context_->CopyResource(staging_nv12_.Get(), nv12_texture);
        srv_source = staging_nv12_.Get();
    }

    // Create SRVs for Y and UV planes of the NV12 texture
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_y;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_uv;

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc_y{};
    srv_desc_y.Format = DXGI_FORMAT_R8_UNORM;
    srv_desc_y.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc_y.Texture2D.MipLevels = 1;

    HRESULT hr = device_->CreateShaderResourceView(
        srv_source, &srv_desc_y, srv_y.GetAddressOf());
    if (FAILED(hr)) {
        Logger::debug("Failed to create Y plane SRV: 0x{:08X}", hr);
        render_frame();  // Fall back to blank frame
        return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc_uv{};
    srv_desc_uv.Format = DXGI_FORMAT_R8G8_UNORM;
    srv_desc_uv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc_uv.Texture2D.MipLevels = 1;

    hr = device_->CreateShaderResourceView(
        srv_source, &srv_desc_uv, srv_uv.GetAddressOf());
    if (FAILED(hr)) {
        Logger::debug("Failed to create UV plane SRV: 0x{:08X}", hr);
        render_frame();
        return;
    }

    // Set render target and clear background (for letterbox bars)
    context_->OMSetRenderTargets(1, render_target_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(render_target_.Get(), constants::kBgClearColor);

    // Set viewport with letterbox/pillarbox for aspect ratio
    set_letterbox_viewport(video_width, video_height);

    // Set primitive topology (triangle list for fullscreen triangle)
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->IASetInputLayout(nullptr);  // No input layout needed

    // Bind shaders
    context_->VSSetShader(video_vs_.Get(), nullptr, 0);
    context_->PSSetShader(video_ps_.Get(), nullptr, 0);

    // Bind textures and sampler
    ID3D11ShaderResourceView* srvs[] = { srv_y.Get(), srv_uv.Get() };
    context_->PSSetShaderResources(0, 2, srvs);
    context_->PSSetSamplers(0, 1, sampler_.GetAddressOf());

    // Draw fullscreen triangle (3 vertices, no vertex buffer)
    context_->Draw(3, 0);

    // Unbind SRVs to avoid resource hazards
    ID3D11ShaderResourceView* null_srvs[] = { nullptr, nullptr };
    context_->PSSetShaderResources(0, 2, null_srvs);

    // Present immediately — DWM handles VSync for windowed apps.
    swap_chain_->Present(0, 0);
}

void D3D11Renderer::shutdown() {
    if (!initialized_) return;
    Logger::info("Shutting down D3D11 renderer");

    // Release video pipeline
    sampler_.Reset();
    video_ps_.Reset();
    video_vs_.Reset();
    staging_nv12_.Reset();

    // Release core resources in reverse order
    render_target_.Reset();
    swap_chain_.Reset();
    context_.Reset();
    device_.Reset();

    staging_width_ = 0;
    staging_height_ = 0;
    window_width_ = 0;
    window_height_ = 0;
    initialized_ = false;
}

} // namespace reflection
