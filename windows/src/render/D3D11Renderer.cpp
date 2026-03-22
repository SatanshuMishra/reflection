#include "render/D3D11Renderer.h"
#include "render/VideoShaders.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"

#include <d3dcompiler.h>
#include <algorithm>
#include <cstring>

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

    // Double-buffered swap chain with flip model (Win10+)
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
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        requested_levels, _countof(requested_levels),
        D3D11_SDK_VERSION, &scd,
        swap_chain_.GetAddressOf(), device_.GetAddressOf(),
        &feature_level, context_.GetAddressOf());

    if (FAILED(hr)) {
        Logger::warn("Flip model failed (0x{:08X}), trying legacy", hr);
        scd.BufferCount = 1;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            requested_levels, _countof(requested_levels),
            D3D11_SDK_VERSION, &scd,
            swap_chain_.GetAddressOf(), device_.GetAddressOf(),
            &feature_level, context_.GetAddressOf());

        if (FAILED(hr)) {
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
                requested_levels, _countof(requested_levels),
                D3D11_SDK_VERSION, &scd,
                swap_chain_.GetAddressOf(), device_.GetAddressOf(),
                &feature_level, context_.GetAddressOf());

            if (FAILED(hr)) {
                Logger::error("D3D11 WARP fallback failed: 0x{:08X}", hr);
                return false;
            }
            Logger::warn("Using WARP software renderer");
        }
    }

    Logger::info("D3D11 device created — feature level: 0x{:X}",
                 static_cast<unsigned int>(feature_level));

    if (!create_render_target()) return false;
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
    HRESULT hr = swap_chain_->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf()));
    if (FAILED(hr)) {
        Logger::error("GetBuffer failed: 0x{:08X}", hr);
        return false;
    }

    hr = device_->CreateRenderTargetView(
        back_buffer.Get(), nullptr, render_target_.GetAddressOf());
    if (FAILED(hr)) {
        Logger::error("CreateRenderTargetView failed: 0x{:08X}", hr);
        return false;
    }
    return true;
}

bool D3D11Renderer::init_video_pipeline() {
    // Compile vertex shader
    Microsoft::WRL::ComPtr<ID3DBlob> vs_blob, error_blob;
    HRESULT hr = D3DCompile(
        shaders::kVideoVertexShader, strlen(shaders::kVideoVertexShader),
        "VideoVS", nullptr, nullptr, "main", "vs_4_0",
        D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
        vs_blob.GetAddressOf(), error_blob.GetAddressOf());
    if (FAILED(hr)) {
        if (error_blob) Logger::error("VS: {}", static_cast<const char*>(error_blob->GetBufferPointer()));
        return false;
    }
    hr = device_->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
                                     nullptr, video_vs_.GetAddressOf());
    if (FAILED(hr)) return false;

    // Compile pixel shader
    Microsoft::WRL::ComPtr<ID3DBlob> ps_blob;
    error_blob.Reset();
    hr = D3DCompile(
        shaders::kVideoPixelShader, strlen(shaders::kVideoPixelShader),
        "VideoPS", nullptr, nullptr, "main", "ps_4_0",
        D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
        ps_blob.GetAddressOf(), error_blob.GetAddressOf());
    if (FAILED(hr)) {
        if (error_blob) Logger::error("PS: {}", static_cast<const char*>(error_blob->GetBufferPointer()));
        return false;
    }
    hr = device_->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(),
                                    nullptr, video_ps_.GetAddressOf());
    if (FAILED(hr)) return false;

    // Bilinear sampler
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    hr = device_->CreateSamplerState(&sd, sampler_.GetAddressOf());
    if (FAILED(hr)) return false;

    Logger::info("Video shader pipeline initialized");
    return true;
}

