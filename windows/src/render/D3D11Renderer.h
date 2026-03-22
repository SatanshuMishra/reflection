#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <cstdint>

namespace reflection {

/// Direct3D 11 renderer for displaying decoded video frames.
///
/// Renders BGRA textures from the Media Foundation decoder using a simple
/// passthrough pixel shader. The decoder outputs RGB32 (BGRA), so no
/// color space conversion is needed — just sample and present.
class D3D11Renderer {
public:
    D3D11Renderer();
    ~D3D11Renderer();

    D3D11Renderer(const D3D11Renderer&) = delete;
    D3D11Renderer& operator=(const D3D11Renderer&) = delete;

    [[nodiscard]] bool init(HWND hwnd, int width, int height);
    void resize(int width, int height);

    /// Render a blank frame (background color only).
    void render_frame();

    /// Render a BGRA video frame from raw CPU pixel data.
    /// Creates the texture on the main thread for GPU driver compatibility.
    void render_video_frame(const uint8_t* bgra_data, int bgra_stride,
                            int video_width, int video_height);

    void shutdown();

    [[nodiscard]] ID3D11Device* device() const { return device_.Get(); }
    [[nodiscard]] bool is_initialized() const { return initialized_; }

private:
    HWND hwnd_ = nullptr;
    bool initialized_ = false;
    int window_width_ = 0;
    int window_height_ = 0;

    // Core D3D11
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_;

    // Shader pipeline
    Microsoft::WRL::ComPtr<ID3D11VertexShader> video_vs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> video_ps_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;

    [[nodiscard]] bool create_render_target();
    [[nodiscard]] bool init_video_pipeline();
    void set_letterbox_viewport(int video_width, int video_height);
};

} // namespace reflection
