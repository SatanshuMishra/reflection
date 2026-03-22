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
///
/// Renders NV12 video by splitting into separate Y (R8_UNORM) and UV (R8G8_UNORM)
/// textures, then sampling both in a YUV→RGB pixel shader. This approach avoids
/// relying on NV12 plane-separated SRVs which have inconsistent driver support.
///
/// The industry-standard D3D11 video rendering pipeline:
///   1. Receive NV12 texture from Media Foundation decoder
///   2. Copy to CPU-readable staging texture
///   3. Extract Y plane → R8_UNORM GPU texture
///   4. Extract UV plane → R8G8_UNORM GPU texture (half resolution)
///   5. Bind both as shader resources
///   6. Fullscreen triangle + YUV→RGB pixel shader
///   7. Present to swap chain
class D3D11Renderer {
public:
    D3D11Renderer();
    ~D3D11Renderer();

    D3D11Renderer(const D3D11Renderer&) = delete;
    D3D11Renderer& operator=(const D3D11Renderer&) = delete;

    [[nodiscard]] bool init(HWND hwnd, int width, int height);
    void resize(int width, int height);
    void render_frame();
    void render_video_frame(ID3D11Texture2D* nv12_texture,
                            int video_width, int video_height);
    void shutdown();

    [[nodiscard]] ID3D11Device* device() const { return device_.Get(); }
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

    // Video shader pipeline
    Microsoft::WRL::ComPtr<ID3D11VertexShader> video_vs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> video_ps_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;

    // CPU-readable staging texture for extracting NV12 planes
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_read_;

    // Separate Y and UV textures for shader sampling (universally compatible)
    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex_y_;    // R8_UNORM, full res
    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex_uv_;   // R8G8_UNORM, half res
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_y_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_uv_;
    int video_tex_width_ = 0;
    int video_tex_height_ = 0;

    [[nodiscard]] bool create_render_target();
    [[nodiscard]] bool init_video_pipeline();
    [[nodiscard]] bool ensure_video_textures(int width, int height);
    bool upload_nv12_planes(ID3D11Texture2D* nv12_texture, int width, int height);
    void set_letterbox_viewport(int video_width, int video_height);
};

} // namespace reflection
