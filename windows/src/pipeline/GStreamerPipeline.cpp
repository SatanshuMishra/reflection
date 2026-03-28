// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "pipeline/GStreamerPipeline.h"
#include "utilities/Logger.h"
#include "utilities/WinUtils.h"

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/videooverlay.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <dxgi1_2.h>

#pragma comment(lib, "dxgi.lib")

#include <atomic>
#include <cstring>
#include <string>

namespace reflection {

namespace {

/// Find the first DXGI adapter that is a real hardware GPU, not a virtual
/// display adapter (RDP, Hyper-V, software renderer). Returns the adapter
/// index for d3d11videosink's "adapter" property, or -1 if no hardware
/// adapter is found.
///
/// When connected via RDP, DXGI may enumerate the Microsoft Remote Display
/// Adapter before the physical GPU. d3d11videosink auto-selects the first
/// adapter, which on RDP may be the virtual one — its swap chain presentation
/// silently fails (DXGI_STATUS_OCCLUDED). Explicitly selecting the physical
/// GPU makes rendering work on both console and RDP sessions.
int find_hardware_adapter_index() {
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                   reinterpret_cast<void**>(&factory)))) {
        return -1;
    }

    int result = -1;
    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0;
         factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND;
         ++i) {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        adapter->Release();

        const bool is_software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
        const std::wstring name(desc.Description);

        Logger::debug("DXGI adapter {}: '{}' (software={}, VRAM={}MB)",
                      i, win_utils::wide_to_utf8(name), is_software,
                      desc.DedicatedVideoMemory / (1024 * 1024));

        // Skip virtual/software adapters
        if (is_software) continue;
        if (name.find(L"Remote Display") != std::wstring::npos) continue;
        if (name.find(L"Basic Render") != std::wstring::npos) continue;
        if (name.find(L"Hyper-V") != std::wstring::npos) continue;

        if (result < 0) {
            result = static_cast<int>(i);
        }
    }

    factory->Release();
    return result;
}

} // anonymous namespace

GStreamerPipeline::~GStreamerPipeline() {
    stop();
}

bool GStreamerPipeline::init(HWND window_handle) {
    if (pipeline_) {
        Logger::warn("GStreamerPipeline::init called when already initialized");
        return true;
    }

    window_handle_ = window_handle;

    // Check for hardware decoder availability
    has_hw_decoder_ = probe_hw_decoder();
    if (has_hw_decoder_) {
        Logger::info("Hardware H.264 decoder (d3d11h264dec) available");
    } else {
        Logger::info("No hardware decoder -- using software decode (avdec_h264)");
    }

    // Build video pipeline
    if (!build_video_pipeline()) {
        Logger::error("Failed to build video pipeline");
        return false;
    }

    // Audio in a SEPARATE pipeline — prevents audio preroll from blocking video.
    if (!build_audio_pipeline()) {
        Logger::warn("Audio pipeline not available -- video only");
    }

    // Set up bus watch for error/EOS messages
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
    gst_bus_add_watch(bus, reinterpret_cast<GstBusFunc>(&on_bus_message), this);
    gst_object_unref(bus);

    Logger::info("GStreamer pipeline initialized (hw_decode={})", has_hw_decoder_);
    return true;
}