bool D3D11Renderer::ensure_video_textures(int width, int height) {
    if (tex_y_ && video_tex_width_ == width && video_tex_height_ == height) {
        return true;
    }

    // Release old textures and SRVs
    srv_y_.Reset();
    srv_uv_.Reset();
    tex_y_.Reset();
    tex_uv_.Reset();
    staging_read_.Reset();

    // CPU-readable staging texture for reading NV12 data from the decoder
    D3D11_TEXTURE2D_DESC staging_desc{};
    staging_desc.Width = static_cast<UINT>(width);
    staging_desc.Height = static_cast<UINT>(height);
    staging_desc.MipLevels = 1;
    staging_desc.ArraySize = 1;
    staging_desc.Format = DXGI_FORMAT_NV12;
    staging_desc.SampleDesc.Count = 1;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    HRESULT hr = device_->CreateTexture2D(&staging_desc, nullptr, staging_read_.GetAddressOf());
    if (FAILED(hr)) {
        Logger::error("Failed to create staging NV12 texture: 0x{:08X}", hr);
        return false;
    }

    // Y plane texture: R8_UNORM, full resolution
    D3D11_TEXTURE2D_DESC y_desc{};
    y_desc.Width = static_cast<UINT>(width);
    y_desc.Height = static_cast<UINT>(height);
    y_desc.MipLevels = 1;
    y_desc.ArraySize = 1;
    y_desc.Format = DXGI_FORMAT_R8_UNORM;
    y_desc.SampleDesc.Count = 1;
    y_desc.Usage = D3D11_USAGE_DEFAULT;
    y_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    hr = device_->CreateTexture2D(&y_desc, nullptr, tex_y_.GetAddressOf());
    if (FAILED(hr)) {
        Logger::error("Failed to create Y texture: 0x{:08X}", hr);
        return false;
    }

    // UV plane texture: R8G8_UNORM, half resolution
    D3D11_TEXTURE2D_DESC uv_desc{};
    uv_desc.Width = static_cast<UINT>(width / 2);
    uv_desc.Height = static_cast<UINT>(height / 2);
    uv_desc.MipLevels = 1;
    uv_desc.ArraySize = 1;
    uv_desc.Format = DXGI_FORMAT_R8G8_UNORM;
    uv_desc.SampleDesc.Count = 1;
    uv_desc.Usage = D3D11_USAGE_DEFAULT;
    uv_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    hr = device_->CreateTexture2D(&uv_desc, nullptr, tex_uv_.GetAddressOf());
    if (FAILED(hr)) {
        Logger::error("Failed to create UV texture: 0x{:08X}", hr);
        return false;
    }

    // Create SRVs (persistent — reused every frame)
    hr = device_->CreateShaderResourceView(tex_y_.Get(), nullptr, srv_y_.GetAddressOf());
    if (FAILED(hr)) {
        Logger::error("Failed to create Y SRV: 0x{:08X}", hr);
        return false;
    }

    hr = device_->CreateShaderResourceView(tex_uv_.Get(), nullptr, srv_uv_.GetAddressOf());
    if (FAILED(hr)) {
        Logger::error("Failed to create UV SRV: 0x{:08X}", hr);
        return false;
    }

    video_tex_width_ = width;
    video_tex_height_ = height;
    Logger::info("Video textures created: Y={}x{} R8, UV={}x{} R8G8",
                 width, height, width / 2, height / 2);
    return true;
}

