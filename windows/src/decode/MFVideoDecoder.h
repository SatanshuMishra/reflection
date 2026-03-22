#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

namespace reflection {

/// Decoded video frame — raw BGRA pixels in CPU memory.
struct DecodedFrame {
    std::vector<uint8_t> bgra;   // BGRA pixel data (width * height * 4 bytes)
    int width = 0;
    int height = 0;
};

/// Media Foundation H.264 decoder.
///
/// Decodes raw H.264 NAL units (Annex B) and produces CPU-side BGRA pixel
/// buffers. Does NOT create D3D11 textures — the caller is responsible for
/// uploading to the GPU on the appropriate thread.
class MFVideoDecoder {
public:
    MFVideoDecoder();
    ~MFVideoDecoder();

    MFVideoDecoder(const MFVideoDecoder&) = delete;
    MFVideoDecoder& operator=(const MFVideoDecoder&) = delete;

    /// Initialize the decoder (no D3D11 device needed — CPU-only decode).
    [[nodiscard]] bool init();

    /// Decode an H.264 NAL unit. Returns true if a decoded frame is available.
    [[nodiscard]] bool decode(
        const uint8_t* data, size_t size, uint64_t timestamp,
        DecodedFrame& out_frame);

    void flush();
    void shutdown();

    [[nodiscard]] bool is_initialized() const { return initialized_; }

private:
    bool initialized_ = false;
    Microsoft::WRL::ComPtr<IMFTransform> mft_;

    bool output_type_set_ = false;
    LONG output_stride_ = 0;
    bool first_frame_logged_ = false;

    [[nodiscard]] bool create_decoder_mft();
    [[nodiscard]] bool set_input_type();
    [[nodiscard]] bool negotiate_output_type();

    [[nodiscard]] bool try_get_output_frame(DecodedFrame& out_frame);
    [[nodiscard]] bool extract_frame_from_sample(IMFSample* sample, DecodedFrame& out_frame);
};

} // namespace reflection
