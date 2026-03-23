#pragma once

#include "pipeline/IGStreamerPipeline.h"

#include <atomic>
#include <thread>

// Forward declare GStreamer types to avoid including gst headers in our header
typedef struct _GstElement GstElement;
typedef struct _GMainLoop GMainLoop;

namespace reflection {

/// Production GStreamer pipeline for hardware-accelerated video decode + render.
///
/// Video pipeline:
///   appsrc → h264parse → d3d11h264dec → d3d11videosink
///   (falls back to: appsrc → h264parse → avdec_h264 → videoconvert → d3d11videosink)
///
/// Audio pipeline:
///   appsrc → aacparse → avdec_aac → audioconvert → audioresample → wasapisink
///
/// Threading model:
///   - GStreamer runs its own internal threads for decode and render
///   - push_video_data/push_audio_data are called from RAOP threads (thread-safe via GstAppSrc)
///   - The GLib main loop runs on a dedicated thread for bus message handling
///   - No main thread CPU work needed (unlike the old MFVideoDecoder + D3D11Renderer)
class GStreamerPipeline : public IGStreamerPipeline {
public:
    GStreamerPipeline() = default;
    ~GStreamerPipeline() override;

    // Non-copyable
    GStreamerPipeline(const GStreamerPipeline&) = delete;
    GStreamerPipeline& operator=(const GStreamerPipeline&) = delete;

    [[nodiscard]] bool init(HWND window_handle) override;
    void start() override;
    void stop() override;
    void push_video_data(const uint8_t* data, size_t size,
                          uint64_t timestamp) override;
    void push_audio_data(const uint8_t* data, size_t size,
                          uint64_t timestamp) override;
    [[nodiscard]] bool is_playing() const override;

private:
    GstElement* pipeline_ = nullptr;        // Video pipeline
    GstElement* audio_pipeline_ = nullptr;  // Separate audio pipeline
    GstElement* video_appsrc_ = nullptr;
    GstElement* audio_appsrc_ = nullptr;
    GstElement* video_sink_ = nullptr;
    GMainLoop* main_loop_ = nullptr;

    std::thread gst_thread_;
    std::atomic<bool> playing_ = false;
    HWND window_handle_ = nullptr;

    bool has_hw_decoder_ = false;

    /// Build the video portion of the pipeline.
    [[nodiscard]] bool build_video_pipeline();

    /// Build the audio portion of the pipeline (optional).
    [[nodiscard]] bool build_audio_pipeline();

    /// Check if hardware H.264 decode is available (d3d11h264dec).
    [[nodiscard]] static bool probe_hw_decoder();

    /// GLib main loop thread entry point.
    void run_main_loop();

    /// GStreamer bus message handler.
    static int on_bus_message(void* bus, void* message, void* user_data);
};

} // namespace reflection
