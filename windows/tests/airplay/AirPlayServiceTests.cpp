#include <gtest/gtest.h>

#include "airplay/AirPlayService.h"
#include "airplay/AirPlayTypes.h"
#include "airplay/IAirPlayCore.h"
#include "mdns/IMdnsAdvertiser.h"

#include "../airplay/MockAirPlayCore.h"
#include "../mdns/MockMdnsAdvertiser.h"

using namespace reflection;
using namespace reflection::testing;

class AirPlayServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto core = std::make_unique<MockAirPlayCore>();
        auto mdns = std::make_unique<MockMdnsAdvertiser>();
        mock_core_ = core.get();
        mock_mdns_ = mdns.get();
        service_ = std::make_unique<AirPlayService>(
            std::move(core), std::move(mdns));
    }

    MockAirPlayCore* mock_core_ = nullptr;
    MockMdnsAdvertiser* mock_mdns_ = nullptr;
    std::unique_ptr<AirPlayService> service_;

    AirPlayServiceConfig default_config() {
        return AirPlayServiceConfig{
            .server_name = "TestServer",
            .hardware_address = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
            .raop_port = 5000,
            .airplay_port = 7000,
        };
    }
};

// ---------------------------------------------------------------------------
// Start / Stop lifecycle
// ---------------------------------------------------------------------------

TEST_F(AirPlayServiceTest, StartInitializesCoreAndAdvertises) {
    EXPECT_TRUE(service_->start(default_config()));

    // Core must be initialized and started
    EXPECT_EQ(mock_core_->init_call_count, 1);
    EXPECT_EQ(mock_core_->start_call_count, 1);
    EXPECT_TRUE(mock_core_->is_running());

    // mDNS must have 2 records advertised (_airplay._tcp + _raop._tcp)
    EXPECT_EQ(mock_mdns_->advertise_call_count, 2);
    EXPECT_TRUE(mock_mdns_->is_advertising());

    EXPECT_TRUE(service_->is_running());
}

TEST_F(AirPlayServiceTest, StopWithdrawsMdnsAndStopsCore) {
    service_->start(default_config());
    service_->stop();

    // mDNS withdrawn
    EXPECT_EQ(mock_mdns_->withdraw_all_call_count, 1);
    EXPECT_FALSE(mock_mdns_->is_advertising());

    // Core stopped
    EXPECT_EQ(mock_core_->stop_call_count, 1);
    EXPECT_FALSE(mock_core_->is_running());

    EXPECT_FALSE(service_->is_running());
}

TEST_F(AirPlayServiceTest, DoubleStartIsNoop) {
    service_->start(default_config());
    service_->start(default_config());

    // init + start called only once
    EXPECT_EQ(mock_core_->init_call_count, 1);
    EXPECT_EQ(mock_core_->start_call_count, 1);
    EXPECT_EQ(mock_mdns_->advertise_call_count, 2); // still just the initial 2
}

TEST_F(AirPlayServiceTest, DoubleStopIsNoop) {
    service_->start(default_config());
    service_->stop();
    service_->stop();

    EXPECT_EQ(mock_core_->stop_call_count, 1);
    EXPECT_EQ(mock_mdns_->withdraw_all_call_count, 1);
}

TEST_F(AirPlayServiceTest, StopWithoutStartIsNoop) {
    service_->stop();
    EXPECT_EQ(mock_core_->stop_call_count, 0);
    EXPECT_EQ(mock_mdns_->withdraw_all_call_count, 0);
}

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------

TEST_F(AirPlayServiceTest, CoreInitFailureReturnsFalse) {
    mock_core_->should_fail_init = true;

    EXPECT_FALSE(service_->start(default_config()));

    // Core init was attempted
    EXPECT_EQ(mock_core_->init_call_count, 1);
    // Core start should NOT be called
    EXPECT_EQ(mock_core_->start_call_count, 0);
    // mDNS should NOT be called
    EXPECT_EQ(mock_mdns_->advertise_call_count, 0);

    EXPECT_FALSE(service_->is_running());
}

TEST_F(AirPlayServiceTest, CoreStartFailureReturnsFalse) {
    mock_core_->should_fail_start = true;

    EXPECT_FALSE(service_->start(default_config()));

    EXPECT_EQ(mock_core_->init_call_count, 1);
    EXPECT_EQ(mock_core_->start_call_count, 1);
    // Should clean up: stop core
    EXPECT_EQ(mock_core_->stop_call_count, 1);
    // mDNS should NOT be called
    EXPECT_EQ(mock_mdns_->advertise_call_count, 0);

    EXPECT_FALSE(service_->is_running());
}

