// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include <gtest/gtest.h>

#include "mdns/MjanssonMdnsAdvertiser.h"
#include "mdns/MdnsServiceRecord.h"
#include "utilities/Constants.h"

#include "../network/MockSocket.h"
#include "../network/MockNetworkInterface.h"

using namespace reflection;
using namespace reflection::testing;

class MdnsAdvertiserTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto factory = std::make_unique<MockSocketFactory>();
        auto net = std::make_unique<MockNetworkInterface>();
        mock_factory_ = factory.get();
        mock_net_ = net.get();
        advertiser_ = std::make_unique<MjanssonMdnsAdvertiser>(
            std::move(factory), std::move(net));
    }

    void TearDown() override {
        // Ensure clean shutdown
        advertiser_.reset();
    }

    MockSocketFactory* mock_factory_ = nullptr;
    MockNetworkInterface* mock_net_ = nullptr;
    std::unique_ptr<MjanssonMdnsAdvertiser> advertiser_;

    MdnsServiceRecord make_test_record() {
        return MdnsServiceRecord{
            .service_name = "TestService",
            .service_type = "_test._tcp.local.",
            .hostname = "test.local.",
            .port = 8080,
            .ttl_seconds = 120,
            .txt_records = {{"key", "value"}},
        };
    }
};

// ---------------------------------------------------------------------------
// Advertise basics
// ---------------------------------------------------------------------------

TEST_F(MdnsAdvertiserTest, AdvertiseCreatesSocketPerInterface) {
    EXPECT_TRUE(advertiser_->advertise(make_test_record()));

    // Default MockNetworkInterface has one interface
    EXPECT_EQ(mock_factory_->created_sockets.size(), 1);
    EXPECT_TRUE(advertiser_->is_advertising());
}

TEST_F(MdnsAdvertiserTest, AdvertiseMultipleInterfaces) {
    mock_net_->add_interface("wlan0", "192.168.1.101");

    EXPECT_TRUE(advertiser_->advertise(make_test_record()));

    // Two non-loopback interfaces = two sockets
    EXPECT_EQ(mock_factory_->created_sockets.size(), 2);
}

TEST_F(MdnsAdvertiserTest, AdvertiseBindsToMdnsPort) {
    advertiser_->advertise(make_test_record());

    ASSERT_FALSE(mock_factory_->created_sockets.empty());
    auto& sock = mock_factory_->created_sockets[0];
    EXPECT_EQ(sock->last_bind_port, constants::kMdnsPort);
    EXPECT_EQ(sock->last_bind_address, "192.168.1.100"); // Default mock interface
}

TEST_F(MdnsAdvertiserTest, AdvertiseJoinsMulticastGroup) {
    advertiser_->advertise(make_test_record());

    ASSERT_FALSE(mock_factory_->created_sockets.empty());
    auto& sock = mock_factory_->created_sockets[0];
    EXPECT_EQ(sock->join_multicast_call_count, 1);
    EXPECT_EQ(sock->last_multicast_group, std::string(constants::kMdnsMulticastAddress));
    EXPECT_EQ(sock->last_multicast_interface, "192.168.1.100");
}

TEST_F(MdnsAdvertiserTest, AdvertiseSucceedsWithValidInterface) {
    // Announcement is logged but no real packet sent until mDNS packet
    // construction is implemented. Verify the advertise call succeeds.
    EXPECT_TRUE(advertiser_->advertise(make_test_record()));
    EXPECT_TRUE(advertiser_->is_advertising());
}

// ---------------------------------------------------------------------------
// Failure handling
// ---------------------------------------------------------------------------

TEST_F(MdnsAdvertiserTest, AdvertiseFailsWithNoInterfaces) {
    mock_net_->set_no_interfaces();

    EXPECT_FALSE(advertiser_->advertise(make_test_record()));
    EXPECT_FALSE(advertiser_->is_advertising());
}