bool GStreamerPipeline::build_video_pipeline() {
    // Create the master pipeline
    pipeline_ = gst_pipeline_new("reflection-pipeline");
    if (!pipeline_) {
        Logger::error("Failed to create GStreamer pipeline");
        return false;
    }

    // Create appsrc — entry point for H.264 NAL units from AirPlay
    video_appsrc_ = gst_element_factory_make("appsrc", "video-appsrc");
    if (!video_appsrc_) {
        Logger::error("Failed to create appsrc element");
        return false;
    }

    // Configure appsrc for live H.264 streaming
    g_object_set(video_appsrc_,
        "stream-type", 0,  // GST_APP_STREAM_TYPE_STREAM
        "format", GST_FORMAT_TIME,
        "is-live", TRUE,
        "do-timestamp", TRUE,
        "max-bytes", static_cast<guint64>(4 * 1024 * 1024),
        nullptr);

    // H.264 Annex B byte stream with AU alignment
    GstCaps* video_caps = gst_caps_new_simple("video/x-h264",
        "stream-format", G_TYPE_STRING, "byte-stream",
        "alignment", G_TYPE_STRING, "au",
        nullptr);
    g_object_set(video_appsrc_, "caps", video_caps, nullptr);
    gst_caps_unref(video_caps);

    // Create H.264 parser
    GstElement* h264parse = gst_element_factory_make("h264parse", "h264-parse");
    if (!h264parse) {
        Logger::error("Failed to create h264parse element");
        return false;
    }

    // Create decoder — use software for reliability testing
    GstElement* decoder = nullptr;
    if (has_hw_decoder_) {
        decoder = gst_element_factory_make("d3d11h264dec", "video-decoder");
    }
    if (!decoder) {
        decoder = gst_element_factory_make("avdec_h264", "video-decoder");
        has_hw_decoder_ = false;
    }
    if (!decoder) {
        Logger::error("Failed to create any H.264 decoder");
        return false;
    }

    // Video convert (needed for SW decode → d3d11videosink format conversion)
    GstElement* videoconvert = gst_element_factory_make("videoconvert", "video-convert");

    // d3d11videosink (DXGI) for all sessions — console and RDP.
    video_sink_ = gst_element_factory_make("d3d11videosink", "video-sink");
    if (!video_sink_) {
        Logger::error("d3d11videosink unavailable -- "
                       "check GPU drivers and GStreamer installation");
        return false;
    }

    // Explicitly select the hardware GPU adapter. Under RDP, DXGI may
    // enumerate the Microsoft Remote Display Adapter first — its swap chain
    // presentation silently fails. Targeting the physical GPU ensures
    // rendering works on both console and RDP sessions.
    const int hw_adapter = find_hardware_adapter_index();
    if (hw_adapter >= 0) {
        g_object_set(video_sink_, "adapter", hw_adapter, nullptr);
        Logger::info("Using d3d11videosink (adapter={})", hw_adapter);
    } else {
        Logger::warn("No hardware DXGI adapter found -- using default adapter");
    }

    // Low-latency: don't sync to pipeline clock
    g_object_set(video_sink_, "sync", FALSE, nullptr);

    // Set the window handle for overlay rendering
    if (window_handle_ && GST_IS_VIDEO_OVERLAY(video_sink_)) {
        gst_video_overlay_set_window_handle(
            GST_VIDEO_OVERLAY(video_sink_),
            reinterpret_cast<guintptr>(window_handle_));
        Logger::info("GStreamer video overlay attached to HWND");
    }

    // Build pipeline: appsrc → h264parse → decoder → [videoconvert] → sink
    if (videoconvert) {
        gst_bin_add_many(GST_BIN(pipeline_),
            video_appsrc_, h264parse, decoder, videoconvert, video_sink_, nullptr);
        if (!gst_element_link_many(video_appsrc_, h264parse, decoder, videoconvert, video_sink_, nullptr)) {
            Logger::error("Failed to link video pipeline (with videoconvert)");
            return false;
        }
    } else {
        gst_bin_add_many(GST_BIN(pipeline_),
            video_appsrc_, h264parse, decoder, video_sink_, nullptr);
        if (!gst_element_link_many(video_appsrc_, h264parse, decoder, video_sink_, nullptr)) {
            Logger::error("Failed to link video pipeline");
            return false;
        }
    }

    Logger::info("Video pipeline built: appsrc → h264parse → {} → {} → d3d11videosink",
                 has_hw_decoder_ ? "d3d11h264dec" : "avdec_h264",
                 videoconvert ? "videoconvert" : "(direct)");
    return true;
}

