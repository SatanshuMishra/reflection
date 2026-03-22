#include "decode/MFVideoDecoder.h"
#include "utilities/Logger.h"

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mftransform.h>

#include <vector>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")

namespace reflection {

namespace {

/// Map MFVideoFormat GUID to DXGI_FORMAT and human-readable name.
struct FormatMapping {
    GUID mf_format;
    DXGI_FORMAT dxgi_format;
    const char* name;
};

const FormatMapping kPreferredFormats[] = {
    { MFVideoFormat_RGB32,  DXGI_FORMAT_B8G8R8A8_UNORM, "RGB32 (BGRA)" },
    { MFVideoFormat_ARGB32, DXGI_FORMAT_B8G8R8A8_UNORM, "ARGB32 (BGRA)" },
    { MFVideoFormat_NV12,   DXGI_FORMAT_NV12,           "NV12" },
};

} // namespace

MFVideoDecoder::MFVideoDecoder() = default;
MFVideoDecoder::~MFVideoDecoder() { shutdown(); }

bool MFVideoDecoder::init(ID3D11Device* device) {
    if (initialized_) return true;
    if (!device) {
        Logger::error("MFVideoDecoder::init called with null device");
        return false;
    }

    Logger::info("Initializing Media Foundation H.264 decoder");
    device_ = device;

    if (!create_decoder_mft()) return false;

    // DO NOT set a D3D device manager on the MFT. Without it, the software
    // decoder allocates plain CPU memory buffers. This is critical because:
    //
    // 1. The decode runs on a background thread while the main thread uses
    //    the D3D11 immediate context for rendering. The immediate context
    //    is NOT thread-safe.
    // 2. If we give the MFT our device manager, it allocates DXGI-backed
    //    buffers. IMFMediaBuffer::Lock() on DXGI buffers internally uses
    //    the immediate context to copy GPU→CPU, which deadlocks with the
    //    main thread's Draw/Present calls.
    // 3. We do NV12→BGRA conversion in CPU anyway, so GPU-backed buffers
    //    provide no benefit — they just add a GPU→CPU copy that we skip
    //    by using plain memory buffers.
    //
    // When hardware DXVA2 decode is implemented in the future (dedicated
    // render thread), the device manager can be re-enabled.
    Logger::info("Using CPU-only decode (no DXVA2 device manager)");

    HRESULT hr;

    if (!set_input_type()) return false;

    // Output type negotiation — may be deferred if MFT needs SPS/PPS first
    if (!negotiate_output_type()) {
        Logger::warn("Output type negotiation deferred — will retry after SPS/PPS");
    }

    hr = mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    if (FAILED(hr)) {
        Logger::error("BEGIN_STREAMING failed: 0x{:08X}", hr);
        return false;
    }

    mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

    initialized_ = true;
    Logger::info("Media Foundation decoder initialized (output: {})",
                 output_type_set_ ? "configured" : "pending");
    return true;
}

bool MFVideoDecoder::create_decoder_mft() {
    MFT_REGISTER_TYPE_INFO input_type{};
    input_type.guidMajorType = MFMediaType_Video;
    input_type.guidSubtype = MFVideoFormat_H264;

    struct EnumAttempt { UINT32 flags; const char* desc; };
    const EnumAttempt attempts[] = {
        { MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER, "hardware" },
        { MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER, "sync+async" },
        { MFT_ENUM_FLAG_ALL | MFT_ENUM_FLAG_SORTANDFILTER, "all (sorted)" },
        { MFT_ENUM_FLAG_ALL, "all" },
    };

    for (const auto& attempt : attempts) {
        IMFActivate** activates = nullptr;
        UINT32 count = 0;

        HRESULT hr = MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER, attempt.flags,
                               &input_type, nullptr, &activates, &count);

        if (SUCCEEDED(hr) && count > 0) {
            hr = activates[0]->ActivateObject(IID_PPV_ARGS(mft_.GetAddressOf()));
            for (UINT32 i = 0; i < count; ++i) activates[i]->Release();
            CoTaskMemFree(activates);

            if (SUCCEEDED(hr)) {
                Logger::info("H.264 decoder MFT created via: {}", attempt.desc);
                return true;
            }
        } else if (activates) {
            for (UINT32 i = 0; i < count; ++i) activates[i]->Release();
            CoTaskMemFree(activates);
        }
    }

