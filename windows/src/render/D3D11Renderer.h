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
/// Two rendering modes:
/// - render_frame(): Clears to background color (no video)
/// - render_video_frame(): Renders an NV12 texture via pixel shader
class D3D11Renderer {
public:
    D3D11Renderer();
    ~D3D11Renderer();

    // Non-copyable
    D3D11Renderer(const D3D11Renderer&) = delete;
    D3D11Renderer& operator=(const D3D11Renderer&) = delete;

    /// Initialize D3D11 device, swap chain, render target, and video pipeline.
    [[nodiscard]] bool init(HWND hwnd, int width, int height);

    /// Resize the swap chain (called on window resize).
    void resize(int width, int height);

    /// Render a blank frame (clear to background color + present).
    void render_frame();

    /// Render a decoded NV12 video frame.
    /// @param nv12_texture  The NV12 texture from MFVideoDecoder
    /// @param video_width   Width of the video frame
    /// @param video_height  Height of the video frame
    void render_video_frame(ID3D11Texture2D* nv12_texture,
                            int video_width, int video_height);

    /// Shut down and release all D3D resources.
    void shutdown();

    /// Get the D3D11 device (shared with MFVideoDecoder for zero-copy).
    [[nodiscard]] ID3D11Device* device() const { return device_.Get(); }

    /// Whether the renderer has been initialized.
    [[nodiscard]] bool is_initialized() const { return initialized_; }

private:
    HWND hwnd_ = nullptr;
    bool initialized_ = false;
    int window_width_ = 0;
    int window_height_ = 0;

    // Core D3D11 resources
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_;

    // Video rendering pipeline
    Microsoft::WRL::ComPtr<ID3D11VertexShader> video_vs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> video_ps_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;

    // Staging texture for NV12 frames that lack BIND_SHADER_RESOURCE
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_nv12_;
    int staging_width_ = 0;
    int staging_height_ = 0;

    /// Create the render target view from the swap chain back buffer.
    [[nodiscard]] bool create_render_target();

    /// Compile shaders and create the video rendering pipeline.
    [[nodiscard]] bool init_video_pipeline();

    /// Ensure the staging texture matches the given dimensions.
    [[nodiscard]] bool ensure_staging_texture(int width, int height);

    /// Set the viewport for letterbox/pillarbox rendering.
    void set_letterbox_viewport(int video_width, int video_height);
};

} // namespace reflection
