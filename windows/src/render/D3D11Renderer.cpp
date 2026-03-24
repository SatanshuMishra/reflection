// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "render/D3D11Renderer.h"
#include "render/VideoShaders.h"
#include "utilities/Constants.h"
#include "utilities/Logger.h"

#include <d3dcompiler.h>
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

    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = static_cast<UINT>(width);
    scd.BufferDesc.Height = static_cast<UINT>(height);
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate = { 60, 1 };
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc = { 1, 0 };
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL feature_level{};
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        levels, _countof(levels), D3D11_SDK_VERSION, &scd,
        swap_chain_.GetAddressOf(), device_.GetAddressOf(),
        &feature_level, context_.GetAddressOf());

    if (FAILED(hr)) {
        // Legacy swap effect fallback
        scd.BufferCount = 1;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            levels, _countof(levels), D3D11_SDK_VERSION, &scd,
            swap_chain_.GetAddressOf(), device_.GetAddressOf(),
            &feature_level, context_.GetAddressOf());

        if (FAILED(hr)) {
            // WARP software renderer
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
                levels, _countof(levels), D3D11_SDK_VERSION, &scd,
                swap_chain_.GetAddressOf(), device_.GetAddressOf(),
                &feature_level, context_.GetAddressOf());
            if (FAILED(hr)) {
                Logger::error("All D3D11 device creation failed: 0x{:08X}", hr);
                return false;
            }
            Logger::warn("Using WARP software renderer");
        }
    }

    Logger::info("D3D11 device created — feature level: 0x{:X}",
                 static_cast<unsigned int>(feature_level));

    if (!create_render_target()) return false;
    if (!init_video_pipeline()) return false;

    initialized_ = true;
    Logger::info("D3D11 renderer initialized");
    return true;
}

bool D3D11Renderer::create_render_target() {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    HRESULT hr = swap_chain_->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf()));
    if (FAILED(hr)) return false;

    hr = device_->CreateRenderTargetView(back_buffer.Get(), nullptr, render_target_.GetAddressOf());
    return SUCCEEDED(hr);
}

bool D3D11Renderer::init_video_pipeline() {
    Microsoft::WRL::ComPtr<ID3DBlob> vs_blob, ps_blob, err;

    HRESULT hr = D3DCompile(shaders::kVideoVertexShader, strlen(shaders::kVideoVertexShader),
                            "VS", nullptr, nullptr, "main", "vs_4_0",
                            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, vs_blob.GetAddressOf(), err.GetAddressOf());
    if (FAILED(hr)) {
        if (err) Logger::error("VS: {}", static_cast<const char*>(err->GetBufferPointer()));
        return false;
    }

    err.Reset();
    hr = D3DCompile(shaders::kPassthroughPixelShader, strlen(shaders::kPassthroughPixelShader),
                    "PS", nullptr, nullptr, "main", "ps_4_0",
                    D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, ps_blob.GetAddressOf(), err.GetAddressOf());
    if (FAILED(hr)) {
        if (err) Logger::error("PS: {}", static_cast<const char*>(err->GetBufferPointer()));
        return false;
    }

    hr = device_->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
                                     nullptr, video_vs_.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = device_->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(),
                                    nullptr, video_ps_.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    hr = device_->CreateSamplerState(&sd, sampler_.GetAddressOf());
    if (FAILED(hr)) return false;

    Logger::info("Video shader pipeline initialized");
    return true;
}

void D3D11Renderer::set_letterbox_viewport(int video_width, int video_height) {
    if (window_width_ <= 0 || window_height_ <= 0) return;
    if (video_width <= 0 || video_height <= 0) return;

    const float wa = static_cast<float>(window_width_) / window_height_;
    const float va = static_cast<float>(video_width) / video_height;

    D3D11_VIEWPORT vp{};
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    if (va > wa) {
        vp.Width = static_cast<float>(window_width_);
        vp.Height = vp.Width / va;
        vp.TopLeftY = (window_height_ - vp.Height) / 2.0f;
    } else {
        vp.Height = static_cast<float>(window_height_);
        vp.Width = vp.Height * va;
        vp.TopLeftX = (window_width_ - vp.Width) / 2.0f;
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

    HRESULT hr = swap_chain_->ResizeBuffers(0, static_cast<UINT>(width),
                                            static_cast<UINT>(height),
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
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);

    swap_chain_->Present(0, 0);
}

void D3D11Renderer::render_video_frame(
    const uint8_t* bgra_data, int bgra_stride,
    int video_width, int video_height
) {
    if (!initialized_ || !render_target_ || !bgra_data) return;
    if (!video_vs_ || !video_ps_ || !sampler_) return;
    if (video_width <= 0 || video_height <= 0) return;

    // Create texture from raw BGRA bytes ON THE MAIN THREAD.
    // This ensures the D3D11 immediate context is used consistently
    // from the same thread, avoiding GPU driver issues with deferred
    // pSysMem copies on virtual GPUs (e.g., Hyper-V).
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(video_width);
    desc.Height = static_cast<UINT>(video_height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;  // Data will not change
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init_data{};
    init_data.pSysMem = bgra_data;
    init_data.SysMemPitch = static_cast<UINT>(bgra_stride);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device_->CreateTexture2D(&desc, &init_data, texture.GetAddressOf());
    if (FAILED(hr)) return;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    hr = device_->CreateShaderResourceView(texture.Get(), nullptr, srv.GetAddressOf());
    if (FAILED(hr)) return;

    // Render
    context_->OMSetRenderTargets(1, render_target_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(render_target_.Get(), constants::kBgClearColor);
    set_letterbox_viewport(video_width, video_height);

    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->IASetInputLayout(nullptr);
    context_->VSSetShader(video_vs_.Get(), nullptr, 0);
    context_->PSSetShader(video_ps_.Get(), nullptr, 0);
    context_->PSSetShaderResources(0, 1, srv.GetAddressOf());
    context_->PSSetSamplers(0, 1, sampler_.GetAddressOf());
    context_->Draw(3, 0);

    ID3D11ShaderResourceView* null_srv = nullptr;
    context_->PSSetShaderResources(0, 1, &null_srv);
    swap_chain_->Present(0, 0);
}

void D3D11Renderer::shutdown() {
    if (!initialized_) return;
    Logger::info("Shutting down D3D11 renderer");

    sampler_.Reset();
    video_ps_.Reset();
    video_vs_.Reset();
    render_target_.Reset();
    swap_chain_.Reset();
    context_.Reset();
    device_.Reset();

    window_width_ = window_height_ = 0;
    initialized_ = false;
}

} // namespace reflection