    Logger::error("No H.264 decoder MFT available");
    return false;
}

bool MFVideoDecoder::set_input_type() {
    Microsoft::WRL::ComPtr<IMFMediaType> type;
    HRESULT hr = MFCreateMediaType(type.GetAddressOf());
    if (FAILED(hr)) return false;

    type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);

    hr = mft_->SetInputType(0, type.Get(), 0);
    if (FAILED(hr)) {
        Logger::error("SetInputType(H.264) failed: 0x{:08X}", hr);
        return false;
    }
    return true;
}

bool MFVideoDecoder::negotiate_output_type() {
    // Enumerate available output types and pick the best one.
    // Priority: RGB32 > ARGB32 > NV12
    // RGB32/ARGB32 produce single-plane BGRA textures, avoiding all NV12
    // multiplanar format handling issues.

    for (const auto& preferred : kPreferredFormats) {
        for (DWORD i = 0; ; ++i) {
            Microsoft::WRL::ComPtr<IMFMediaType> output_type;
            HRESULT hr = mft_->GetOutputAvailableType(0, i, output_type.GetAddressOf());
            if (hr == MF_E_NO_MORE_TYPES) break;
            if (FAILED(hr)) break;

            GUID subtype{};
            if (FAILED(output_type->GetGUID(MF_MT_SUBTYPE, &subtype))) continue;

            if (subtype == preferred.mf_format) {
                hr = mft_->SetOutputType(0, output_type.Get(), 0);
                if (SUCCEEDED(hr)) {
                    output_mf_format_ = preferred.mf_format;
                    output_dxgi_format_ = preferred.dxgi_format;
                    output_type_set_ = true;

                    // Read stride
                    UINT32 w = 0, h = 0;
                    if (SUCCEEDED(MFGetAttributeSize(output_type.Get(), MF_MT_FRAME_SIZE, &w, &h))) {
                        UINT32 stride_val = 0;
                        if (SUCCEEDED(output_type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride_val))) {
                            output_stride_ = static_cast<LONG>(stride_val);
                        } else {
                            // RGB32 stride = width * 4 bytes per pixel
                            output_stride_ = (preferred.dxgi_format == DXGI_FORMAT_B8G8R8A8_UNORM)
                                ? static_cast<LONG>(w * 4)
                                : static_cast<LONG>(w);
                        }
                        Logger::info("MFT output: {} {}x{}, stride={}",
                                     preferred.name, w, h, output_stride_);
                    }
                    return true;
                }
            }
        }
    }

    Logger::error("Failed to set any output type on MFT");
    return false;
}

bool MFVideoDecoder::decode(
    const uint8_t* data, size_t size, uint64_t timestamp,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& out_texture
) {
    if (!initialized_ || !mft_) return false;
    if (!data || size == 0) return false;

    // Log first frame for diagnostics
    if (!first_frame_logged_) {
        first_frame_logged_ = true;
        if (size >= 5) {
            Logger::info("First H.264 frame: size={}, bytes=[{:02X} {:02X} {:02X} {:02X} {:02X}]",
                         size, data[0], data[1], data[2], data[3], data[4]);
        }
    }

    // Create MF sample from NAL data
    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(size), buffer.GetAddressOf());
    if (FAILED(hr)) return false;

    BYTE* buf_ptr = nullptr;
    hr = buffer->Lock(&buf_ptr, nullptr, nullptr);
    if (FAILED(hr)) return false;
    memcpy(buf_ptr, data, size);
    buffer->Unlock();
    buffer->SetCurrentLength(static_cast<DWORD>(size));

    Microsoft::WRL::ComPtr<IMFSample> sample;
    hr = MFCreateSample(sample.GetAddressOf());
    if (FAILED(hr)) return false;
    sample->AddBuffer(buffer.Get());
    sample->SetSampleTime(static_cast<LONGLONG>(timestamp) * 10);

    // Feed to decoder
    hr = mft_->ProcessInput(0, sample.Get(), 0);

    if (hr == MF_E_NOTACCEPTING) {
        if (try_get_output_frame(out_texture)) return true;
        return false;
    }

    if (FAILED(hr)) {
        if (size >= 4) {
            Logger::error("ProcessInput failed: 0x{:08X}, size={}, bytes=[{:02X} {:02X} {:02X} {:02X}]",
                          hr, size, data[0], data[1], data[2], data[3]);
        }
        return false;
    }

    return try_get_output_frame(out_texture);
}

