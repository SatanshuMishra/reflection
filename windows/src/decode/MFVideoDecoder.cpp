// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "decode/MFVideoDecoder.h"
#include "utilities/Logger.h"

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mftransform.h>

#include <atomic>
#include <vector>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")

namespace reflection {

MFVideoDecoder::MFVideoDecoder() = default;
MFVideoDecoder::~MFVideoDecoder() { shutdown(); }

bool MFVideoDecoder::init() {
    if (initialized_) return true;

    Logger::info("Initializing Media Foundation H.264 decoder (CPU-only)");

    if (!create_decoder_mft()) return false;
    if (!set_input_type()) return false;

    // Output type negotiation may be deferred until SPS/PPS arrives
    if (!negotiate_output_type()) {
        Logger::warn("Output type deferred — will retry after SPS/PPS");
    }

    HRESULT hr = mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    if (FAILED(hr)) {
        Logger::error("BEGIN_STREAMING failed: 0x{:08X}", hr);
        return false;
    }
    mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);

    initialized_ = true;
    Logger::info("Media Foundation decoder initialized");
    return true;
}

bool MFVideoDecoder::create_decoder_mft() {
    MFT_REGISTER_TYPE_INFO input_type{};
    input_type.guidMajorType = MFMediaType_Video;
    input_type.guidSubtype = MFVideoFormat_H264;

    struct EnumAttempt { UINT32 flags; const char* desc; };
    const EnumAttempt attempts[] = {
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
    for (DWORD i = 0; ; ++i) {
        Microsoft::WRL::ComPtr<IMFMediaType> output_type;
        HRESULT hr = mft_->GetOutputAvailableType(0, i, output_type.GetAddressOf());
        if (hr == MF_E_NO_MORE_TYPES) break;
        if (FAILED(hr)) break;

        GUID subtype{};
        if (FAILED(output_type->GetGUID(MF_MT_SUBTYPE, &subtype))) continue;

        if (subtype == MFVideoFormat_NV12) {
            hr = mft_->SetOutputType(0, output_type.Get(), 0);
            if (SUCCEEDED(hr)) {
                output_type_set_ = true;

                UINT32 w = 0, h = 0;
                if (SUCCEEDED(MFGetAttributeSize(output_type.Get(), MF_MT_FRAME_SIZE, &w, &h))) {
                    UINT32 stride_val = 0;
                    output_stride_ = SUCCEEDED(output_type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride_val))
                        ? static_cast<LONG>(stride_val)
                        : static_cast<LONG>(w);
                    Logger::info("MFT output: NV12 {}x{}, stride={}", w, h, output_stride_);
                }
                return true;
            }
        }
    }

    Logger::error("Failed to set output type");
    return false;
}

bool MFVideoDecoder::decode(
    const uint8_t* data, size_t size, uint64_t timestamp,
    DecodedFrame& out_frame
) {
    if (!initialized_ || !mft_) return false;
    if (!data || size == 0) return false;

    // Guard against integer overflow: network-supplied size truncated to DWORD
    constexpr size_t kMaxFrameSize = 4 * 1024 * 1024;  // 4 MiB
    if (size > kMaxFrameSize) {
        Logger::warn("Rejecting oversized video frame: {} bytes (max={})", size, kMaxFrameSize);
        return false;
    }

    if (!first_frame_logged_) {
        first_frame_logged_ = true;
        if (size >= 5) {
            Logger::info("First H.264 frame: size={}, bytes=[{:02X} {:02X} {:02X} {:02X} {:02X}]",
                         size, data[0], data[1], data[2], data[3], data[4]);
        }
    }

    // Create MF sample
    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(size), buffer.GetAddressOf());
    if (FAILED(hr)) return false;

    BYTE* buf_ptr = nullptr;
    if (FAILED(buffer->Lock(&buf_ptr, nullptr, nullptr))) return false;
    memcpy(buf_ptr, data, size);
    buffer->Unlock();
    buffer->SetCurrentLength(static_cast<DWORD>(size));

    Microsoft::WRL::ComPtr<IMFSample> sample;
    if (FAILED(MFCreateSample(sample.GetAddressOf()))) return false;
    sample->AddBuffer(buffer.Get());
    sample->SetSampleTime(static_cast<LONGLONG>(timestamp) * 10);

    // Feed to decoder
    hr = mft_->ProcessInput(0, sample.Get(), 0);

    if (hr == MF_E_NOTACCEPTING) {
        if (try_get_output_frame(out_frame)) return true;
        return false;
    }

    if (FAILED(hr)) {
        if (size >= 4) {
            Logger::error("ProcessInput failed: 0x{:08X}, size={}, bytes=[{:02X} {:02X} {:02X} {:02X}]",
                          hr, size, data[0], data[1], data[2], data[3]);
        }
        return false;
    }

    return try_get_output_frame(out_frame);
}

