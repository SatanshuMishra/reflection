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

/// Media Foundation H.264 decoder.
///
/// Decodes raw H.264 NAL units (Annex B) and produces BGRA textures with
/// pre-created SRVs ready for immediate rendering. Reuses internal buffers
/// across frames to minimize per-frame allocation overhead.
class MFVideoDecoder {
public:
    MFVideoDecoder();
    ~MFVideoDecoder();

    MFVideoDecoder(const MFVideoDecoder&) = delete;
    MFVideoDecoder& operator=(const MFVideoDecoder&) = delete;

    [[nodiscard]] bool init(ID3D11Device* device);

    /// Decode an H.264 NAL unit. Returns true if a decoded frame is available.
    /// Both out_texture and out_srv are set on success, ready for rendering.
    [[nodiscard]] bool decode(
        const uint8_t* data, size_t size, uint64_t timestamp,
        Microsoft::WRL::ComPtr<ID3D11Texture2D>& out_texture,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& out_srv);

    void flush();
    void shutdown();

    [[nodiscard]] bool is_initialized() const { return initialized_; }

private:
    bool initialized_ = false;

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<IMFTransform> mft_;

    bool output_type_set_ = false;
    GUID output_mf_format_{};
    DXGI_FORMAT output_dxgi_format_ = DXGI_FORMAT_UNKNOWN;
    LONG output_stride_ = 0;
    bool first_frame_logged_ = false;


    [[nodiscard]] bool create_decoder_mft();
    [[nodiscard]] bool set_input_type();
    [[nodiscard]] bool negotiate_output_type();

    [[nodiscard]] bool try_get_output_frame(
        Microsoft::WRL::ComPtr<ID3D11Texture2D>& out_texture,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& out_srv);

    [[nodiscard]] bool extract_texture_from_sample(
        IMFSample* sample,
        Microsoft::WRL::ComPtr<ID3D11Texture2D>& out_texture,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& out_srv);
};

} // namespace reflection