bool MFVideoDecoder::try_get_output_frame(
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& out_texture
) {
    MFT_OUTPUT_DATA_BUFFER output_buffer{};
    output_buffer.dwStreamID = 0;

    MFT_OUTPUT_STREAM_INFO stream_info{};
    HRESULT hr = mft_->GetOutputStreamInfo(0, &stream_info);
    if (FAILED(hr)) return false;

    Microsoft::WRL::ComPtr<IMFSample> output_sample;
    if (!(stream_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)) {
        hr = MFCreateSample(output_sample.GetAddressOf());
        if (FAILED(hr)) return false;

        Microsoft::WRL::ComPtr<IMFMediaBuffer> out_buf;
        DWORD buf_size = (stream_info.cbSize > 0) ? stream_info.cbSize : (1920 * 1080 * 4);
        hr = MFCreateMemoryBuffer(buf_size, out_buf.GetAddressOf());
        if (FAILED(hr)) return false;

        output_sample->AddBuffer(out_buf.Get());
        output_buffer.pSample = output_sample.Get();
    }

    DWORD status = 0;
    hr = mft_->ProcessOutput(0, 1, &output_buffer, &status);

    if (output_buffer.pEvents) {
        output_buffer.pEvents->Release();
    }

    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) return false;

    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
        Logger::info("MFT stream change — renegotiating output type");
        if (!negotiate_output_type()) {
            Logger::error("Failed to renegotiate output type");
            return false;
        }
        return try_get_output_frame(out_texture);
    }

    if (FAILED(hr)) {
        Logger::error("ProcessOutput failed: 0x{:08X}", hr);
        return false;
    }

    IMFSample* result = output_buffer.pSample;
    if (!result) return false;

    return extract_texture_from_sample(result, out_texture);
}