bool GStreamerPipeline::build_audio_pipeline() {
    // Audio in a SEPARATE GStreamer pipeline to avoid blocking video's
    // PAUSED→PLAYING transition (audio data arrives later than video).
    audio_pipeline_ = gst_pipeline_new("reflection-audio-pipeline");
    if (!audio_pipeline_) return false;

    audio_appsrc_ = gst_element_factory_make("appsrc", "audio-appsrc");
    if (!audio_appsrc_) {
        gst_object_unref(audio_pipeline_);
        audio_pipeline_ = nullptr;
        return false;
    }

    g_object_set(audio_appsrc_,
        "stream-type", 0,
        "format", GST_FORMAT_TIME,
        "is-live", TRUE,
        "do-timestamp", TRUE,
        nullptr);

    GstCaps* audio_caps = gst_caps_new_simple("audio/mpeg",
        "mpegversion", G_TYPE_INT, 4,
        "stream-format", G_TYPE_STRING, "raw",
        "channels", G_TYPE_INT, 2,
        "rate", G_TYPE_INT, 44100,
        nullptr);
    g_object_set(audio_appsrc_, "caps", audio_caps, nullptr);
    gst_caps_unref(audio_caps);

    GstElement* aacparse = gst_element_factory_make("aacparse", "aac-parse");
    GstElement* aacdec = gst_element_factory_make("avdec_aac", "aac-decoder");
    GstElement* audioconvert = gst_element_factory_make("audioconvert", "audio-convert");
    GstElement* audioresample = gst_element_factory_make("audioresample", "audio-resample");
    GstElement* audiosink = gst_element_factory_make("wasapisink", "audio-sink");

    if (!audiosink) {
        audiosink = gst_element_factory_make("autoaudiosink", "audio-sink");
    }

    if (!aacparse || !aacdec || !audioconvert || !audioresample || !audiosink) {
        Logger::warn("Some audio elements not available -- audio disabled");
        if (audio_appsrc_) { gst_object_unref(audio_appsrc_); audio_appsrc_ = nullptr; }
        if (aacparse) gst_object_unref(aacparse);
        if (aacdec) gst_object_unref(aacdec);
        if (audioconvert) gst_object_unref(audioconvert);
        if (audioresample) gst_object_unref(audioresample);
        if (audiosink) gst_object_unref(audiosink);
        gst_object_unref(audio_pipeline_);
        audio_pipeline_ = nullptr;
        return false;
    }

    g_object_set(audiosink, "sync", FALSE, nullptr);

    gst_bin_add_many(GST_BIN(audio_pipeline_),
        audio_appsrc_, aacparse, aacdec, audioconvert, audioresample, audiosink, nullptr);

    if (!gst_element_link_many(audio_appsrc_, aacparse, aacdec, audioconvert, audioresample, audiosink, nullptr)) {
        Logger::warn("Failed to link audio pipeline -- audio disabled");
        gst_object_unref(audio_pipeline_);
        audio_pipeline_ = nullptr;
        audio_appsrc_ = nullptr;
        return false;
    }

    // Add bus watch for audio pipeline errors (e.g., WASAPI exclusive mode)
    GstBus* audio_bus = gst_pipeline_get_bus(GST_PIPELINE(audio_pipeline_));
    gst_bus_add_watch(audio_bus, reinterpret_cast<GstBusFunc>(&on_bus_message), this);
    gst_object_unref(audio_bus);

    // Start the audio pipeline immediately — it'll stay in PAUSED until data arrives,
    // but won't block the video pipeline.
    gst_element_set_state(audio_pipeline_, GST_STATE_PLAYING);

    Logger::info("Audio pipeline built (separate): appsrc → aacparse → avdec_aac → wasapisink");
    return true;
}

