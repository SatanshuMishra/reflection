#include "pipeline/GStreamerPipeline.h"
#include "utilities/Logger.h"

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/videooverlay.h>

#include <cstring>

namespace reflection {

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
        Logger::info("No hardware decoder — using software decode (avdec_h264)");
    }

    // Build video pipeline
    if (!build_video_pipeline()) {
        Logger::error("Failed to build video pipeline");
        return false;
    }

    // Build audio pipeline (optional — don't fail if unavailable)
    if (!build_audio_pipeline()) {
        Logger::warn("Audio pipeline not available — video only");
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

    // Configure appsrc
    g_object_set(video_appsrc_,
        "stream-type", 0,  // GST_APP_STREAM_TYPE_STREAM
        "format", GST_FORMAT_TIME,
        "is-live", TRUE,
        "max-bytes", static_cast<guint64>(1024 * 1024),  // 1MB buffer
        nullptr);

    // Set caps for H.264 byte-stream
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

    // Create decoder (hardware or software)
    GstElement* decoder = nullptr;
    if (has_hw_decoder_) {
        decoder = gst_element_factory_make("d3d11h264dec", "video-decoder");
        if (!decoder) {
            Logger::warn("d3d11h264dec creation failed despite probe — falling back to software");
            has_hw_decoder_ = false;
        }
    }
    if (!decoder) {
        decoder = gst_element_factory_make("avdec_h264", "video-decoder");
        if (!decoder) {
            Logger::error("Failed to create any H.264 decoder (tried d3d11h264dec and avdec_h264)");
            return false;
        }
    }

    // Create video sink (d3d11videosink for Windows)
    video_sink_ = gst_element_factory_make("d3d11videosink", "video-sink");
    if (!video_sink_) {
        // Fallback to auto video sink
        Logger::warn("d3d11videosink not available — trying autovideosink");
        video_sink_ = gst_element_factory_make("autovideosink", "video-sink");
    }
    if (!video_sink_) {
        Logger::error("Failed to create any video sink");
        return false;
    }

    // Configure video sink
    g_object_set(video_sink_, "sync", FALSE, nullptr);  // Low-latency: don't sync to clock

    if (has_hw_decoder_) {
        // Hardware path: appsrc → h264parse → d3d11h264dec → d3d11videosink
        // Zero-copy: decoded NV12 stays on GPU, d3d11videosink renders directly
        gst_bin_add_many(GST_BIN(pipeline_),
            video_appsrc_, h264parse, decoder, video_sink_, nullptr);

        if (!gst_element_link_many(video_appsrc_, h264parse, decoder, video_sink_, nullptr)) {
            Logger::error("Failed to link hardware video pipeline");
            return false;
        }
    } else {
        // Software path: appsrc → h264parse → avdec_h264 → videoconvert → d3d11videosink
        GstElement* videoconvert = gst_element_factory_make("videoconvert", "video-convert");
        if (!videoconvert) {
            Logger::error("Failed to create videoconvert element");
            return false;
        }

        gst_bin_add_many(GST_BIN(pipeline_),
            video_appsrc_, h264parse, decoder, videoconvert, video_sink_, nullptr);

        if (!gst_element_link_many(video_appsrc_, h264parse, decoder, videoconvert, video_sink_, nullptr)) {
            Logger::error("Failed to link software video pipeline");
            return false;
        }
    }

    // Set the window handle on the video sink for overlay rendering
    if (window_handle_ && GST_IS_VIDEO_OVERLAY(video_sink_)) {
        gst_video_overlay_set_window_handle(
            GST_VIDEO_OVERLAY(video_sink_),
            reinterpret_cast<guintptr>(window_handle_));
        Logger::info("GStreamer video overlay attached to HWND");
    }

    Logger::info("Video pipeline built: appsrc → h264parse → {} → {}",
                 has_hw_decoder_ ? "d3d11h264dec" : "avdec_h264 → videoconvert",
                 "d3d11videosink");
    return true;
}

