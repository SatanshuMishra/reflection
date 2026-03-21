#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <wrl/client.h>

#include <cstdint>

namespace reflection {

/// Media Foundation H.264 decoder with D3D11 hardware acceleration (DXVA2).
///
/// Accepts raw H.264 NAL units from RPiPlay's RAOP callbacks, decodes them
/// using a hardware-accelerated MFT, and produces NV12 ID3D11Texture2D
/// frames for zero-copy rendering via the D3D11 pixel shader pipeline.
///
/// Usage:
///   MFVideoDecoder decoder;
///   decoder.init(d3d_device);  // Share the renderer's D3D11 device
///   ...
///   ComPtr<ID3D11Texture2D> texture;
///   if (decoder.decode(nal_data, size, pts, texture)) {
///       renderer.render_video_frame(texture.Get());
///   }
class MFVideoDecoder {
public:
    MFVideoDecoder();
    ~MFVideoDecoder();

    // Non-copyable
    MFVideoDecoder(const MFVideoDecoder&) = delete;
    MFVideoDecoder& operator=(const MFVideoDecoder&) = delete;

    /// Initialize the decoder with a shared D3D11 device.
    /// The device MUST be the same one used by D3D11Renderer for zero-copy.
    [[nodiscard]] bool init(ID3D11Device* device);

    /// Decode an H.264 NAL unit. Returns true if a decoded frame is available.
    /// @param data     Raw H.264 NAL unit bytes
    /// @param size     Size of the NAL data
    /// @param timestamp Presentation timestamp from RPiPlay
    /// @param out_texture  Receives the decoded NV12 texture (only valid when returning true)
    [[nodiscard]] bool decode(
        const uint8_t* data, size_t size, uint64_t timestamp,
        Microsoft::WRL::ComPtr<ID3D11Texture2D>& out_texture);

    /// Flush any buffered frames from the decoder.
    void flush();

    /// Shut down and release all resources.
    void shutdown();

    /// Whether the decoder has been successfully initialized.
    [[nodiscard]] bool is_initialized() const { return initialized_; }

private:
    bool initialized_ = false;

    // D3D11 device (shared with renderer — not owned)
    Microsoft::WRL::ComPtr<ID3D11Device> device_;

    // DXVA2 device manager for hardware-accelerated decode
    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> device_manager_;
    UINT device_manager_token_ = 0;

    // Media Foundation H.264 decoder transform
    Microsoft::WRL::ComPtr<IMFTransform> mft_;

    // Track output type negotiation
    bool output_type_set_ = false;

    // NV12 output stride (may differ from width due to alignment padding)
    LONG output_stride_ = 0;

    // Diagnostic: log first frame details once
    bool first_frame_logged_ = false;

    /// Find and create the hardware H.264 decoder MFT.
    [[nodiscard]] bool create_decoder_mft();

    /// Configure the input media type (H.264).
    [[nodiscard]] bool set_input_type();

    /// Negotiate and set the output media type (NV12).
    [[nodiscard]] bool negotiate_output_type();

    /// Try to produce a decoded frame from the MFT.
    [[nodiscard]] bool try_get_output_frame(
        Microsoft::WRL::ComPtr<ID3D11Texture2D>& out_texture);

    /// Extract ID3D11Texture2D from an MF output sample.
    [[nodiscard]] bool extract_texture_from_sample(
        IMFSample* sample,
        Microsoft::WRL::ComPtr<ID3D11Texture2D>& out_texture);
};

} // namespace reflection
