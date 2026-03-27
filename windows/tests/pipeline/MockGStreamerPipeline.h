// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#include "pipeline/IGStreamerPipeline.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace reflection::testing {

/// Mock GStreamer pipeline for testing App orchestration and callback wiring.
///
/// Tracks method call order via `call_log` so tests can verify lifecycle
/// sequences (e.g., init → set_window_handle → start → stop → init).
///
/// Follows the same pattern as MockAirPlayCore: configurable failures,
/// call counters, captured state, and test helpers.
class MockGStreamerPipeline : public IGStreamerPipeline {
public:
    // --- Configurable behavior ---
    bool should_fail_init = false;

    // --- Call tracking ---
    std::vector<std::string> call_log;
    int init_call_count = 0;
    int start_call_count = 0;
    int stop_call_count = 0;
    int push_video_count = 0;
    int push_audio_count = 0;
    int set_window_handle_count = 0;
    int flush_and_reset_count = 0;

    // --- Captured state ---
    HWND last_init_hwnd = nullptr;
    HWND last_window_handle = nullptr;
    size_t last_video_size = 0;
    uint64_t last_video_timestamp = 0;
    size_t last_audio_size = 0;
    uint64_t last_audio_timestamp = 0;

    // --- Internal state ---
    bool playing_ = false;
    bool initialized_ = false;

    // --- IGStreamerPipeline overrides ---

    [[nodiscard]] bool init(HWND window_handle) override {
        call_log.push_back("init");
        ++init_call_count;
        last_init_hwnd = window_handle;
        if (should_fail_init) return false;
        initialized_ = true;
        return true;
    }

    void start() override {
        call_log.push_back("start");
        ++start_call_count;
        playing_ = true;
    }

    void stop() override {
        call_log.push_back("stop");
        ++stop_call_count;
        playing_ = false;
        // Mirror real GStreamerPipeline::stop() behavior: internal elements
        // are nulled, but the object persists. init() can be called again.
        initialized_ = false;
    }

    void push_video_data(const uint8_t* /*data*/, size_t size,
                          uint64_t timestamp) override {
        ++push_video_count;
        last_video_size = size;
        last_video_timestamp = timestamp;
    }

    void push_audio_data(const uint8_t* /*data*/, size_t size,
                          uint64_t timestamp) override {
        ++push_audio_count;
        last_audio_size = size;
        last_audio_timestamp = timestamp;
    }

    void set_window_handle(HWND window_handle) override {
        call_log.push_back("set_window_handle");
        ++set_window_handle_count;
        last_window_handle = window_handle;
    }

    void flush_and_reset() override {
        call_log.push_back("flush_and_reset");
        ++flush_and_reset_count;
    }

    [[nodiscard]] bool is_playing() const override {
        return playing_;
    }
};

} // namespace reflection::testing