bool GStreamerPipeline::build_audio_pipeline() {
    // Audio pipeline: appsrc → aacparse → avdec_aac → audioconvert → audioresample → wasapisink
    audio_appsrc_ = gst_element_factory_make("appsrc", "audio-appsrc");
    if (!audio_appsrc_) return false;

    // Configure audio appsrc
    g_object_set(audio_appsrc_,
        "stream-type", 0,
        "format", GST_FORMAT_TIME,
        "is-live", TRUE,
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
        Logger::warn("wasapisink not available — trying autoaudiosink");
        audiosink = gst_element_factory_make("autoaudiosink", "audio-sink");
    }

    if (!aacparse || !aacdec || !audioconvert || !audioresample || !audiosink) {
        Logger::warn("Some audio elements not available — audio disabled");
        // Clean up any that were created
        if (audio_appsrc_) { gst_object_unref(audio_appsrc_); audio_appsrc_ = nullptr; }
        if (aacparse) gst_object_unref(aacparse);
        if (aacdec) gst_object_unref(aacdec);
        if (audioconvert) gst_object_unref(audioconvert);
        if (audioresample) gst_object_unref(audioresample);
        if (audiosink) gst_object_unref(audiosink);
        return false;
    }

    g_object_set(audiosink, "sync", FALSE, nullptr);

    gst_bin_add_many(GST_BIN(pipeline_),
        audio_appsrc_, aacparse, aacdec, audioconvert, audioresample, audiosink, nullptr);

    if (!gst_element_link_many(audio_appsrc_, aacparse, aacdec, audioconvert, audioresample, audiosink, nullptr)) {
        Logger::warn("Failed to link audio pipeline — audio disabled");
        return false;
    }

    Logger::info("Audio pipeline built: appsrc → aacparse → avdec_aac → wasapisink");
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

    // Transition to PLAYING
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        Logger::error("Failed to set GStreamer pipeline to PLAYING state");
        return;
    }

    playing_.store(true);

    // Start GLib main loop on dedicated thread for bus message handling
    main_loop_ = g_main_loop_new(nullptr, FALSE);
    gst_thread_ = std::thread(&GStreamerPipeline::run_main_loop, this);

    Logger::info("GStreamer pipeline started");
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
    audio_appsrc_ = nullptr;
    video_sink_ = nullptr;

    Logger::info("GStreamer pipeline stopped");
}

void GStreamerPipeline::push_video_data(const uint8_t* data, size_t size,
                                         uint64_t timestamp) {
    if (!video_appsrc_ || !playing_.load()) return;

    // Allocate GstBuffer and copy data (GStreamer takes ownership)
    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, size, nullptr);
    if (!buffer) {
        Logger::warn("Failed to allocate GstBuffer for video frame (size={})", size);
        return;
    }

    // Copy NAL unit data into the buffer
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
        std::memcpy(map.data, data, size);
        gst_buffer_unmap(buffer, &map);
    }

    // Set timestamp (convert NTP microseconds to GStreamer nanoseconds)
    GST_BUFFER_PTS(buffer) = timestamp * 1000;  // µs → ns
    GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;

    // Push to appsrc (thread-safe — GstAppSrc handles locking internally)
    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(video_appsrc_), buffer);
    if (ret != GST_FLOW_OK) {
        // Buffer was consumed by push_buffer even on error — don't unref
        static uint64_t error_count = 0;
        if (++error_count % 100 == 1) {
            Logger::warn("gst_app_src_push_buffer failed: {} (count={})",
                         static_cast<int>(ret), error_count);
        }
    }
}

void GStreamerPipeline::push_audio_data(const uint8_t* data, size_t size,
                                         uint64_t timestamp) {
    if (!audio_appsrc_ || !playing_.load()) return;

    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, size, nullptr);
    if (!buffer) return;

    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
        std::memcpy(map.data, data, size);
        gst_buffer_unmap(buffer, &map);
    }

    GST_BUFFER_PTS(buffer) = timestamp * 1000;
    GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;

    gst_app_src_push_buffer(GST_APP_SRC(audio_appsrc_), buffer);
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
