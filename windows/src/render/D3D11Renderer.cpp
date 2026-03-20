#include "render/D3D11Renderer.h"
#include "utilities/Logger.h"

namespace reflection {

D3D11Renderer::D3D11Renderer() = default;
D3D11Renderer::~D3D11Renderer() { shutdown(); }

bool D3D11Renderer::init(HWND hwnd, int width, int height) {
    if (initialized_) return true;
    hwnd_ = hwnd;

    Logger::info("Initializing D3D11 renderer: {}x{}", width, height);

    // TODO (Milestone 3):
    // 1. D3D11CreateDeviceAndSwapChain
    // 2. Create render target view from back buffer
    // 3. Create NV12→RGB pixel shader
    // 4. Create ID3D11VideoProcessor for color conversion
    // 5. Set viewport

    initialized_ = true;
    return true;
}

void D3D11Renderer::resize(int width, int height) {
    if (!initialized_) return;
    Logger::debug("D3D11Renderer::resize {}x{}", width, height);
    // TODO (Milestone 3): Release render target, resize swap chain buffers, recreate
}

void D3D11Renderer::render_frame() {
    if (!initialized_) return;
    // TODO (Milestone 3): Copy decoded texture to back buffer, Present
}

void D3D11Renderer::shutdown() {
    if (!initialized_) return;
    Logger::info("Shutting down D3D11 renderer");
    // TODO (Milestone 3): Release all COM objects
    initialized_ = false;
}

} // namespace reflection
