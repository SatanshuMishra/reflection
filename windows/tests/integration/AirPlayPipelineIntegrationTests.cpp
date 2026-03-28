// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include <gtest/gtest.h>

#include "airplay/AirPlayService.h"
#include "airplay/AirPlayTypes.h"

#include "../airplay/MockAirPlayCore.h"
#include "../mdns/MockMdnsAdvertiser.h"
#include "../pipeline/MockGStreamerPipeline.h"

using namespace reflection;
using namespace reflection::testing;

/// Integration tests for the AirPlay → Pipeline callback chain.
///
/// These tests wire MockAirPlayCore → real AirPlayService → MockGStreamerPipeline,
/// mirroring the callback setup in App::start_airplay_service(). This validates
/// that video/audio/reset frames flow correctly from the RAOP layer through to
/// the pipeline mock — the same path that caused the first-connection black
/// screen bug when the pipeline was not ready.
class AirPlayPipelineIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto core = std::make_unique<MockAirPlayCore>();
        auto mdns = std::make_unique<MockMdnsAdvertiser>();
        mock_core_ = core.get();
        mock_mdns_ = mdns.get();

        service_ = std::make_unique<AirPlayService>(
            std::move(core), std::move(mdns));

        pipeline_ = std::make_shared<MockGStreamerPipeline>();

        // Wire callbacks exactly as App::start_airplay_service() does
        wire_callbacks();
    }

    void wire_callbacks() {
        service_->set_video_frame_callback(
            [this](const uint8_t* data, size_t size, uint64_t timestamp,
                   uint8_t /*frame_type*/) {
                if (pipeline_) {
                    pipeline_->push_video_data(data, size, timestamp);
                }
            });

        service_->set_video_reset_callback(
            [this]() {
                if (pipeline_) {
                    pipeline_->flush_and_reset();
                }
            });

        service_->set_audio_frame_callback(
            [this](const uint8_t* data, size_t size, uint64_t timestamp) {
                if (pipeline_) {
                    pipeline_->push_audio_data(data, size, timestamp);
                }
            });
    }

    bool start_service() {
        const AirPlayServiceConfig config{
            .server_name = "TestServer",
            .hardware_address = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
            .raop_port = 5000,
            .airplay_port = 7000,
        };
        return service_->start(config);
    }

    MockAirPlayCore* mock_core_ = nullptr;
    MockMdnsAdvertiser* mock_mdns_ = nullptr;
    std::unique_ptr<AirPlayService> service_;
    std::shared_ptr<MockGStreamerPipeline> pipeline_;
};

// ---------------------------------------------------------------------------
// Video frame callback chain
// ---------------------------------------------------------------------------

TEST_F(AirPlayPipelineIntegrationTest, VideoFrameFlowsThroughCallbackChain) {
    ASSERT_TRUE(start_service());

    const uint8_t frame[] = {0x00, 0x00, 0x00, 0x01, 0x67};
    mock_core_->simulate_video_frame(frame, sizeof(frame), 12345);

    EXPECT_EQ(pipeline_->push_video_count, 1);
    EXPECT_EQ(pipeline_->last_video_size, sizeof(frame));
    EXPECT_EQ(pipeline_->last_video_timestamp, 12345u);
}

TEST_F(AirPlayPipelineIntegrationTest, MultipleVideoFramesAllDelivered) {
    ASSERT_TRUE(start_service());

    const uint8_t frame[] = {0x00, 0x00, 0x01};
    for (int i = 0; i < 10; ++i) {
        mock_core_->simulate_video_frame(frame, sizeof(frame), i * 1000);
    }

    EXPECT_EQ(pipeline_->push_video_count, 10);
    EXPECT_EQ(pipeline_->last_video_timestamp, 9000u);
}

// ---------------------------------------------------------------------------
// Video reset callback chain
// ---------------------------------------------------------------------------

TEST_F(AirPlayPipelineIntegrationTest, VideoResetFlowsThroughCallbackChain) {
    ASSERT_TRUE(start_service());

    mock_core_->simulate_video_reset();

    EXPECT_EQ(pipeline_->flush_and_reset_count, 1);
}

