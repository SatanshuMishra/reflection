// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#pragma once

#include "airplay/IAirPlayCore.h"

namespace reflection::testing {

/// Mock AirPlay core for testing AirPlayService orchestration.
class MockAirPlayCore : public IAirPlayCore {
public:
    // Configurable behavior
    bool should_fail_init = false;
    bool should_fail_start = false;

    // Call tracking
    int init_call_count = 0;
    int start_call_count = 0;
    int stop_call_count = 0;
    int set_video_cb_count = 0;
    int set_audio_cb_count = 0;
    int set_conn_cb_count = 0;
    int set_disconn_cb_count = 0;

    // Captured config
    AirPlayCoreConfig last_config;

    // Stored callbacks (so tests can simulate events)
    VideoFrameCallback video_callback;
    AudioFrameCallback audio_callback;
    ConnectionCallback connection_callback;
    DisconnectionCallback disconnection_callback;

    // State
    bool running_ = false;
    bool initialized_ = false;

    bool init(const AirPlayCoreConfig& config) override {
        ++init_call_count;
        last_config = config;
        if (should_fail_init) return false;
        initialized_ = true;
        return true;
    }

    bool start() override {
        ++start_call_count;
        if (should_fail_start) return false;
        running_ = true;
        return true;
    }

    void stop() override {
        ++stop_call_count;
        running_ = false;
    }

    [[nodiscard]] bool is_running() const override { return running_; }

    void set_video_callback(VideoFrameCallback callback) override {
        ++set_video_cb_count;
        video_callback = std::move(callback);
    }

    void set_audio_callback(AudioFrameCallback callback) override {
        ++set_audio_cb_count;
        audio_callback = std::move(callback);
    }

    void set_connection_callback(ConnectionCallback callback) override {
        ++set_conn_cb_count;
        connection_callback = std::move(callback);
    }

    void set_disconnection_callback(DisconnectionCallback callback) override {
        ++set_disconn_cb_count;
        disconnection_callback = std::move(callback);
    }

    // --- Test helpers: simulate RPiPlay events ---

    void simulate_video_frame(const uint8_t* data, size_t size, uint64_t ts) {
        if (video_callback) {
            video_callback({data, size, ts, 0});
        }
    }

    void simulate_audio_frame(const uint8_t* data, size_t size, uint64_t ts) {
        if (audio_callback) {
            audio_callback({data, size, ts, 0, 44100, 2});
        }
    }

    void simulate_connection(const std::string& id, const std::string& name) {
        if (connection_callback) {
            connection_callback({id, name, "iPad", 1920, 1080});
        }
    }

    void simulate_disconnection(const std::string& id) {
        if (disconnection_callback) {
            disconnection_callback(id);
        }
    }
};

} // namespace reflection::testing