bool GStreamerPipeline::probe_hw_decoder() {
    GstElementFactory* factory = gst_element_factory_find("d3d11h264dec");
    if (factory) {
        gst_object_unref(factory);
        return true;
    }
    return false;
}

void GStreamerPipeline::start() {
    if (playing_.load() || !pipeline_) return;

    Logger::info("Starting GStreamer pipeline");

    // Start GLib main loop FIRST — bus messages need it for dispatch
    main_loop_ = g_main_loop_new(nullptr, FALSE);
    gst_thread_ = std::thread(&GStreamerPipeline::run_main_loop, this);

    // Mark as playing BEFORE the state transition — the appsrc is live
    // (is-live=TRUE) and needs data to complete preroll. The RAOP thread
    // must be allowed to push frames immediately so the first keyframe
    // reaches the decoder and preroll completes.
    playing_.store(true);

    // Transition to PLAYING (async — completes when preroll frame arrives)
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        Logger::error("Failed to set GStreamer pipeline to PLAYING state");
        playing_.store(false);
        return;
    }

    Logger::info("GStreamer pipeline started (preroll pending)");
}

void GStreamerPipeline::stop() {
    if (!pipeline_) return;

    Logger::info("Stopping GStreamer pipeline");
    playing_.store(false);

    // Stop the GLib main loop
    if (main_loop_) {
        g_main_loop_quit(main_loop_);
    }

    // Wait for the main loop thread to finish
    if (gst_thread_.joinable()) {
        gst_thread_.join();
    }

    // Transition to NULL
    gst_element_set_state(pipeline_, GST_STATE_NULL);

    // Clean up
    if (main_loop_) {
        g_main_loop_unref(main_loop_);
        main_loop_ = nullptr;
    }

    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
    video_appsrc_ = nullptr;  // Owned by pipeline
    video_sink_ = nullptr;

    // Stop audio pipeline (separate from video)
    if (audio_pipeline_) {
        gst_element_set_state(audio_pipeline_, GST_STATE_NULL);
        gst_object_unref(audio_pipeline_);
        audio_pipeline_ = nullptr;
        audio_appsrc_ = nullptr;
    }

    Logger::info("GStreamer pipeline stopped");
}

void GStreamerPipeline::push_video_data(const uint8_t* data, size_t size,
                                         uint64_t timestamp) {
    // Don't gate on playing_ — GStreamer can buffer during PAUSED state.
    // The first keyframe (SPS+PPS+IDR) arrives before start() completes.
    if (!video_appsrc_) return;

    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, size, nullptr);
    if (!buffer) {
        Logger::warn("Failed to allocate GstBuffer for video frame (size={})", size);
        return;
    }

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
        gst_buffer_unref(buffer);
        Logger::warn("gst_buffer_map failed for video frame (size={})", size);
        return;
    }
    std::memcpy(map.data, data, size);
    gst_buffer_unmap(buffer, &map);

    // Let appsrc assign timestamps (do-timestamp=TRUE) for live streaming.
    // Manual NTP timestamps cause pipeline to stall on non-monotonic values.
    GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;

    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(video_appsrc_), buffer);
    if (ret != GST_FLOW_OK) {
        static std::atomic<uint64_t> error_count{0};
        uint64_t count = ++error_count;
        if (count % 100 == 1) {
            Logger::warn("gst_app_src_push_buffer failed: ret={} (count={})",
                         static_cast<int>(ret), count);
        }
    }
}

void GStreamerPipeline::push_audio_data(const uint8_t* data, size_t size,
                                         uint64_t timestamp) {
    if (!audio_appsrc_) return;

    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, size, nullptr);
    if (!buffer) return;

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
        gst_buffer_unref(buffer);
        Logger::warn("gst_buffer_map failed for audio frame (size={})", size);
        return;
    }
    std::memcpy(map.data, data, size);
    gst_buffer_unmap(buffer, &map);

    GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;

    gst_app_src_push_buffer(GST_APP_SRC(audio_appsrc_), buffer);
}

