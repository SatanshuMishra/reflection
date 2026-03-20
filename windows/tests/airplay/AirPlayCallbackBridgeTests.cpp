#include <gtest/gtest.h>

#include "airplay/AirPlayCallbackBridge.h"
#include "airplay/AirPlayCoreTypes.h"

using namespace reflection;

/// Fixture that unconditionally resets all AirPlayCallbackBridge global
/// state before and after each test, preventing cross-test interference.
class AirPlayCallbackBridgeTest : public ::testing::Test {
protected:
    void SetUp() override { reset_all(); }
    void TearDown() override { reset_all(); }

    static void reset_all() {
        AirPlayCallbackBridge::clear_context();
        AirPlayCallbackBridge::set_video_callback(nullptr);
        AirPlayCallbackBridge::set_audio_callback(nullptr);
        AirPlayCallbackBridge::set_connection_callback(nullptr);
        AirPlayCallbackBridge::set_disconnection_callback(nullptr);
    }

    // Dummy context — any non-null address enables callbacks
    int dummy_context_ = 42;
};

// ---------------------------------------------------------------------------
// Context pointer management
// ---------------------------------------------------------------------------

TEST_F(AirPlayCallbackBridgeTest, SetAndClearContext) {
    AirPlayCallbackBridge::set_context(&dummy_context_);
    EXPECT_TRUE(AirPlayCallbackBridge::has_context());

    AirPlayCallbackBridge::clear_context();
    EXPECT_FALSE(AirPlayCallbackBridge::has_context());
}

TEST_F(AirPlayCallbackBridgeTest, ClearWithoutSetIsNoop) {
    AirPlayCallbackBridge::clear_context();
    EXPECT_FALSE(AirPlayCallbackBridge::has_context());
}

// ---------------------------------------------------------------------------
// Video frame routing
// ---------------------------------------------------------------------------

TEST_F(AirPlayCallbackBridgeTest, VideoFrameRoutedToCallback) {
    bool called = false;
    size_t received_size = 0;
    uint64_t received_ts = 0;

    AirPlayCallbackBridge::set_video_callback(
        [&](const AirPlayVideoFrame& frame) {
            called = true;
            received_size = frame.size;
            received_ts = frame.timestamp;
        });

    AirPlayCallbackBridge::set_context(&dummy_context_);

    uint8_t data[] = {0x00, 0x00, 0x00, 0x01, 0x67};
    AirPlayCallbackBridge::on_video_frame(data, sizeof(data), 12345);

    EXPECT_TRUE(called);
    EXPECT_EQ(received_size, sizeof(data));
    EXPECT_EQ(received_ts, 12345);
}

TEST_F(AirPlayCallbackBridgeTest, VideoFrameWithoutContextIsNoop) {
    bool called = false;
    AirPlayCallbackBridge::set_video_callback(
        [&](const AirPlayVideoFrame&) { called = true; });

    // No context set — callback should not fire
    uint8_t data[] = {0x01};
    AirPlayCallbackBridge::on_video_frame(data, 1, 0);
    EXPECT_FALSE(called);
}

// ---------------------------------------------------------------------------
// Audio frame routing
// ---------------------------------------------------------------------------

TEST_F(AirPlayCallbackBridgeTest, AudioFrameRoutedToCallback) {
    bool called = false;
    size_t received_size = 0;

    AirPlayCallbackBridge::set_audio_callback(
        [&](const AirPlayAudioFrame& frame) {
            called = true;
            received_size = frame.size;
        });

    AirPlayCallbackBridge::set_context(&dummy_context_);

    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    AirPlayCallbackBridge::on_audio_frame(data, sizeof(data), 99999);

    EXPECT_TRUE(called);
    EXPECT_EQ(received_size, sizeof(data));
}

TEST_F(AirPlayCallbackBridgeTest, AudioFrameWithoutContextIsNoop) {
    bool called = false;
    AirPlayCallbackBridge::set_audio_callback(
        [&](const AirPlayAudioFrame&) { called = true; });

    uint8_t data[] = {0x01};
    AirPlayCallbackBridge::on_audio_frame(data, 1, 0);
    EXPECT_FALSE(called);
}

// ---------------------------------------------------------------------------
// Connection event routing
// ---------------------------------------------------------------------------

TEST_F(AirPlayCallbackBridgeTest, ConnectionEventRouted) {
    std::string received_id;
    std::string received_name;

    AirPlayCallbackBridge::set_connection_callback(
        [&](const AirPlayConnectionEvent& event) {
            received_id = event.device_id;
            received_name = event.device_name;
        });

    AirPlayCallbackBridge::set_context(&dummy_context_);

    AirPlayCallbackBridge::on_client_connected("iPad-ABC", "My iPad Pro");

    EXPECT_EQ(received_id, "iPad-ABC");
    EXPECT_EQ(received_name, "My iPad Pro");
}

// ---------------------------------------------------------------------------
// Disconnection event routing
// ---------------------------------------------------------------------------

TEST_F(AirPlayCallbackBridgeTest, DisconnectionEventRouted) {
    std::string received_id;

    AirPlayCallbackBridge::set_disconnection_callback(
        [&](const std::string& id) { received_id = id; });

    AirPlayCallbackBridge::set_context(&dummy_context_);

    AirPlayCallbackBridge::on_client_disconnected("iPad-ABC");

    EXPECT_EQ(received_id, "iPad-ABC");
}

// ---------------------------------------------------------------------------
// Null callback is safe
// ---------------------------------------------------------------------------

TEST_F(AirPlayCallbackBridgeTest, NullCallbacksDoNotCrash) {
    AirPlayCallbackBridge::set_context(&dummy_context_);

    // No callbacks set — these should not crash
    uint8_t data[] = {0x01};
    AirPlayCallbackBridge::on_video_frame(data, 1, 0);
    AirPlayCallbackBridge::on_audio_frame(data, 1, 0);
    AirPlayCallbackBridge::on_client_connected("id", "name");
    AirPlayCallbackBridge::on_client_disconnected("id");
}