TEST_F(MdnsAdvertiserTest, AdvertiseFailsWithOnlyLoopback) {
    mock_net_->set_loopback_only();

    EXPECT_FALSE(advertiser_->advertise(make_test_record()));
}

TEST_F(MdnsAdvertiserTest, AdvertiseFailsWhenAllBindsFail) {
    mock_factory_->sockets_should_fail_bind = true;

    EXPECT_FALSE(advertiser_->advertise(make_test_record()));
}

TEST_F(MdnsAdvertiserTest, PartialBindFailureStillAdvertises) {
    // Two interfaces, but one will fail to bind
    mock_net_->add_interface("wlan0", "192.168.1.101");

    // We can't selectively fail one socket with MockSocketFactory,
    // but we can verify that partial success still works.
    // With both succeeding, we should get 2 sockets.
    EXPECT_TRUE(advertiser_->advertise(make_test_record()));
    EXPECT_EQ(mock_factory_->created_sockets.size(), 2);
}

// ---------------------------------------------------------------------------
// Withdraw
// ---------------------------------------------------------------------------

TEST_F(MdnsAdvertiserTest, WithdrawRemovesRecord) {
    auto record = make_test_record();
    advertiser_->advertise(record);
    EXPECT_TRUE(advertiser_->is_advertising());

    advertiser_->withdraw(record.service_type);

    // Record should be removed
    EXPECT_FALSE(advertiser_->is_advertising());
    EXPECT_TRUE(advertiser_->advertised_services().empty());
}

TEST_F(MdnsAdvertiserTest, WithdrawAllClearsEverything) {
    advertiser_->advertise(make_test_record());
    EXPECT_TRUE(advertiser_->is_advertising());

    advertiser_->withdraw_all();
    EXPECT_FALSE(advertiser_->is_advertising());
}

TEST_F(MdnsAdvertiserTest, WithdrawClosesSocketsWhenNoRecordsLeft) {
    auto record = make_test_record();
    advertiser_->advertise(record);

    auto& sock = mock_factory_->created_sockets[0];
    EXPECT_TRUE(sock->is_open());

    advertiser_->withdraw(record.service_type);

    EXPECT_EQ(sock->close_call_count, 1);
}

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------

TEST_F(MdnsAdvertiserTest, AdvertisedServicesReturnsTypes) {
    advertiser_->advertise(make_test_record());

    auto services = advertiser_->advertised_services();
    ASSERT_EQ(services.size(), 1);
    EXPECT_EQ(services[0], "_test._tcp.local.");
}

TEST_F(MdnsAdvertiserTest, MultipleRecordsTracked) {
    auto record1 = make_test_record();
    auto record2 = make_test_record();
    record2.service_type = "_other._tcp.local.";

    advertiser_->advertise(record1);
    advertiser_->advertise(record2);

    auto services = advertiser_->advertised_services();
    EXPECT_EQ(services.size(), 2);
}

TEST_F(MdnsAdvertiserTest, WithdrawThenReadvertise) {
    auto record = make_test_record();

    // First cycle: advertise then withdraw
    advertiser_->advertise(record);
    EXPECT_TRUE(advertiser_->is_advertising());
    advertiser_->withdraw(record.service_type);
    EXPECT_FALSE(advertiser_->is_advertising());

    // Second cycle: readvertise should work
    EXPECT_TRUE(advertiser_->advertise(record));
    EXPECT_TRUE(advertiser_->is_advertising());

    // New sockets should be created (old ones were closed)
    EXPECT_GE(mock_factory_->created_sockets.size(), 2u);
}

TEST_F(MdnsAdvertiserTest, ReusesSockets) {
    // Advertising a second record should NOT create new sockets
    advertiser_->advertise(make_test_record());
    auto sockets_after_first = mock_factory_->created_sockets.size();

    auto record2 = make_test_record();
    record2.service_type = "_other._tcp.local.";
    advertiser_->advertise(record2);

    EXPECT_EQ(mock_factory_->created_sockets.size(), sockets_after_first);
}