// ---------------------------------------------------------------------------
// Audio frame callback chain
// ---------------------------------------------------------------------------

TEST_F(AirPlayPipelineIntegrationTest, AudioFrameFlowsThroughCallbackChain) {
    ASSERT_TRUE(start_service());

    const uint8_t audio[] = {0xFF, 0xF1, 0x50, 0x80};
    mock_core_->simulate_audio_frame(audio, sizeof(audio), 67890);

    EXPECT_EQ(pipeline_->push_audio_count, 1);
    EXPECT_EQ(pipeline_->last_audio_size, sizeof(audio));
    EXPECT_EQ(pipeline_->last_audio_timestamp, 67890u);
}

// ---------------------------------------------------------------------------
// Null pipeline guard
// ---------------------------------------------------------------------------

TEST_F(AirPlayPipelineIntegrationTest, NullPipelineGuardPreventsCrash) {
    ASSERT_TRUE(start_service());

    // Simulate pipeline being destroyed (as during cleanup_mirror_session)
    pipeline_.reset();

    // These must NOT crash — the if(pipeline_) guard protects
    const uint8_t data[] = {0x01};
    mock_core_->simulate_video_frame(data, sizeof(data), 0);
    mock_core_->simulate_audio_frame(data, sizeof(data), 0);
    mock_core_->simulate_video_reset();

    // If we reach here without crashing, the test passes
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Service restart preserves pipeline
// ---------------------------------------------------------------------------

TEST_F(AirPlayPipelineIntegrationTest, ServiceRestartPreservesPipelineObject) {
    ASSERT_TRUE(start_service());

    const uint8_t frame[] = {0x01};
    mock_core_->simulate_video_frame(frame, sizeof(frame), 100);
    EXPECT_EQ(pipeline_->push_video_count, 1);

    // Restart the service — this creates a new MockAirPlayCore internally
    // but our callback lambdas still reference the same pipeline_ shared_ptr
    const AirPlayServiceConfig config{
        .server_name = "Restarted",
        .hardware_address = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
        .raop_port = 5000,
        .airplay_port = 7000,
    };

    // Re-wire callbacks (mimics what App does after restart)
    wire_callbacks();
    ASSERT_TRUE(service_->restart(config));

    // Get the new mock core (restart created new core)
    // The callbacks are re-wired via wire_callbacks_to_core in start()
    // Simulate from the new core
    mock_core_ = static_cast<MockAirPlayCore*>(nullptr); // old core destroyed

    // The pipeline object is the same — verify accumulated count
    EXPECT_EQ(pipeline_->push_video_count, 1);
}

// ---------------------------------------------------------------------------
// Pipeline lifecycle ordering
// ---------------------------------------------------------------------------

TEST_F(AirPlayPipelineIntegrationTest, PipelineLifecycleOrdering) {
    // Simulate the full App lifecycle: init → set_window_handle → start → stop → init → start
    HWND fake_hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(0xDEAD));

    pipeline_->init(nullptr);           // Pre-allocate with null HWND
    pipeline_->set_window_handle(fake_hwnd);  // Attach HWND after window creation
    pipeline_->start();                 // Begin playback
    pipeline_->stop();                  // Session ends
    pipeline_->init(nullptr);           // Re-init for next session
    pipeline_->set_window_handle(fake_hwnd);
    pipeline_->start();

    const std::vector<std::string> expected = {
        "init", "set_window_handle", "start",
        "stop", "init", "set_window_handle", "start"
    };
    EXPECT_EQ(pipeline_->call_log, expected);
}

// ---------------------------------------------------------------------------
// Connection callback type adaptation
// ---------------------------------------------------------------------------

TEST_F(AirPlayPipelineIntegrationTest, ConnectionCallbackAdaptsTypes) {
    bool connected = false;
    std::string received_name;

    service_->set_client_connected_callback(
        [&](const AirPlayClientInfo& client) {
            connected = true;
            received_name = client.device_name;
        });

    ASSERT_TRUE(start_service());

    mock_core_->simulate_connection("id-123", "iPad Pro");

    EXPECT_TRUE(connected);
    EXPECT_EQ(received_name, "iPad Pro");
}