void GStreamerPipeline::set_window_handle(HWND window_handle) {
    window_handle_ = window_handle;
    if (video_sink_ && window_handle_ && GST_IS_VIDEO_OVERLAY(video_sink_)) {
        gst_video_overlay_set_window_handle(
            GST_VIDEO_OVERLAY(video_sink_),
            reinterpret_cast<guintptr>(window_handle_));
        Logger::info("Video overlay HWND updated");
    }
}

void GStreamerPipeline::flush_and_reset() {
    if (!playing_.load() || !pipeline_ || !video_appsrc_) return;

    Logger::info("Flushing video pipeline (stream discontinuity)");

    // Send flush-start + flush-stop through the video appsrc.
    // This propagates through the entire downstream chain:
    //   appsrc → h264parse → decoder → sink
    //
    // flush-start: drains all queued buffers and resets element state
    // flush-stop(reset_time=TRUE): resets the running time so the decoder
    //   accepts new timestamps from the restarted stream
    //
    // This clears stale reference frames from the H.264 decoder's DPB
    // (decoded picture buffer), preventing color artifacts and corruption
    // when the iPad restarts its video stream after lock/unlock.
    gst_element_send_event(video_appsrc_,
                            gst_event_new_flush_start());
    gst_element_send_event(video_appsrc_,
                            gst_event_new_flush_stop(TRUE));
}

bool GStreamerPipeline::is_playing() const {
    return playing_.load();
}

void GStreamerPipeline::run_main_loop() {
    Logger::info("GStreamer main loop started");
    g_main_loop_run(main_loop_);
    Logger::info("GStreamer main loop exited");
}

int GStreamerPipeline::on_bus_message(void* /*bus_ptr*/, void* msg_ptr, void* user_data) {
    auto* msg = static_cast<GstMessage*>(msg_ptr);
    auto* self = static_cast<GStreamerPipeline*>(user_data);

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr;
            gchar* debug_info = nullptr;
            gst_message_parse_error(msg, &err, &debug_info);
            Logger::error("GStreamer error: {} ({})",
                         err->message, debug_info ? debug_info : "no debug info");
            g_error_free(err);
            g_free(debug_info);
            break;
        }
        case GST_MESSAGE_WARNING: {
            GError* err = nullptr;
            gchar* debug_info = nullptr;
            gst_message_parse_warning(msg, &err, &debug_info);
            Logger::warn("GStreamer warning: {}", err->message);
            g_error_free(err);
            g_free(debug_info);
            break;
        }
        case GST_MESSAGE_STATE_CHANGED: {
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(self->pipeline_)) {
                GstState old_state, new_state, pending;
                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending);
                Logger::debug("GStreamer pipeline state: {} → {}",
                             gst_element_state_get_name(old_state),
                             gst_element_state_get_name(new_state));

                // Re-set overlay HWND when the sink reaches PAUSED/PLAYING.
                // d3d11videosink calls CreateSwapChainForHwnd during
                // READY→PAUSED — re-applying the handle ensures it targets
                // the correct child HWND after the swap chain is created.
                // Safe without a playing_ guard: gst_video_overlay_set_window_handle
                // is a no-op after GST_STATE_NULL, and stop() joins the GLib thread.
                if ((new_state == GST_STATE_PAUSED ||
                     new_state == GST_STATE_PLAYING) &&
                    self->video_sink_ &&
                    self->window_handle_ &&
                    GST_IS_VIDEO_OVERLAY(self->video_sink_)) {
                    gst_video_overlay_set_window_handle(
                        GST_VIDEO_OVERLAY(self->video_sink_),
                        reinterpret_cast<guintptr>(self->window_handle_));
                }
            }
            break;
        }
        case GST_MESSAGE_EOS:
            Logger::info("GStreamer: end of stream");
            break;
        default:
            break;
    }

    return TRUE;  // Keep watching
}

} // namespace reflection