bool MFVideoDecoder::extract_texture_from_sample(
    IMFSample* sample,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& out_texture
) {
    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = sample->ConvertToContiguousBuffer(buffer.GetAddressOf());
    if (FAILED(hr)) return false;

    // Get dimensions from current output type
    Microsoft::WRL::ComPtr<IMFMediaType> output_type;
    hr = mft_->GetOutputCurrentType(0, output_type.GetAddressOf());
    if (FAILED(hr)) return false;

    UINT32 width = 0, height = 0;
    hr = MFGetAttributeSize(output_type.Get(), MF_MT_FRAME_SIZE, &width, &height);
    if (FAILED(hr) || width == 0 || height == 0) return false;

    // Get raw pixel data via IMFMediaBuffer::Lock().
    // For NV12, Lock() returns CONTIGUOUS memory: Y plane followed by UV plane.
    // We intentionally avoid IMF2DBuffer::Lock2D() for NV12 because Lock2D
    // on multiplanar formats may only return the Y plane or use a different
    // internal stride that doesn't match the contiguous NV12 layout.
    BYTE* raw_data = nullptr;
    DWORD max_len = 0, cur_len = 0;
    hr = buffer->Lock(&raw_data, &max_len, &cur_len);
    if (FAILED(hr) || !raw_data) return false;

    // Determine stride: for NV12, stride = width (1 byte per Y pixel).
    // For BGRA, stride = width * 4.
    LONG stride = output_stride_;
    if (stride <= 0) {
        stride = static_cast<LONG>(width);
    }

    // Log stride once for diagnostics
    static bool stride_logged = false;
    if (!stride_logged) {
        stride_logged = true;
        Logger::info("Decode buffer: {}x{}, stride={}, buffer_size={}, format={}",
                     width, height, stride, cur_len,
                     (output_dxgi_format_ == DXGI_FORMAT_NV12) ? "NV12" : "BGRA");
    }

    // Determine the MFT's actual output format (not our cached value,
    // which must stay as NV12 to ensure this branch runs every frame).
    GUID actual_subtype{};
    output_type->GetGUID(MF_MT_SUBTYPE, &actual_subtype);
    const bool is_nv12_output = (actual_subtype == MFVideoFormat_NV12);

    // If the output is NV12, convert to BGRA in CPU.
    // NV12 can't be used as a shader resource on D3D11 (multiplanar format).
    // The conversion is fast enough for our resolution (1312x976 @ 30fps).
    if (is_nv12_output) {
        // Allocate BGRA buffer
        const UINT bgra_stride = width * 4;
        std::vector<uint8_t> bgra(bgra_stride * height);

        const BYTE* y_plane = raw_data;
        const BYTE* uv_plane = raw_data + stride * height;

        // BT.601 NV12 → BGRA conversion
        for (UINT row = 0; row < height; ++row) {
            const BYTE* y_row = y_plane + row * stride;
            const BYTE* uv_row = uv_plane + (row / 2) * stride;
            uint8_t* bgra_row = bgra.data() + row * bgra_stride;

            for (UINT col = 0; col < width; ++col) {
                const int y_val = y_row[col];
                const int u_val = uv_row[(col & ~1u)] - 128;      // Even byte = U
                const int v_val = uv_row[(col & ~1u) + 1] - 128;  // Odd byte = V

                // BT.601 limited range → full range
                int r = y_val + ((359 * v_val) >> 8);
                int g = y_val - ((88 * u_val + 183 * v_val) >> 8);
                int b = y_val + ((454 * u_val) >> 8);

                // Clamp to [0, 255]
                bgra_row[col * 4 + 0] = static_cast<uint8_t>((b < 0) ? 0 : (b > 255) ? 255 : b);
                bgra_row[col * 4 + 1] = static_cast<uint8_t>((g < 0) ? 0 : (g > 255) ? 255 : g);
                bgra_row[col * 4 + 2] = static_cast<uint8_t>((r < 0) ? 0 : (r > 255) ? 255 : r);
                bgra_row[col * 4 + 3] = 255;  // Alpha
            }
        }

        // Unlock source buffer
        buffer->Unlock();

        // Create BGRA texture
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA init_data{};
        init_data.pSysMem = bgra.data();
        init_data.SysMemPitch = bgra_stride;

        hr = device_->CreateTexture2D(&desc, &init_data, out_texture.GetAddressOf());
        if (FAILED(hr)) {
            Logger::error("CreateTexture2D (BGRA from NV12) failed: 0x{:08X}", hr);
            return false;
        }

        return true;
    }

    // BGRA path: create texture directly from the buffer data
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = output_dxgi_format_;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init_data{};
    init_data.pSysMem = raw_data;
    init_data.SysMemPitch = static_cast<UINT>(stride);

    hr = device_->CreateTexture2D(&desc, &init_data, out_texture.GetAddressOf());

    buffer->Unlock();

    if (FAILED(hr)) {
        Logger::error("CreateTexture2D failed: 0x{:08X} ({}x{}, fmt={})",
                      hr, width, height, static_cast<int>(output_dxgi_format_));
        return false;
    }

    return true;
}

void MFVideoDecoder::flush() {
    if (!initialized_ || !mft_) return;
    mft_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
}

void MFVideoDecoder::shutdown() {
    if (!initialized_) return;
    Logger::info("Shutting down Media Foundation decoder");

    if (mft_) {
        mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
    }

    mft_.Reset();
    device_manager_.Reset();
    device_.Reset();
    device_manager_token_ = 0;
    output_type_set_ = false;
    output_mf_format_ = {};
    output_dxgi_format_ = DXGI_FORMAT_UNKNOWN;
    output_stride_ = 0;
    first_frame_logged_ = false;
    initialized_ = false;
}

} // namespace reflection