bool D3D11Renderer::upload_nv12_planes(
    ID3D11Texture2D* nv12_texture, int width, int height
) {
    // Step 1: Copy NV12 texture to CPU-readable staging
    context_->CopyResource(staging_read_.Get(), nv12_texture);

    // Step 2: Map staging texture for CPU read
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = context_->Map(staging_read_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        Logger::error("Failed to map staging NV12: 0x{:08X}", hr);
        return false;
    }

    const auto* src = static_cast<const uint8_t*>(mapped.pData);
    const UINT src_pitch = mapped.RowPitch;

    // Step 3: Upload Y plane (first width*height bytes, row pitch = src_pitch)
    // UpdateSubresource expects a contiguous source, so we use it row-by-row
    // via a D3D11_BOX to handle potential row padding.
    D3D11_BOX y_box{};
    y_box.left = 0;
    y_box.top = 0;
    y_box.front = 0;
    y_box.right = static_cast<UINT>(width);
    y_box.bottom = 1;
    y_box.back = 1;

    for (int row = 0; row < height; ++row) {
        y_box.top = static_cast<UINT>(row);
        y_box.bottom = static_cast<UINT>(row + 1);
        context_->UpdateSubresource(
            tex_y_.Get(), 0, &y_box,
            src + row * src_pitch,
            static_cast<UINT>(width), 0);
    }

    // Step 4: Upload UV plane (starts after Y plane in NV12 layout)
    // UV plane: width/2 pairs of (U,V) bytes = width bytes per row, height/2 rows
    const auto* uv_src = src + src_pitch * height;
    const int uv_height = height / 2;
    const int uv_width = width / 2;  // In R8G8 texels (each is 2 bytes = one UV pair)

    D3D11_BOX uv_box{};
    uv_box.front = 0;
    uv_box.back = 1;

    for (int row = 0; row < uv_height; ++row) {
        uv_box.left = 0;
        uv_box.top = static_cast<UINT>(row);
        uv_box.right = static_cast<UINT>(uv_width);
        uv_box.bottom = static_cast<UINT>(row + 1);
        context_->UpdateSubresource(
            tex_uv_.Get(), 0, &uv_box,
            uv_src + row * src_pitch,
            static_cast<UINT>(uv_width * 2), 0);  // 2 bytes per R8G8 texel
    }

    context_->Unmap(staging_read_.Get(), 0);
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
        vp.Width = static_cast<float>(window_width_);
        vp.Height = vp.Width / video_aspect;
        vp.TopLeftX = 0.0f;
        vp.TopLeftY = (window_height_ - vp.Height) / 2.0f;
    } else {
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

    render_target_.Reset();
    context_->OMSetRenderTargets(0, nullptr, nullptr);

    HRESULT hr = swap_chain_->ResizeBuffers(
        0, static_cast<UINT>(width), static_cast<UINT>(height),
        DXGI_FORMAT_UNKNOWN, 0);

    if (FAILED(hr)) {
        Logger::error("ResizeBuffers failed: 0x{:08X}", hr);
        return;
    }

    create_render_target();
}

void D3D11Renderer::render_frame() {
    if (!initialized_ || !render_target_) return;

    context_->OMSetRenderTargets(1, render_target_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(render_target_.Get(), constants::kBgClearColor);

    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(window_width_);
    vp.Height = static_cast<float>(window_height_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);

    swap_chain_->Present(0, 0);
}

void D3D11Renderer::render_video_frame(
    ID3D11Texture2D* nv12_texture, int video_width, int video_height
) {
    if (!initialized_ || !render_target_ || !nv12_texture) return;
    if (!video_vs_ || !video_ps_ || !sampler_) return;

    // Ensure Y and UV textures exist at the right resolution
    if (!ensure_video_textures(video_width, video_height)) {
        render_frame();
        return;
    }

    // Extract NV12 planes into separate Y and UV textures
    if (!upload_nv12_planes(nv12_texture, video_width, video_height)) {
        render_frame();
        return;
    }

    // Set render target and clear (for letterbox bars)
    context_->OMSetRenderTargets(1, render_target_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(render_target_.Get(), constants::kBgClearColor);

    // Set viewport with letterbox/pillarbox
    set_letterbox_viewport(video_width, video_height);

    // Draw fullscreen triangle with NV12→RGB shader
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->IASetInputLayout(nullptr);
    context_->VSSetShader(video_vs_.Get(), nullptr, 0);
    context_->PSSetShader(video_ps_.Get(), nullptr, 0);

    ID3D11ShaderResourceView* srvs[] = { srv_y_.Get(), srv_uv_.Get() };
    context_->PSSetShaderResources(0, 2, srvs);
    context_->PSSetSamplers(0, 1, sampler_.GetAddressOf());

    context_->Draw(3, 0);

    ID3D11ShaderResourceView* null_srvs[] = { nullptr, nullptr };
    context_->PSSetShaderResources(0, 2, null_srvs);

    swap_chain_->Present(0, 0);
}

void D3D11Renderer::shutdown() {
    if (!initialized_) return;
    Logger::info("Shutting down D3D11 renderer");

    srv_uv_.Reset();
    srv_y_.Reset();
    tex_uv_.Reset();
    tex_y_.Reset();
    staging_read_.Reset();
    sampler_.Reset();
    video_ps_.Reset();
    video_vs_.Reset();
    render_target_.Reset();
    swap_chain_.Reset();
    context_.Reset();
    device_.Reset();

    video_tex_width_ = 0;
    video_tex_height_ = 0;
    window_width_ = 0;
    window_height_ = 0;
    initialized_ = false;
}

} // namespace reflection
