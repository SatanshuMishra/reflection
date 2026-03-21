#include "decode/MFVideoDecoder.h"
#include "utilities/Logger.h"

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mftransform.h>
#include <codecapi.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")

namespace reflection {

MFVideoDecoder::MFVideoDecoder() = default;

MFVideoDecoder::~MFVideoDecoder() {
    shutdown();
}

bool MFVideoDecoder::init(ID3D11Device* device) {
    if (initialized_) return true;
    if (!device) {
        Logger::error("MFVideoDecoder::init called with null device");
        return false;
    }

    Logger::info("Initializing Media Foundation H.264 decoder");

    device_ = device;

    // Step 1: Create DXGI device manager for DXVA2 hardware acceleration
    HRESULT hr = MFCreateDXGIDeviceManager(
        &device_manager_token_, device_manager_.GetAddressOf());
    if (FAILED(hr)) {
        Logger::error("MFCreateDXGIDeviceManager failed: 0x{:08X}", hr);
        return false;
    }

    // Step 2: Associate our D3D11 device with the manager
    hr = device_manager_->ResetDevice(device_.Get(), device_manager_token_);
    if (FAILED(hr)) {
        Logger::error("IMFDXGIDeviceManager::ResetDevice failed: 0x{:08X}", hr);
        return false;
    }

    // Step 3: Find and create the H.264 decoder MFT
    if (!create_decoder_mft()) {
        return false;
    }

    // Step 4: Enable DXVA2 on the MFT
    hr = mft_->ProcessMessage(
        MFT_MESSAGE_SET_D3D_MANAGER,
        reinterpret_cast<ULONG_PTR>(device_manager_.Get()));
    if (FAILED(hr)) {
        Logger::warn("Failed to set D3D manager on MFT: 0x{:08X} — "
                     "falling back to software decode", hr);
        // Continue without DXVA2 — software decode still works
    }

    // Step 5: Set input type (H.264)
    if (!set_input_type()) {
        return false;
    }

    // Step 6: Negotiate output type (NV12)
    if (!negotiate_output_type()) {
        return false;
    }

    // Step 7: Signal the MFT to begin streaming
    hr = mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    if (FAILED(hr)) {
        Logger::error("MFT_MESSAGE_NOTIFY_BEGIN_STREAMING failed: 0x{:08X}", hr);
        return false;
    }

    hr = mft_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    if (FAILED(hr)) {
        Logger::warn("MFT_MESSAGE_NOTIFY_START_OF_STREAM failed: 0x{:08X}", hr);
        // Non-fatal — some MFTs don't require this
    }

    initialized_ = true;
    Logger::info("Media Foundation H.264 decoder initialized (DXVA2)");
    return true;
}

bool MFVideoDecoder::create_decoder_mft() {
    // Enumerate hardware H.264 decoders
    MFT_REGISTER_TYPE_INFO input_type{};
    input_type.guidMajorType = MFMediaType_Video;
    input_type.guidSubtype = MFVideoFormat_H264;

    IMFActivate** activates = nullptr;
    UINT32 count = 0;

    HRESULT hr = MFTEnumEx(
        MFT_CATEGORY_VIDEO_DECODER,
        MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER,
        &input_type,
        nullptr,  // Any output type
        &activates,
        &count);

    if (FAILED(hr) || count == 0) {
        Logger::warn("No hardware H.264 decoder found, trying software");

        // Fall back to software decoders
        hr = MFTEnumEx(
            MFT_CATEGORY_VIDEO_DECODER,
            MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER,
            &input_type,
            nullptr,
            &activates,
            &count);

        if (FAILED(hr) || count == 0) {
            Logger::error("No H.264 decoder MFT available: 0x{:08X}", hr);
            return false;
        }
    }

    // Activate the first (best) decoder
    hr = activates[0]->ActivateObject(IID_PPV_ARGS(mft_.GetAddressOf()));

    // Release all activation objects
    for (UINT32 i = 0; i < count; ++i) {
        activates[i]->Release();
    }
    CoTaskMemFree(activates);

    if (FAILED(hr)) {
        Logger::error("Failed to activate H.264 decoder MFT: 0x{:08X}", hr);
        return false;
    }

    Logger::info("H.264 decoder MFT created");
    return true;
}

bool MFVideoDecoder::set_input_type() {
    Microsoft::WRL::ComPtr<IMFMediaType> input_type;
    HRESULT hr = MFCreateMediaType(input_type.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (FAILED(hr)) return false;

    hr = input_type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    if (FAILED(hr)) return false;

    hr = mft_->SetInputType(0, input_type.Get(), 0);
    if (FAILED(hr)) {
        Logger::error("SetInputType(H.264) failed: 0x{:08X}", hr);
        return false;
    }

    Logger::debug("MFT input type set: H.264");
    return true;
}

bool MFVideoDecoder::negotiate_output_type() {
    // Enumerate available output types and pick NV12
    for (DWORD i = 0; ; ++i) {
        Microsoft::WRL::ComPtr<IMFMediaType> output_type;
        HRESULT hr = mft_->GetOutputAvailableType(0, i, output_type.GetAddressOf());

        if (hr == MF_E_NO_MORE_TYPES) {
            break;
        }
        if (FAILED(hr)) {
            Logger::error("GetOutputAvailableType({}) failed: 0x{:08X}", i, hr);
            break;
        }

        GUID subtype{};
        hr = output_type->GetGUID(MF_MT_SUBTYPE, &subtype);
        if (FAILED(hr)) continue;

        // Prefer NV12 — it's the native DXVA2 output format
        if (subtype == MFVideoFormat_NV12) {
            hr = mft_->SetOutputType(0, output_type.Get(), 0);
            if (SUCCEEDED(hr)) {
                Logger::debug("MFT output type set: NV12");
                output_type_set_ = true;

                // Read output stride for software fallback path
                UINT32 w = 0, h = 0;
                if (SUCCEEDED(MFGetAttributeSize(
                        output_type.Get(), MF_MT_FRAME_SIZE, &w, &h))) {
                    UINT32 stride_val = 0;
                    if (SUCCEEDED(output_type->GetUINT32(
                            MF_MT_DEFAULT_STRIDE, &stride_val)) && stride_val != 0) {
                        output_stride_ = static_cast<LONG>(stride_val);
                    } else {
                        output_stride_ = static_cast<LONG>(w);
                    }
                    Logger::debug("MFT output: {}x{}, stride={}", w, h, output_stride_);
                }

                return true;
            }
        }
    }

    // If NV12 not available, try the first available type
    Microsoft::WRL::ComPtr<IMFMediaType> fallback;
    HRESULT hr = mft_->GetOutputAvailableType(0, 0, fallback.GetAddressOf());
    if (SUCCEEDED(hr)) {
        hr = mft_->SetOutputType(0, fallback.Get(), 0);
        if (SUCCEEDED(hr)) {
            GUID subtype{};
            fallback->GetGUID(MF_MT_SUBTYPE, &subtype);
            Logger::warn("Using non-NV12 output type (subtype GUID ends: ...{:08X})",
                         subtype.Data1);
            output_type_set_ = true;

            // Read stride for fallback type too
            UINT32 w = 0, h = 0;
            if (SUCCEEDED(MFGetAttributeSize(
                    fallback.Get(), MF_MT_FRAME_SIZE, &w, &h))) {
                UINT32 stride_val = 0;
                if (SUCCEEDED(fallback->GetUINT32(
                        MF_MT_DEFAULT_STRIDE, &stride_val)) && stride_val != 0) {
                    output_stride_ = static_cast<LONG>(stride_val);
                } else {
                    output_stride_ = static_cast<LONG>(w);
                }
            }

            return true;
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

    // Diagnostic: log first frame details to confirm Annex B format
    if (!first_frame_logged_) {
        first_frame_logged_ = true;
        if (size >= 5) {
            Logger::info("First H.264 frame: size={}, bytes=[{:02X} {:02X} {:02X} {:02X} {:02X}]",
                         size, data[0], data[1], data[2], data[3], data[4]);
        } else {
            Logger::info("First H.264 frame: size={}", size);
        }
    }

    // Step 1: Create IMFMediaBuffer with a copy of the NAL data
    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateMemoryBuffer(
        static_cast<DWORD>(size), buffer.GetAddressOf());
    if (FAILED(hr)) {
        Logger::error("MFCreateMemoryBuffer failed: 0x{:08X}", hr);
        return false;
    }

    BYTE* buf_ptr = nullptr;
    hr = buffer->Lock(&buf_ptr, nullptr, nullptr);
    if (FAILED(hr)) return false;

    memcpy(buf_ptr, data, size);
    buffer->Unlock();
    buffer->SetCurrentLength(static_cast<DWORD>(size));

    // Step 2: Create IMFSample and attach the buffer
    Microsoft::WRL::ComPtr<IMFSample> sample;
    hr = MFCreateSample(sample.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = sample->AddBuffer(buffer.Get());
    if (FAILED(hr)) return false;

    // Set the presentation timestamp (100-nanosecond units for MF)
    // RPiPlay provides microsecond timestamps
    sample->SetSampleTime(static_cast<LONGLONG>(timestamp) * 10);

    // Step 3: Feed the sample to the decoder
    hr = mft_->ProcessInput(0, sample.Get(), 0);

    if (hr == MF_E_NOTACCEPTING) {
        // MFT buffer is full — drain output first, then retry
        if (try_get_output_frame(out_texture)) {
            // Got a frame while draining. Try feeding input again next time.
            return true;
        }
        // Still not accepting — decoder is backed up
        return false;
    }

    if (FAILED(hr)) {
        // Log at ERROR level — this often indicates missing SPS/PPS or wrong format
        if (size >= 4) {
            Logger::error("ProcessInput failed: 0x{:08X}, size={}, "
                          "first_bytes=[{:02X} {:02X} {:02X} {:02X}]",
                          hr, size,
                          data[0], data[1], data[2], data[3]);
        } else {
            Logger::error("ProcessInput failed: 0x{:08X}, size={}", hr, size);
        }
        return false;
    }

    // Step 4: Try to get a decoded frame
    return try_get_output_frame(out_texture);
}

bool MFVideoDecoder::try_get_output_frame(
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& out_texture
) {
    MFT_OUTPUT_DATA_BUFFER output_buffer{};
    output_buffer.dwStreamID = 0;
    output_buffer.pSample = nullptr;
    output_buffer.dwStatus = 0;
    output_buffer.pEvents = nullptr;

    // Check if MFT allocates its own output samples (DXVA2 decoders do)
    MFT_OUTPUT_STREAM_INFO stream_info{};
    HRESULT hr = mft_->GetOutputStreamInfo(0, &stream_info);
    if (FAILED(hr)) return false;

    Microsoft::WRL::ComPtr<IMFSample> output_sample;
    if (!(stream_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)) {
        // MFT doesn't provide samples — we must allocate one
        hr = MFCreateSample(output_sample.GetAddressOf());
        if (FAILED(hr)) return false;

        Microsoft::WRL::ComPtr<IMFMediaBuffer> out_buf;
        hr = MFCreateMemoryBuffer(stream_info.cbSize, out_buf.GetAddressOf());
        if (FAILED(hr)) return false;

        hr = output_sample->AddBuffer(out_buf.Get());
        if (FAILED(hr)) return false;

        output_buffer.pSample = output_sample.Get();
    }

    DWORD status = 0;
    hr = mft_->ProcessOutput(0, 1, &output_buffer, &status);

    // Release events if any
    if (output_buffer.pEvents) {
        output_buffer.pEvents->Release();
    }

    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        // Decoder needs more NAL units before producing a frame
        return false;
    }

    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
        // Output format changed — renegotiate
        Logger::info("MFT stream change — renegotiating output type");
        if (!negotiate_output_type()) {
            Logger::error("Failed to renegotiate output type after stream change");
            return false;
        }
        // Retry ProcessOutput with the new type
        return try_get_output_frame(out_texture);
    }

    if (FAILED(hr)) {
        Logger::error("ProcessOutput failed: 0x{:08X}", hr);
        return false;
    }

    // Extract the texture from the output sample
    IMFSample* result_sample = output_buffer.pSample;
    if (!result_sample) return false;

    return extract_texture_from_sample(result_sample, out_texture);
}

bool MFVideoDecoder::extract_texture_from_sample(
    IMFSample* sample,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& out_texture
) {
    // Get the buffer from the sample
    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = sample->ConvertToContiguousBuffer(buffer.GetAddressOf());
    if (FAILED(hr)) {
        Logger::error("ConvertToContiguousBuffer failed: 0x{:08X}", hr);
        return false;
    }

    // Try to get a DXGI buffer (hardware-decoded texture)
    Microsoft::WRL::ComPtr<IMFDXGIBuffer> dxgi_buffer;
    hr = buffer.As(&dxgi_buffer);
    if (SUCCEEDED(hr)) {
        // Hardware path: extract the D3D11 texture directly
        hr = dxgi_buffer->GetResource(IID_PPV_ARGS(out_texture.GetAddressOf()));
        if (SUCCEEDED(hr)) {
            return true;
        }
        Logger::debug("GetResource from DXGI buffer failed: 0x{:08X}", hr);
    }

    // Software fallback: create a texture from CPU memory
    // Determine actual row pitch — MF decoders may pad rows for alignment
    LONG actual_stride = output_stride_;

    // Try IMF2DBuffer for the most accurate stride
    Microsoft::WRL::ComPtr<IMF2DBuffer> buffer_2d;
    if (SUCCEEDED(buffer.As(&buffer_2d))) {
        BYTE* scanline0 = nullptr;
        LONG pitch_2d = 0;
        if (SUCCEEDED(buffer_2d->Lock2D(&scanline0, &pitch_2d))) {
            actual_stride = (pitch_2d < 0) ? -pitch_2d : pitch_2d;
            buffer_2d->Unlock2D();
        }
    }

    BYTE* raw_data = nullptr;
    DWORD max_length = 0;
    DWORD current_length = 0;
    hr = buffer->Lock(&raw_data, &max_length, &current_length);
    if (FAILED(hr) || !raw_data) return false;

    // Get output dimensions from the current output type
    Microsoft::WRL::ComPtr<IMFMediaType> output_type;
    hr = mft_->GetOutputCurrentType(0, output_type.GetAddressOf());
    if (FAILED(hr)) {
        buffer->Unlock();
        return false;
    }

    UINT32 width = 0, height = 0;
    hr = MFGetAttributeSize(output_type.Get(), MF_MT_FRAME_SIZE, &width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        buffer->Unlock();
        return false;
    }

    // Use width as final fallback if stride is still 0
    if (actual_stride <= 0) {
        actual_stride = static_cast<LONG>(width);
    }

    // Create a staging NV12 texture and copy the data
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_NV12;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    // NV12 layout: Y plane (width*height) + UV plane (width*(height/2))
    D3D11_SUBRESOURCE_DATA init_data{};
    init_data.pSysMem = raw_data;
    init_data.SysMemPitch = static_cast<UINT>(actual_stride);

    hr = device_->CreateTexture2D(&desc, &init_data, out_texture.GetAddressOf());
    buffer->Unlock();

    if (FAILED(hr)) {
        Logger::error("CreateTexture2D (software fallback) failed: 0x{:08X}, "
                      "{}x{}, stride={}", hr, width, height, actual_stride);
        return false;
    }

    return true;
}

void MFVideoDecoder::flush() {
    if (!initialized_ || !mft_) return;

    mft_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
    Logger::debug("MFT flushed");
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
    output_stride_ = 0;
    first_frame_logged_ = false;
    initialized_ = false;
}

} // namespace reflection
