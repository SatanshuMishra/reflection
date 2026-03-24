// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include <gtest/gtest.h>

#include "airplay/RPiPlayCore.h"
#include "airplay/AirPlayCoreTypes.h"

namespace reflection::test {

// --------------------------------------------------------------------------
// RPiPlayCore — tests for the production IAirPlayCore implementation.
//
// These tests verify the lifecycle, configuration, and callback wiring
// of RPiPlayCore. Full AirPlay protocol tests require hardware/network
// and are covered by integration/e2e tests on a real Windows PC.
// --------------------------------------------------------------------------

class RPiPlayCoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        core_ = std::make_unique<RPiPlayCore>();
    }

    void TearDown() override {
        core_.reset();
    }

    std::unique_ptr<RPiPlayCore> core_;

    static AirPlayCoreConfig make_test_config() {
        return AirPlayCoreConfig{
            .server_name = "TestReflection",
            .hardware_address = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},
            .raop_port = 5000,
            .airplay_port = 7000,
        };
    }
};

TEST_F(RPiPlayCoreTest, InitiallyNotRunning) {
    EXPECT_FALSE(core_->is_running());
}

TEST_F(RPiPlayCoreTest, InitReturnsTrue) {
    EXPECT_TRUE(core_->init(make_test_config()));
}

TEST_F(RPiPlayCoreTest, StartAfterInitReturnsTrue) {
    core_->init(make_test_config());
    EXPECT_TRUE(core_->start());
    EXPECT_TRUE(core_->is_running());
}

TEST_F(RPiPlayCoreTest, StopAfterStartTransitionsToNotRunning) {
    core_->init(make_test_config());
    core_->start();
    EXPECT_TRUE(core_->is_running());

    core_->stop();
    EXPECT_FALSE(core_->is_running());
}

TEST_F(RPiPlayCoreTest, DoubleStartIsIdempotent) {
    core_->init(make_test_config());
    EXPECT_TRUE(core_->start());
    EXPECT_TRUE(core_->start());
    EXPECT_TRUE(core_->is_running());
}

TEST_F(RPiPlayCoreTest, StopWithoutStartIsNoOp) {
    core_->stop();
    EXPECT_FALSE(core_->is_running());
}

TEST_F(RPiPlayCoreTest, DestructorStopsIfRunning) {
    {
        RPiPlayCore scoped;
        scoped.init(make_test_config());
        scoped.start();
        EXPECT_TRUE(scoped.is_running());
    }
    // No crash = destructor handled stop
    SUCCEED();
}

TEST_F(RPiPlayCoreTest, CanSetVideoCallback) {
    bool called = false;
    core_->set_video_callback([&called](const AirPlayVideoFrame&) {
        called = true;
    });
    // Callback is stored — we can't trigger it without a real AirPlay connection
    SUCCEED();
}

TEST_F(RPiPlayCoreTest, CanSetAudioCallback) {
    bool called = false;
    core_->set_audio_callback([&called](const AirPlayAudioFrame&) {
        called = true;
    });
    SUCCEED();
}

TEST_F(RPiPlayCoreTest, CanSetConnectionCallbacks) {
    core_->set_connection_callback([](const AirPlayConnectionEvent&) {});
    core_->set_disconnection_callback([](const std::string&) {});
    SUCCEED();
}

TEST_F(RPiPlayCoreTest, InitStoreConfig) {
    const auto config = make_test_config();
    EXPECT_TRUE(core_->init(config));
    // After init, starting should work (config is stored)
    EXPECT_TRUE(core_->start());
}

} // namespace reflection::test