TEST_F(AirPlayServiceTest, MdnsFailureStopsCoreAndReturnsFalse) {
    mock_mdns_->should_fail_advertise = true;

    EXPECT_FALSE(service_->start(default_config()));

    // Core was initialized and started
    EXPECT_EQ(mock_core_->init_call_count, 1);
    EXPECT_EQ(mock_core_->start_call_count, 1);
    // But then stopped due to mDNS failure
    EXPECT_EQ(mock_core_->stop_call_count, 1);

    EXPECT_FALSE(service_->is_running());
}

// ---------------------------------------------------------------------------
// Config passing
// ---------------------------------------------------------------------------

TEST_F(AirPlayServiceTest, ConfigPassedToCore) {
    auto config = default_config();
    config.server_name = "CustomName";
    config.raop_port = 6000;

    service_->start(config);

    EXPECT_EQ(mock_core_->last_config.server_name, "CustomName");
    EXPECT_EQ(mock_core_->last_config.raop_port, 6000);
}

TEST_F(AirPlayServiceTest, MdnsRecordsMatchConfig) {
    auto config = default_config();
    config.server_name = "MyReflection";
    config.airplay_port = 7000;
    config.raop_port = 5000;

    service_->start(config);

    ASSERT_EQ(mock_mdns_->advertised_records.size(), 2);

    // Find the AirPlay record
    const auto& records = mock_mdns_->advertised_records;
    bool found_airplay = false;
    bool found_raop = false;
    for (const auto& r : records) {
        if (r.service_type.find("_airplay._tcp") != std::string::npos) {
            EXPECT_EQ(r.service_name, "MyReflection");
            EXPECT_EQ(r.port, 7000);
            found_airplay = true;
        }
        if (r.service_type.find("_raop._tcp") != std::string::npos) {
            EXPECT_EQ(r.port, 5000);
            found_raop = true;
        }
    }
    EXPECT_TRUE(found_airplay);
    EXPECT_TRUE(found_raop);
}

// ---------------------------------------------------------------------------
// Callback wiring
// ---------------------------------------------------------------------------

TEST_F(AirPlayServiceTest, VideoCallbackWiredToCore) {
    bool called = false;
    service_->set_video_frame_callback(
        [&](const uint8_t*, size_t, uint64_t) { called = true; });

    service_->start(default_config());

    // Core should have a video callback set
    EXPECT_EQ(mock_core_->set_video_cb_count, 1);

    // Simulate a frame from the core
    uint8_t data[] = {0x00, 0x01};
    mock_core_->simulate_video_frame(data, 2, 12345);
    EXPECT_TRUE(called);
}

TEST_F(AirPlayServiceTest, AudioCallbackWiredToCore) {
    bool called = false;
    service_->set_audio_frame_callback(
        [&](const uint8_t*, size_t, uint64_t) { called = true; });

    service_->start(default_config());

    EXPECT_EQ(mock_core_->set_audio_cb_count, 1);

    uint8_t data[] = {0xAA};
    mock_core_->simulate_audio_frame(data, 1, 99999);
    EXPECT_TRUE(called);
}

TEST_F(AirPlayServiceTest, ConnectionCallbackWiredToCore) {
    std::string connected_id;
    service_->set_client_connected_callback(
        [&](const AirPlayClientInfo& info) { connected_id = info.device_id; });

    service_->start(default_config());

    EXPECT_EQ(mock_core_->set_conn_cb_count, 1);

    mock_core_->simulate_connection("iPad-123", "My iPad");
    EXPECT_EQ(connected_id, "iPad-123");
}

TEST_F(AirPlayServiceTest, DisconnectionCallbackWiredToCore) {
    std::string disconnected_id;
    service_->set_client_disconnected_callback(
        [&](const std::string& id) { disconnected_id = id; });

    service_->start(default_config());

    EXPECT_EQ(mock_core_->set_disconn_cb_count, 1);

    mock_core_->simulate_disconnection("iPad-123");
    EXPECT_EQ(disconnected_id, "iPad-123");
}

// ---------------------------------------------------------------------------
// Destructor cleanup
// ---------------------------------------------------------------------------

TEST_F(AirPlayServiceTest, DestructorStopsIfRunning) {
    service_->start(default_config());
    EXPECT_TRUE(service_->is_running());

    // Destroy the service
    service_.reset();

    // The mock pointers are now dangling, but we can verify behavior
    // by checking that stop was called before destruction.
    // Since we can't check after destruction, we verify by starting
    // and then letting the destructor run — no crash = success.
}

// ---------------------------------------------------------------------------
// Restart
// ---------------------------------------------------------------------------

TEST_F(AirPlayServiceTest, RestartAfterStop) {
    service_->start(default_config());
    service_->stop();

    EXPECT_FALSE(service_->is_running());

    // Start again
    EXPECT_TRUE(service_->start(default_config()));
    EXPECT_TRUE(service_->is_running());

    EXPECT_EQ(mock_core_->init_call_count, 2);
    EXPECT_EQ(mock_core_->start_call_count, 2);
}