bool MFVideoDecoder::try_get_output_frame(DecodedFrame& out_frame) {
    // Iterative loop with bounded retries (prevents stack overflow on
    // repeated MF_E_TRANSFORM_STREAM_CHANGE from malformed streams)
    constexpr int kMaxStreamChangeRetries = 4;

    for (int attempt = 0; attempt < kMaxStreamChangeRetries; ++attempt) {
        MFT_OUTPUT_DATA_BUFFER output_buffer{};
        output_buffer.dwStreamID = 0;

        MFT_OUTPUT_STREAM_INFO stream_info{};
        if (FAILED(mft_->GetOutputStreamInfo(0, &stream_info))) return false;

        Microsoft::WRL::ComPtr<IMFSample> output_sample;
        if (!(stream_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)) {
            if (FAILED(MFCreateSample(output_sample.GetAddressOf()))) return false;

            Microsoft::WRL::ComPtr<IMFMediaBuffer> out_buf;
            DWORD buf_size = (stream_info.cbSize > 0) ? stream_info.cbSize : (1920 * 1080 * 4);
            if (FAILED(MFCreateMemoryBuffer(buf_size, out_buf.GetAddressOf()))) return false;
            output_sample->AddBuffer(out_buf.Get());
            output_buffer.pSample = output_sample.Get();
        }

        DWORD status = 0;
        HRESULT hr = mft_->ProcessOutput(0, 1, &output_buffer, &status);

        if (output_buffer.pEvents) output_buffer.pEvents->Release();

        if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) return false;

        if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
            Logger::info("MFT stream change — renegotiating output type (attempt {})", attempt + 1);
            if (!negotiate_output_type()) return false;
            continue;  // retry with new output type
        }

        if (FAILED(hr)) {
            Logger::error("ProcessOutput failed: 0x{:08X}", hr);
            return false;
        }

        if (!output_buffer.pSample) return false;
        return extract_frame_from_sample(output_buffer.pSample, out_frame);
    }

    Logger::error("MFT stream change retries exhausted");
    return false;
}

bool MFVideoDecoder::extract_frame_from_sample(IMFSample* sample, DecodedFrame& out_frame) {
    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = sample->ConvertToContiguousBuffer(buffer.GetAddressOf());
    if (FAILED(hr)) return false;

    // Get dimensions
    Microsoft::WRL::ComPtr<IMFMediaType> output_type;
    hr = mft_->GetOutputCurrentType(0, output_type.GetAddressOf());
    if (FAILED(hr)) return false;

    UINT32 width = 0, height = 0;
    hr = MFGetAttributeSize(output_type.Get(), MF_MT_FRAME_SIZE, &width, &height);
    if (FAILED(hr) || width == 0 || height == 0) return false;

    // Lock NV12 buffer (contiguous Y + UV in CPU memory)
    BYTE* raw_data = nullptr;
    DWORD max_len = 0, cur_len = 0;
    hr = buffer->Lock(&raw_data, &max_len, &cur_len);
    if (FAILED(hr) || !raw_data) return false;

    LONG stride = output_stride_;
    if (stride <= 0) stride = static_cast<LONG>(width);

    // Log buffer details once (atomic to avoid data race if called concurrently)
    static std::atomic<bool> buffer_logged{false};
    if (!buffer_logged.load(std::memory_order_relaxed)) {
        buffer_logged.store(true, std::memory_order_relaxed);
        Logger::info("Decode buffer: {}x{}, stride={}, buf_size={}", width, height, stride, cur_len);
    }

    // NV12 → BGRA conversion (BT.601 full-range — verified correct)
    const UINT bgra_stride = width * 4;
    out_frame.width = static_cast<int>(width);
    out_frame.height = static_cast<int>(height);
    out_frame.bgra.resize(static_cast<size_t>(bgra_stride) * height);

    const BYTE* y_plane = raw_data;
    const BYTE* uv_plane = raw_data + stride * height;

    for (UINT row = 0; row < height; ++row) {
        const BYTE* y_row = y_plane + row * stride;
        const BYTE* uv_row = uv_plane + (row / 2) * stride;
        uint8_t* bgra_row = out_frame.bgra.data() + row * bgra_stride;

        for (UINT col = 0; col < width; ++col) {
            const int y = y_row[col];
            const int u = uv_row[(col & ~1u)] - 128;
            const int v = uv_row[(col & ~1u) + 1] - 128;

            // BT.601 full-range (verified to produce correct images)
            int r = y + ((359 * v) >> 8);
            int g = y - ((88 * u + 183 * v) >> 8);
            int b = y + ((454 * u) >> 8);

            bgra_row[col * 4 + 0] = static_cast<uint8_t>((b < 0) ? 0 : (b > 255) ? 255 : b);
            bgra_row[col * 4 + 1] = static_cast<uint8_t>((g < 0) ? 0 : (g > 255) ? 255 : g);
            bgra_row[col * 4 + 2] = static_cast<uint8_t>((r < 0) ? 0 : (r > 255) ? 255 : r);
            bgra_row[col * 4 + 3] = 255;
        }
    }

    buffer->Unlock();
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
    output_type_set_ = false;
    output_stride_ = 0;
    first_frame_logged_ = false;
    initialized_ = false;
}

} // namespace reflection
